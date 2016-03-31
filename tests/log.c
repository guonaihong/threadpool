#include "threadpool.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void tst_debug(char **argv) {
    tp_log_t *log = tp_log_new(TP_DEBUG, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO,  "2222\n");
    tp_log(log, TP_WARN,  "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "You must print debug, info, warn, error log level\n");
    tp_log_free(log);
}

static void tst_info(char **argv) {
    tp_log_t *log = tp_log_new(TP_INFO, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO,  "2222\n");
    tp_log(log, TP_WARN,  "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "You must print      , info, warn, error log level\n");
    tp_log_free(log);
}

static void tst_warn(char **argv) {
    tp_log_t *log = tp_log_new(TP_WARN, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO,  "2222\n");
    tp_log(log, TP_WARN,  "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "You must print      ,     , warn, error log level\n");
    tp_log_free(log);
}

static void tst_error(char **argv) {
    tp_log_t *log = tp_log_new(TP_ERROR, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO,  "2222\n");
    tp_log(log, TP_WARN,  "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "You must print      ,     ,     , error log level");
    tp_log_free(log);
}

#include <wchar.h>

static void tst_write_long_string(char **argv) {
    tp_log_t *log         = NULL;
    char buf[1024 * 1024] = "";
    int i, c;

    log = tp_log_new(TP_DEBUG, argv[0], 0);

    for (i = 0, c = 'a'; i < BUFSIZ * 3; i++) {
        if (c > 'z') {
            c = 'a';
        }
        buf[i] = c++;
    }

    tp_log(log, TP_DEBUG, "%s", buf);
    tp_log_free(log);
}

int main(int argc, char **argv) {

    tst_debug(argv);
    tst_info(argv);
    tst_warn(argv);
    tst_error(argv);

    tst_write_long_string(argv);
    return 0;
}
