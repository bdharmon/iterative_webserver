#include "ServerOperations.h"
#include "RouteTable.h"
#include "RouteHandlers.h"
#include "ThreadPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

volatile bool server_running = true;
int epoll_fd = 0; // epoll instance descriptor
struct epoll_event ev;
struct epoll_event events[EPOLL_MAX_EVENTS];
RouteTable *route_table = NULL;

void start_server()
{
    route_table = create_route_table(); // Initialize the route table
    insert_route(route_table, "/", handle_home);
    insert_route(route_table, "/about", handle_about);
    insert_route(route_table, "/contact", handle_contact);
    insert_route(route_table, "/delay", handle_delay);

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

    memset(&ev, 0, sizeof(ev));
    memset(&events, 0, sizeof(events));

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

                // printf("Accepted new connection on client_socket %d\n", client_socket);
            }
            else if (events[i].events & EPOLLIN)
            {
                // Read data from the client socket
                int client_socket = events[i].data.fd;
                add_task_to_pool(pool, client_socket); // Add the client to the task pool for processing
            }
        }
    }

    // Once the server is shutting down, cleanup.
    printf("Server is shutting down...\n");
    free_route_table(route_table);
    thread_pool_shutdown(pool);
    close(server_socket);
    close(epoll_fd);
}

void shutdown_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        printf("\nReceived termination signal. Shutting down gracefully...\n");
        server_running = false; // Set the flag to stop accepting connections
    }
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