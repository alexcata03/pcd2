#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

// Structura pentru threadpool
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    void (**tasks)(void *);
    void **task_args;
    int thread_count;
    int task_queue_size;
    int head;
    int tail;
    int count;
    int shutdown;
    int started;
} threadpool_t;

threadpool_t *threadpool_create(int thread_count, int task_queue_size);
int threadpool_add(threadpool_t *pool, void (*routine)(void *), void *arg);
int threadpool_destroy(threadpool_t *pool, int flags);

#endif // THREADPOOL_H
