#pragma once

#include <sys/epoll.h>

/** Maximum number of events returned by epoll_wait in one call */
#define EPOLL_MAX_EVENTS 128

/**
 * @brief Initialize an epoll instance.
 * @return File descriptor for the epoll instance, or -1 on failure.
 */
int epoll_server_init(void);

/**
 * @brief Add an fd from the kernel’s socket watchlist.
 * @param epoll_fd The epoll instance fd
 * @param fd       The target file descriptor
 * @param event    The epoll_event struct describing which events to watch
 * @return Return 0 = success, -1 = error
 */
int epoll_server_add(int epoll_fd, int fd, struct epoll_event *event);

/**
 * @brief Modify an fd from the kernel’s socket watchlist.
 * @param epoll_fd The epoll instance fd
 * @param fd       The target file descriptor
 * @param event    The epoll_event struct describing which events to watch
 * @return Return 0 = success, -1 = error
 */
int epoll_server_modify(int epoll_fd, int fd, struct epoll_event *event);

/**
 * @brief Delete an fd from the kernel’s socket watchlist.
 * @param epoll_fd The epoll instance fd
 * @param fd       The target file descriptor
 * @return Return 0 = success, -1 = error
 */
int epoll_server_delete(int epoll_fd, int fd);

/**
 * @brief Set a socket fd to non-blocking
 * @param fd The file descriptor to modify to non-blocking
 * @return -1 for error 0 on success
 */
int set_non_blocking(int fd);

/**
 * @brief epoll_wait() → Suspend the calling thread until one or more fds in the kernel’s watchlist become ready, or a timeout occurs.
 * @param epoll_fd The epoll instance fd
 * @param events  An array where kernel will write the list of triggered events.
 * @param timeout → How long to wait:
 *   - 0 → return immediately (non-blocking poll)
 *   - -1 → wait forever until at least one event is ready
 *   - >0 → wait that many milliseconds
 * @return Return
 *   - > 0 → Number of file descriptors (events) that are ready.
 *   - 0 → Timeout occurred (only happens if timeout ≥ 0).
 *   - -1 → Error occurred (check errno)
 */

int epoll_server_wait(int epoll_fd, struct epoll_event *events, int timeouts);