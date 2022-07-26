#include "esma.h"

int tests_run = 0;
int tests_ok = 0;
int tests_failed = 0;
int log_ind = 0;

void test_decode_url();
void test_parse_args();

int
main(void)
{
    esma_run_test(test_decode_url);
    esma_run_test(test_parse_args);
    esma_report();
}
