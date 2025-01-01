#include <pthread.h>
#include <stdbool.h>

#define THREAD_POOL_SIZE 2  // Number of threads in the pool
#define TASK_QUEUE_SIZE 10  // Maximum number of tasks in the queue

typedef struct task_t
{
    int client_socket;
} task_t;

typedef struct thread_pool_t
{
    pthread_t *threads;          // Array of worker threads.
    task_t *task_queue;          // Task queue.
    int task_queue_size;         // Maximum size of the task queue.
    int task_count;              // Current number of tasks in the queue.
    int task_front;              // Front of the task queue.
    int task_rear;               // Rear of the task queue.
    pthread_mutex_t queue_mutex; // Mutex for the task queue.
    pthread_cond_t queue_cond;   // Condition variable for task availability.
    bool shutdown;               // Flag to indicate shutdown of thread pool.
} thread_pool_t;

void *worker_thread(void *arg);
void add_task_to_pool(thread_pool_t *pool, int client_socket);
void thread_pool_shutdown(thread_pool_t *pool);
thread_pool_t *thread_pool_init(int pool_size, int queue_size);