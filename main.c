#include <stdint.h>
#include <stdio.h>

#include "libnl_getlink.h"
#include "list.h"

#include "leak_detector_c.h"

/* cli arguments parse macro and functions */
#define NEXT_ARG()                         \
  do {                                     \
    argv++;                                \
    if (--argc <= 0) incomplete_command(); \
  } while (0)
#define NEXT_ARG_OK() (argc - 1 > 0)
#define PREV_ARG() \
  do {             \
    argv--;        \
    argc++;        \
  } while (0)

/**
 * @brief print message and exit with code -1
 *
 */
static void incomplete_command(void) {
  fprintf(stdout, "Command line is not complete. Try -h or --help\n");
  exit(-1);
}

/**
 * @brief check if 'prefix' matches string
 *
 * @param prefix
 * @param string
 * @return true if 'prefix' is a not empty prefix of 'string'
 * @return false
 */
static bool matches(const char *prefix, const char *string) {
  if (!*prefix)
    return false;
  while (*string && *prefix == *string) {
    prefix++;
    string++;
  }
  return !*prefix;
}

int main() {
  setup_syslog2(LOG_DEBUG, false);

  netdev_item_s list;
  INIT_LIST_HEAD(&list.list);
  get_netdev(&list);

  netdev_item_s *tmp;
  netdev_item_s *master_dev, *link_dev;

  list_for_each_entry(tmp, &list.list, list) {

    if (tmp->master_idx > 0) {
      master_dev = ll_get_by_index(list, tmp->master_idx);
    } else {
      master_dev = NULL;
    }

    if (tmp->ifla_link_idx > 0) {
      link_dev = ll_get_by_index(list, tmp->ifla_link_idx);
    } else {
      link_dev = NULL;
    }

    uint8_t *addr_raw = tmp->ll_addr;
    printf("%3d: "                                 // индекс (3 символа)
           "master: %3d %-10s "                    // master id и имя (3 знака и 10 символов)
           "ifla_link: %3d %-10s "                 // ifla_link_idx и имя (3 знака и 10 символов)
           "is_bridge: %-5d "                      // is_bridge (5 символов)
           "kind: %-15s "                          // kind (15 символов)
           "name: %-15s "                          // name (15 символов)
           "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", // MAC-адрес (стандартный формат)
           tmp->index,
           tmp->master_idx, master_dev ? master_dev->name : "EMPTY",
           tmp->ifla_link_idx, link_dev ? link_dev->name : "",
           tmp->is_bridge,
           tmp->kind, tmp->name,
           addr_raw[0], addr_raw[1], addr_raw[2], addr_raw[3], addr_raw[4], addr_raw[5]);
  }
  free_netdev_list(&list);
#ifdef LEAKCHECK
  report_mem_leak();
#endif
}