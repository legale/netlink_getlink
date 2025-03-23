#ifndef NETLINK_GET_ADDR_LIBNL_GETLINK_H
#define NETLINK_GET_ADDR_LIBNL_GETLINK_H

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>

#include <stdarg.h>      // va_list, va_start(), va_end()
#include <stdio.h>       // printf()
#include <sys/syscall.h> // SYS_gettid
#include <syslog.h>      // syslog()
#include <unistd.h>      // syscall()

#include "list.h"

#define NLMSG_TAIL(nmsg) \
  ((struct rtattr *)(((void *)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

typedef struct netdev_item {
  int index;
  int master_idx;              /* master device */
  int ifla_link_idx;       /* ifla_link index */
  char kind[IFNAMSIZ + 1]; /* vlan, bridge, etc. IFLA_INFO_KIND nested in rtattr IFLA_LINKINFO  */
  bool is_bridge;
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

#endif // NETLINK_GET_ADDR_LIBNL_GETLINK_H
