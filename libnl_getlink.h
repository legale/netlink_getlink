#ifndef NETLINK_GET_ADDR_LIBNL_GETLINK_H
#define NETLINK_GET_ADDR_LIBNL_GETLINK_H


#include <stdint.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>


#include "list.h"


#define NLMSG_TAIL(nmsg) \
    ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

typedef struct netdev_item {
    int index;
    char kind[IFNAMSIZ]; /* IFLA_INFO_KIND nested in rtattr IFLA_LINKINFO  */
    char name[IFNAMSIZ];
    uint8_t ll_addr[IFHWADDRLEN];
    struct list_head list;
} netdev_item_s;

typedef struct nl_req {
    struct nlmsghdr hdr;
    struct rtgenmsg gen;
} nl_req_s;

typedef struct nlcache {
    struct nlmsghdr *nl_hdr;
    struct rtattr tb[IFLA_MAX + 1];
} nlcache;


int get_netdev(char *name, size_t name_len, netdev_item_s *list);

void free_netdev_list(netdev_item_s *list);

#endif //NETLINK_GET_ADDR_LIBNL_GETLINK_H
