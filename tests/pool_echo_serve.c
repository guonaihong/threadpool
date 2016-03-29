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

#include "threadpool.h"

#ifdef  TP_PORT
# undef  TP_PORT
#endif

#define TP_PORT 5678

#ifdef TP_MODULE_NAME
# undef  TP_MODULE_NAME
#endif

#define TP_MODULE_NAME "echo server"

tp_log_t *mylog;
static void get_ns(uint64_t *u64) {
    struct timespec t;
    int rv = 0;
    rv = clock_gettime(CLOCK_REALTIME, &t);
    *u64 = (rv == 0) ? (t.tv_sec * 1000 * 1000 * 1000 + t.tv_nsec) : 0;
}

int listen_new(unsigned ip, unsigned short port) {
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

typedef struct echo_server_t echo_server_t;
struct echo_server_t {
    tp_chan_t *chan;
};

int producer_global_init(tp_vtable_global_arg_t *g) {
    echo_server_t *s = NULL;
    s = malloc(sizeof(*s));
    assert(s != NULL);

    s->chan = tp_chan_new(30);
    g->producer_arg  = s;
    tp_log(mylog, TP_INFO, "%s\n", __func__);
    return 0;
}

int producer_create(tp_vtable_child_arg_t *c) {
    c->child_arg = strdup("producer create ok");
    assert(c->child_arg != NULL);
    tp_log(mylog, TP_INFO, "%s\n", __func__);
    return 0;
}

/* listening socket */
int producer_process(tp_vtable_child_arg_t *arg) {
    tp_vtable_global_arg_t *g         = arg->global;
    echo_server_t          *s         = g->producer_arg;
    tp_chan_t              *chan      = s->chan;
    int                     listen_fd = -1;
    int                     connfd    = -1;
    uint64_t                id        = 0;

    tp_log(mylog, TP_INFO, "run process start user data:%s\n",
          (char *)arg->child_arg);

    listen_fd = listen_new(0, htons(TP_PORT));

    for (;;) {
        get_ns(&id);
        connfd = accept(listen_fd, NULL, NULL);
        if (connfd == -1) {
            tp_log(mylog, TP_INFO, "accept error:%s\n", strerror(errno));
            break;
        }

        tp_log(mylog, TP_INFO, "accept send fd before:%lu\n", id);
        if (tp_chan_send_timedwait(chan, (void *)(intptr_t)connfd, -1) != 0) {
            tp_log(mylog, TP_WARN, "send data error\n");
        }
        tp_log(mylog, TP_INFO, "accept send fd after:%lu\n", id);
    }

    return 0;
}

void producer_destroy(tp_vtable_child_arg_t *arg) {
    free(arg->child_arg);
}

void producer_global_destroy(tp_vtable_global_arg_t *g) {
    echo_server_t *s = g->producer_arg;
    tp_chan_free(s->chan);
    free(s);
}

/* from the client to read and write data back */
int consumer_process(tp_vtable_child_arg_t *arg) {
    tp_vtable_global_arg_t  *g       = arg->global;
    echo_server_t           *s       = g->producer_arg;
    tp_chan_t               *chan    = s->chan;
    uint64_t                 id      = 0;
    void                    *val     = NULL;
    char                     buf[128]= "";
    int                      rv      = 0;

    tp_log(mylog, TP_INFO, "%ld:%s\n", pthread_self(), __func__);
    for (;;) {
        val = (void *)(intptr_t)-1;
        get_ns(&id);
        tp_log(mylog, TP_INFO, "%ld:%s:id(%ld) recv fd before\n", pthread_self(), __func__, id);
        rv = tp_chan_recv_timedwait(chan, &val, -1);
        if (rv != 0) {
            break;
        }

        tp_log(mylog, TP_INFO, "%ld:%s:id(%ld) recv data before\n", pthread_self(), __func__, id);
        for (;;) {
            rv = read((intptr_t)val, buf, sizeof(buf));
            if (rv <= 0) {
                break;
            }
            tp_log(mylog, TP_INFO, "%ld:%s:%d:rv(%d)\n", pthread_self(), __func__, *(int *)buf, rv);
            send((intptr_t)val, buf, rv, MSG_NOSIGNAL);
        }
        tp_log(mylog, TP_INFO, "%ld:%s:id(%ld) recv data after\n", pthread_self(), __func__, id);
        close((intptr_t)val);
    }
    printf("%s %ldbye bye\n", __func__, pthread_self());
    return 0;
}

static tp_plugin_t producer = {
    .vtable = {
        .global_init = producer_global_init,
        .create = producer_create,
        .process = producer_process,
        .destroy = producer_destroy,
        .global_destroy = NULL,
    },
    .user_data   = "accept",
    .plugin_name = TP_MODULE_NAME,
};

static tp_plugin_t consumer = {
    .vtable= {
        .global_init = NULL,
        .create = NULL,
        .process = consumer_process,
        .destroy = NULL,
        .global_destroy = NULL,
    },
    .user_data   = "read write socket",
    .plugin_name = TP_MODULE_NAME,
};

#define TOTAL 10000
void echo_client(void *arg) {
    int s, r, n;
    struct sockaddr_in client;
    uint64_t id;

    get_ns(&id);

    tp_log(mylog, TP_INFO, "client start id:%lu\n", id);
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

    tp_log(mylog, TP_INFO, "client connect ok:%lu\n", id);
    /*n = __sync_add_and_fetch(&count, 1);*/
    n = (intptr_t)arg;
    send(s, &n, sizeof(n), MSG_NOSIGNAL);
fail:
    close(s);
    tp_log(mylog, TP_INFO, "client end id:%lu\n", id);
}

void start_client(tp_pool_t *pool) {
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

    /*
     * begin to create 5 threads,
     * you can only create a maximum of 30 threads
     */
    tp_pool_t *pool, *client_pool;

    mylog = tp_log_new(TP_INFO, "echo client", TP_LOG_LOCK);
    pool = tp_pool_create(30, 30, TP_NULL);
    assert(pool != NULL);

    client_pool = tp_pool_create(30, 30, TP_NULL);
    assert(pool != NULL);

    fprintf(stderr, "pool create ok\n");

    tp_pool_plugin_producer_consumer_add(pool, &producer, 1, &consumer, 29);

    start_client(client_pool);

    tp_pool_wait(client_pool, TP_FAST);
    tp_pool_wait(pool, TP_SLOW);

    tp_pool_destroy(client_pool);
    tp_pool_destroy(pool);
    return 0;
}
