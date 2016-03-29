#include <stdio.h>
#include <stdlib.h>
#include "threadpool.h"

int main() {
    tp_list_t *list = tp_list_new();
    tp_list_node_t *tmp = NULL;

    tp_list_node_t *node = NULL;
    tp_list_add(list, "aaaaa");
    tp_list_add(list, "bbbbb");
    tp_list_add(list, "ccccc");
    tp_list_add(list, "ddddd");

    TP_LIST_FOREACH(node, &list->list) {
        printf("node = %s\n", (char *)node->value);
    }

    TP_LIST_FOREACH_SAFE(node, tmp, &list->list) {
        printf("2.node = %s\n", (char *)node->value);
        free(node);
    }
    return 0;
}
