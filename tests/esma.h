#ifndef _MIMINO_ESMA_H

// Unit testing microframework

#ifndef esma_log
#include <stdio.h>
#define esma_log printf
#endif

// VT100 colors
#define ESMA_C_RESET    "\x1b[0m"
#define ESMA_C_FG_BLACK "\x1b[30m"
#define ESMA_C_FG_RED   "\x1b[31m"
#define ESMA_C_FG_GREEN "\x1b[32m"
#define ESMA_C_BG_RED   "\x1b[41m"
#define ESMA_C_BG_GREEN "\x1b[42m"

#define esma_log_fail(msg) \
    esma_log(ESMA_C_FG_RED "FAIL" ESMA_C_RESET ": " #msg "\n");
#define esma_log_ok(msg) \
    esma_log(ESMA_C_FG_GREEN "OK" ESMA_C_RESET ": " #msg "\n");

/* #define esma_log_fail(msg) esma_log("FAIL: " #msg "\n"); */
/* #define esma_log_ok(msg) esma_log("OK: " #msg "\n"); */

extern int tests_run;
extern int tests_ok;
extern int tests_failed;

#define esma_assert(expr) do {    \
    if (!(expr)) {                \
        esma_log_fail((expr));   \
        tests_run++;              \
        tests_failed++;           \
    } else {                      \
        esma_log_ok((expr));     \
        tests_run++;              \
        tests_ok++;               \
    }                             \
} while(0);                       \

void
esma_report()
{
    if (tests_failed == 0) {
        esma_log(ESMA_C_BG_GREEN ESMA_C_FG_BLACK);
    } else {
        esma_log(ESMA_C_BG_RED ESMA_C_FG_BLACK);
    }
    esma_log("=== Results ===\n");
    esma_log(ESMA_C_RESET);
    esma_log("TOTAL: %i\n", tests_run);
    esma_log("OK: %i\n", tests_ok);
    esma_log("FAIL: %i\n", tests_failed);
}

#endif // _MIMINO_ESMA_H
