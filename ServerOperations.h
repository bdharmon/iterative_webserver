#include <stdbool.h>
#include <sys/epoll.h>

#define PORT 8080  // Port to listen on
#define BACKLOG 10 // Max number of pending connections
#define EPOLL_MAX_EVENTS 10 // Max events to handle at once

extern volatile bool server_running; // Flag to control server operation

extern int epoll_fd; // epoll instance descriptor
extern struct epoll_event ev; // ev
extern struct epoll_event events[EPOLL_MAX_EVENTS]; // events[EPOLL_MAX_EVENTS]

void start_server();
void shutdown_handler(int signum);
void set_socket_nonblocking(int socket);