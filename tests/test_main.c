#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "arg.h"
#include "http.h"
#include "esma.h"

#define lenof(x) (sizeof(x) / sizeof(*(x)))

int tests_run = 0;
int tests_ok = 0;
int tests_failed = 0;
int log_ind = 0;

int
strcmp_safe(char *a, char *b)
{
    if (a == NULL && b == NULL) return 0;
    if (a == NULL) return 1;
    if (b == NULL) return -1;

    return strcmp(a, b);
}

int
strcmp_free_first(char *a, char *b)
{
    int ret = strcmp(a, b);
    free(a);
    return ret;
}

void
test_decode_url()
{
    // cmpstr to avoid clashing with possible 'strcmp' macro
    #define cmpstr strcmp_free_first

    esma_assert(!cmpstr(decode_url("/file%20space"), "/file space"));
    esma_assert(!cmpstr(decode_url("/file%20with%20spaces/"), "/file with spaces/"));
    esma_assert(!cmpstr(decode_url("%E1%83%A5%E1%83%90%E1%83%A0%E1%83%97%E1%83%A3%E1%83%9A%E1%83%98"), "ქართული"));
    esma_assert(!cmpstr(decode_url("http://asdf.com/../?#%"), "http://asdf.com/../?#%"));
    esma_assert(!cmpstr(decode_url("%%/%65%6d%41%63%24/kool"), "%/emAc$/kool"));
    esma_assert(!cmpstr(decode_url("%%/%65%6%41%63%24/kool"), "%/e%6Ac$/kool"));
    esma_assert(!cmpstr(decode_url("%CE%A3%CE%BA%CE%AD%CF%86%CF%84%CE%BF%CE%BC%CE%B1%CE%B9%2C%20%CE%AC%CF%81%CE%B1%20%CF%85%CF%80%CE%AC%CF%81%CF%87%CF%89"), "Σκέφτομαι, άρα υπάρχω"));
    esma_assert(!cmpstr(decode_url("%E6%88%91%E6%80%9D%E6%95%85%E6%88%91%E5%9C%A8"), "我思故我在"));

    #undef cmpstr
}

// Resets all .value and .err fields in given argdef array
void
argdef_undo_parse(Argdef *a, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        memset(&(a->value), 0, sizeof(a->value));
        a->err = NULL;
    }
}

void
test_parse_args()
{
    #define cmpstr strcmp_safe

    esma_log_test("Handle one short argument");
    {
        Argdef ad[1];
        memset(ad, 0, sizeof(ad));
        ad[0] = (Argdef) {
            .short_arg = 'v',
            .type = ARGDEF_TYPE_BOOL,
        };

        char *argv[] = {"./mimino", "-v"};
        esma_assert(parse_args(lenof(argv), argv, lenof(ad), &ad));
        esma_assert(ad[0].value.b);
    }


    esma_log_test("Handle one raw argument");
    {
        Argdef ad_raw[1];
        memset(ad_raw, 0, sizeof(ad_raw));
        ad_raw[0] = (Argdef) {
            .type = ARGDEF_TYPE_RAW,
        };

        char *argv_raw[] = {"./mimino", "./."};
        esma_assert(parse_args(lenof(argv_raw), argv_raw, lenof(ad_raw), &ad_raw));
        esma_assert(!cmpstr(ad_raw[0].value.s, "./."));
    }

    esma_log_test("Handle two raw arguments");
    {
        Argdef ad_raw[2];
        memset(ad_raw, 0, sizeof(ad_raw));
        ad_raw[0] = (Argdef) {
            .type = ARGDEF_TYPE_RAW,
        };
        ad_raw[1] = (Argdef) {
            .type = ARGDEF_TYPE_RAW,
        };

        esma_log_subtest("Proper order");
        char *argv_raw[] = {"./mimino", "./.", "123"};
        esma_assert(parse_args(lenof(argv_raw), argv_raw, lenof(ad_raw), &ad_raw));
        esma_assert(!cmpstr(ad_raw[0].value.s, "./."));
        esma_assert(!cmpstr(ad_raw[1].value.s, "123"));

        // Reset
        argdef_undo_parse(ad_raw, lenof(ad_raw));

        esma_log_subtest("Reverse order");
        char *argv_raw_rev[] = {"./mimino", "123", "./."};
        esma_assert(parse_args(lenof(argv_raw_rev), argv_raw_rev, lenof(ad_raw), &ad_raw));
        esma_assert(!cmpstr(ad_raw[0].value.s, "123"));
        esma_assert(!cmpstr(ad_raw[1].value.s, "./."));
    }


    esma_log_test("Handle one long boolean argument");
    {
        Argdef ad_long[1];
        memset(ad_long, 0, sizeof(ad_long));
        ad_long[0] = (Argdef) {
            .long_arg = "verbose",
            .type = ARGDEF_TYPE_BOOL,
        };

        // Normal
        esma_log_subtest("Normal");
        char *argv_l_norm[] = {"./mimino", "--verbose"};
        esma_assert(parse_args(lenof(argv_l_norm), argv_l_norm, lenof(ad_long), &ad_long));
        esma_assert(ad_long[0].value.b);

        // Reset
        argdef_undo_parse(ad_long, lenof(ad_long));

        // Using '='
        esma_log_subtest("Using '='");
        char *argv_l_eq[] = {"./mimino", "--verbose=1"};
        esma_assert(parse_args(lenof(argv_l_eq), argv_l_eq, lenof(ad_long), &ad_long));
        esma_assert(ad_long[0].value.b);
    }


    esma_log_test("Handle one long string argument");
    {
        Argdef ad_long[1];
        memset(ad_long, 0, sizeof(ad_long));
        ad_long[0] = (Argdef) {
            .long_arg = "port",
            .type = ARGDEF_TYPE_STRING,
        };

        // Normal
        esma_log_subtest("Normal");
        char *argv_norm[] = {"./mimino", "--port", "8080"};
        esma_assert(parse_args(lenof(argv_norm), argv_norm, lenof(ad_long), &ad_long));
        esma_assert(!cmpstr(ad_long[0].value.s, "8080"));

        // Reset
        argdef_undo_parse(ad_long, lenof(ad_long));

        // Using '='
        esma_log_subtest("Using '='");
        char *argv_eq[] = {"./mimino", "--port=8080"};
        esma_assert(parse_args(lenof(argv_eq), argv_eq, lenof(ad_long), &ad_long));
        esma_assert(!cmpstr(ad_long[0].value.s, "8080"));
    }

    /* Argdef argdefs[5]; */
    /* memset(argdefs, 0, sizeof(argdefs)); */
    /* argdefs[0] = (Argdef) { */
    /*     .id = 0, */
    /*     .short_arg = 'v', */
    /*     .long_arg = "verbose", */
    /*     .type = ARGDEF_TYPE_BOOL, */
    /* }; */
    /* argdefs[1] = (Argdef) { */
    /*     .id = 1, */
    /*     .short_arg = 'q', */
    /*     .long_arg = "quiet", */
    /*     .type = ARGDEF_TYPE_BOOL, */
    /* }; */
    /* argdefs[2] = (Argdef) { */
    /*     .id = 2, */
    /*     .short_arg = 'p', */
    /*     .long_arg = "port", */
    /*     .type = ARGDEF_TYPE_STRING, */
    /* }; */
    /* argdefs[3] = (Argdef) { */
    /*     .id = 3, */
    /*     .short_arg = 'i', */
    /*     .long_arg = "index", */
    /*     .type = ARGDEF_TYPE_STRING, */
    /* }; */
    /* argdefs[4] = (Argdef) { */
    /*     .id = 4, */
    /*     .type = ARGDEF_TYPE_RAW, */
    /* }; */

    /* const int argc = 6; */
    /* char *argv[] = {"./mimino", "-vu", "-p8080", "-i", "index.html", "./testdir/"}; */

    /* parse_args(argc, argv, 5, argdefs); */

    /* esma_assert(argdefs[0].value.b == 0); */
    /* esma_assert(argdefs[1].value.b == 0); */
    /* esma_assert(!cmpstr(argdefs[2].value.s, "8080")); */
    /* esma_assert(argdefs[3].value.b == 1); */
    /* esma_assert(!cmpstr(argdefs[3].value.s, "index.html")); */
    /* esma_assert(!cmpstr(argdefs[4].value.s, "./testdir/")); */
}

int
main(int argc, char **argv)
{
    esma_run_test(test_decode_url);
    esma_run_test(test_parse_args);
    esma_report();
}
