#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "http.h"
#include "esma.h"

int tests_run = 0;
int tests_ok = 0;
int tests_failed = 0;

int
cmpstr(char *a, char *b)
{
    int ret = strcmp(a, b);
    free(a);
    return ret;
}

void
test_decode_url()
{
    // NOTE: memory leaks ignored for code clarity
    esma_assert(!cmpstr(decode_url("/file%20space"), "/file space"));
    esma_assert(!cmpstr(decode_url("/file%20with%20spaces/"), "/file with spaces/"));
    esma_assert(!cmpstr(decode_url("%E1%83%A5%E1%83%90%E1%83%A0%E1%83%97%E1%83%A3%E1%83%9A%E1%83%98"), "ქართული"));
    esma_assert(!cmpstr(decode_url("http://asdf.com/../?#%"), "http://asdf.com/../?#%"));
    esma_assert(!cmpstr(decode_url("%%/%65%6d%41%63%24/kool"), "%/emAc$/kool"));
    esma_assert(!cmpstr(decode_url("%%/%65%6%41%63%24/kool"), "%/e%6Ac$/kool"));
    esma_assert(!cmpstr(decode_url("%CE%A3%CE%BA%CE%AD%CF%86%CF%84%CE%BF%CE%BC%CE%B1%CE%B9%2C%20%CE%AC%CF%81%CE%B1%20%CF%85%CF%80%CE%AC%CF%81%CF%87%CF%89"), "Σκέφτομαι, άρα υπάρχω"));
    esma_assert(!cmpstr(decode_url("%E6%88%91%E6%80%9D%E6%95%85%E6%88%91%E5%9C%A8"), "我思故我在"));
}

int
main(int argc, char **argv)
{
    esma_run_test(test_decode_url);
    esma_report();
}
