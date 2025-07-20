#pragma once

#include <stddef.h>
#define MAX_HEADERS 32

/**
 * @file http_types.h
 * @brief Represents a HTTP header as a key-value pair
 */

typedef struct
{
    char *key;   /**< Header name (e.g. "Host": "Content-Type") */
    char *value; /**< Header value (e.g., "example.com", "application/json") */
} Header;

/**
 * @brief Represents a parsed HTTP request.
 */

typedef struct
{
    char methode[8];             /**< HTTP method (e.g., "GET", "POST") */
    char http_version[16];       /**< HTTP version (e.g., "HTTP/1.1") */
    char *path;                  /**< Requested path (e.g., "/index.html") */
    Header Headers[MAX_HEADERS]; /**< Array of parsed HTTP headers */
    int header_count;            /**< Number of headers parsed */

    char *body;         /**< Pointer to the request body (if present) */
    size_t body_length; /**< Length of the request body in bytes */
} HttpRequest;
