#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include "threadpool.h"

/* test sendtimeout begin */
void *chan_recv_pause(void *arg) {
    tp_chan_t *chan = (tp_chan_t *)arg;
    void *v;
    int   err;

    sleep(2);
    if ((err = tp_chan_recv_timedwait(chan, &v, 1000)) != 0) {
        printf("recv:%s\n", strerror(err));
        return 0;
    }

    printf("recv %ld\n", (intptr_t)v);
    return (void *)0;
}

void tst_chan_send_timeout() {
    int size = 5;
    tp_chan_t *chan = tp_chan_new(size);
    int i;
    pthread_t tid;

    pthread_create(&tid, NULL, chan_recv_pause, chan);
    for (i = 1; i < 10; i++) {
        int err = tp_chan_send_timedwait(chan, (void *)(intptr_t)i, 10);
        /*send timeout*/
        if (err != 0) {
            ;
        }

        printf("send (%d) %s\n", i, (err == 0) ? "ok" : strerror(err));
    }

    printf("end send\n");
    pthread_join(tid, NULL);
}
/* test sendtimeout end */

/* test recvtimeout begin */
void *chan_send(void *arg) {
    int i;
    tp_chan_t *chan = (tp_chan_t *)arg;

    for (i = 1; i < 10; i++) {
        int err = tp_chan_send_timedwait(chan, (void *)(intptr_t)i, 1000);
        if (err != 0) {
            ;
        }
        printf("send (%d) %s\n", i, (err == 0) ? "ok" : strerror(err));

        sleep(2);
    }

    return (void *)0;
}

void tst_chan_recv_timeout() {
    int size = 5;
    int i, err;
    tp_chan_t *chan = tp_chan_new(size);
    pthread_t tid;
    void *v;

    pthread_create(&tid, NULL, chan_send, chan);

    for (i = 0; i < 10; i++) {
        if ((err = tp_chan_recv_timedwait(chan, &v, 3000)) != 0) {
            printf("recv:%s\n", strerror(err));
        } else {
            printf("recv::%ld\n", (intptr_t)v);
        }
    }

    printf("end recv\n");
    pthread_join(tid, NULL);
}
/* test recvtimeout end */

int main() {

    tst_chan_send_timeout();

    tst_chan_recv_timeout();
    return 0;
}
