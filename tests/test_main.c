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

// Resets all .value, .bvalue and .err fields in given argdef array
void
argdef_undo_parse(Argdef *a, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        a[i].value = NULL;
        a[i].bvalue = 0;
        a[i].err = NULL;
    }
}

void
test_parse_args()
{
    #define cmpstr strcmp_safe

    esma_log_test("Handle one short bool argument");
    {
        Argdef ad[1];
        memset(ad, 0, sizeof(ad));
        ad[0] = (Argdef) {
            .short_arg = 'v',
            .type = ARGDEF_TYPE_BOOL,
        };

        char *argv[] = {"./mimino", "-v"};
        esma_assert(parse_args(lenof(argv), argv, lenof(ad), &ad));
        esma_assert(ad[0].bvalue);
    }


    esma_log_test("Handle one short string argument");
    {
        Argdef ad[1];
        memset(ad, 0, sizeof(ad));
        ad[0] = (Argdef) {
            .short_arg = 'i',
            .type = ARGDEF_TYPE_STRING,
        };

        char *argv[] = {"./mimino", "-i", "asdf"};
        esma_assert(parse_args(lenof(argv), argv, lenof(ad), &ad));
        esma_assert(ad[0].bvalue);
        esma_assert(!cmpstr(ad[0].value, "asdf"));
    }


    esma_log_test("Handle one raw argument");
    {
        Argdef ad_raw[1];
        memset(ad_raw, 0, sizeof(ad_raw));
        ad_raw[0] = (Argdef) {
            .type = ARGDEF_TYPE_RAW,
        };

        char *argv_raw[] = {"./mimino", "./."};
        esma_assert(parse_args(lenof(argv_raw), argv_raw, lenof(ad_raw),
                               &ad_raw));
        esma_assert(!cmpstr(ad_raw[0].value, "./."));
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
        esma_assert(parse_args(lenof(argv_raw), argv_raw, lenof(ad_raw),
                               &ad_raw));
        esma_assert(!cmpstr(ad_raw[0].value, "./."));
        esma_assert(!cmpstr(ad_raw[1].value, "123"));

        // Reset
        argdef_undo_parse(ad_raw, lenof(ad_raw));

        esma_log_subtest("Reverse order");
        char *argv_raw_rev[] = {"./mimino", "123", "./."};
        esma_assert(parse_args(lenof(argv_raw_rev), argv_raw_rev, lenof(ad_raw),
                               &ad_raw));
        esma_assert(!cmpstr(ad_raw[0].value, "123"));
        esma_assert(!cmpstr(ad_raw[1].value, "./."));
    }


    esma_log_test("Handle one short unsupported argument");
    {
        Argdef ad[1];
        memset(ad, 0, sizeof(ad));
        ad[0] = (Argdef) {
            .short_arg = 'v',
            .type = ARGDEF_TYPE_BOOL,
        };

        char *argv[] = {"./mimino", "-h"};
        esma_assert(parse_args(lenof(argv), argv, lenof(ad), &ad));
        esma_assert(!ad[0].bvalue);
    }


    esma_log_test("Handle one long unsupported argument");
    {
        Argdef ad[1];
        memset(ad, 0, sizeof(ad));
        ad[0] = (Argdef) {
            .short_arg = 'v',
            .type = ARGDEF_TYPE_BOOL,
        };

        char *argv[] = {"./mimino", "--help"};
        esma_assert(parse_args(lenof(argv), argv, lenof(ad), &ad));
        esma_assert(!ad[0].bvalue);
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
        esma_assert(parse_args(lenof(argv_l_norm), argv_l_norm, lenof(ad_long),
                               &ad_long));
        esma_assert(ad_long[0].bvalue);

        // Reset
        argdef_undo_parse(ad_long, lenof(ad_long));

        // Using '='
        esma_log_subtest("Using '='");
        char *argv_l_eq[] = {"./mimino", "--verbose=1"};
        esma_assert(parse_args(lenof(argv_l_eq), argv_l_eq, lenof(ad_long),
                               &ad_long));
        esma_assert(ad_long[0].bvalue);
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
        esma_assert(parse_args(lenof(argv_norm), argv_norm, lenof(ad_long),
                               &ad_long));
        esma_assert(!cmpstr(ad_long[0].value, "8080"));

        // Reset
        argdef_undo_parse(ad_long, lenof(ad_long));

        // Using '='
        esma_log_subtest("Using '='");
        char *argv_eq[] = {"./mimino", "--port=8080"};
        esma_assert(parse_args(lenof(argv_eq), argv_eq, lenof(ad_long),
                               &ad_long));
        esma_assert(!cmpstr(ad_long[0].value, "8080"));
    }


    Argdef argdefs[6];
    memset(argdefs, 0, sizeof(argdefs));
    argdefs[0] = (Argdef) {
        .short_arg = 'v',
        .long_arg = "verbose",
        .type = ARGDEF_TYPE_BOOL,
    };
    argdefs[1] = (Argdef) {
        .short_arg = 'u',
        .long_arg = "unsafe",
        .type = ARGDEF_TYPE_BOOL,
    };
    argdefs[2] = (Argdef) {
        .short_arg = 'p',
        .long_arg = "port",
        .type = ARGDEF_TYPE_STRING,
    };
    argdefs[3] = (Argdef) {
        .short_arg = 'i',
        .long_arg = "index",
        .type = ARGDEF_TYPE_STRING,
    };
    argdefs[4] = (Argdef) {
        .short_arg = 'e',
        .long_arg = "error-files",
        .type = ARGDEF_TYPE_BOOL,
    };
    argdefs[5] = (Argdef) {
        .type = ARGDEF_TYPE_RAW,
    };


    esma_log_test("Handle multiple short-form args");
    {
        char *argv[] = {"./mimino", "-v", "-u"};
        esma_assert(parse_args(lenof(argv), argv, lenof(argdefs), argdefs));
        esma_assert(argdefs[0].bvalue);
        esma_assert(argdefs[1].bvalue);
        esma_assert(argdefs[4].bvalue == 0);
        argdef_undo_parse(argdefs, lenof(argdefs));
    }


    esma_log_test("Handle multiple consecutive short-form args");
    {
        char *argv[] = {"./mimino", "-ve"};
        esma_assert(parse_args(lenof(argv), argv, lenof(argdefs), argdefs));
        esma_assert(argdefs[0].bvalue);
        esma_assert(argdefs[1].bvalue == 0);
        esma_assert(argdefs[4].bvalue);
        argdef_undo_parse(argdefs, lenof(argdefs));
    }


    esma_log_test("Handle 2 consecutive short-form args,"
                  "last one with a value");
    {
        char *argv[] = {"./mimino", "-veiindex.asdf"};
        esma_assert(parse_args(lenof(argv), argv, lenof(argdefs), argdefs));
        esma_assert(argdefs[0].bvalue);
        esma_assert(argdefs[1].bvalue == 0);
        esma_assert(argdefs[4].bvalue);
        esma_assert(!cmpstr(argdefs[3].value, "index.asdf"));
        argdef_undo_parse(argdefs, lenof(argdefs));
    }


    esma_log_test("Handle 2+ consecutive short-form args,"
                  "last one with a value");
    {
        char *argv[] = {"./mimino", "-vueiindex.asdf"};
        esma_assert(parse_args(lenof(argv), argv, lenof(argdefs), argdefs));
        esma_assert(argdefs[0].bvalue);
        esma_assert(argdefs[1].bvalue);
        esma_assert(argdefs[4].bvalue);
        esma_assert(!cmpstr(argdefs[3].value, "index.asdf"));
        argdef_undo_parse(argdefs, lenof(argdefs));
    }


    esma_log_test("Handle long, short, and raw args all together");
    {
        esma_log_subtest("When raw arg comes last");
        char *argv[] = {"./mimino", "-vu", "--port", "8090", "-iindex.asdf", "dir/"};
        esma_assert(parse_args(lenof(argv), argv, lenof(argdefs), argdefs));
        esma_assert(argdefs[0].bvalue);
        esma_assert(argdefs[1].bvalue);
        esma_assert(!cmpstr(argdefs[2].value, "8090"));
        esma_assert(!cmpstr(argdefs[3].value, "index.asdf"));
        esma_assert(!cmpstr(argdefs[5].value, "dir/"));

        argdef_undo_parse(argdefs, lenof(argdefs));

        esma_log_subtest("When raw arg is in the middle");
        char *argv_m[] = {"./mimino", "-vu", "--port", "8090", "dir/", "-iindex.asdf"};
        esma_assert(parse_args(lenof(argv_m), argv_m, lenof(argdefs), argdefs));
        esma_assert(argdefs[0].bvalue);
        esma_assert(argdefs[1].bvalue);
        esma_assert(!cmpstr(argdefs[2].value, "8090"));
        esma_assert(!cmpstr(argdefs[3].value, "index.asdf"));
        esma_assert(!cmpstr(argdefs[5].value, "dir/"));

        argdef_undo_parse(argdefs, lenof(argdefs));

        esma_log_subtest("When raw arg comes first");
        char *argv_f[] = {"./mimino", "dir/", "-vu", "--port", "8090", "-iindex.asdf"};
        esma_assert(parse_args(lenof(argv_f), argv_f, lenof(argdefs), argdefs));
        esma_assert(argdefs[0].bvalue);
        esma_assert(argdefs[1].bvalue);
        esma_assert(!cmpstr(argdefs[2].value, "8090"));
        esma_assert(!cmpstr(argdefs[3].value, "index.asdf"));
        esma_assert(!cmpstr(argdefs[5].value, "dir/"));

        argdef_undo_parse(argdefs, lenof(argdefs));
    }

    esma_log_test("Return parse error on incomplete args, but still parse ok");
    {
        char *argv[] = {"./mimino", "--port=", "dir/", "-i"};
        esma_assert(!parse_args(lenof(argv), argv, lenof(argdefs), argdefs));
        esma_assert(!argdefs[0].bvalue);
        esma_assert(!argdefs[1].bvalue);

        // --port=
        esma_assert(argdefs[2].bvalue);
        esma_assert(argdefs[2].err);
        esma_assert(argdefs[2].value == NULL);

        // -i
        esma_assert(argdefs[3].bvalue);
        esma_assert(argdefs[3].err);
        esma_assert(argdefs[3].value == NULL);

        esma_assert(!cmpstr(argdefs[5].value, "dir/"));
        argdef_undo_parse(argdefs, lenof(argdefs));
    }
}

int
main(int argc, char **argv)
{
    esma_run_test(test_decode_url);
    esma_run_test(test_parse_args);
    esma_report();
}
