#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include "threadpool.h"

struct tp_pool_t {
    int              max_thread;
    int              min_thread;
    int              flags;
    int              ms;
    int              run_task_threads;
    int              run_threads;
    int              die_threads;
    int              shutdown;
    tp_chan_t       *task_chan;
    tp_log_t        *log;
    tp_list_t       *list_thread;
    tp_hash_t       *plugins;
    pthread_mutex_t  plugin_lock;
    pthread_cond_t   new_wait;
    pthread_cond_t   free_wait;
    pthread_mutex_t  list_mutex;
    pthread_mutex_t  l;
    pthread_cond_t   pool_wait;
};

static void tst_tp_pool_destroy_all_fail() {
    tp_pool_t       *pool;
    pool = calloc(1, sizeof(*pool));
    assert(pool != NULL);
    tp_pool_wait(pool, TP_FAST);
    tp_pool_destroy(pool);
}

static void tst_tp_pool_destroy_only_log_ok() {
    tp_pool_t       *pool;
    pool = calloc(1, sizeof(*pool));
    assert(pool != NULL);

    pool->log = tp_log_new(TP_ERROR, TP_MODULE_NAME""TP_VERSION, 0);
    assert(pool->log != NULL);

    tp_pool_wait(pool, TP_FAST);
    tp_pool_destroy(pool);
}

static void tst_tp_pool_destroy_log_chan_ok() {
    tp_pool_t       *pool;
    pool = calloc(1, sizeof(*pool));
    assert(pool != NULL);

    pool->log = tp_log_new(TP_ERROR, TP_MODULE_NAME""TP_VERSION, 0);
    assert(pool->log != NULL);

    pool->task_chan = tp_chan_new(12);
    assert(pool->task_chan != NULL);

    tp_pool_wait(pool, TP_FAST);
    tp_pool_destroy(pool);
}

static void tst_tp_pool_destroy() {
    tst_tp_pool_destroy_all_fail();
    tst_tp_pool_destroy_only_log_ok();
    tst_tp_pool_destroy_log_chan_ok();
}

int main() {
    tst_tp_pool_destroy();
    return 0;
}
