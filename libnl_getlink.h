#ifndef NETLINK_GET_ADDR_LIBNL_GETLINK_H
#define NETLINK_GET_ADDR_LIBNL_GETLINK_H


#include <stdint.h>
#include <stdbool.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>


#include "list.h"


#define NLMSG_TAIL(nmsg) \
    ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

typedef struct netdev_item {
    int index;
    int master; /* master device */
    int ifla_link_idx; /* ifla_link index */
    char kind[IFNAMSIZ + 1]; /* IFLA_INFO_KIND nested in rtattr IFLA_LINKINFO  */
    char name[IFNAMSIZ + 1];
    uint8_t ll_addr[IFHWADDRLEN];
    struct list_head list;
} netdev_item_s;

typedef struct nl_req {
    struct nlmsghdr hdr;
    struct rtgenmsg gen;
} nl_req_s;


int get_netdev(netdev_item_s *list);
netdev_item_s *ll_get_by_index(netdev_item_s list, int index);
void free_netdev_list(netdev_item_s *list);

#ifndef syslogwda
#define syslog2(pri, fmt, ...)                                                          \
  {                                                                                       \
    char msg[1024];                                                                       \
    snprintf(msg, 1024, "[%d] %s:%d: " fmt, getpid(), __FILE__, __LINE__, ##__VA_ARGS__); \
    syslog(pri, "%s", msg);                                                               \
    if ((setlogmask(0) & LOG_MASK(pri))) {                                                \
      printf("%s", msg);                                                                  \
    }                                                                                     \
  }
#endif /* syslogwda */


#endif //NETLINK_GET_ADDR_LIBNL_GETLINK_H
