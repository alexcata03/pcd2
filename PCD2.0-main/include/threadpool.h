#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

/* Threadpool structure */
typedef struct {
    pthread_mutex_t lock; /* Mutex thread to lock the data */
    pthread_cond_t notify; /* Conditional thread */
    pthread_t *threads;
    void (**tasks)(void *);
    void **task_args;
    int thread_count; /* Number of threads*/
    int task_queue_size; /* The size of the queue */
    int head; /* Index for the head of the thread pool */
    int tail; /* Index for the tail of the thread pool */
    int count; /* Number of elements */
    int shutdown;
    int started; 
} threadpool_t;

threadpool_t *threadpool_create(int thread_count, int task_queue_size); /* Function that creates a thread pool */
int threadpool_add(threadpool_t *pool, void (*routine)(void *), void *arg); /* Function that adds a thread to the queue*/
int threadpool_destroy(threadpool_t *pool, int flags); /* Function that destroys the threadpool */

#endif // THREADPOOL_H
