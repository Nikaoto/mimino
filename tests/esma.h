// Unit testing microframework

#ifndef _MIMINO_ESMA_H
#define _MIMINO_ESMA_H

// VT100 colors
#define ESMA_C_RESET     "\x1b[0m"
#define ESMA_C_FG_BWHITE "\x1b[97m"
#define ESMA_C_FG_GRAY   "\x1b[2m"
#define ESMA_C_FG_BLACK  "\x1b[30m"
#define ESMA_C_FG_RED    "\x1b[31m"
#define ESMA_C_FG_GREEN  "\x1b[32m"
#define ESMA_C_FG_YELLOW "\x1b[33m"
#define ESMA_C_BG_RED    "\x1b[41m"
#define ESMA_C_BG_GREEN  "\x1b[42m"
#define ESMA_C_BG_YELLOW "\x1b[44m"

extern int tests_run;
extern int tests_ok;
extern int tests_failed;
extern int log_ind;

#ifndef esma_printf
#include <stdio.h>
#define esma_printf printf
#endif

#define esma_log(...) do {            \
    for (int i = 0; i < log_ind; i++) \
        esma_printf(" ");             \
    esma_printf(__VA_ARGS__);         \
} while (0);

#define esma_log_fail(msg) do {                 \
    log_ind++;                                  \
    esma_log("%s %s%s:%i%s %s\n",               \
             ESMA_C_FG_RED "FAIL" ESMA_C_RESET, \
             ESMA_C_FG_GRAY,                    \
             __FILE__,                          \
             __LINE__,                          \
             ESMA_C_RESET,                      \
             msg);                              \
    log_ind--;                                  \
} while (0);

#define esma_log_ok(msg) do {                   \
    log_ind++;                                  \
    esma_log("%s %s%s:%i%s %s\n",               \
             ESMA_C_FG_GREEN "OK" ESMA_C_RESET, \
             ESMA_C_FG_GRAY,                    \
             __FILE__,                          \
             __LINE__,                          \
             ESMA_C_RESET,                      \
             msg);                              \
    log_ind--;                                  \
} while (0);

#define esma_log_test(...) do {                \
    log_ind = 1;                               \
    esma_log("%s: ",                           \
        ESMA_C_FG_YELLOW "TEST" ESMA_C_RESET); \
    esma_printf(__VA_ARGS__);                  \
    esma_printf("\n");                         \
} while (0);

#define esma_log_subtest(...) do {                \
    log_ind = 2;                                  \
    esma_log("%s: ",                              \
        ESMA_C_FG_YELLOW "SUBTEST" ESMA_C_RESET); \
    esma_printf(__VA_ARGS__);                     \
    esma_printf("\n");                            \
} while (0);


#define esma_assert(expr) do { \
    if (!(expr)) {             \
        esma_log_fail(#expr);  \
        tests_run++;           \
        tests_failed++;        \
    } else {                   \
        esma_log_ok(#expr);    \
        tests_run++;           \
        tests_ok++;            \
    }                          \
} while (0);

#define esma_run_test(test) do {                               \
    log_ind = 0;                                               \
    esma_log("\n");                                            \
    esma_log("%s", ESMA_C_FG_YELLOW "RUNS" ESMA_C_RESET ": "); \
    esma_log("%s\n", ESMA_C_FG_BWHITE #test ESMA_C_RESET);     \
    test();                                                    \
} while (0);

#define esma_report() do {                         \
    esma_log("\n");                                \
    if (tests_failed == 0) {                       \
        esma_log(ESMA_C_BG_GREEN ESMA_C_FG_BLACK); \
    } else {                                       \
        esma_log(ESMA_C_BG_RED ESMA_C_FG_BLACK);   \
    }                                              \
    esma_log("=== Results ===");                   \
    esma_log(ESMA_C_RESET);                        \
    esma_log("\n");                                \
    esma_log("TOTAL: %i\n", tests_run);            \
    esma_log("OK: %i\n", tests_ok);                \
    esma_log("FAIL: %i\n", tests_failed);          \
} while (0);

#endif // _MIMINO_ESMA_H
