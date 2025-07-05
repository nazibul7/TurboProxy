#pragma once

#include <stddef.h>
#define MAX_HEADERS 32

typedef struct
{
    char *key;
    char *value;
} Header;

typedef struct
{
    char methode[8];
    char http_version[16];
    char *path;
    Header Headers[MAX_HEADERS];
    int header_count;

    char *body;
    size_t body_length;
} HttpRequest;

