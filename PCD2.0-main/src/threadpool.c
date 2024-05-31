#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

static void *thread_do_work(void *pool);

threadpool_t *threadpool_create(int thread_count, int task_queue_size) {
    threadpool_t *pool;
    int i;

    if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
        return NULL;
    }

    pool->thread_count = thread_count;
    pool->task_queue_size = task_queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->tasks = (void (**)(void *))malloc(sizeof(void (*)(void *)) * task_queue_size);
    pool->task_args = (void **)malloc(sizeof(void *) * task_queue_size);

    if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
        (pthread_cond_init(&(pool->notify), NULL) != 0) ||
        (pool->threads == NULL) ||
        (pool->tasks == NULL) ||
        (pool->task_args == NULL)) {
        return NULL;
    }

    for (i = 0; i < thread_count; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, thread_do_work, (void*)pool) != 0) {
            threadpool_destroy(pool, 0);
            return NULL;
        }
        pool->started++;
    }

    return pool;
}

int threadpool_add(threadpool_t *pool, void (*routine)(void *), void *arg) {
    int next;

    if (pool == NULL || routine == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    next = (pool->tail + 1) % pool->task_queue_size;

    if (pool->count == pool->task_queue_size) {
        return -1;
    }

    pool->tasks[pool->tail] = routine;
    pool->task_args[pool->tail] = arg;
    pool->tail = next;
    pool->count += 1;

    if (pthread_cond_signal(&(pool->notify)) != 0) {
        return -1;
    }

    if (pthread_mutex_unlock(&pool->lock) != 0) {
        return -1;
    }

    return 0;
}

int threadpool_destroy(threadpool_t *pool, int flags) {
    if (pool == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    pool->shutdown = 1;

    if ((pthread_cond_broadcast(&(pool->notify)) != 0) ||
        (pthread_mutex_unlock(&(pool->lock)) != 0)) {
        return -1;
    }

    for (int i = 0; i < pool->thread_count; i++) {
        if (pthread_join(pool->threads[i], NULL) != 0) {
            return -1;
        }
    }

    if (pthread_mutex_destroy(&(pool->lock)) != 0 ||
        pthread_cond_destroy(&(pool->notify)) != 0) {
        return -1;
    }

    free(pool->threads);
    free(pool->tasks);
    free(pool->task_args);
    free(pool);

    return 0;
}

static void *thread_do_work(void *pool) {
    threadpool_t *threadpool = (threadpool_t *)pool;
    void (*task)(void *);
    void *task_arg;

    while (1) {
        pthread_mutex_lock(&(threadpool->lock));

        while (threadpool->count == 0 && !threadpool->shutdown) {
            pthread_cond_wait(&(threadpool->notify), &(threadpool->lock));
        }

        if (threadpool->shutdown) {
            break;
        }

        task = threadpool->tasks[threadpool->head];
        task_arg = threadpool->task_args[threadpool->head];
        threadpool->head = (threadpool->head + 1) % threadpool->task_queue_size;
        threadpool->count -= 1;

        pthread_mutex_unlock(&(threadpool->lock));

        (*(task))(task_arg);
    }

    pthread_mutex_unlock(&(threadpool->lock));
    pthread_exit(NULL);
    return NULL;
}
