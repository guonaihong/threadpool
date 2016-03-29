#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "threadpool.h"

static void my_free(void *arg) {
    free(arg);
}

int main() {
    tp_hash_t *hash = tp_hash_new(15, my_free);
    void      *val  = NULL;

    assert(hash != NULL);

    tp_hash_put(hash, "hello", TP_KEY_STR, strdup("world"));
    tp_hash_put(hash, "hello1", TP_KEY_STR, strdup("world1"));
    tp_hash_put(hash, "hello2", TP_KEY_STR, strdup("world2"));
    tp_hash_put(hash, "hello3", TP_KEY_STR, strdup("world3"));
    tp_hash_put(hash, "hello4", TP_KEY_STR, strdup("world4"));

    val = tp_hash_get(hash, "hello", TP_KEY_STR);
    printf("%s\n", (char *)val);
#if 0
    val = tp_hash_del(hash, "hello", TP_KEY_STR);
    free(val);
    val = tp_hash_del(hash, "hello0", TP_KEY_STR);
    free(val);
    val = tp_hash_del(hash, "hello2", TP_KEY_STR);
    free(val);
    val = tp_hash_del(hash, "hello3", TP_KEY_STR);
    free(val);
    val = tp_hash_del(hash, "hello4", TP_KEY_STR);
    free(val);

    printf("del after \n");
    tp_hash_dump_str(hash);

#endif

    tp_hash_free(hash);
    return 0;
}
