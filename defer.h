/*
  A dumb and simple way to defer freeing memory.
*/

#ifndef _MIMINO_DEFER_H
#define _MIMINO_DEFER_H

typedef struct {
    void (*func_arr[64])(void*);
    void *arg_arr[64];
    int n_items;
} Defer_Queue;

#define NULL_DEFER_QUEUE ((Defer_Queue) {.n_items = 0});

void defer(Defer_Queue *q, void (func)(void*), void *arg);
void* fulfill(Defer_Queue *q, void* return_value);

#endif // _MIMINO_DEFER_H
