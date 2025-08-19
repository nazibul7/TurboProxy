#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
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
        int nfds = epoll_server_wait(epoll_fd, events, -1);

        if (nfds < 0)
        {
            /**
             * Interrupted by signal — just restart epoll_wait
             */
            if (errno == EINTR)
                continue;
            else
            {
                log_error("main: epoll_wait failed");
                break;
            }
        }

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                // accepts the new connections which came to connects
                client_fd = accept_client(server_fd);
                if (client_fd < 0)
                {
                    log_error("Failed to accept the client fd");
                    continue;
                }

                // make client_ids non-blocking
                if (set_non_blocking(client_fd))
                {
                    log_error("main: failed to set client fd non-blocking");
                    close(client_fd);
                    continue;
                }
                /**
                 * Here for Event flag for epoll that tells you:
                 * "This file descriptor (socket) has data
                 * you can read *without blocking*."
                 */
                event.events = EPOLLIN;
                event.data.fd = client_fd;

                if (epoll_server_add(epoll_fd, client_fd, &event)<0){
                    log_error("Failed to add client fd in epoll watchlist");
                    close(client_fd);
                    continue;
                }
            }
            else
            {
            }
        }
    }

    return 0;
}