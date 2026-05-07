/*
 * 5G UE Simulation Application
 * Thread Pool Implementation
 */

#include "thread_pool.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Worker thread function */
static void* thread_pool_worker(void* arg) {
    thread_pool_t* pool = (thread_pool_t*)arg;
    
    while (true) {
        thread_task_t* task = NULL;
        
        /* Lock the queue mutex */
#ifdef _WIN32
        WaitForSingleObject(pool->queue_mutex, INFINITE);
#else
        pthread_mutex_lock(&pool->queue_mutex);
#endif
        
        /* Wait for tasks or shutdown */
        while (pool->task_queue_head == NULL && !atomic_load(&pool->shutdown)) {
#ifdef _WIN32
            ReleaseMutex(pool->queue_mutex);
            WaitForSingleObject(pool->queue_cond, INFINITE);
            WaitForSingleObject(pool->queue_mutex, INFINITE);
#else
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
#endif
        }
        
        /* Check for shutdown */
        if (atomic_load(&pool->shutdown)) {
#ifdef _WIN32
            ReleaseMutex(pool->queue_mutex);
#else
            pthread_mutex_unlock(&pool->queue_mutex);
#endif
            break;
        }
        
        /* Get task from queue */
        task = pool->task_queue_head;
        if (task != NULL) {
            pool->task_queue_head = task->next;
            if (pool->task_queue_head == NULL) {
                pool->task_queue_tail = NULL;
            }
            pool->task_count--;
            pool->active_tasks++;
        }
        
#ifdef _WIN32
        ReleaseMutex(pool->queue_mutex);
#else
        pthread_mutex_unlock(&pool->queue_mutex);
#endif
        
        /* Execute task */
        if (task != NULL) {
            task->function(task->argument);
            uesim_free(task);
            
            /* Decrement active tasks and signal if waiting */
#ifdef _WIN32
            WaitForSingleObject(pool->queue_mutex, INFINITE);
            pool->active_tasks--;
            if (pool->active_tasks == 0 && pool->task_queue_head == NULL) {
                SetEvent(pool->finished_cond);
            }
            ReleaseMutex(pool->queue_mutex);
#else
            pthread_mutex_lock(&pool->queue_mutex);
            pool->active_tasks--;
            if (pool->active_tasks == 0 && pool->task_queue_head == NULL) {
                pthread_cond_broadcast(&pool->finished_cond);
            }
            pthread_mutex_unlock(&pool->queue_mutex);
#endif
        }
    }
    
    return NULL;
}

uesim_error_t thread_pool_create(uint32_t thread_count, thread_pool_t** pool) {
    if (pool == NULL || thread_count == 0) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Allocate pool structure */
    thread_pool_t* new_pool = (thread_pool_t*)uesim_calloc(1, sizeof(thread_pool_t));
    if (new_pool == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    /* Initialize pool fields */
    new_pool->thread_count = thread_count;
    new_pool->task_queue_head = NULL;
    new_pool->task_queue_tail = NULL;
    new_pool->task_count = 0;
    new_pool->active_tasks = 0;
    atomic_store(&new_pool->shutdown, 0);
    
    /* Allocate thread array */
    new_pool->threads = (pthread_t*)uesim_calloc(thread_count, sizeof(pthread_t));
    if (new_pool->threads == NULL) {
        uesim_free(new_pool);
        return UESIM_ERROR_MEMORY;
    }
    
    /* Initialize mutex and condition variables */
#ifdef _WIN32
    new_pool->queue_mutex = CreateMutex(NULL, FALSE, NULL);
    new_pool->queue_cond = CreateEvent(NULL, FALSE, FALSE, NULL);
    new_pool->finished_cond = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    if (new_pool->queue_mutex == NULL || new_pool->queue_cond == NULL || new_pool->finished_cond == NULL) {
        if (new_pool->queue_mutex) CloseHandle(new_pool->queue_mutex);
        if (new_pool->queue_cond) CloseHandle(new_pool->queue_cond);
        if (new_pool->finished_cond) CloseHandle(new_pool->finished_cond);
        uesim_free(new_pool->threads);
        uesim_free(new_pool);
        return UESIM_ERROR_THREAD;
    }
#else
    if (pthread_mutex_init(&new_pool->queue_mutex, NULL) != 0 ||
        pthread_cond_init(&new_pool->queue_cond, NULL) != 0 ||
        pthread_cond_init(&new_pool->finished_cond, NULL) != 0) {
        pthread_mutex_destroy(&new_pool->queue_mutex);
        pthread_cond_destroy(&new_pool->queue_cond);
        pthread_cond_destroy(&new_pool->finished_cond);
        uesim_free(new_pool->threads);
        uesim_free(new_pool);
        return UESIM_ERROR_THREAD;
    }
#endif
    
    /* Create worker threads */
    for (uint32_t i = 0; i < thread_count; i++) {
#ifdef _WIN32
        new_pool->threads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_pool_worker, new_pool, 0, NULL);
        if (new_pool->threads[i] == NULL) {
#else
        if (pthread_create(&new_pool->threads[i], NULL, thread_pool_worker, new_pool) != 0) {
#endif
            /* Failed to create thread - cleanup and return error */
            atomic_store(&new_pool->shutdown, 1);
            
            /* Signal any waiting threads */
#ifdef _WIN32
            SetEvent(new_pool->queue_cond);
#else
            pthread_cond_broadcast(&new_pool->queue_cond);
#endif
            
            /* Wait for created threads to finish */
            for (uint32_t j = 0; j < i; j++) {
#ifdef _WIN32
                WaitForSingleObject(new_pool->threads[j], INFINITE);
                CloseHandle(new_pool->threads[j]);
#else
                pthread_join(new_pool->threads[j], NULL);
#endif
            }
            
#ifdef _WIN32
            CloseHandle(new_pool->queue_mutex);
            CloseHandle(new_pool->queue_cond);
            CloseHandle(new_pool->finished_cond);
#else
            pthread_mutex_destroy(&new_pool->queue_mutex);
            pthread_cond_destroy(&new_pool->queue_cond);
            pthread_cond_destroy(&new_pool->finished_cond);
#endif
            uesim_free(new_pool->threads);
            uesim_free(new_pool);
            return UESIM_ERROR_THREAD;
        }
    }
    
    *pool = new_pool;
    printf("Thread pool created with %u threads\n", thread_count);
    return UESIM_SUCCESS;
}

uesim_error_t thread_pool_destroy(thread_pool_t* pool) {
    if (pool == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Signal shutdown */
    atomic_store(&pool->shutdown, 1);
    
    /* Wake up all worker threads */
#ifdef _WIN32
    SetEvent(pool->queue_cond);
#else
    pthread_mutex_lock(&pool->queue_mutex);
    pthread_cond_broadcast(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
#endif
    
    /* Join all worker threads */
    for (uint32_t i = 0; i < pool->thread_count; i++) {
#ifdef _WIN32
        WaitForSingleObject(pool->threads[i], INFINITE);
        CloseHandle(pool->threads[i]);
#else
        pthread_join(pool->threads[i], NULL);
#endif
    }
    
    /* Free remaining tasks in queue */
    thread_task_t* task = pool->task_queue_head;
    while (task != NULL) {
        thread_task_t* next = task->next;
        uesim_free(task);
        task = next;
    }
    
    /* Destroy mutex and condition variables */
#ifdef _WIN32
    CloseHandle(pool->queue_mutex);
    CloseHandle(pool->queue_cond);
    CloseHandle(pool->finished_cond);
#else
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    pthread_cond_destroy(&pool->finished_cond);
#endif
    
    uesim_free(pool->threads);
    uesim_free(pool);
    
    printf("Thread pool destroyed\n");
    return UESIM_SUCCESS;
}

uesim_error_t thread_pool_submit(thread_pool_t* pool, thread_task_fn function, void* argument) {
    if (pool == NULL || function == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
    /* Create task */
    thread_task_t* task = (thread_task_t*)uesim_malloc(sizeof(thread_task_t));
    if (task == NULL) {
        return UESIM_ERROR_MEMORY;
    }
    
    task->function = function;
    task->argument = argument;
    task->next = NULL;
    
    /* Add task to queue */
#ifdef _WIN32
    WaitForSingleObject(pool->queue_mutex, INFINITE);
#else
    pthread_mutex_lock(&pool->queue_mutex);
#endif
    
    if (pool->task_queue_tail == NULL) {
        pool->task_queue_head = task;
        pool->task_queue_tail = task;
    } else {
        pool->task_queue_tail->next = task;
        pool->task_queue_tail = task;
    }
    pool->task_count++;
    
    /* Signal worker thread */
#ifdef _WIN32
    SetEvent(pool->queue_cond);
    ReleaseMutex(pool->queue_mutex);
#else
    pthread_cond_signal(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
#endif
    
    return UESIM_SUCCESS;
}

uesim_error_t thread_pool_wait(thread_pool_t* pool) {
    if (pool == NULL) {
        return UESIM_ERROR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    WaitForSingleObject(pool->queue_mutex, INFINITE);
    while (pool->active_tasks > 0 || pool->task_queue_head != NULL) {
        ReleaseMutex(pool->queue_mutex);
        WaitForSingleObject(pool->finished_cond, INFINITE);
        WaitForSingleObject(pool->queue_mutex, INFINITE);
    }
    ReleaseMutex(pool->queue_mutex);
#else
    pthread_mutex_lock(&pool->queue_mutex);
    while (pool->active_tasks > 0 || pool->task_queue_head != NULL) {
        pthread_cond_wait(&pool->finished_cond, &pool->queue_mutex);
    }
    pthread_mutex_unlock(&pool->queue_mutex);
#endif
    
    return UESIM_SUCCESS;
}

uint32_t thread_pool_get_thread_count(thread_pool_t* pool) {
    return pool ? pool->thread_count : 0;
}

uint32_t thread_pool_get_active_tasks(thread_pool_t* pool) {
    return pool ? pool->active_tasks : 0;
}

uint32_t thread_pool_get_pending_tasks(thread_pool_t* pool) {
    return pool ? pool->task_count : 0;
}