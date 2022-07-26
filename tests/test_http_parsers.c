#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "esma.h"
#include "http.h"

void
test_http_parsers()
{
    esma_log_test("ll_power()");
    {
        esma_assert(ll_power(12, 3) == 1728);
        esma_assert(ll_power(13, 7) == 62748517);
        esma_assert(ll_power(10, 11) == 100000000000);
        esma_assert(ll_power(2, 6) == 64);
        esma_assert(ll_power(1, 576) == 1);
        esma_assert(ll_power(0, 36) == 0);
    }

    esma_log_test("consume_next_num()");

    esma_log_subtest("Parses one digit");
    {
        char *str = "6";
        char *p = str;
        char *end = str + strlen(str) - 1;
        esma_assert(consume_next_num(&p, end) == 6);
        esma_assert(p == str + 1);
    }

    esma_log_subtest("Parses \"6a\" into 6");
    {
        char *str = "6a";
        char *p = str;
        char *end = str + 1;
        esma_assert(consume_next_num(&p, end) == 6);
        esma_assert(p == str + 1);
    }

    esma_log_subtest("Parses \"123b678\" into 123");
    {
        char *str = "123b678";
        char *p = str;
        char *end = str + strlen(str) - 1;
        esma_assert(consume_next_num(&p, end) == 123);
        esma_assert(p == str + 3);
    }

    esma_log_subtest("Parses \"123456789\" into 123456789");
    {
        char *str = "123456789";
        char *p = str;
        char *end = str + strlen(str) - 1;
        esma_assert(consume_next_num(&p, end) == 123456789);
        esma_assert(p == str + 9);
    }
}
