#include "esma.h"

int tests_run = 0;
int tests_ok = 0;
int tests_failed = 0;
int log_ind = 0;

void test_decode_url();
void test_buf_encode_url();
void test_parse_args();
void test_http_parsers();

int
main(void)
{
    esma_run_test(test_decode_url);
    esma_run_test(test_buf_encode_url);
    esma_run_test(test_parse_args);
    esma_run_test(test_http_parsers);
    esma_report();
}
