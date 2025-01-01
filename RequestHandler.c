#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include "RequestHandler.h"
#include "RouteHandlers.h"
#include "RouteTable.h"

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

    // Find the corresponding handler for the path
    RouteEntry *route = search_route_table(route_table, path);
    if (route)
    {
        route->handler(client_socket);
    }
    else
    {
        handle_not_found(client_socket);
    }

    // Close the connection
    close(client_socket);
}