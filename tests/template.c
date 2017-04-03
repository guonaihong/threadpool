#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <strings.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>

#include "threadpool.h"

#ifdef PLUGIN_NAME
# undef PLUGIN_NAME
#endif

#define PLUGIN_NAME "template"
#define TASK_ERR    -1
#define TASK_EOF    0

typedef struct template_producer_t template_producer_t;

struct template_producer_t {
    tp_chan_t *chan;
    int        listen_fd;
};

static int template_producer_global_init(tp_vtable_global_arg_t *arg);
static int template_producer_process(tp_vtable_child_arg_t *arg);
static void template_producer_global_destroy(tp_vtable_global_arg_t *arg);

static struct tp_plugin_t template_produces = {
    {
        template_producer_global_init,
        NULL,
        template_producer_process,
        NULL,
        template_producer_global_destroy,
    },
    NULL,
    PLUGIN_NAME,
};

static int engine_init(tp_vtable_global_arg_t *g);
static int engine_channel_init(tp_vtable_child_arg_t *c);
static int template_consumer_process(tp_vtable_child_arg_t *arg);
static void engine_channel_destroy(tp_vtable_child_arg_t *c);
static void engine_destroy(tp_vtable_global_arg_t *g);

static struct tp_plugin_t template_consumers = {
    {
        engine_init,
        engine_channel_init,
        template_consumer_process,
        engine_channel_destroy,
        engine_destroy,
    },
    NULL,
    PLUGIN_NAME,
};

struct template_conf {
    struct   sockaddr_in sa;
    int      max_nthreads;
    int      min_nthreads;
};

struct engine_base {
    char *test;
};

static int engine_init(tp_vtable_global_arg_t *g) {

    static struct engine_base engine;

    struct template_conf *conf = (struct template_conf *)g->user_data;

    g->consumer_arg = (void *)&engine;
    printf("=============engine init ok:%p\n", (void *)conf);
    return 0;
}

static void engine_destroy(tp_vtable_global_arg_t *g) {
    struct engine_base *engine = (struct engine_base *)g->consumer_arg;
    printf("=============engine destroy ok:%p\n", (void *)engine);
}

static int engine_channel_init(tp_vtable_child_arg_t *c) {
    tp_vtable_global_arg_t *g = c->global;

    struct engine_base *engine = (struct engine_base *)g->consumer_arg;
    printf("=============create session ok:%p\n", (void *)engine);
    return 0;
}

static void engine_channel_destroy(tp_vtable_child_arg_t *c) {
    /* TODO */
    c->child_arg = NULL;
}

#if 0
static int engine_channel_reset(tp_vtable_child_arg_t *c) {
    engine_channel_destroy(c);
    /* TODO */
    return engine_channel_init(c);
}
#endif

#if 0
__attribute__((unused)) static void wfile(char *buffer, int n) {
    int fd = -1;
    char fname[64];
    struct timeval tv = {0};

    gettimeofday(&tv, NULL);
    snprintf(fname, sizeof(fname), "%lx", tv.tv_sec * 1000 + tv.tv_usec / 1000);
    fd = creat(fname, 0644);

    if (fd == -1)
        return;

    write(fd, buffer, n);
    close(fd);
}
#endif

#define NO_TIMEOUT -1
#define MIN(x, y) (x) > (y) ? (y) : (x)
#if 0
static int do_decode(int fd, char *buffer, int n, cserver::cs_req &req, char *id) {
    int rv;
    int nhead;
    /*read 4byte head*/
    rv = readn_timedwait(fd, buffer, 4,  NO_TIMEOUT);
    if (rv < 0)
        return TASK_ERR;
    if (rv == 0)
        return TASK_EOF;

    nhead = htonl(*(uint32_t *)buffer);

    printf("    read head = %d\n", nhead);
    rv = readn_timedwait(fd, buffer + 4, MIN(nhead, n - 4), NO_TIMEOUT);
    if (rv < 0)
        return TASK_ERR;
    if (rv == 0)
        return TASK_EOF;


    /*debug*/
    /*parser data*/
    if (!req.ParseFromArray(buffer + 4, rv)) {
        LOG_WARN("ParseFromArray error %s\n", id);
        return TASK_ERR;
        /* wfile(buffer + 4, rv);*/
    }

    return 1;
}
#endif

__attribute__((unused)) static void pr_x(char *p, int n) {
    int i;
    for (i = 0; i < n; i++)
        printf("%0x ", p[i]);
}

#if 0
static int do_code(int fd, char *buffer, int n, int err, cserver::cs_rsp &rsp, char *id) {
    rsp.set_errcode(err);
    int rsp_byte_size = rsp.ByteSize();
    int size = MIN(n - 4, rsp_byte_size);
    int rv;

    *(uint32_t *)buffer = htonl(size);

    if (!rsp.SerializeToArray(buffer + 4, size)) {
        printf("SerializeToArray error\n");
        return TASK_ERR;
        /* wfile(buffer + 4, size); */
    }

    LOG_DEBUG("    write head:total = %d:size = %d:err = %d\n", size + 4, size, err);
    if ((rv = writen_timedwait(fd, buffer, size + 4, -1)) <= 0) {
        printf("    write rv = %d\n", rv);
        return TASK_ERR;
    }

    return 0;
}
#endif

#if 0
static void session_gen(char *s, int n) {
    struct timespec t;
    uint64_t u64;

    int rv = 0;
    rv = clock_gettime(CLOCK_REALTIME, &t);
    u64 = (rv == 0) ? (t.tv_sec * 1000 * 1000 * 1000 + t.tv_nsec) : 0;

    snprintf(s, n, "0x%lx", u64);
}
#endif

int engine_msg_process(tp_vtable_child_arg_t *c, int fd) {
    /* set_fl(fd, O_NONBLOCK); */

#if 0
    char buffer[1024 * 1024];/* 1MB */
    int    rv0= -1;
    int    rv = 0;

    char   sessionid[67] = "";
    session_gen(sessionid, sizeof(sessionid));/* 生成session id */
#endif

    struct engine_base *engine= NULL;
    tp_vtable_global_arg_t *g;
    g = c->global;
    engine = (struct engine_base *)g->consumer_arg;

#if 0
    for (;;) {
        rv0 = do_decode(fd, buffer, sizeof(buffer), msg.req, sessionid);
        if (rv0 == TASK_ERR || rv0 == TASK_EOF) {
            LOG_WARN("...do_decode:id = %s:rv0(%d) fd(%d)\n", sessionid, rv0, fd);
            return -1;
        }

        if (do_code(fd, buffer, sizeof(buffer), rv, msg.rsp, sessionid) == TASK_ERR) {
            LOG_WARN("...do_code:id =%s\n", sessionid);
            return -1;
        }


    }
#endif
    return 0;
}

#define MAX_TIME 19
#define MIN_TIME 5
static int template_consumer_process(tp_vtable_child_arg_t *arg) {
    tp_vtable_global_arg_t  *g       = arg->global;
    template_producer_t         *consumer= (template_producer_t *)g->producer_arg;
    tp_chan_t               *chan    = consumer->chan;
    tp_pool_t               *pool    = g->self_pool;
    uint64_t                 id      = 0;
    void                    *val     = NULL;
    int                      rv      = 0;
    struct template_conf        *conf    = (struct template_conf *)g->user_data;
    int                      min_nt  = conf->min_nthreads;

    int                      time_out    = MAX_TIME;

    for (;;) {
        val = (void *)(intptr_t)-1;
        printf("recv fd before\n");
        rv = tp_chan_recv_timedwait(chan, &val, -1);
        if (rv == TP_SHUTDOWN) {
            printf("id(%ld) shutdown\n", id);
            return TP_PLUGIN_EXIT;
        }

        printf("recv fd after\n");
        /*
         * 如果要收缩线程的话, timeout时可以退出，
         * 并退出template_consumer_process
         */
        if (ATOMIC_LOAD(&g->c_ref_count) + ATOMIC_LOAD(&g->p_ref_count) > min_nt
            && rv == ETIMEDOUT) {
            printf("delete %s\n", __func__);
            tp_pool_thread_deln(pool, 1);
            return TP_PLUGIN_EXIT;
        }

        engine_msg_process(arg, (intptr_t)val);

        close((intptr_t)val);

        /* 第一次用max_time, 以后用min_time */
        if (time_out == MAX_TIME) {
            time_out = MIN_TIME;
        }
    }

    printf("%s %ldbye bye\n", __func__, (long)pthread_self());
    return 0;
}

static int listen_new(unsigned ip, unsigned short port) {
    struct sockaddr_in ser;
    int listen_fd, rv;

    ser.sin_family = AF_INET;
    ser.sin_addr.s_addr = ip;
    ser.sin_port   = port;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
        return -1;

    rv = bind(listen_fd, (struct sockaddr *)&ser, sizeof(struct sockaddr_in));
    if (rv == -1)
        return -1;

    if (listen(listen_fd, 0) == -1) {
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

static int template_producer_global_init(tp_vtable_global_arg_t *g) {
    template_producer_t *producer = NULL;
    struct template_conf *conf    = (struct template_conf *)g->user_data;

    producer = (template_producer_t *)malloc(sizeof(*producer));
    assert(producer != NULL);

    producer->chan  = tp_chan_new(30);
    assert(producer->chan != NULL);

    producer->listen_fd = listen_new(0, conf->sa.sin_port);
    if (producer->listen_fd == -1) {
        printf("listen new fd fail:%s\n",
              strerror(errno));
        exit(1);
    }

    g->producer_arg = producer;

    printf("%s ok\n", __func__);
    return 0;
}

static int accept_timedwait(int listen_fd, int sec) {
    fd_set rfd;
    struct timeval tv;
    tv.tv_sec  = sec;
    tv.tv_usec = 0;

    FD_ZERO(&rfd);
    FD_SET(listen_fd, &rfd);
    return select(listen_fd + 1, &rfd, NULL, NULL, &tv);
}

int template_producer_process(tp_vtable_child_arg_t *c) {
    tp_vtable_global_arg_t *g         = c->global;
    template_producer_t        *producer  = (template_producer_t *)g->producer_arg;
    tp_chan_t              *chan      = producer->chan;
    tp_pool_t              *pool      = g->self_pool;
    int                     connfd    = -1;
    int                     listen_fd = producer->listen_fd;
    int                     rv, n;

    for (;;) {
        rv = accept_timedwait(listen_fd, 5/* sec */);
        if (rv > 0) {
            /* on success */
            connfd = accept(listen_fd, NULL, NULL);
            if (connfd == -1) {
                printf("accept error:%s\n", strerror(errno));
                break;
            }
        } else if (rv == -1) {
            /* select listen fd fail */
            printf("accept_timedwait error:%s", strerror(errno));
            exit(1);
        } else if (rv == 0){
            /* select listen fd timeout */
            continue;
        }

        printf("accept send fd before:%u\n", connfd);
        n = 3;
        while (n--) {
            rv = tp_chan_send_timedwait(chan, (void *)(intptr_t)connfd, 500 /*ms*/);
            if (rv == 0) {
                connfd = -1;
                break;
            }
            /* 下面者段代码不会执行, 除非调用tp_chan_wake */
            if (rv == TP_SHUTDOWN) {
                close(connfd);
                goto done;
            }

            if (rv == ETIMEDOUT) {
                /*add new thread */
                rv = tp_pool_thread_addn(pool, 1);
                if (rv == 0) {
                    /* add consumers plugins */
                    tp_pool_plugin_producer_consumer_add(pool, NULL, 0, &template_consumers, 1);
                }
            }
        }
        if (connfd != -1) {
            close(connfd);
        }
        printf("accept send fd after:%u\n", connfd);
    }

done:
    return TP_PLUGIN_EXIT;
}

void template_producer_global_destroy(tp_vtable_global_arg_t *g) {
    template_producer_t *producer = (template_producer_t *)g->producer_arg;

    close(producer->listen_fd);
    tp_chan_free(producer->chan);
    free(producer);
}

int main(int argc, char **argv) {

    tp_pool_t *pool;

    struct template_conf conf;
    pool = tp_pool_new(conf.max_nthreads, (conf.max_nthreads + conf.min_nthreads) / 2,
           conf.min_nthreads, TP_MIN_CREATE, TP_NULL);

    if (pool == NULL) {
        printf("thread pool new fail\n");
        return 1;
    }

    template_produces.user_data = &conf;
    template_consumers.user_data = &conf;

    tp_pool_plugin_producer_consumer_add(pool, &template_produces,
                                        1, &template_consumers, conf.max_nthreads - 1);

    tp_pool_wait(pool, TP_SLOW);

    tp_pool_free(pool);

    return 0;
}
