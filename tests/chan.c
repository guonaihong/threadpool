#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "threadpool.h"

static void tst_chan_send_timeout() {
    tp_chan_t *chan = tp_chan_new(5);
    char      *p    = NULL;
    void      *v    = NULL;
    int        i, err;

    for (i = 1; i < 10; i++) {
        p = (char *)"ok";
        err = tp_chan_send_timedwait(chan, (void *)(intptr_t)i, 10/* ms */);
        if (err != 0) {
            p = (char *)"unknown error";
            if (err == ETIMEDOUT) {
                p = (char *)"send timeout";
            }
        }

        printf("(tst send timeout)send %d message, the value of %d %s\n",
               i, i, p);
    }

    printf("end send...bye bye\n");

    while (tp_chan_count(chan) > 0) {
        tp_chan_recv_timedwait(chan, &v, 1);
    }
    tp_chan_free(chan);
}

static void tst_chan_recv_timeout() {
    int i, err;
    tp_chan_t *chan = tp_chan_new(5);
    void      *v;
    char      *p;

    for (i = 0; i < 10; i++) {
        p = (char *)"ok";
        err = tp_chan_recv_timedwait(chan, &v, 10);
        if (err != 0) {
            p = (char *)"unknown error";
            if (err == ETIMEDOUT) {
                p = (char *)"recv timeout";
            }
        }
        printf("(tst recv timeout)recv %d message, %s\n",
               i, p);
    }

    printf("end recv...bye bye\n");
    tp_chan_free(chan);
}

int main() {

    tst_chan_send_timeout();

    tst_chan_recv_timeout();

    return 0;
}
