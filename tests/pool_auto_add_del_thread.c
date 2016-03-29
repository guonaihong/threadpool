#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include "threadpool.h"

#define MAX_THREADS 10
#define MIN_THREADS 1
#define CHAN_SIZE MAX_THREADS
#define STR_WC "ps -eLf |grep 'pool_auto_add_del_thread$'" \
               "|grep -v grep |wc -l"

static void ni(void *arg) {
    printf("tid %ld 1.hello world:%s\n", 
          pthread_self(), arg == NULL ? "default" : (char *)arg);
    pause();
}

static void hao(void *arg) {
    printf("tid %ld 2.hello world:%s\n", 
          pthread_self(), arg == NULL ? "default" : (char *)arg);
}

void wc_threadnum() {
    system(STR_WC);
}

int main() {
    int i;
    int rv;
    tp_pool_t *pool = NULL;

    pool = tp_pool_create(MAX_THREADS, MAX_THREADS * 0.8, MIN_THREADS,
                          TP_AUTO_ADD | TP_AUTO_DEL,500/*500 ms*/, TP_NULL);

    assert(pool != NULL);

    /* create threads started min_threads */
    wc_threadnum(STR_WC);
    printf("add task before\n");
    for (i = 0; i < CHAN_SIZE / 2; i++) {
        if (tp_pool_add(pool, ni, NULL) != 0) {
            printf("tp_pool_add fail\n");
        }

        wc_threadnum(STR_WC);
    }

    for (i = 0; i < MAX_THREADS * 2; i++) {
        if ((rv = tp_pool_add(pool, hao, NULL)) != 0) {
            printf("tp_pool_add fail:%s\n", strerror(rv));
        }

        wc_threadnum(STR_WC);
    }

    tp_pool_wait(pool, TP_SLOW);
    tp_pool_destroy(pool);
    return 0;
}
