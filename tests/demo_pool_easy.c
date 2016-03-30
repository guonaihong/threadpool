#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include "threadpool.h"

#define TOTAL 100000

static int total;

void print1(void *arg) {
    printf("hello::%s\n", (char *)arg);
    __sync_add_and_fetch(&total, 1);/* ++total */
}

void print2(void *arg) {
    printf("world::%s\n", (char *)arg);
    __sync_add_and_fetch(&total, 1);/* ++total */
}

void *start_add_task(void *arg) {
    tp_pool_t *pool = (tp_pool_t *)arg;

    int i;
    for (i = 0; i < TOTAL; i++) {

        /* Add tasks to the thread pool */
        if (tp_pool_add(pool, print1, "print1") != 0) {
            printf("(%d) add print1 function to the thread pool timeout\n",
                   i);
            exit(0);
        }

        /* Add tasks to the thread pool */
        if (tp_pool_add(pool, print2, "print2") != 0) {
            printf("(%d) add print2 function to the thread pool timeout\n",
                   i);
            exit(0);
        }
    }
    return 0;
}

int main() {
    tp_pool_t *pool = tp_pool_create(10/*Number of threads*/,
                                     10/*The maximum capacity of the chan*/,
                                     TP_NULL);

    assert(pool != NULL);

    fprintf(stderr, "pool create ok\n");

    start_add_task(pool);

    tp_pool_wait(pool, TP_FAST);
    fprintf(stderr, "wait after\n");

    tp_pool_destroy(pool);

    printf("total(%d)\n", total);
    assert(TOTAL * 2 == total);
    return 0;
}
