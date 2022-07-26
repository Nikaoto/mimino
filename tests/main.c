#include <stdio.h>
#include <string.h>
#include "esma.h"

int tests_run = 0;
int tests_ok = 0;
int tests_failed = 0;

int
main(int argc, char **argv)
{
    esma_assert(7 == 7);
    esma_assert(strcmp("asdf", "asdfg") < 0);
    esma_report();
}
