/*
  A dumb and simple way to defer freeing memory.
*/

#ifndef _MIMINO_DEFER_H
#define _MIMINO_DEFER_H

#include <stdio.h>

typedef struct {
    void (*func_arr[64])(void*);
    void *arg_arr[64];
    int n_items;
} Defer_Queue;

#define NULL_DEFER_QUEUE ((Defer_Queue) {.n_items = 0});

inline void
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

inline void*
fulfill(Defer_Queue *q, void* return_value)
{
    for (int i = q->n_items - 1; i > 0; i--) {
        (q->func_arr[i])(q->arg_arr[i]);
    }

    return return_value;
}

#endif _MIMINO_DEFER_H
