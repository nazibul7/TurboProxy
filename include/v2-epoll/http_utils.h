#pragma once

#include <stdbool.h>
#include <v2-epoll/buffer.h>

/**
 * @brief Check if proxy has completed reading request from client
 *
 * @return true for complete & flase for incomplete
 */
bool http_request_complete(const buffer_t *buf);

// typedef struct {
//     Route routes[MAX_ROUTES];
//     int route_count;
//     int epoll_fd;
//     ThreadPool *pool;
// } ServerContext;
