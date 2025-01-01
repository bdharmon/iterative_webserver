#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>

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