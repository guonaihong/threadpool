#include "threadpool.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
void tst_debug(char **argv) {
    tp_log_t *log = tp_log_new(TP_DEBUG, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO, "2222\n");
    tp_log(log, TP_WARN, "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "tst debug end\n");
    tp_log_free(log);
}

void tst_info(char **argv) {
    tp_log_t *log = tp_log_new(TP_INFO, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO, "2222\n");
    tp_log(log, TP_WARN, "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "tst info end\n");
    tp_log_free(log);
}

void tst_warn(char **argv) {
    tp_log_t *log = tp_log_new(TP_WARN, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO, "2222\n");
    tp_log(log, TP_WARN, "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "tst warn end\n");
    tp_log_free(log);
}

void tst_error(char **argv) {
    tp_log_t *log = tp_log_new(TP_ERROR, argv[0], 0);
    tp_log(log, TP_DEBUG, "1111\n");
    tp_log(log, TP_INFO, "2222\n");
    tp_log(log, TP_WARN, "3333\n");
    tp_log(log, TP_ERROR, "4444\n");
    tp_log(log, TP_ERROR, "tst error end\n");
    tp_log_free(log);
}

#include <wchar.h>
void tst_long_string(char **argv) {
    tp_log_t *log = tp_log_new(TP_DEBUG, argv[0], 0);
    char buf[1024 * 1024] = "";
    int rv = read(0, buf, sizeof(buf));
    if (rv > 1) {
        buf[rv] = '\0';
    }
#if 0
    write(1, buf, sizeof(buf));
    printf("%s", buf);
#endif
    /* printf("rv = %d:%lu:%lu\n", rv, strlen(buf), wcslen(buf)); */
    tp_log(log, TP_DEBUG, "%s", buf);
    tp_log_free(log);
}

int main(int argc, char **argv) {
#if 0
    tst_debug(argv);
    tst_info(argv);
    tst_warn(argv);
    tst_error(argv);
#endif
    tst_long_string(argv);
    return 0;
}
