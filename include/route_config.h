#pragma once

#define MAX_ROUTES 10
#define MAX_PREFIX_LEN 32
#define MAX_HOST_LEN 64

/**
 * @file rout_config.h
 * @brief Routing configuration for reverse proxy
 *
 * Defines the Route struct and helper functions
 * to load and match backend routes.
 *
 * @note Thread Safety
 *  We load at once at startup
 *  We are not modifying while serving the request
 */

/**
 * @brief Represents a single routing rule
 *
 * @example
 *  prefix: "/api/"
 *  host: "localhost"
 *  port: 8080
 */

typedef struct
{
    char prefix[MAX_PREFIX_LEN];
    char host[MAX_HOST_LEN];
    int port;
} Route;

/**
 * @brief Loads routes from a text file
 *
 * @param filename Path to the config file
 * @param routes   Pointer to an array of routes
 * @param max_routes maximum number of routes to load
 * @return Number of routes load or error on -1
 */
int load_routes(const char *filename, Route *routes, int max_routes);

/**
 * @brief Find backend route for a given request path.
 *
 * @param routes Array of routes
 * @param route_count Number of routes in the array
 * @param path Request path
 * @return Pointer to matching route or NULL if none found
 */
Route *find_backend(Route *routes, int route_count, const char *path);