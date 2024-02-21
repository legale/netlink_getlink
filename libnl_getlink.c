#include "list.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h> // fchmod
#include <sys/time.h> /* timeval_t struct */
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "libnl_getlink.h"
#include "syslog.h"

#include "leak_detector_c.h"

#define parse_rtattr_nested(tb, max, rta) \
  (parse_rtattr((tb), (max), RTA_DATA(rta), RTA_PAYLOAD(rta)))

static int parse_rtattr_flags(struct rtattr *tb[], int max, struct rtattr *rta, int len, unsigned short flags) {
  // syslog2(LOG_INFO, "%s\n", __func__);
  unsigned short type;

  memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
  while (RTA_OK(rta, len)) {
    type = rta->rta_type & ~flags;
    if ((type <= max) && (!tb[type]))
      tb[type] = rta;
    rta = RTA_NEXT(rta, len);
  }
  if (len)
    fprintf(stderr, "!!!Deficit %d, rta_len=%d\n",
            len, rta->rta_len);
  return 0;
}

static int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len) {
  return parse_rtattr_flags(tb, max, rta, len, 0);
}

/* parse netlink message */
static ssize_t parse_nlbuf(struct nlmsghdr *nh, struct rtattr **tb) {
  // syslog2(LOG_INFO, "%s\n", __func__);
  unsigned int len = nh->nlmsg_len;              /* netlink message length including header */
  struct ifinfomsg *msg = NLMSG_DATA(nh);        /* macro to get a ptr right after header */
  uint32_t msg_len = NLMSG_LENGTH(sizeof(*msg)); /* netlink message length without header */
  void *p = nh;                                  /* ptr to nh */
  /* this is very first rtnetlink attribute ptr */
  struct rtattr *rta = (struct rtattr *)(p + msg_len); /* move ptr forward */
  len -= msg_len;                                      /* count message length */
  parse_rtattr(tb, IFLA_MAX, rta, len);                /* fill tb attribute buffer */
  return nh->nlmsg_len;
}

static int addattr_l(struct nlmsghdr *n, unsigned int maxlen, int type, const void *data, int alen) {
  // syslog2(LOG_INFO, "%s\n", __func__);
  int len = RTA_LENGTH(alen);
  struct rtattr *rta;

  if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
    syslog2(LOG_NOTICE, "addattr_l ERROR: message exceeded bound of %d\n", maxlen);
    return -1;
  }
  rta = NLMSG_TAIL(n);
  rta->rta_type = type;
  rta->rta_len = len;
  if (alen)
    memcpy(RTA_DATA(rta), data, alen);
  n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
  return 0;
}

int addattr32(struct nlmsghdr *n, unsigned int maxlen, int type, __u32 data) {
  return addattr_l(n, maxlen, type, &data, sizeof(__u32));
}

void free_netdev_list(netdev_item_s *list) {
  // syslog2(LOG_INFO, "%s\n", __func__);
  netdev_item_s *item, *tmp;
  list_for_each_entry_safe(item, tmp, &list->list, list) {
    free(item);
  }
}

netdev_item_s *ll_get_by_index(netdev_item_s list, int index) {
  // syslog2(LOG_INFO, "%s\n", __func__);
  netdev_item_s *tmp;
  list_for_each_entry(tmp, &list.list, list) {
    if (tmp->index == index) return tmp;
  }

  return NULL;
}

static int send_msg() {
  // syslog2(LOG_INFO, "%s\n", __func__);
  ssize_t status;
  struct {
    struct nlmsghdr nlh;
    struct ifinfomsg m;
    char *buf[256];
  } req = {
      .nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
      .nlh.nlmsg_type = RTM_GETLINK,
      .nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP | NLM_F_ACK,
      .nlh.nlmsg_pid = 0,
      .nlh.nlmsg_seq = 1,
  };
  int err = addattr32(&req.nlh, sizeof(req), IFLA_EXT_MASK, RTEXT_FILTER_VF);
  if (err) {
    syslog2(LOG_NOTICE, "%s addattr32(&req.nlh, sizeof(req), IFLA_EXT_MASK, RTEXT_FILTER_VF)\n", strerror(errno));
    return -1;
  }

  int sd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE); /* open socket */
  if (sd < 0) {
    syslog2(LOG_NOTICE, "%s socket()\n", strerror(errno));
    return -1;
  }
  fchmod(sd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  // set socket nonblocking flag
  int flags = fcntl(sd, F_GETFL, 0);
  fcntl(sd, F_SETFL, flags | O_NONBLOCK);

  // set socket timeout 100ms
  struct timeval tv = {.tv_sec = 0, .tv_usec = 100000};
  if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror("setsockopt");
    return -2;
  }

  /* send message */
  status = send(sd, &req, req.nlh.nlmsg_len, 0);
  if (status < 0) {
    status = errno;
    syslog2(LOG_NOTICE, "%s send()\n", strerror(errno));
    close(sd); /* close socket */
    return -1;
  }
  return sd;
}

static ssize_t recv_msg(int sd, void **buf) {
  // syslog2(LOG_INFO, "%s\n", __func__);
  ssize_t bufsize = 512;
  *buf = malloc(bufsize);
  struct iovec iov = {.iov_base = *buf, .iov_len = bufsize};
  struct sockaddr_nl sa;
  struct msghdr msg = {
      .msg_name = &sa,
      .msg_namelen = sizeof(sa),
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = NULL,
      .msg_controllen = 0,
      .msg_flags = 0};

  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(sd, &readset);
  struct timeval timeout = {.tv_sec = 1, .tv_usec = 0}; // 1 sec. timeout
  int select_result = select(sd + 1, &readset, NULL, NULL, &timeout);
  if (select_result == -1) return select_result; // error
  if (select_result == 0) return 0;              // no data

  ssize_t len = recvmsg(sd, &msg, MSG_PEEK | MSG_TRUNC | MSG_DONTWAIT); // MSG_DONTWAIT to enable non-blocking mode
  if (len <= 0) return len;

  if (len > bufsize) {
    bufsize = len;
    *buf = realloc(*buf, bufsize);
    iov.iov_base = *buf;
    iov.iov_len = bufsize;
  }
  len = recvmsg(sd, &msg, MSG_DONTWAIT);
  return len;
}

static int parse_recv_chunk(void *buf, ssize_t len, netdev_item_s *list) {
  // syslog2(LOG_INFO, "%s\n", __func__);
  size_t counter = 0;
  struct nlmsghdr *nh;

  /*check length */
  if (len <= 0) {
    return -1;
  }

  for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
    if (counter > 100) {
      syslog2(LOG_ALERT, "counter %zu > 100\n", counter);
      break;
    }

    // syslog2(LOG_DEBUG, "cnt: %zu msg type len: %d NLMSG len: %zu FLAGS NLM_F_MULTI: %s\n", counter++, nh->nlmsg_type, (size_t)nh->nlmsg_len, nh->nlmsg_flags & NLM_F_MULTI ? "true" : "false");

    /* The end of multipart message */
    if (nh->nlmsg_type == NLMSG_DONE) {
      // syslog2(LOG_DEBUG, "NLMSG_DONE\n");
      return -1;
    }

    /* Error handling */
    if (nh->nlmsg_type == NLMSG_ERROR) {
      // syslog2(LOG_DEBUG, "NLMSG_ERROR\n");
      continue;
    }

    struct rtattr *tb[IFLA_MAX + 1] = {0};
		(void)parse_nlbuf(nh, tb);
    //ssize_t nlmsg_len = parse_nlbuf(nh, tb);
		//syslog2(LOG_INFO, "parsed nlmsg_len: %zd\n", nlmsg_len);

    netdev_item_s *dev = NULL;
    struct ifinfomsg *msg = NLMSG_DATA(nh); /* macro to get a ptr right after header */
    /* skip loopback device and other non ARPHRD_ETHER */

    if (msg->ifi_type != ARPHRD_ETHER) {
      continue;
    }

    if (!dev) {
      dev = calloc(1, sizeof(netdev_item_s));
      if (!dev) {
        syslog2(LOG_ALERT, "Failed to allocate memory for netdev_item_s.\n");
        return -1;
      }
    }

    dev->index = msg->ifi_index;

    if (tb[IFLA_LINKINFO]) {
      struct rtattr *linkinfo[IFLA_INFO_MAX + 1];
      parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);

      if (linkinfo[IFLA_INFO_KIND]) {
        memcpy(dev->kind, RTA_DATA(linkinfo[IFLA_INFO_KIND]), IFNAMSIZ);
      }
    }

    if (!tb[IFLA_IFNAME]) {
      syslog2(LOG_WARNING, "IFLA_IFNAME attribute is missing.\n");
      continue;
    } else {
      strcpy(dev->name, (char *)RTA_DATA(tb[IFLA_IFNAME]));
    }

    if (tb[IFLA_LINK]) {
      dev->ifla_link_idx = *(uint32_t *)RTA_DATA(tb[IFLA_LINK]);
    }

    if (tb[IFLA_MASTER]) {
      dev->master = *(uint32_t *)RTA_DATA(tb[IFLA_MASTER]);
    }

    /* mac */
    if (tb[IFLA_ADDRESS]) {
      memcpy((void *)&dev->ll_addr, RTA_DATA(tb[IFLA_ADDRESS]), IFHWADDRLEN);
    }

    if (dev) list_add_tail(&dev->list, &list->list); // append dev to list tail

    // syslog2(LOG_DEBUG, "FLAGS NLM_F_MULTI: %s\n", nh->nlmsg_flags & NLM_F_MULTI ? "true" : "false");
  }

  return 0;
}

int get_netdev(netdev_item_s *list) {
  // syslog2(LOG_DEBUG, "%s\n", __func__);
  int sd;
  void *buf;
  ssize_t len;
  /*open socket and send req */
  sd = send_msg();

  /* recv and parse kernel answers */
  int status = 0;
  while (status == 0) {
    len = recv_msg(sd, &buf);
    status = parse_recv_chunk(buf, len, list);
    free(buf);
  }

  close(sd); /* close socket */
  return 0;
}
