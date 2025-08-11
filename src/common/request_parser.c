#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/request_parser.h"
#include "common/error_handler.h"

int parse_http_request(char *buffer, HttpRequest *req)
{
    if (!buffer || !req)
    {
        log_error("Invalid arguments to parse_http_request (buffer=%p, req=%p)", (void *)buffer, (void *)req);
        return -1;
    }

    memset(req, 0, sizeof(HttpRequest));

    char *parseCopy = strdup(buffer);
    if (!parseCopy)
    {
        log_errno("strdup failed for parseCopy");
        return -1;
    }
    // Work on a copy so we never corrupt the raw buffer
    /**
     * strdup
     * -Allocating enough memory for the string (including null terminator),
     * -Copying the contents of the string into that new memory,
     * -Returning a pointer to the newly allocated copy.
     */
    char *bufferCopy = strdup(buffer);
    if (!bufferCopy)
    {
        log_errno("strdup failed for bufferCopy");
        free(parseCopy);
        return -1;
    }

    // ---Find the header-body separator---

    // strstr only searches for a substring and returns a pointer within the original string but doesn't modify original string.
    char *body_start = strstr(bufferCopy, "\r\n\r\n");
    if (body_start)
    {
        *body_start = '\0'; // cut headers
        body_start += 4;    // skip the \r\n\r\n & point to body
    }

    // ---Parse the request line(GET /index.html HTTP/1.1\r\n)---

    /**
     * strtok modifies the original string by inserting '\0' at the delimiter positions
     * returns a char*, this points to the start of the token it just found.
     */
    char *line = strtok(parseCopy, "\r\n");
    if (!line)
    {
        log_error("Request line is missing");
        free(parseCopy);
        free(bufferCopy);
        return -1;
    }

    char *request_line = strdup(line);
    if (!request_line)
    {
        log_errno("strdup failed for request_line");
        free(parseCopy);
        free(bufferCopy);
        return -1;
    }
    char *method = strtok(request_line, " "); // Parse methode
    char *path = strtok(NULL, " ");           // Parse Path version
    char *version = strtok(NULL, " ");        // Parse HTTP version
    if (!method || !path || !version)
    {
        log_error("parse_http_request: Malformed request line: '%s'", line);
        free(parseCopy);
        free(bufferCopy);
        return -1;
    }

    strncpy(req->methode, method, sizeof(req->methode) - 1);
    req->path = strdup(path);
    if (!req->path)
    {
        log_errno("strdup failed for path");
        free(parseCopy);
        free(bufferCopy);
        free(request_line);
        return -1;
    }
    strncpy(req->http_version, version, sizeof(req->http_version) - 1);

    // Initialize header count to zero
    req->header_count = 0;

    // Header parser

    line = strtok(bufferCopy, "\r\n"); // skip request line in bufferCopy
    if (!line)
    {
        log_error("Header parsing failed: no lines found");
        free(request_line);
        free(bufferCopy);
        free(parseCopy);
        return -1;
    }
    while ((line = strtok(NULL, "\r\n")) && *line != '\0')
    {
        if (strlen(line) == 0)
        {
            printf("Hello\n");
            break;
        }
        printf("Header line: %s\n", line);
        if (req->header_count >= MAX_HEADERS)
        {
            log_error("parse_http_request: Maximum header count (%d) reached, ignoring further headers", MAX_HEADERS);
            break;
        }

        char *colon = strchr(line, ':');
        if (!colon)
            continue;

        *colon = '\0';
        char *key = line;
        char *value = colon + 1;
        while (*value == ' ')
            value++; // trim space

        printf("Header found - Key: '%s', Value: '%s'\n", key, value);
        req->Headers[req->header_count].key = strdup(key);
        req->Headers[req->header_count].value = strdup(value);
        if (!req->Headers[req->header_count].key || !req->Headers[req->header_count].value)
        {
            log_errno("strdup failed for header key/value at index %d (line: %s)", req->header_count, line);
            free(parseCopy);
            free(bufferCopy);
            free(request_line);
            return -1;
        }
        req->header_count++;
    }

    // Body parser

    if (body_start)
    {
        if (*body_start != '\0')
        {
            req->body = strdup(body_start);
            if (!req->body)
            {
                log_errno("strdup failed for body");
                free(parseCopy);
                free(bufferCopy);
                free(request_line);
                return -1;
            }
            req->body_length = strlen(body_start);
            printf("Body: %s\n", req->body);
        }
        else
        {
            req->body = NULL;
            req->body_length = 0;
            printf("Body not found\n");
        }
    }
    else
    {
        req->body = NULL;
        req->body_length = 0;
        printf("Body not found\n");
    }

    if (req->header_count > 0)
    {
        for (int i = 0; i < req->header_count; i++)
        {
            printf("Header[%d]: %s => %s\n",
                   i,
                   req->Headers[i].key,
                   req->Headers[i].value);
        }
    }
    else
    {
        printf("No headers were parsed.\n");
    }
    free(parseCopy);
    free(request_line);
    free(bufferCopy);
    return 0;
}

void free_http_request(HttpRequest *req)
{
    if (!req)
        return;

    // Free path
    if (req->path)
    {
        free(req->path);
        req->path = NULL;
    }

    // Free headers
    for (int i = 0; i < req->header_count; i++)
    {
        if (req->Headers[i].key)
        {
            free(req->Headers[i].key);
            req->Headers[i].key = NULL;
        }
        if (req->Headers[i].value)
        {
            free(req->Headers[i].value);
            req->Headers[i].value = NULL;
        }
    }
    req->header_count = 0;

    // Free body
    if (req->body)
    {
        free(req->body);
        req->body = NULL;
    }
    req->body_length = 0;
}