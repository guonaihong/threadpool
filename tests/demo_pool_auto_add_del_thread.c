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
#define NTH_WC "ps -eLf |grep 'demo_pool_auto_add_del_thread$'" \
               "|grep -v grep |wc -l"

static void ni(void *arg) {
    printf("tid %ld 1.hello world:%s\n", 
          (long)pthread_self(), arg == NULL ? "default" : (char *)arg);

    /* If a slow function calls */
    sleep(10);
}

static void hao(void *arg) {
    printf("tid %ld 2.hello world:%s\n", 
          (long)pthread_self(), arg == NULL ? "default" : (char *)arg);
}

static void wc_threadnum() {
    FILE *fp       = NULL;
    char  buf[128] = "";
    int   rv;

    fp = popen(NTH_WC, "r");
    if (fp == NULL) {
        return;
    }

    rv = snprintf(buf, sizeof(buf), "current thread number:");
    fgets(buf + rv, sizeof(buf) - rv, fp);
    pclose(fp);

    printf("%s\n", buf);
}

volatile int need_exit;

static void *pr_threadnum(void *arg) {
    for (;;) {
        wc_threadnum();
        usleep(250 * 1000);
        if (__sync_fetch_and_add(&need_exit, 0) > 0) {
            return 0;
        }
    }
    return 0;
}

int main() {
    int        i,rv;
    tp_pool_t *pool = NULL;
    pthread_t  pr_tid;

    pool = tp_pool_new(MAX_THREADS, MAX_THREADS * 0.8 /* chan size */, MIN_THREADS,
                          TP_AUTO_ADD | TP_AUTO_DEL,500/*500 ms*/, TP_NULL);

    assert(pool != NULL);

    /* create threads started min_threads */
    wc_threadnum();
    printf("add task before\n");
    for (i = 0; i < CHAN_SIZE / 2; i++) {
        if (tp_pool_add(pool, ni, NULL) != 0) {
            printf("tp_pool_add fail\n");
        }

        wc_threadnum();
    }

    for (i = 0; i < MAX_THREADS * 2; i++) {
        if ((rv = tp_pool_add(pool, hao, NULL)) != 0) {
            printf("tp_pool_add fail:%s\n", strerror(rv));
        }

        wc_threadnum();
    }

    pthread_create(&pr_tid, NULL, pr_threadnum, NULL);
    tp_pool_wait(pool, TP_SLOW);
    tp_pool_free(pool);

    __sync_fetch_and_add(&need_exit, 1);
    pthread_join(pr_tid, NULL);
    return 0;
}
