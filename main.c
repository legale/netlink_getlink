#include <stdio.h>
#include <stdint.h>

#include "list.h"
#include "libnl_getlink.h"

#include "leak_detector_c.h"


int main() {
    netdev_item_s list;
    INIT_LIST_HEAD(&list.list);
    get_netdev(&list);

    netdev_item_s *tmp;
    netdev_item_s *master_dev, *link_dev;
    list_for_each_entry(tmp, &list.list, list) {
        
        if (tmp->master > 0){
            master_dev = ll_get_by_index(list, tmp->master);
        } else {
            master_dev = NULL;
        }

        if (tmp->ifla_link_idx > 0){
            link_dev = ll_get_by_index(list, tmp->ifla_link_idx);
        } else {
            link_dev = NULL;
        }

        uint8_t *addr_raw = tmp->ll_addr;
        printf( "%d: "
                "master: %3d %5s "
                "ifla_link: %3d %5s "
                "kind: %10s name: %10s"
                " %02x:%02x:%02x:%02x:%02x:%02x\n", 
                tmp->index, 
                tmp->master, master_dev ? master_dev->name : "", 
                tmp->ifla_link_idx, link_dev ? link_dev->name : "", 
                tmp->kind, tmp->name,
               addr_raw[0], addr_raw[1], addr_raw[2], addr_raw[3], addr_raw[4], addr_raw[5]);
    }
    free_netdev_list(&list);
    #ifdef LEAKCHECK
    report_mem_leak();
    #endif
}