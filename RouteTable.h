#include "RequestHandler.h"

#define TABLE_SIZE 4

// Function that handles a route.
typedef void (*HandlerFunction)(int client_socket);

// The structure for the route entry node in the route table.
typedef struct RouteEntry
{
    char *key; // URI path
    HandlerFunction handler;
    struct RouteEntry *next;
} RouteEntry;

// The hash table structure for routes.
typedef struct RouteTable
{
    RouteEntry *table[TABLE_SIZE];
} RouteTable;

// Hash table for storing the routes
extern RouteTable *route_table;

unsigned int hash(char *key);

// Create a new route table.
RouteTable *create_route_table();

// Insert a route into the route table
void insert_route(RouteTable *ht, char *key, HandlerFunction handler_function);

// Search for a value by key in the hash table
RouteEntry *search_route_table(RouteTable *ht, char *key);

// Delete a key-value pair from the hash table
void delete_route(RouteTable *ht, char *key);

// Print the hash table.
void print_route_table(RouteTable *ht);

// Free the entire route table
void free_route_table(RouteTable *rt);