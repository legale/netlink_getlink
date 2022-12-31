#include "libnl_getlink.h"
#include "list.h"

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define parse_rtattr_nested(tb, max, rta) \
	(parse_rtattr((tb), (max), RTA_DATA(rta), RTA_PAYLOAD(rta)))


static int parse_rtattr_flags(struct rtattr *tb[], int max, struct rtattr *rta,
		       int len, unsigned short flags)
{
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

static int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	return parse_rtattr_flags(tb, max, rta, len, 0);
}


/* parse arp cache netlink messages */
static ssize_t parse_nlbuf(void *buf, size_t buf_size, nlcache *cache) {
    void *p = buf; /* just a ptr to work with netlink answer data */

    struct nlmsghdr *hdr = p; /* netlink header structure */
    uint32_t len = hdr->nlmsg_len; /* netlink message length including header */
    struct ifinfomsg *msg = NLMSG_DATA(hdr); /* macro to get a ptr right after header */
    cache->nl_hdr = hdr; //save hdr ptr to cache
    uint32_t msg_len = NLMSG_LENGTH(sizeof(*msg)); /* netlink message length without header */
    p += msg_len; /* move ptr forward */
    len -= msg_len; /* count message length left */
    /* this is very first rtnetlink attribute */
    struct rtattr *rta = p;
    parse_rtattr((struct rtattr **) &cache->tb, IFLA_MAX, rta, len); /* fill tb attribute buffer */

    return hdr->nlmsg_len;
}


int addattr_l(struct nlmsghdr *n, int maxlen, int type, const void *data,
              int alen) {
    int len = RTA_LENGTH(alen);
    struct rtattr *rta;

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
        fprintf(stderr,
                "addattr_l ERROR: message exceeded bound of %d\n",
                maxlen);
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


int addattr32(struct nlmsghdr *n, int maxlen, int type, __u32 data) {
    return addattr_l(n, maxlen, type, &data, sizeof(__u32));
}


void free_netdev_list(netdev_item_s *list) {
    netdev_item_s *item, *tmp;
    list_for_each_entry_safe(item, tmp, &list->list, list) {
        free(item);
    }
}

netdev_item_s *ll_get_by_index(netdev_item_s list, unsigned int index)
{
    netdev_item_s *tmp;
    list_for_each_entry(tmp, &list.list, list) {
        if(tmp->index == index) return tmp;
    }
    
    return NULL;
}


int get_netdev(char *name, size_t name_len, netdev_item_s *list) {
    struct nlmsghdr *nl_hdr; //ptr to netlink msg header for req and answer
    ssize_t status;
    void *buf; /* ptr to a buffer */
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg m;
        char *buf[256];
    } req = {
            .nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
            .nlh.nlmsg_type = RTM_GETLINK,
            .nlh.nlmsg_flags =  NLM_F_REQUEST | NLM_F_DUMP,
            .nlh.nlmsg_pid = 0,
            .nlh.nlmsg_seq = 1,
    };
    int err = addattr32(&req.nlh, sizeof(req), IFLA_EXT_MASK, RTEXT_FILTER_VF);
    if (err) return err;


    int sd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE); /* open socket */
    /* send message */
    status = send(sd, &req, req.nlh.nlmsg_len, 0);
    if (status < 0) {
        fprintf(stderr, "error: send %zd %d\n", status, errno);
        return status;
    }

    while (1) {
        /* get an answer */
        /*first we need to find out buffer size needed */
        ssize_t expected_buf_size = 512; /* initial buffer size */
        buf = malloc(expected_buf_size); /* alloc memory for a buffer */

        /*
         * MSG_TRUNC will return data size even if buffer is smaller than data
         * MSG_PEEK will receive queue without removing that data from the queue.
         * Thus, a subsequent receive call will return the same data.
         */
        status = recv(sd, buf, expected_buf_size, MSG_TRUNC | MSG_PEEK);
        if (status < 0) {
            fprintf(stderr, "error: recv %zd %d\n", status, errno);
            free(buf);
            break; // there is no messages left to receive
        }


        if (status > expected_buf_size) {
            expected_buf_size = status; /* this is real size */
            buf = realloc(buf, expected_buf_size); /* increase buffer size */
            status = recv(sd, buf, expected_buf_size, 0); /* now we get the full message */
            if (status < 0) fprintf(stderr, "error: recv %zd %d\n", status, errno);
        }

        nl_hdr = (struct nlmsghdr *) buf;
        if (nl_hdr->nlmsg_type == NLMSG_DONE) {
            free(buf);
            break;
        }

        void *p = buf;
        size_t chunk_len = status; //received chunk length
        size_t nlmsg_len; //parsed nl msg length


        while (chunk_len) {
            netdev_item_s *dev = NULL;
            nlcache cache = {0}; //single nl message attributes table
            nlmsg_len = parse_nlbuf(p, chunk_len, (nlcache *) &cache);
            chunk_len -= nlmsg_len;
            p += nlmsg_len;
            struct ifinfomsg *msg = NLMSG_DATA(cache.nl_hdr); /* macro to get a ptr right after header */
            if (!dev) dev = calloc(1, sizeof(netdev_item_s));
            dev->index = msg->ifi_index;

            struct rtattr **tb = (struct rtattr **) &cache.tb;

            /* skip loopback device and other non ARPHRD_ETHER */
            if(msg->ifi_type != ARPHRD_ETHER){
                continue;
            }

            if (tb[IFLA_LINKINFO]) {
                struct rtattr *linkinfo[IFLA_INFO_MAX+1];
                parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb[IFLA_LINKINFO]);

                if(linkinfo[IFLA_INFO_KIND]){
                    memcpy(dev->kind, RTA_DATA(linkinfo[IFLA_INFO_KIND]), IFNAMSIZ);
                }
            }

            if (tb[IFLA_MASTER]) {
                dev->master = *(uint32_t *)RTA_DATA(tb[IFLA_MASTER]);
            }

            if (tb[IFLA_IFNAME]) {
                strcpy(dev->name, (char *) RTA_DATA(tb[IFLA_IFNAME]));
            }

            /* mac */
            if (tb[IFLA_ADDRESS]) {
                memcpy((void *) &dev->ll_addr, RTA_DATA(tb[IFLA_ADDRESS]), IFHWADDRLEN);
            }

            if (dev) list_add_tail(&dev->list, &list->list); //append dev to list tail

            if (name) {
                if (memcmp(dev->name, name, name_len) == 0) {

                }
            }
        }

        free(buf);
    }

    return 0;
}

