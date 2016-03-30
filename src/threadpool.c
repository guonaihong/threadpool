/*
 * Copyright (c) 2016, guonaihong <guonaihong@qq.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of threadpool nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "threadpool.h"

/* chan module */
struct tp_chan_t {
    int             count;
    int             nput;
    int             nput_laps; /* nput is the first laps */
    int             nget;
    int             nget_laps; /* nget is the first laps */
    void          **arr;
    int             size;       /* chan size */
    int             exit;
    pthread_mutex_t l;
    pthread_mutex_t rl;
    pthread_mutex_t wl;
    pthread_cond_t  rwait;
    pthread_cond_t  wwait;
};

struct tp_chan_t *tp_chan_new(int size) {
    tp_chan_t *chan = NULL;
    if (size <= 0) {
        return NULL;
    }

    if ((chan = calloc(1, sizeof(tp_chan_t))) == NULL)
        return NULL;

    chan->exit = 0;
    chan->count= 0;
    chan->nput = chan->nget = 0;
    chan->size = size;
    chan->arr  = (void **)malloc(sizeof(void *) * chan->size);
    if(chan->arr == NULL) {
        goto fail0;
    }

    if (pthread_cond_init(&chan->rwait, NULL) != 0) {
        goto fail1;
    }

    if (pthread_cond_init(&chan->wwait, NULL) != 0) {
        goto fail2;
    }

    if (pthread_mutex_init(&chan->l, NULL) != 0) {
        goto fail3;
    }

    if (pthread_mutex_init(&chan->rl, NULL) != 0) {
        goto fail4;
    }

    if (pthread_mutex_init(&chan->wl, NULL) != 0) {
        goto fail5;
    }
    return chan;

fail5: pthread_mutex_destroy(&chan->rl);
fail4: pthread_mutex_destroy(&chan->l);
fail3: pthread_cond_destroy(&chan->wwait);
fail2: pthread_cond_destroy(&chan->rwait);
fail1: free((void **)chan->arr);
fail0: free(chan);
    return NULL;
}

static int tp_chan_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, int msec) {
    int err;
    struct timeval   now;
    struct timespec  after;

    if (msec != -1) {
        gettimeofday(&now, NULL);
        after.tv_sec  =  now.tv_sec + msec / 1000;
        after.tv_nsec =  (now.tv_usec + msec % 1000) * 1000;
        after.tv_sec  += after.tv_nsec / (1000 * 1000 * 1000);
        after.tv_nsec %= (1000 * 1000 * 1000);
        err = pthread_cond_timedwait(cond, mutex, &after);
    } else {
        err = pthread_cond_wait(cond, mutex);
    }


    return err;
}

__attribute__((unused)) static void tp_chan_dump(tp_chan_t *c, const char *s) {
    printf("    chan is %s chan size(%d) nget(%d) nput(%d) "
            "nput_laps(%d) nget_laps(%d)\n"
            ,s
            ,c->size, c->nput, c->nget
            ,c->nput_laps, c->nget_laps
          );
}

int tp_chan_send_timedwait(tp_chan_t *c, void *v, int msec) {
    int err = 0;

    pthread_mutex_lock(&c->wl);
    pthread_mutex_lock(&c->l);

    /* check the chan has not full */
    while ((c->nput == c->nget) && (c->nput_laps != c->nget_laps)) {
        /* tp_chan_dump(c, "full"); */

        if (ATOMIC_LOAD(&c->exit) == TP_SHUTDOWN) {
            err = TP_SHUTDOWN;
            goto fail;
        }

        err = tp_chan_wait(&c->wwait, &c->l, msec);

        if (err != 0) {
            goto fail;
        }
    }

    c->arr[c->nput++] = v;

    if (c->nput == c->size) {
        c->nput = 0;
        if (++c->nput_laps == INT_MAX) {
            c->nput_laps = 0;
        }
    }

    ATOMIC_INC(&c->count);

    pthread_cond_signal(&c->rwait);

fail:
    pthread_mutex_unlock(&c->l);
    pthread_mutex_unlock(&c->wl);

    return err;
}

int tp_chan_recv_timedwait(tp_chan_t *c, void **v, int msec) {
    int   err = 0;

    pthread_mutex_lock(&c->rl);
    pthread_mutex_lock(&c->l);

    while (c->nput == c->nget && (c->nput_laps == c->nget_laps)) {
        /* tp_chan_dump(c, "empty"); */

        if (ATOMIC_LOAD(&c->exit) == TP_SHUTDOWN) {
            err = TP_SHUTDOWN;
            goto fail;
        }

        err = tp_chan_wait(&c->rwait, &c->l, msec);

        if (err != 0) {
            goto fail;
        }

    }

    *v = c->arr[c->nget++];

    if (c->nget == c->size) {
        c->nget = 0;
        if (++c->nget_laps == INT_MAX) {
            c->nget_laps = 0;
        }
    }

    ATOMIC_DEC(&c->count);

    pthread_cond_signal(&c->wwait);

fail:
    pthread_mutex_unlock(&c->l);
    pthread_mutex_unlock(&c->rl);

    return err;
}

int tp_chan_count(tp_chan_t *c) {
    return ATOMIC_LOAD(&c->count);
}

int tp_chan_wake(tp_chan_t *c) {
    if (tp_chan_count(c) > 0) {
        return -1;
    }

    ATOMIC_ADD(&c->exit, TP_SHUTDOWN);
    pthread_cond_broadcast(&c->rwait);
    pthread_cond_broadcast(&c->wwait);
    return 0;
}

int tp_chan_free(tp_chan_t *c) {
    if (c == NULL) {
        return -1;
    }

    if (tp_chan_count(c) > 0) {
        printf("chan is not empty\n");
        return -1;
    }

    pthread_mutex_destroy(&c->l);
    pthread_mutex_destroy(&c->rl);
    pthread_mutex_destroy(&c->wl);
    pthread_cond_destroy(&c->wwait);
    pthread_cond_destroy(&c->rwait);
    free((void **)c->arr);
    free(c);
    return 0;
}

/* log module */
struct tp_log_t {
    int              level;
    char            *prog;
    int              flags;
    pthread_mutex_t  lock;
};

tp_log_t *tp_log_new(int level, char *prog, int flags) {
    tp_log_t *log;
    log = malloc(sizeof(tp_log_t));
    if (log == NULL)
        return log;

    if (flags == TP_LOG_LOCK) {
        if (pthread_mutex_init(&log->lock, NULL) != 0) {
            free(log);
            return NULL;
        }
    }
    log->level = level;
    log->prog  = prog != NULL ? strdup(prog) : NULL;
    log->flags = flags;
    return log;
}

static int tp_log_time_add(char *time, int time_size) {
    struct timeval tv;
    struct tm result;
    int    pos, millisec;

    if (gettimeofday(&tv, NULL) != 0) {
        return errno;
    }

    /* Round to nearest millisec */
    millisec = lrint(tv.tv_usec/1000.0);
    /* Allow for rounding up to nearest second */
    if (millisec >= 1000) {
        millisec -= 1000;
        tv.tv_sec++;
    }

    localtime_r(&tv.tv_sec, &result);
    
    /* YYYY-MM-DD hh:mm:ss */
    pos = strftime(time, time_size, "%Y-%m-%d %H:%M:%S", &result);
    snprintf(time + pos, time_size - pos, ".%03d", millisec);
    return 0;
}

static const char *tp_log_level2str(int level) {
    switch(level) {
        case TP_DEBUG:
            return "DEBUG";
        case TP_INFO:
            return "INFO";
        case TP_WARN:
            return "WARN";
        case TP_ERROR:
            return "ERROR";
    }
    return "UNKNOWN";
}

int tp_log(tp_log_t *log, int level, const char *fmt, ...) {
    int n;

    char buffer[BUF_SIZE] = "";
    char title[64] = "";
    char *p = buffer;

    int buffer_size = BUF_SIZE;
    int title_size  = 0;
    va_list ap;

    if (level < log->level) {
        return -1;
    }

    tp_log_time_add(title, sizeof(title));

    for (;;) {
        va_start(ap, fmt);

        title_size = snprintf(p, buffer_size, "[%s] [%-5s] ", title, 
                tp_log_level2str(level));

        n = vsnprintf(p + title_size, buffer_size - title_size, fmt, ap);

        va_end(ap);

        if (n + title_size < buffer_size) {
            break;
        }
        buffer_size = n + title_size + 1;
        p = malloc(buffer_size);
    }

    if (log->flags == TP_LOG_LOCK) {
        pthread_mutex_lock(&log->lock);
    }

    write(1, p, n + title_size);
    if (log->flags == TP_LOG_LOCK) {
        pthread_mutex_unlock(&log->lock);
    }

    if (p != buffer) {
        free(p);
    }

    return 0;
}

void tp_log_free(tp_log_t *log) {
    if (log == NULL)
        return ;
    if (log->flags == TP_LOG_LOCK) {
        pthread_mutex_destroy(&log->lock);
    }
    free(log->prog);
    free(log);
}

/*
 * list module
 */
tp_list_t *tp_list_new(void) {
    tp_list_t      *list = NULL;

    list = malloc(sizeof(*list));
    if (list == NULL)
        return list;

    list->count = 0;
    TP_INIT_LIST_HEAD(&list->list);
    return list;
}

tp_list_node_t *tp_list_node_new(void *value) {
    tp_list_node_t *node = NULL;
    node = malloc(sizeof(*node));
    if (node == NULL)
        return node;
    node->value = value;
    return node;
}

/* old:prev-->next */
/* old:prev<--next */
/* new:prev-->newp-->next */
/* new:prev<--newp<--next */
static void tp_list_add_core(tp_list_node_t *newp,
                             tp_list_node_t *prev,
                             tp_list_node_t *next) {
    prev->next = newp;
    newp->next = next;
    newp->prev = prev;
    next->prev = newp;
}

/* old:prev-->node-->next */
/* new:prev-->next        */
/* new:prev<--next        */
static void tp_list_del_core(tp_list_node_t *prev,
                             tp_list_node_t *next) {
    prev->next = next;
    next->prev = prev;
}

tp_list_node_t *tp_list_add(tp_list_t *list, void *value) {
    tp_list_node_t *node = tp_list_node_new(value);
    if (node == NULL)
        return NULL;

    tp_list_add_core(node, &list->list, list->list.next);
    ATOMIC_INC(&list->count);
    return node;
}

tp_list_node_t *tp_list_tail_add(tp_list_t *list, void *value) {
    tp_list_node_t *node = tp_list_node_new(value);
    if (node == NULL)
        return NULL;

    tp_list_add_core(node, list->list.prev, &list->list);
    list->count++;

    return node;
}

void *tp_list_del(tp_list_t *list, tp_list_node_t *node) {
    if (list->count == 0)
        return NULL;
    tp_list_del_core(node->prev, node->next);
    ATOMIC_DEC(&list->count);
    return node;
}

int tp_list_len(tp_list_t *list) {
    return ATOMIC_LOAD(&list->count);
}

/*
 * hash table module
 */
struct tp_hash_t {
    tp_hash_node_t  **buckets;
    int               count;
    int               size;
    unsigned (*hash)(unsigned char *key, int *klen);
    void     (*free)(void *arg);
};

struct tp_hash_node_t {
    const void     *key;
    int             klen;
    void           *val;
    tp_hash_node_t *next;
};

/* the default hash function */
static unsigned hash_func(unsigned char *key, int *klen) {
    int            i;
    unsigned char *p;
    unsigned int   hash;

    hash = 0;

    if (*klen == TP_KEY_STR) {
        for (p = key; *p; p++) {
            hash = hash * 31 + *p;
        }
        *klen = p - key;
    } else {
        for (p = key, i = *klen; i > 0; i--, p++) {
            hash = hash * 31 + *p;
        }
    }

    return hash;
}

tp_hash_t *tp_hash_new(int size,
                       void (*free)(void *arg)) {
    int        i;
    tp_hash_t *hash;

    /* i = 2 ^ n - 1 (4 <= n <= 32) */
    for (i = 15; i < INT_MAX && i < size; ) {
        /* select the hash table default length */
        i = i * 2 + 1;
    }

    hash = calloc(1, sizeof(tp_hash_t));
    if (hash == NULL)
        return hash;

    hash->size    = i;
    hash->buckets = calloc(hash->size + 1,
                           sizeof(tp_hash_node_t *));

    if (hash->buckets == NULL) {
        goto fail;
    }

    hash->free = free;
    hash->hash = hash_func;
    return hash;

fail: free(hash);
    return NULL;
}

static tp_hash_node_t **tp_hash_find(tp_hash_t  *hash,
                                     const void *key,
                                     int        *klen,
                                     unsigned   *hash_val) {

    tp_hash_node_t **pp;
    unsigned         hval;
    if (hash_val)
        hval = *hash_val = hash->hash((unsigned char *)key, klen);
    else
        hval = hash->hash((unsigned char *)key, klen);

    pp = &hash->buckets[hval & hash->size];

    for (; pp && *pp; pp = &(*pp)->next) {
        if ((*pp)->klen == *klen &&
            memcmp((*pp)->key, key, *klen) == 0) {
            break;
        }
    }

    return pp;
}

void *tp_hash_get(tp_hash_t *hash, const void *key, int klen) {
    tp_hash_node_t **pp;

    pp = tp_hash_find(hash, key, &klen, NULL);

    if (*pp != NULL) {
        return (*pp)->val;
    }

    return NULL;
}


void *tp_hash_put(tp_hash_t  *hash,
                  const void *key,
                  int         klen,
                  void       *val) {

    void             *prev;
    unsigned          hash_val;
    tp_hash_node_t  **pp, *newp;

    if (val == NULL) {
        return val;
    }

    prev = NULL;
    pp = tp_hash_find(hash, key, &klen, &hash_val);

    /* key not exits */
    if (*pp == NULL) {
        newp = malloc(sizeof(*newp));
        if (newp == NULL) {
            return NULL;
        }

        newp->key  = calloc(1, klen + 1);
        if (newp->key == NULL) {
            return NULL;
        }
        memcpy((void *)newp->key, key, klen);

        newp->val  = val;
        newp->klen = klen;
        newp->next = hash->buckets[hash_val & hash->size];
        hash->buckets[hash_val & hash->size] = newp;
    } else {
        prev = (*pp)->val;
        (*pp)->val = val;
    }

    return prev;
}

void *tp_hash_del(tp_hash_t *hash, const void *key, int klen) {
    tp_hash_node_t **pp, *p;
    void           *val;

    val = NULL;
    pp = tp_hash_find(hash, key, &klen, NULL);

    if (*pp != NULL) {
        p = *pp;
        *pp = p->next;
        val = p->val;
        free((void *)p->key);
        free(p);
        return val;
    }
    return NULL;
}

void tp_hash_free(tp_hash_t *hash) {

    int             i, len;
    tp_hash_node_t *p, *n;

    i = 0, len = hash->size + 1;

    for (; i < len; i++) {

        p = hash->buckets[i];

        for (; p; p = n) {
            n  = p->next;

            if (hash->free) {
                hash->free(p->val);
            }

            free((void *)p->key);
            free(p);
        }
    }

    free(hash->buckets);
    free(hash);
}

static void plugin_arg_free(void *arg);
static void tp_pool_plugin_main(void *arg);

struct tp_pool_t {
    int              max_thread;
    int              min_thread;
    int              flags;
    int              ms;
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

typedef struct tp_thread_arg_t tp_thread_arg_t;
typedef struct tp_task_arg_t   tp_task_arg_t;

struct tp_thread_arg_t {
    tp_pool_t *self_pool;
    void      *self_node;
    pthread_t  tid;
};

struct tp_task_arg_t {
    int  type;
    void (*function)(void *);
    void *arg;
};

static tp_thread_arg_t *thread_arg_new(tp_pool_t *self_pool) {
    tp_thread_arg_t *arg = NULL;

    arg = malloc(sizeof(*arg));
    if (arg == NULL)
        return arg;
    arg->self_pool = self_pool;
    return arg;
}

static void thread_arg_free(tp_thread_arg_t *arg) {
    free(arg);
}

static tp_task_arg_t *task_arg_new(void (*function)(void *), void *a) {
    tp_task_arg_t *arg = NULL;
    arg = malloc(sizeof(*arg));
    if (arg == NULL)
        return arg;
    arg->type     = TP_TASK;
    arg->function = function;
    arg->arg      = a;

    return arg;
}

static void task_arg_free(void *arg) {
    free(arg);
}

static void tp_pool_thread_node_del(tp_pool_t *pool, tp_thread_arg_t *arg) {
    tp_list_node_t  *node = arg->self_node;

    pthread_mutex_lock(&pool->list_mutex);
    tp_list_del(pool->list_thread, node);
    pthread_mutex_unlock(&pool->list_mutex);

    assert(pthread_detach(pthread_self()) == 0);

    thread_arg_free(arg);
    free(node);
}

/* pool is empty returns true, false otherwise */
int tp_pool_isempty(tp_pool_t *pool) {
    if (tp_chan_count(pool->task_chan) == 0 &&
        ATOMIC_LOAD(&pool->run_threads) == 0) {
        return 1;
    }

    return 0;
}

static int tp_pool_thread_isexit(tp_pool_t *pool, tp_thread_arg_t *arg, int *countdown) {

    if (ATOMIC_LOAD(&pool->die_threads) > 0) {
        tp_pool_thread_node_del(pool, arg);

        ATOMIC_DEC(&pool->die_threads);
        tp_log(pool->log, TP_INFO, "thread %ld manually delete \n",
                pthread_self());
        return 1;
    }

    if ((pool->flags & TP_AUTO_DEL) && (*countdown)-- <= 0) {
        tp_pool_thread_node_del(pool, arg);

        tp_log(pool->log, TP_INFO, "thread %ld auto delete \n",
                pthread_self());
        return 1;
    }

    if (ATOMIC_LOAD(&pool->shutdown) == TP_SHUTDOWN) {
        tp_log(pool->log, TP_INFO, "thread %ld exits \n",
                pthread_self());
        return 1;
    }

    return 0;
}

static void *tp_pool_thread(void *arg) {
    tp_thread_arg_t *a    = (tp_thread_arg_t *)arg;
    tp_pool_t       *pool = a->self_pool;
    int ms                = pool->ms;
    tp_task_arg_t   *task = NULL;
    int countdown         = 3;
    int err               = 0;

    tp_log(pool->log, TP_INFO, "thread %ld create ok\n",
           pthread_self());

    for (;;) {
        /* get task from queue */
        err = tp_chan_recv_timedwait(pool->task_chan, (void **)&task, ms * 10);

        if (err == 0) {
            /* pool->run_threads++ */
            ATOMIC_INC(&pool->run_threads);

            if (task->type & TP_TASK) {
                task->function(task->arg);
                task_arg_free((void *)task);
            } else {
                tp_pool_plugin_main((tp_plugin_t *)task);
                plugin_arg_free((void *)task);
            }

            /* pool->run_threads-- */
            ATOMIC_DEC(&pool->run_threads);

            countdown = 3;
            continue;
        }

        if (ATOMIC_LOAD(&pool->shutdown) == 0 && tp_pool_isempty(pool)) {
            tp_log(pool->log, TP_DEBUG, "thread %ld notify tp_poll_wait function sets shutdown\n",
                    pthread_self());
            pthread_cond_signal(&pool->pool_wait);
        }

        if (tp_pool_thread_isexit(pool, arg, &countdown)) {
            break;
        }

        tp_log(pool->log, TP_DEBUG, "thread %ld recv task timeout\n",
               pthread_self());
    }

    tp_log(pool->log, TP_DEBUG, "thread %ld bye bye\n",
            pthread_self());
    return (void *)0;
}

static int tp_pool_thread_add_core(tp_pool_t *pool) {
    tp_thread_arg_t *arg;
    int              rv;

    rv = 0;
    if (ATOMIC_LOAD(&pool->die_threads) > 0) {
        ATOMIC_DEC(&pool->die_threads);
    }

    if (tp_list_len(pool->list_thread) >= pool->max_thread) {
        tp_log(pool->log, TP_INFO, "current threads(%d) > max threads(%d)"
               "add thread fail\n", tp_list_len(pool->list_thread),
               pool->max_thread);
        return -1;
    }

    if ((arg = thread_arg_new(pool)) == NULL) {
        return ENOMEM;
    }

    arg->self_node = tp_list_add(pool->list_thread, arg);
    if (arg->self_node == NULL) {
        rv = ENOMEM;
        goto fail_arg;
    }

    rv = pthread_create(&arg->tid, NULL, tp_pool_thread, arg);
    if (rv != 0) {
        goto fail_list;
    }
    return 0;

fail_list:tp_list_del(pool->list_thread, arg->self_node);
fail_arg: thread_arg_free(arg);
    return rv;
}

static int tp_pool_max_thread_set(tp_pool_t *pool, int max_thread) {
    pool->max_thread  = max_thread;
    /* get number of cpus */
    if (max_thread <= 0 &&
            (pool->max_thread = sysconf(_SC_NPROCESSORS_ONLN)) == -1) {
        tp_log(pool->log, TP_ERROR,
                "could not determine number of CPUs online:%s\n",
                strerror(errno));
        return -1;
    }
    return 0;
}

static void tp_pool_args_set(tp_pool_t *pool, va_list ap) {
    int i, val;

    for (i = 0; (val = va_arg(ap, int)) != TP_NULL; i++) {
        if (i == 0) {
            pool->min_thread = val;
        } else if (i == 1) {
            pool->flags = val;
        } else if (i == 2) {
            pool->ms = val;
        }
    }
}

static int tp_pool_lock_wait_init(tp_pool_t *pool) {
    int err = 0;

    if (pthread_mutex_init(&pool->list_mutex, NULL) != 0) {
        return -1;
    }

    if ((err = pthread_mutex_init(&pool->l, NULL)) != 0) {
        goto fail1;
    }

    if ((err = pthread_cond_init(&pool->pool_wait, NULL)) != 0) {
        goto fail2;
    }

    if ((err = pthread_mutex_init(&pool->plugin_lock, NULL)) != 0) {
        goto fail3;
    }

    if ((err = pthread_cond_init(&pool->new_wait, NULL)) != 0) {
        goto fail4;
    }

    if ((err = pthread_cond_init(&pool->free_wait, NULL)) != 0) {
        goto fail5;
    }

    return err;
fail5: pthread_cond_destroy(&pool->new_wait);
fail4: pthread_mutex_destroy(&pool->plugin_lock);
fail3: pthread_cond_destroy(&pool->pool_wait);
fail2: pthread_mutex_destroy(&pool->l);
fail1: pthread_mutex_destroy(&pool->list_mutex);
       return err;
}

static void myfree(void *val) {
    free(val);
}

/* tp_pool_t *tp_pool_create(int max_thread, int chan_size,
 *                           int flags, int min_threads, int ms) */
tp_pool_t *tp_pool_create(int max_thread, int chan_size, ...) {
    va_list          ap;
    tp_pool_t       *pool;

    pool = calloc(1, sizeof(*pool));
    if (pool == NULL) {
        return pool;
    }

    if (tp_pool_lock_wait_init(pool) == -1) {
        goto fail0;
    }

    pool->log = tp_log_new(TP_ERROR, TP_MODULE_NAME""TP_VERSION, 0);
    if (pool->log == NULL) {
        goto fail;
    }

    if ((pool->plugins = tp_hash_new(15, myfree)) == NULL) {
        goto fail;
    }

    pool->run_threads = pool->shutdown = 0;

    if (tp_pool_max_thread_set(pool, max_thread) != 0) {
        goto fail;
    }

    pool->ms = 500;
    pool->task_chan = tp_chan_new(chan_size);
    if (pool->task_chan == NULL) {
        goto fail;
    }

    va_start(ap, chan_size);
    tp_pool_args_set(pool, ap);
    va_end(ap);

    pool->list_thread = tp_list_new();

    if (tp_pool_thread_addn(pool, pool->min_thread > 0 ?
                pool->min_thread : pool->max_thread) != 0) {
        goto fail;
    }

    return pool;

fail0: free(pool);
      return NULL;
fail:
    tp_pool_wait(pool, TP_FAST);
    tp_pool_destroy(pool);
    return NULL;
}

static int tp_pool_task_add_core(tp_pool_t *pool, void *task_arg) {
    int            ms = pool->ms;
    int            err, try;

    for (try = 3; try > 0; try--) {
        err = tp_chan_send_timedwait(pool->task_chan, task_arg, ms);
        if (err == 0) {
            return err;
        }

        if (pool->flags & TP_AUTO_ADD) {
            err = tp_pool_thread_addn(pool, 1);
            /* add thread fail */
            if (err == -1) {
                return err;
            }
            /* add thread ok */
        }
    }

    return err;
}

int tp_pool_add(tp_pool_t *pool, void (*function)(void *), void *arg) {
    tp_task_arg_t *a;

    a = task_arg_new(function, arg);
    if (a == NULL) {
        return -1;
    }

    if (tp_pool_task_add_core(pool, a) != 0) {
        task_arg_free(a);
        return -1;
    }

    return 0;
}

int tp_pool_thread_addn(tp_pool_t *pool, int n) {
    int i, rv;

    rv = 0;
    for (i = 0; i < n; i++) {
        pthread_mutex_lock(&pool->list_mutex);
        rv = tp_pool_thread_add_core(pool);
        pthread_mutex_unlock(&pool->list_mutex);
        if (rv != 0) {
            return rv;
        }
    }

    return rv;
}

int tp_pool_thread_deln(tp_pool_t *pool, int n) {
    int i;
    for (i = 0; i < n; i++) {
        if (ATOMIC_LOAD(&pool->die_threads) >= pool->max_thread) {
            return 0;
        }
        ATOMIC_INC(&pool->die_threads);
    }

    return 0;
}

int tp_pool_wait(tp_pool_t *pool, int flags) {
    if (pool->task_chan == NULL || pool->log == NULL) {
        return -1;
    }

    if (flags == TP_SLOW) {
        pthread_mutex_lock(&pool->l);
        do {
            pthread_cond_wait(&pool->pool_wait, &pool->l);
        } while (!tp_pool_isempty(pool));
        pthread_mutex_unlock(&pool->l);
    }

    tp_log(pool->log, TP_INFO, "chan is empty(%d), run threads is empty(%d)\n",
            tp_chan_count(pool->task_chan), ATOMIC_LOAD(&pool->run_threads));
    ATOMIC_ADD(&pool->shutdown, TP_SHUTDOWN);
    return 0;
}

int tp_pool_destroy(tp_pool_t *pool) {

    tp_list_node_t *tmp, *node;
    tp_thread_arg_t *arg;

    if (pool == NULL) {
        return -1;
    }

    pthread_mutex_destroy(&pool->list_mutex);

    tp_chan_wake(pool->task_chan);
    if (pool->list_thread != NULL) {
        TP_LIST_FOREACH_SAFE(node, tmp, &pool->list_thread->list) {
            arg = (tp_thread_arg_t *)node->value;
            pthread_join(arg->tid, NULL);
            thread_arg_free(arg);
            free(node);
        }
    }

    free(pool->list_thread);

    pthread_mutex_destroy(&pool->l);
    pthread_cond_destroy(&pool->new_wait);
    pthread_cond_destroy(&pool->free_wait);
    pthread_mutex_destroy(&pool->plugin_lock);
    pthread_mutex_destroy(&pool->list_mutex);
    pthread_cond_destroy(&pool->pool_wait);

    tp_chan_free(pool->task_chan);
    tp_log_free(pool->log);
    tp_hash_free(pool->plugins);
    free(pool);
    return 0;
}

/*
 * plugin module
 */
typedef struct tp_plugin_arg_t tp_plugin_arg_t;

struct tp_plugin_arg_t {
    int                     type;
    tp_pool_t              *self_pool;
    tp_plugin_t            *plugin;
    tp_vtable_global_arg_t *g;
};

static tp_plugin_arg_t *plugin_arg_new(tp_pool_t *pool, tp_plugin_t *plugin, int type) {
    tp_plugin_arg_t *arg;
    arg = malloc(sizeof(*arg));

    if (arg == NULL)
        return arg;
    arg->type      = type;
    arg->self_pool = pool;
    arg->plugin    = plugin;
    return arg;
}

static void plugin_arg_free(void *arg) {
    free(arg);
}

static int tp_pool_plugin_add(tp_pool_t *pool, tp_plugin_t *plugin, int type) {
    tp_plugin_arg_t *a;
    a = plugin_arg_new(pool, plugin, type);
    if (a == NULL) {
        return -1;
    }

    if (tp_pool_task_add_core(pool, a) != 0) {
        plugin_arg_free(a);
        return -1;
    }
    return 0;
}

int tp_pool_plugin_addn(tp_pool_t *pool, int n, tp_plugin_t *plugin, int type) {
    int i, rv;

    for (i = 0; i < n; i++) {
        rv = tp_pool_plugin_add(pool, plugin, type);
        if (rv != 0) {
            return rv;
        }
    }
    return 0;
}

static void tp_plugin_to_child_arg(tp_vtable_child_arg_t *c, tp_plugin_arg_t *a) {
    c->user_data = a->plugin->user_data;
    c->global    = a->g;
}

static void tp_pool_plugin_child_loop(tp_plugin_arg_t *a) {
    tp_vtable_child_arg_t   c;
    tp_plugin_t            *p = a->plugin;

    tp_plugin_to_child_arg(&c, a);
    if (p->vtable.create && p->vtable.create(&c) != 0) {
        return ;
    }

    for (;;) {
        if (p->vtable.process(&c) != 0) {
            break;
        }
    }

    if (p->vtable.destroy) {
        p->vtable.destroy(&c);
    }
}

static int tp_pool_plugin_consumer(tp_plugin_arg_t *a) {
    tp_pool_t              *pool;
    tp_vtable_global_arg_t *g = NULL;
    tp_plugin_t            *p;
    int                     err = 0;

    pool = a->self_pool;
    p    = a->plugin;
    pthread_mutex_lock(&pool->plugin_lock);

    if ((a->type & TP_MOD_PRODUCER_EMPTY) ||
        (a->type & TP_MOD_PRODUCER_GFUNC_EMPTY)) {
        g = tp_hash_get(pool->plugins, p->plugin_name, TP_KEY_STR);
        if (g == NULL) {
            g = calloc(1, sizeof(*g));
            assert(g != NULL);
            tp_hash_put(pool->plugins, p->plugin_name, TP_KEY_STR, g);
        }

        if (p->vtable.global_init != NULL)
            goto cinit;
        goto cexit;
    }
    /* consumers wait for produces to produce ok */
    for (;;) {
        g = tp_hash_get(pool->plugins, p->plugin_name, TP_KEY_STR);
        if (g != NULL) {
            /* produces to produce fail */
            if ((g->status & TP_MOD_PRODUCER_GINIT_FAIL)
                || (g->status & TP_MOD_PRODUCER_EXIT)) {
                err = -1;
                goto cexit;
            }
            break;
        }

        pthread_cond_wait(&pool->new_wait, &pool->plugin_lock);
    }

cinit:
    if (p->vtable.global_init && g->c_ref_count == 0) {
        err = p->vtable.global_init(g);
        if (err != 0) {
            goto cexit;
        }
    }
    g->c_ref_count++;

cexit:
    a->g = g;
    pthread_mutex_unlock(&pool->plugin_lock);
    return err;
}

static int tp_pool_plugin_producer(tp_plugin_arg_t *a) {
    tp_pool_t              *pool;
    int                     err;
    tp_vtable_global_arg_t *g;
    tp_plugin_t            *p;

    g    = NULL;
    err  = 0;
    p    = a->plugin;
    pool = a->self_pool;

    pthread_mutex_lock(&pool->plugin_lock);

    if (tp_hash_get(pool->plugins, p->plugin_name, TP_KEY_STR) == NULL) {
        g = calloc(1, sizeof(*g));
        assert(g != NULL);

        err = p->vtable.global_init(g);
        if (err != 0) {
            /* producer init fail */
            g->status |= TP_MOD_PRODUCER_GINIT_FAIL;
        }
        tp_hash_put(pool->plugins, p->plugin_name, TP_KEY_STR, g);
    }

    a->g = g;
    pthread_mutex_unlock(&pool->plugin_lock);
    if (err != 0) {
        pthread_cond_broadcast(&pool->new_wait);
        return err;
    }

    pthread_cond_signal(&pool->new_wait);
    g->p_ref_count++;
    return 0;
}

static void tp_pool_plugin_global_destroy(tp_plugin_arg_t *a) {
    tp_pool_t              *pool = a->self_pool;
    tp_vtable_global_arg_t *g = NULL;
    tp_plugin_t            *p = a->plugin;

    /*global_init and global_destroy together to have a value or no value */
    assert((p->vtable.global_init == NULL &&
           p->vtable.global_destroy == NULL) ||
           (p->vtable.global_init != NULL &&
           p->vtable.global_destroy != NULL));

    pthread_mutex_lock(&pool->plugin_lock);
    g = tp_hash_get(pool->plugins, p->plugin_name, TP_KEY_STR);
    if (g == NULL) {
        goto done;
    }

    if (a->type == TP_MOD_CONSUMER) {
        g->c_ref_count = g->c_ref_count > 0 ? --g->c_ref_count : 0;
        if (g->c_ref_count == 0) {
            pthread_cond_signal(&pool->free_wait);
            if (p->vtable.global_destroy) {
                p->vtable.global_destroy(g);
            }

            if (g->status & TP_MOD_CONSUMER_EXIT) {
                g->status |= TP_MOD_CONSUMER_EXIT;
            }
        }

    } else if (a->type == TP_MOD_PRODUCER){
        g->p_ref_count = g->p_ref_count > 0 ? --g->p_ref_count : 0;
        if (g->p_ref_count == 0) {
            while (g->c_ref_count > 0) {
                pthread_cond_wait(&pool->free_wait, &pool->plugin_lock);
            }
            if (p->vtable.global_destroy) {
                p->vtable.global_destroy(g);
            }

            if (g->status & TP_MOD_PRODUCER_EXIT) {
                g->status |= TP_MOD_PRODUCER_EXIT;
            }
        }
    }

done:
    pthread_mutex_unlock(&pool->plugin_lock);
}

static void tp_pool_plugin_main(void *arg) {
    tp_plugin_arg_t        *a;
    tp_pool_t              *pool;

    a = (tp_plugin_arg_t *)arg;
    pool = a->self_pool;

    if (a->plugin->vtable.process == NULL) {
        return;
    }

    if (a->type & TP_MOD_CONSUMER) {
        tp_pool_plugin_consumer(a);
    } else if (a->type & TP_MOD_PRODUCER) {
        tp_pool_plugin_producer(a);
    }

    tp_pool_plugin_child_loop(a);

    tp_pool_plugin_global_destroy(a);
}

int tp_pool_plugin_producer_consumer_add(tp_pool_t   *pool,
                                         tp_plugin_t *produces,
                                         int          np,
                                         tp_plugin_t *consumers,
                                         int          nc) {
    int rv;
    int status = 0;
    if (produces == NULL) {
        status |= TP_MOD_PRODUCER_EMPTY;
    } else {
        rv = tp_pool_plugin_addn(pool, np, produces, TP_MOD_PRODUCER);
        if (rv != 0) {
            return rv;
        }

    }

    if (produces && produces->vtable.global_init == NULL) {
        status |= TP_MOD_PRODUCER_GFUNC_EMPTY;
    }


    rv = tp_pool_plugin_addn(pool, nc, consumers, TP_MOD_CONSUMER | status);
    if (rv != 0) {
        return rv;
    }
    return 0;
}
