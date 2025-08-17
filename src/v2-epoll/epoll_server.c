#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include "v2-epoll/epoll_server.h"
#include "common/error_handler.h"

/**initialize epoll fd */
int epoll_server_init()
{
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0){
        log_errno("epoll_server_init: epoll_create1() failed");
        return -1;
    }
    return epoll_fd;
}

/**
 * struct epoll_event event is just a C struct that you fill out to tell the epoll system call
 * - Which events you want to monitor (event.events)
 * - Which user data should be returned to you when those events happen (event.data)
 */

int epoll_server_add(int epoll_fd, int fd, struct epoll_event *event)
{
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, event);
    if (ret < 0){
        log_errno("epoll_server_add: epoll_ctl EPOLL_CTL_ADD failed");
        return -1;
    }
    else
        return 0;
}

int epoll_server_modify(int epoll_fd, int fd, struct epoll_event *event)
{
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, event);
    if (ret < 0)
        return -1;
    else
        return 0;
}

int epoll_server_delete(int epoll_fd, int fd)
{
    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (ret < 0)
        return -1;
    else
        return 0;
}

int set_non_blocking(int fd)
{
    /**
     * This reads the current file descriptor flags. A socket might already have other flags.
     * If you set new flags blindly, we might unintentionally remove important settings already present.
     */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        log_errno("set_nonblocking: fcntl F_GETFL");
        return -1;
    }
    /**
     * The | (bitwise OR) adds O_NONBLOCK to the existing flags without removing the others.
     * If we skip first one & set fcntl(fd, F_SETFL, O_NONBLOCK);  // WRONG!
     * Because we'd wipe out all previous settings and set only non-blocking mode.
     * By using flags | O_NONBLOCK, you turn on non-blocking mode without disturbing other settings.
     */
    int updatedFlags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (updatedFlags == -1)
    {
        perror("set_nonblocking: fcntl F_SETFL O_NONBLOCK");
        return -1;
    }
    return 0;
}

int epoll_server_wait(int epoll_fd, struct epoll_event *events, int timeouts)
{
    int nfds = epoll_wait(epoll_fd, events, EPOLL_MAX_EVENTS, timeouts);
    if (nfds == -1 && errno != EINTR)
    {
        log_errno("epoll_server_wait: epoll_wait failed");
        return -1;
    }
    return nfds;
}
