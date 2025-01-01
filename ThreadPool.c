#include "ThreadPool.h"
#include "RequestHandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

void *worker_thread(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (true)
    {
        pthread_mutex_lock(&pool->queue_mutex);

        // Wait for a task or shutdown signal.
        while (pool->task_count == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }

        // If shutdown is requested, exit.
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        // Retrieve the task from the queue.
        task_t task = pool->task_queue[pool->task_front];
        pool->task_front = (pool->task_front + 1) % pool->task_queue_size;
        pool->task_count--;

        pthread_mutex_unlock(&pool->queue_mutex);

        // Process the task (handle the client)
        handle_client_request(task.client_socket);
    }
}

void add_task_to_pool(thread_pool_t *pool, int client_socket)
{
    pthread_mutex_lock(&pool->queue_mutex);

    // Check if queue is full.
    if (pool->task_count == pool->task_queue_size)
    {
        printf("Task queue is full, dropping client connection.\n");
        close(client_socket); // Close the connection if the queue is full.
    }
    else
    {
        // Add the new task.
        task_t task;
        task.client_socket = client_socket;
        pool->task_queue[pool->task_rear] = task;
        pool->task_rear = (pool->task_rear + 1) % pool->task_queue_size;
        pool->task_count++;

        // Signal one worker thread to start processing the task.
        pthread_cond_signal(&pool->queue_cond);
    }

    pthread_mutex_unlock(&pool->queue_mutex);
}

void thread_pool_shutdown(thread_pool_t *pool)
{
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->queue_cond); // Wake up all threads.
    pthread_mutex_unlock(&pool->queue_mutex);

    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_join(pool->threads[i], NULL);
    }

    // Clean up resources
    free(pool->task_queue);
    free(pool->threads);
    free(pool);
}

thread_pool_t *thread_pool_init(int pool_size, int queue_size)
{
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    if (!pool)
    {
        printf("Failed to allocate memory for thread pool.");
        return NULL;
    }

    pool->threads = malloc(pool_size * sizeof(pthread_t));
    if (!pool->threads)
    {
        printf("Failed to allocate memory for threads.");
        free(pool);
        return NULL;
    }

    pool->task_queue = malloc(queue_size * sizeof(task_t));
    if (!pool->task_queue)
    {
        printf("Failed to allocate memory for task queue.");
        free(pool->threads);
        free(pool);
        return NULL;
    }

    pool->task_queue_size = queue_size;
    pool->task_count = 0;
    pool->task_front = 0;
    pool->task_rear = 0;
    pool->shutdown = false;

    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_cond, NULL);

    for (int i = 0; i < pool_size; i++)
    {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, (void *)pool) != 0)
        {
            printf("Failed to create thread.");
            free(pool->task_queue);
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}