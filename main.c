#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h> // For fcntl
#include <errno.h>
#include <sys/epoll.h>

#define PORT 8080  // Port to listen on
#define BACKLOG 10 // Max number of pending connections
#define BUFFER_SIZE 1024
#define THREAD_POOL_SIZE 2  // Number of threads in the pool
#define TASK_QUEUE_SIZE 10  // Maximum number of tasks in the queue
#define EPOLL_MAX_EVENTS 10 // Max events to handle at once

int epoll_fd; // epoll instance descriptor
struct epoll_event ev;
struct epoll_event events[EPOLL_MAX_EVENTS];

volatile bool server_running = true; // Flag to control server operation

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

void handle_home(int client_socket);
void handle_about(int client_socket);
void handle_contact(int client_socket);
void handle_delay(int client_socket);
void handle_not_found(int client_socket);
void handle_client_request(int client_socket);
void shutdown_handler(int signum);
void set_socket_nonblocking(int socket);
void *worker_thread(void *arg);
void add_task_to_pool(thread_pool_t *pool, int client_socket);
void thread_pool_shutdown(thread_pool_t *pool);
thread_pool_t *thread_pool_init(int pool_size, int queue_size);

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

void set_socket_nonblocking(int socket)
{
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
    {
        printf("fcntl(F_GETFL) failed");
        exit(EXIT_FAILURE);
    }

    if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        printf("fcntl(F_SETFL) failed");
        exit(EXIT_FAILURE);
    }
}

void shutdown_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        printf("\nReceived termination signal. Shutting down gracefully...\n");
        server_running = false; // Set the flag to stop accepting connections
    }
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

// Function to handle HTTP request
void handle_client_request(int client_socket)
{
    char buffer[BUFFER_SIZE];

    // Read the HTTP request from the client
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0)
    {
        printf("Failed to receive request.\n");
        close(client_socket);
    }

    // Null-terminate the string
    buffer[bytes_received] = '\0';

    // Parse the HTTP request and extract the path
    char method[16];
    char path[256];
    char version[16];

    sscanf(buffer, "%s %s %s", method, path, version);

    printf("\n\t----- Received Request -----\n");
    printf("method: %s\n", method);
    printf("path: %s\n", path);
    printf("version: %s\n", version);
    printf("\t----- Request End -----\n\n");

    // Handle different endpoints
    if (strcmp(path, "/") == 0)
    {
        handle_home(client_socket);
    }
    else if (strcmp(path, "/about") == 0)
    {
        handle_about(client_socket);
    }
    else if (strcmp(path, "/contact") == 0)
    {
        handle_contact(client_socket);
    }
    else if (strcmp(path, "/delay") == 0)
    {
        handle_delay(client_socket);
    }
    else
    {
        handle_not_found(client_socket);
    }

    // Close the connection
    close(client_socket);
}

void handle_home(int client_socket)
{
    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=UTF-8\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "<html><body><h1>Welcome to the Home Page!</h1></body></html>";
    send(client_socket, response, strlen(response), 0);
}

void handle_about(int client_socket)
{
    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=UTF-8\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "<html><body><h1>About Us</h1><p>This is a simple C-based web server.</p></body></html>";
    send(client_socket, response, strlen(response), 0);
}

void handle_contact(int client_socket)
{
    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=UTF-8\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "<html><body><h1>Contact Us</h1><p>Email: contact@example.com</p></body></html>";
    send(client_socket, response, strlen(response), 0);
}

void handle_delay(int client_socket)
{
    sleep(4);
    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=UTF-8\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "<html><body><h1>Delay</h1></body></html>";
    send(client_socket, response, strlen(response), 0);
}

void handle_not_found(int client_socket)
{
    const char *response = "HTTP/1.1 404 Not Found\r\n"
                           "Content-Type: text/html; charset=UTF-8\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "<html><body><h1>404 Not Found</h1><p>The requested page does not exist.</p></body></html>";
    send(client_socket, response, strlen(response), 0);
}

void start_server()
{
    int server_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    thread_pool_t *pool = thread_pool_init(THREAD_POOL_SIZE, TASK_QUEUE_SIZE);

    // Create the socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("server_socket file descriptor: %d\n", server_socket);
    }

    // Set up the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    server_addr.sin_port = htons(PORT);       // Port to bind to

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        printf("setsockopt failed.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Bind failed.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, BACKLOG) < 0)
    {
        printf("Listen failed.");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Register signal handlers for SIGINT (Ctrl+C) and SIGTERM
    signal(SIGINT, shutdown_handler);
    signal(SIGTERM, shutdown_handler);

    // Set server socket to non-blocking mode
    set_socket_nonblocking(server_socket);

    // Create an epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        printf("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Add the server socket to the epoll instance
    ev.events = EPOLLIN; // Monitor for input events (new connections)
    ev.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &ev) == -1)
    {
        printf("epoll_ctl: server_socket");
        exit(EXIT_FAILURE);
    }

    while (server_running)
    {
        // Wait for events on the epoll instance
        int nfds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, -1); // Blocking call
        if (nfds == -1)
        {
            printf("epoll_wait");
            break;
        }

        // Process the events
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // New connection on the server socket
                int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
                if (client_socket == -1)
                {
                    printf("accept failed");
                    continue;
                }

                // Set client socket to non-blocking mode
                set_socket_nonblocking(client_socket);

                // Add the new client socket to the epoll instance
                ev.events = EPOLLIN | EPOLLET; // Edge-triggered read event
                ev.data.fd = client_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &ev) == -1)
                {
                    printf("epoll_ctl: client_socket");
                    close(client_socket);
                    continue;
                }

                printf("Accepted new connection on client_socket %d\n", client_socket);
            }
            else if (events[i].events & EPOLLIN)
            {
                // Read data from the client socket
                int client_socket = events[i].data.fd;
                add_task_to_pool(pool, client_socket); // Add the client to the task pool for processing
            }
        }
    }

    // Once the server is shutting down, close the server socket.
    printf("Server is shutting down...\n");
    thread_pool_shutdown(pool);
    close(server_socket);
    close(epoll_fd);
}

int main()
{
    start_server();
    return 0;
}
