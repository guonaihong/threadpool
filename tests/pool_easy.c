#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include "threadpool.h"

#define TOTAL 100000
static int total;
void print1(void *arg) {
    printf("####### ni hao:%s\n", (char *)arg);
    __sync_add_and_fetch(&total, 1);
}

void print2(void *arg) {
    printf("####### ni hao:%s\n", (char *)arg);
    __sync_add_and_fetch(&total, 1);
}

void *func3(void *arg) {
    tp_pool_t *pool = (tp_pool_t *)arg;

    int i;
    for (i = 0; i < TOTAL; i++) {
        if (tp_pool_add(pool, print1, "print1") != 0) {
            printf("send print1 (%d) timeout\n", i);
            exit(0);
        }
        if (tp_pool_add(pool, print2, "print2") != 0) {
            printf("send print2 (%d) timeout\n", i);
            exit(0);
        }
    }
    return 0;
}

int main() {
    tp_pool_t *pool = tp_pool_create(10, 10, TP_NULL);

    assert(pool != NULL);

    fprintf(stderr, "pool create ok\n");

    func3(pool);

    tp_pool_wait(pool, TP_FAST);
    fprintf(stderr, "wait after\n");

    tp_pool_destroy(pool);

    printf("total(%d)\n", total);
    assert(TOTAL * 2 == total);
    return 0;
}
