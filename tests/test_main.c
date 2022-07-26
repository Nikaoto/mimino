#include <stdio.h>
#include <string.h>
#include "http.h"
#include "esma.h"

int tests_run = 0;
int tests_ok = 0;
int tests_failed = 0;

void
test_decode_url()
{
    // NOTE: memory leaks ignored for code clarity
    esma_assert(!strcmp(decode_url("/file%20space"), "/file space"));
    esma_assert(!strcmp(decode_url("/file%20with%20spaces/"), "/file with spaces/"));
    esma_assert(!strcmp(decode_url("%E1%83%A5%E1%83%90%E1%83%A0%E1%83%97%E1%83%A3%E1%83%9A%E1%83%98"), "ქართული"));
    esma_assert(!strcmp(decode_url("http:asdf.com/../?#%"), "http:asdf.com/../?#%"));
    esma_assert(!strcmp(decode_url("%%/%65%6d%41%63%24/kool"), "%/emAc$/kool"));
}

int
main(int argc, char **argv)
{
    esma_run_test(test_decode_url);
    esma_report();
}
