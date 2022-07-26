// Unit testing microframework

#ifndef _MIMINO_ESMA_H

#ifndef esma_log
#include <stdio.h>
#define esma_log printf
#endif

// VT100 colors
#define ESMA_C_RESET     "\x1b[0m"
#define ESMA_C_FG_BWHITE "\x1b[97m"
#define ESMA_C_FG_BLACK  "\x1b[30m"
#define ESMA_C_FG_RED    "\x1b[31m"
#define ESMA_C_FG_GREEN  "\x1b[32m"
#define ESMA_C_FG_YELLOW "\x1b[33m"
#define ESMA_C_BG_RED    "\x1b[41m"
#define ESMA_C_BG_GREEN  "\x1b[42m"
#define ESMA_C_BG_YELLOW "\x1b[44m"

#define esma_log_fail(msg) \
    esma_log("%s\n", ESMA_C_FG_RED "FAIL" ESMA_C_RESET ": " #msg);
#define esma_log_ok(msg) \
    esma_log("%s\n", ESMA_C_FG_GREEN "OK" ESMA_C_RESET ": " #msg);

extern int tests_run;
extern int tests_ok;
extern int tests_failed;

#define esma_assert(expr) do {    \
    if (!(expr)) {                \
        esma_log_fail(expr);      \
        tests_run++;              \
        tests_failed++;           \
    } else {                      \
        esma_log_ok(expr);        \
        tests_run++;              \
        tests_ok++;               \
    }                             \
} while(0);

#define esma_run_test(test) do {                               \
    esma_log("\n");                                            \
    esma_log("%s", ESMA_C_FG_YELLOW "RUNS" ESMA_C_RESET ": "); \
    esma_log("%s\n", ESMA_C_FG_BWHITE #test ESMA_C_RESET);     \
    test();                                                    \
} while(0);

void
esma_report()
{
    esma_log("\n");
    if (tests_failed == 0) {
        esma_log(ESMA_C_BG_GREEN ESMA_C_FG_BLACK);
    } else {
        esma_log(ESMA_C_BG_RED ESMA_C_FG_BLACK);
    }
    esma_log("=== Results ===");
    esma_log(ESMA_C_RESET);
    esma_log("\n");
    esma_log("TOTAL: %i\n", tests_run);
    esma_log("OK: %i\n", tests_ok);
    esma_log("FAIL: %i\n", tests_failed);
}

#endif // _MIMINO_ESMA_H
