#include <stdio.h>
#include <stdint.h>

#include "list.h"
#include "libnl_getlink.h"


int main(int argc, char *argv[]) {
    netdev_item_s list;
    INIT_LIST_HEAD(&list.list);
    get_netdev(NULL, 0, &list);


    netdev_item_s *tmp;
    list_for_each_entry(tmp, &list.list, list) {
        uint8_t *addr_raw = tmp->ll_addr;
        printf( "%d: kind: '%s' wireless: %s name: %s"
                " %02x:%02x:%02x:%02x:%02x:%02x\n", 
                tmp->index, tmp->kind, tmp->wireless ? "true" : "false", tmp->name,
               addr_raw[0], addr_raw[1], addr_raw[2], addr_raw[3], addr_raw[4], addr_raw[5]);
    }
    free_netdev_list(&list);

}