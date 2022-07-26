/*
  A dumb and simple way to defer freeing memory.
*/

#include <stdio.h>
#include "defer.h"

void
defer(Defer_Queue *q, void (func)(void*), void *arg)
{
    if (q->n_items >= (int) sizeof(q->arg_arr)) {
        fprintf(stderr, "FATAL ERR: defer queue overflow %d\n", q->n_items);
        return;
    }

    q->func_arr[q->n_items] = func;
    q->arg_arr[q->n_items] = arg;
    q->n_items++;
}

void*
fulfill(Defer_Queue *q, void* return_value)
{
    for (int i = q->n_items - 1; i >= 0; i--) {
        (q->func_arr[i])(q->arg_arr[i]);
    }

    return return_value;
}

