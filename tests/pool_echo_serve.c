#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "threadpool.h"

#ifdef  TP_PORT
# undef  TP_PORT
#endif

#define TP_PORT 5678

#ifdef TP_PLUGIN_NAME
# undef  TP_PLUGIN_NAME
#endif

#define TP_PLUGIN_NAME "echo server"

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

tp_log_t *mylog;

typedef struct echo_server_t echo_server_t;

struct echo_server_t {
    tp_chan_t *chan;
    int        listen_fd;
};

static int set_fl(int fd, int flags);

static int  producer_global_init(tp_vtable_global_arg_t *g);
static int  producer_create(tp_vtable_child_arg_t *c);
static int  producer_process(tp_vtable_child_arg_t *arg);
static void producer_destroy(tp_vtable_child_arg_t *arg);
static void producer_global_destroy(tp_vtable_global_arg_t *g);

static tp_plugin_t producer = {
    {
        producer_global_init,
        producer_create,
        producer_process,
        producer_destroy,
        producer_global_destroy,
    },
    (void *)"accept",
    TP_PLUGIN_NAME,
};

static int consumer_process(tp_vtable_child_arg_t *arg);

static tp_plugin_t consumer = {
    {
        NULL,
        NULL,
        consumer_process,
        NULL,
        NULL,
    },
    (void *)"read write socket",
    TP_PLUGIN_NAME,
};

static void get_ns(uint64_t *u64) {
#ifdef __linux__
    struct timespec t;
    int rv = 0;
    rv = clock_gettime(CLOCK_REALTIME, &t);
    *u64 = (rv == 0) ? (t.tv_sec * 1000 * 1000 * 1000 + t.tv_nsec) : 0;
#endif

#ifdef __APPLE__
    *u64 = 0;
#endif
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

static int producer_global_init(tp_vtable_global_arg_t *g) {
    echo_server_t *s = NULL;

    s = (echo_server_t *)malloc(sizeof(*s));
    assert(s != NULL);

    s->chan = tp_chan_new(30);

    s->listen_fd = listen_new(0, htons(TP_PORT));
    if (s->listen_fd == -1) {
        tp_log(mylog, TP_ERROR, "listen new fd fail:%s\n",
              strerror(errno));
        return -1;
    }

    set_fl(s->listen_fd, O_NONBLOCK);

    g->producer_arg  = s;
    tp_log(mylog, TP_INFO, "%s\n", __func__);
    return 0;
}

static int producer_create(tp_vtable_child_arg_t *c) {
    c->child_arg = strdup("producer create ok");
    assert(c->child_arg != NULL);
    tp_log(mylog, TP_INFO, "%s\n", __func__);
    return 0;
}

static int set_fl(int fd, int flags) {
    int val;
    val = fcntl(fd, F_GETFL, 0);
    if (val == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    val |= flags;
    if (fcntl(fd, F_SETFL, val) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
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

/* listening socket */
static int producer_process(tp_vtable_child_arg_t *arg) {
    tp_vtable_global_arg_t *g         = arg->global;
    echo_server_t          *s         = (echo_server_t *)g->producer_arg;
    tp_chan_t              *chan      = s->chan;
    int                     connfd    = -1;
    int                     listen_fd = s->listen_fd;
    uint64_t                id        = 0;
    int                     rv        = 0;

    tp_log(mylog, TP_INFO, "run process start user data:%s\n",
          (char *)arg->child_arg);

    for (;;) {
        get_ns(&id);
        rv = accept_timedwait(listen_fd, 5/* sec */);
        if (rv > 0) {
            /* on success */
            connfd = accept(listen_fd, NULL, NULL);
            if (connfd == -1) {
                tp_log(mylog, TP_INFO, "accept error:%s\n", strerror(errno));
                break;
            }
        } else if (rv == -1) {
            /* select listen fd fail */
            tp_log(mylog, TP_ERROR, "accept_timedwait error:%s", strerror(errno));
            exit(1);
        } else if (rv == 0){
            /* select listen fd timeout */
            tp_chan_wake(chan);
            tp_log(mylog, TP_WARN, "accept_timedwait timeout\n");
            /*
             * This is just a test program, where quit. 
             * Real business server is generally not quit
             */
            goto done;
        }

        tp_log(mylog, TP_DEBUG, "accept send fd before:%lu\n", id);
        if ((rv = tp_chan_send_timedwait(chan, (void *)(intptr_t)connfd, -1)) != 0) {
            if (rv == TP_SHUTDOWN) {
                goto done;
            }

            tp_log(mylog, TP_WARN, "send data error\n");
        }
        tp_log(mylog, TP_DEBUG, "accept send fd after:%lu\n", id);
    }

    return 0;

done:
    return TP_PLUGIN_EXIT;
}

static void producer_destroy(tp_vtable_child_arg_t *arg) {
    free(arg->child_arg);
}

static void producer_global_destroy(tp_vtable_global_arg_t *g) {
    echo_server_t *s = (echo_server_t *)g->producer_arg;
    tp_chan_free(s->chan);
    close(s->listen_fd);
    free(s);
    tp_log(mylog, TP_INFO, "%s\n", __func__);
}

/* from the client to read and write data back */
static int consumer_process(tp_vtable_child_arg_t *arg) {
    tp_vtable_global_arg_t  *g       = arg->global;
    echo_server_t           *s       = (echo_server_t *)g->producer_arg;
    tp_chan_t               *chan    = s->chan;
    uint64_t                 id      = 0;
    void                    *val     = NULL;
    char                     buf[128]= "";
    int                      rv      = 0;

    tp_log(mylog, TP_INFO, "%ld:%s\n", pthread_self(), __func__);
    for (;;) {
        val = (void *)(intptr_t)-1;
        get_ns(&id);
        tp_log(mylog, TP_DEBUG, "%ld:%s:id(%ld) recv fd before\n", pthread_self(), __func__, id);
        rv = tp_chan_recv_timedwait(chan, &val, -1);
        if (rv == TP_SHUTDOWN) {
            tp_log(mylog, TP_INFO, "%ld:%s:id(%ld) shutdown\n", pthread_self(), __func__, id);
            return TP_PLUGIN_EXIT;
        }

        tp_log(mylog, TP_DEBUG, "%ld:%s:id(%ld) recv data before\n", pthread_self(), __func__, id);
        for (;;) {
            rv = read((intptr_t)val, buf, sizeof(buf));
            if (rv <= 0) {
                break;
            }
            tp_log(mylog, TP_DEBUG, "%ld:%s:%d:rv(%d)\n", pthread_self(), __func__, *(int *)buf, rv);
            send((intptr_t)val, buf, rv, MSG_NOSIGNAL);
        }
        tp_log(mylog, TP_DEBUG, "%ld:%s:id(%ld) recv data after\n", pthread_self(), __func__, id);
        close((intptr_t)val);
    }
    printf("%s %ldbye bye\n", __func__, (long)pthread_self());
    return 0;
}

#define TOTAL 100
static void echo_client(void *arg) {
    int s, r, n;
    struct sockaddr_in client;
    uint64_t id;

    get_ns(&id);

    tp_log(mylog, TP_DEBUG, "client start id:%lu\n", id);
    s = socket(AF_INET, SOCK_STREAM, 0);
    assert(s != -1);

    memset(&client, 0, sizeof(client));
    client.sin_family = AF_INET;
    if (inet_aton("127.0.0.1", &client.sin_addr) == 0) {
        perror("inet_aton");
        exit(EXIT_FAILURE);
    }

    client.sin_port = htons(TP_PORT);
    r = connect(s, (struct sockaddr*)&client, sizeof(client));

    if (r != 0) {
        tp_log(mylog, TP_INFO, "connect:%s\n", strerror(errno));
        goto fail;
    }

    tp_log(mylog, TP_DEBUG, "client connect ok:%lu\n", id);
    /*n = __sync_add_and_fetch(&count, 1);*/
    n = (intptr_t)arg;
    send(s, &n, sizeof(n), MSG_NOSIGNAL);
fail:
    close(s);
    tp_log(mylog, TP_DEBUG, "client end id:%lu\n", id);
}

static void start_client(tp_pool_t *pool) {
    int        i;

    for (i = 0; i < TOTAL; i++) {
        if (tp_pool_add(pool, echo_client, (void *)(intptr_t)i) != 0) {
            printf("send echo_client (%d) timeout\n", i);
            usleep(500 * 1000);
            i--;
        }
    }
}

int main() {

    tp_pool_t *server_pool, *client_pool;

    mylog = tp_log_new(TP_INFO, (char *)"echo client", TP_LOG_LOCK);
    server_pool = tp_pool_new(30 /*max thread number*/, 30/* chan size */, 
            1/* min threads */, TP_AUTO_ADD | TP_AUTO_DEL, TP_NULL);
    assert(server_pool != NULL);

    client_pool = tp_pool_new(30, 30, TP_NULL);
    assert(server_pool != NULL);

    fprintf(stderr, "server_pool create ok\n");

    tp_pool_plugin_producer_consumer_add(server_pool, &producer, 1, &consumer, 29);

    usleep(1000);/* sleep 1ms wait server listen ok */
    start_client(client_pool);

    tp_pool_wait(client_pool, TP_FAST);
    tp_pool_wait(server_pool, TP_SLOW);

    tp_pool_free(client_pool);
    tp_pool_free(server_pool);

    tp_log(mylog, TP_INFO, "run ok\n");
    tp_log_free(mylog);
    return 0;
}
