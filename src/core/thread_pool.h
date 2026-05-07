/*
 * 5G UE Simulation Application
 * Thread Pool Header
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "../uesim.h"

/* Task function signature */
typedef void* (*thread_task_fn)(void* arg);

/* Thread pool task structure */
typedef struct thread_task {
    thread_task_fn function;
    void* argument;
    struct thread_task* next;
} thread_task_t;

/* Thread pool structure */
struct thread_pool {
    pthread_t* threads;
    uint32_t thread_count;
    thread_task_t* task_queue_head;
    thread_task_t* task_queue_tail;
    uint32_t task_count;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    pthread_cond_t finished_cond;
    uint32_t active_tasks;
#ifdef _WIN32
    volatile LONG shutdown;
#else
    atomic_bool shutdown;
#endif
};

/* Thread pool API */
uesim_error_t thread_pool_create(uint32_t thread_count, thread_pool_t** pool);
uesim_error_t thread_pool_destroy(thread_pool_t* pool);
uesim_error_t thread_pool_submit(thread_pool_t* pool, thread_task_fn function, void* argument);
uesim_error_t thread_pool_wait(thread_pool_t* pool);
uint32_t thread_pool_get_thread_count(thread_pool_t* pool);
uint32_t thread_pool_get_active_tasks(thread_pool_t* pool);
uint32_t thread_pool_get_pending_tasks(thread_pool_t* pool);

#endif /* THREAD_POOL_H */