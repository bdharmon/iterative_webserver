#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "RouteTable.h"

// A simple hashing function based on DJB2 hash algorithm.
unsigned int hash(char *key)
{
    unsigned int hash = 5381;
    int c;

    while ((c = *key++))
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash % TABLE_SIZE;
}

// Create a new route table.
RouteTable *create_route_table()
{
    RouteTable *ht = (RouteTable *)malloc(sizeof(RouteTable));
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        ht->table[i] = NULL; // Initialize all buckets as NULL
    }
    return ht;
}

// Insert a route into the route table
void insert_route(RouteTable *ht, char *key, HandlerFunction handler_function)
{
    unsigned int index = hash(key);
    RouteEntry *route = (RouteEntry *)malloc(sizeof(RouteEntry));
    route->key = strdup(key);
    route->handler = handler_function;
    route->next = NULL;

    // Insert at the beginning of the linked list at the computed index
    if (ht->table[index] == NULL)
    {
        ht->table[index] = route;
    }
    else
    {
        route->next = ht->table[index];
        ht->table[index] = route;
    }
}

// Search for a value by key in the hash table
RouteEntry *search_route_table(RouteTable *ht, char *key)
{
    unsigned int index = hash(key);
    RouteEntry *current = ht->table[index];

    while (current != NULL)
    {
        if (strcmp(current->key, key) == 0)
        {
            return current; // Key found, return its value (i.e. - RouteEntry struct)
        }
        current = current->next; // Move to the next node in the chain
    }
    return NULL; // Key not found
}

// Delete a key-value pair from the hash table
void delete_route(RouteTable *ht, char *key)
{
    unsigned int index = hash(key);
    RouteEntry *current = ht->table[index];
    RouteEntry *prev = NULL;

    while (current != NULL)
    {
        if (strcmp(current->key, key) == 0)
        {
            if (prev == NULL)
            {
                // If the node to be deleted is the first node in the chain
                ht->table[index] = current->next;
            }
            else
            {
                prev->next = current->next;
            }

            free(current->key); // Free the duplicated key
            free(current);      // Free the node
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Print the hash table.
void print_route_table(RouteTable *ht)
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        if (ht->table[i] != NULL)
        {
            RouteEntry *current = ht->table[i];
            printf("Index %d: ", i);
            while (current != NULL)
            {
                printf("(%s) -> ", current->key);
                current = current->next;
            }
            printf("NULL\n");
        }
    }
}

// Free the entire route table
void free_route_table(RouteTable *rt)
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        RouteEntry *current = rt->table[i];
        while (current != NULL)
        {
            RouteEntry *temp = current;
            current = current->next;
            
            free(temp->key);
            free(temp);
        }
    }

    free(rt);
}