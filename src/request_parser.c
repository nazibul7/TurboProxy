#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request_parser.h"

int parse_http_request(char *buffer, HttpRequest *req)
{
    if (!buffer || !req)
        return -1;

    memset(req, 0, sizeof(HttpRequest));

    char *parseCopy = strdup(buffer);
    if (!parseCopy)
    {
        return -1;
    }
    // Work on a copy so we never corrupt the raw buffer
    char *bufferCopy = strdup(buffer);
    if (!bufferCopy)
    {
        free(parseCopy);
        return -1;
    }

    // ---Find the header-body separator---
    char *body_start = strstr(bufferCopy, "\r\n\r\n");
    if (body_start)
    {
        *body_start = '\0'; // cut headers
        body_start += 4;    // skip the \r\n\r\n
    }


    // ---Parse the request line---

    char *line = strtok(parseCopy, "\r\n");
    if (!line)
    {
        free(parseCopy);
        free(bufferCopy);
        return -1;
    }

    char *method = strtok(line, " ");  // Parse methode
    char *path = strtok(NULL, " ");    // Parse Path version
    char *version = strtok(NULL, " "); // Parse HTTP version
    if (!method || !path || !version)
    {
        free(parseCopy);
        free(bufferCopy);
        return -1;
    }

    strncpy(req->methode, method, sizeof(req->methode) - 1);
    req->path = strdup(path);
    strncpy(req->http_version, version, sizeof(req->http_version) - 1);

    // Initialize header count to zero
    req->header_count = 0;

    // Header parser

    line = strtok(bufferCopy, "\r\n"); // skip request line in bufferCopy
    if (!line)
    {
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
            break;

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
        req->header_count++;
    }

    // Body parser

    if (body_start)
    {
        body_start += 4; // skip the \r\n\r\n
        if (*body_start != '\0')
        {
            req->body = strdup(body_start);
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

    free(bufferCopy);
    return 0;
}