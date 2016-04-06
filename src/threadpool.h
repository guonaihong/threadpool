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

#ifndef __THREAD_POOL_H
#define __THREAD_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

#define TP_VERSION "v0.0.0"
#define TP_MODULE_NAME "threadpool"

#define ATOMIC_DEC(ptr)  __sync_sub_and_fetch(ptr, 1)
#define ATOMIC_INC(ptr)  __sync_add_and_fetch(ptr, 1)
#define ATOMIC_LOAD(ptr) __sync_add_and_fetch(ptr, 0)
#define ATOMIC_ADD(ptr, v) __sync_add_and_fetch(ptr, v)

/*
 * chan module
 */
typedef struct tp_chan_t tp_chan_t;

struct tp_chan_t *tp_chan_new(int size);

int tp_chan_send_timedwait(tp_chan_t *c, void *v, int msec);

int tp_chan_recv_timedwait(tp_chan_t *c, void **v, int msec);

int tp_chan_count(tp_chan_t *c);

int tp_chan_wake(tp_chan_t *c);

int tp_chan_free(tp_chan_t *c);
/*
 * log module
 */
typedef struct tp_log_t tp_log_t;
#define TP_DEBUG 0x1
#define TP_INFO  0x2
#define TP_WARN  0x4
#define TP_ERROR 0x8
#define BUF_SIZE BUFSIZ

#define TP_LOG_LOCK  0x1

tp_log_t *tp_log_new(int level, char *prog, int flags);

int tp_log(tp_log_t *log, int level, const char *fmt, ...);

void tp_log_free(tp_log_t *log);


/*
 * list module
 */
typedef struct tp_list_node_t tp_list_node_t;
typedef struct tp_list_t tp_list_t;

#define TP_INIT_LIST_HEAD(ptr) do { \
    (ptr)->prev = ptr; (ptr)->next = ptr; \
} while (0) \

struct tp_list_node_t {
    void *value;
    tp_list_node_t *prev;
    tp_list_node_t *next;
};

struct tp_list_t {
    int count;
    void (*free)(void *ptr);
    tp_list_node_t list;
};

tp_list_t *tp_list_new(void);

tp_list_node_t *tp_list_node_new(void *value);

tp_list_node_t *tp_list_add(tp_list_t *list, void *value);

tp_list_node_t *tp_list_tail_add(tp_list_t *list, void *value);

void *tp_list_del(tp_list_t *list, tp_list_node_t *node);

#define TP_LIST_FOREACH(pos, list)  \
    for (pos = (list)->next; pos != (list); pos = pos->next)

#define TP_LIST_FOREACH_SAFE(pos, n, list) \
    for (pos = (list)->next, n = (pos)->next; pos != (list); \
            pos = n, n = n->next)

/*
 * hash table module
 */
#define TP_KEY_STR -1
typedef struct tp_hash_t tp_hash_t;
typedef struct tp_hash_node_t tp_hash_node_t;

void *tp_hash_get(tp_hash_t *hash, const void *key, int klen);

tp_hash_t *tp_hash_new(int size,
                       void (*free)(void *arg));

void *tp_hash_put(tp_hash_t  *hash,
                  const void *key,
                  int         klen,
                  void       *val);

void *tp_hash_del(tp_hash_t *hash, const void *key, int klen);

void tp_hash_free(tp_hash_t *hash);

#define TP_TASK                         (1 << 1)
#define TP_PLUGIN_PRODUCER              (1 << 2)
#define TP_PLUGIN_CONSUMER              (1 << 3)
#define TP_PLUGIN_CONSUMER_EXIT         (1 << 4)
#define TP_PLUGIN_PRODUCER_EMPTY        (1 << 5)
#define TP_PLUGIN_PRODUCER_GFUNC_EMPTY  (1 << 6)
#define TP_PLUGIN_PRODUCER_GINIT_FAIL   (1 << 7)
#define TP_PLUGIN_PRODUCER_EXIT         (1 << 8)
#define TP_PLUGIN_EXIT                  (1 << 9)

/*
 * threadpool module
 */
#define TP_AUTO_ADD 0x1
#define TP_AUTO_DEL 0x2
#define TP_NULL     -2
#define TP_SHUTDOWN 0x1
#define TP_FAST     0x0
#define TP_SLOW     0x1

typedef struct tp_pool_t tp_pool_t;

/* tp_pool_t *tp_pool_new(int max_thread, int chan_size,
 *                           int min_threads, int flags, int ms, int tp_null) */

/* tp_pool_t *tp_pool_new(int max_thread, int chan_size,
 *                           int min_threads, int tp_null) */

/* tp_pool_t *tp_pool_new(int max_thread, int chan_size, int tp_null)
 */

tp_pool_t *tp_pool_new(int max_thread, int chan_size, ...);

int tp_pool_add(tp_pool_t *pool, void (*function)(void *), void *arg);

int tp_pool_thread_addn(tp_pool_t *pool, int n);

int tp_pool_thread_deln(tp_pool_t *pool, int n);

int tp_pool_wait(tp_pool_t *pool, int flags);

int tp_pool_free(tp_pool_t *pool);


/*
 * plugins module
 */

typedef struct tp_vtable_t tp_vtable_t;
typedef struct tp_vtable_global_arg_t tp_vtable_global_arg_t;
typedef struct tp_vtable_child_arg_t tp_vtable_child_arg_t;
typedef struct tp_plugin_t     tp_plugin_t;

struct tp_vtable_t {
    int  (*global_init)(tp_vtable_global_arg_t *g);

    int  (*create)(tp_vtable_child_arg_t *c);
    int  (*process)(tp_vtable_child_arg_t *c);
    void (*destroy)(tp_vtable_child_arg_t *c);

    void (*global_destroy)(tp_vtable_global_arg_t *g);
};

struct tp_vtable_global_arg_t {
    void *producer_arg;
    void *consumer_arg;
    int   p_ref_count;
    int   c_ref_count;
    int   status;
};

struct tp_vtable_child_arg_t {
    void                   *child_arg;
    void                   *user_data;
    tp_vtable_global_arg_t *global;
};

struct tp_plugin_t {
    tp_vtable_t vtable;
    void       *user_data;
    const char *plugin_name;
};

int tp_pool_plugin_producer_consumer_add(tp_pool_t   *pool,
                                         tp_plugin_t *produces,
                                         int          np,
                                         tp_plugin_t *consumers,
                                         int          nc);

int tp_pool_plugin_addn(tp_pool_t *pool, int n, tp_plugin_t *plugin, int type);
#ifdef __cplusplus
}
#endif

#endif
