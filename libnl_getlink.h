#ifndef NETLINK_GET_ADDR_LIBNL_GETLINK_H
#define NETLINK_GET_ADDR_LIBNL_GETLINK_H

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>

#include <syslog.h>   // syslog()
#include <stdarg.h>   // va_list, va_start(), va_end()
#include <stdio.h>    // printf()
#include <unistd.h>   // syscall()
#include <sys/syscall.h>   // SYS_gettid

#include "list.h"

#define NLMSG_TAIL(nmsg) \
  ((struct rtattr *)(((void *)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

typedef struct netdev_item {
  int index;
  int master;              /* master device */
  int ifla_link_idx;       /* ifla_link index */
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

#ifndef syslog2
#define syslog2(pri, fmt, ...)                                                                                 \
  {                                                                                                            \
    static char msg__[32768];                                                                                  \
    (void)snprintf(msg__, 32768, "[%ld] %s:%d: " fmt, syscall(SYS_gettid), __FILE__, __LINE__, ##__VA_ARGS__); \
    syslog(pri, "%s", msg__);                                                                                  \
    if ((setlogmask(0) & LOG_MASK(pri))) {                                                                     \
      (void)printf("%s", msg__);                                                                               \
    }                                                                                                          \
  }
#endif /* syslog2 */

#endif // NETLINK_GET_ADDR_LIBNL_GETLINK_H
