#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <common/server.h>
#include <v2-epoll/epoll_server.h>
#include <common/error_handler.h>
#include <common/route_config.h>

#define PORT 8000
#define BUFFER_SIZE 16384

int main()
{
    // Ignore SIGPIPE globally so the server doesn't crash on client disconnects
    signal(SIGPIPE, SIG_IGN);

    int server_fd, client_fd, epoll_fd;
    char buffer[BUFFER_SIZE];

    server_fd = setup_server(PORT);

    if (server_fd < 0)
    {
        log_error("Failed to start server");
        return 1;
    }

    if (set_non_blocking(server_fd))
    {
        log_error("main: failed to set server fd non-blocking");
        close(server_fd);
        return 1;
    }

    printf("Server is listening on port %d\n", PORT);

    /** initialize epoll fd */
    epoll_fd = epoll_server_init();
    if (epoll_fd < 0)
    {
        log_error("failed to create epoll instance");
        close(server_fd);
        return 1;
    }

    /**
     * struct epoll_event event is just a C struct that you fill out to tell the epoll system call:
     * - Which events to monitor (event.events)
     * - Which user data should be returned to u when those events happens
     */

    struct epoll_event event, events[EPOLL_MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    /**
     * epoll_server_add → Add an fd from the kernel’s watchlist.
     */
    if (epoll_server_add(epoll_fd, server_fd, &event))
    {
        log_error("Could not add server fd to epoll watchlist");
        close(server_fd);
        close(epoll_fd);
        return 1;
    }

    /** load routes */
    Route routes[MAX_ROUTES];
    int route_count = load_routes("routes.conf", routes, MAX_ROUTES);
    if (route_count <= 0)
    {
        log_error("No routes loaded");
        close(server_fd);
        close(epoll_fd);
        return 1;
    }

    while (1)
    {
    }

    return 0;
}