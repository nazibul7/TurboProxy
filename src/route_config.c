#include "route_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LEN 256

int load_routes(const char *filename, Route *routes, int max_routes)
{
    if (!filename || !routes || max_routes <= 0)
    {
        perror("Invalid parameter to load the routes");
        return -1;
    }
    // Open the config file for reading
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Could not open routes file");
        return -1; // Error: file couldn't be opened
    }

    int count = 0;
    char line[MAX_PREFIX_LEN];

    //  Read file line by line until end of file or max_routes reached
    while (fgets(line, sizeof(line), file) && count < max_routes)
    {
        // Remove newline character because fgets keeps newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
        }

        // Skip empty lines
        if (strlen(line) == 0)
            continue;

        // Skip comment lines
        if (line[0] == '#')
            continue;

        // Temporary buffers for parse fields prefix, host, port
        char prefix[32];
        char host[64];
        int port;

        /*
            The sscanf() function in C is a standard library function used for reading formatted input from a string.
            It functions similarly to scanf(), but instead of reading from standard input(like the keyboard),
            it reads from a specified character array(string)
        */

        // Expect format: "/api example.com 8080"
        int parts = sscanf(line, "%s %s %d", prefix, host, &port);
        if (parts != 3)
        {
            // If the line doesn't match format, warn and skip
            printf("Invalid route line: %s\n", line);
            continue;
        }

        // Initialize the route struct.
        memset(&routes[count], 0, sizeof(Route));

        strncpy(routes[count].prefix, prefix, sizeof(routes[count].prefix) - 1);
        routes[count].prefix[sizeof(routes[count].prefix) - 1] = '\0';

        strncpy(routes[count].host, host, sizeof(routes[count].host) - 1);
        routes[count].host[sizeof(routes[count].host) - 1] = '\0';

        routes[count].port = port;

        count++;
    }

    fclose(file);
    return count;
}

/**
 * find_backend: Find the best matching backend for a given path.
 *
 * Uses **Longest Prefix Match** so more specific prefixes take priority.
 * Example: "/api/users" beats "/api" for path "/api/users/123".
 *
 * Returns NULL if no match is found.
 */

Route *find_backend(Route *routes, int route_count, const char *path)
{
    if (!routes || !route_count || !path)
    {
        perror("Invalid parameter to load the routes");
        return NULL;
    }

    if (path[0] != '/')
    {
        fprintf(stderr, "find_backend: Path must start with '/'\n");
        return NULL;
    }

    Route *best_match = NULL;
    size_t best_match_len = 0;

    // Loop through all configured routes.
    for (int i = 0; i < route_count; i++)
    {
        size_t prefix_len = strlen(routes[i].prefix);
        if (strncmp(path, routes[i].prefix, prefix_len) == 0)
        {
            // longest prefix match
            if (prefix_len > best_match_len)
            {
                best_match = &routes[i];
                best_match_len = prefix_len;
            }
        }
    }
    return best_match;
}