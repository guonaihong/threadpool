#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threadpool.h"

int main() {
    tp_list_t *list = tp_list_new();
    tp_list_node_t *tmp = NULL;

    tp_list_node_t *node = NULL;
    /*
     * p = strdup(str) =
     * p = malloc(strlen(str) + 1), strcpy(p, str)
     *
     */
    tp_list_add(list, strdup("aaaaa"));
    tp_list_add(list, strdup("bbbbb"));
    tp_list_add(list, strdup("ccccc"));
    tp_list_add(list, strdup("ddddd"));

    TP_LIST_FOREACH(node, &list->list) {
        printf("node = %s\n", (char *)node->value);
    }

    TP_LIST_FOREACH_SAFE(node, tmp, &list->list) {
        printf("2.node = %s\n", (char *)node->value);
        free(node->value);
        free(node);
    }

    free(list);
    return 0;
}
