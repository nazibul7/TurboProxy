#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <common/server.h>
#include <v2-epoll/epoll_server.h>
#include <common/error_handler.h>
#include <common/route_config.h>
#include <common/request_parser.h>
#include <common/proxy.h>
#include <common/rebuild_request.h>

#define PORT 8000
#define BUFFER_SIZE 16384

int main()
{
    // Ignore SIGPIPE globally so the server doesn't crash on client disconnects
    signal(SIGPIPE, SIG_IGN);

    int server_fd, client_fd, epoll_fd;

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

            /**Could add more error handiling to make it robust */
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

                if (epoll_server_add(epoll_fd, client_fd, &event) < 0)
                {
                    log_error("Failed to add client fd in epoll watchlist");
                    close(client_fd);
                    continue;
                }
            }
            else
            {
                char buffer[BUFFER_SIZE];
                /** reset to zero */
                memset(buffer, 0, BUFFER_SIZE);

                int current_fd = events[i].data.fd;

                /** Initialize variables for this client iteration */
                int targetfd = -1;
                bool req_parsed = false;

                int bytes_read = read(current_fd, buffer, BUFFER_SIZE - 1);
                if (bytes_read < 0)
                {
                    if (errno == EINTR) // Interrupted, retry
                        continue;
                    // For READ, you get: ECONNRESET, but not EPIPE
                    if (errno == ECONNRESET)
                    {
                        log_errno("Client disconnected (ECONNRESET)");
                    }
                    else
                    {
                        log_errno("Failed to read response");
                    }
                    goto cleanup;
                }
                /**
                 *  According to POSIX and common socket programming practice, documented in StackOverflow, a read() or recv() returning 0 means:
                 *  - Remote peer closed connection normally
                 *  - No error, just that no bytes were read" because the remote socket was closed gracefully
                 *  - Do not consider it an error; treat it as connection end.
                 */
                if (bytes_read == 0)
                {
                    log_error("send returned 0 bytes (connection closed)");
                    goto cleanup;
                }

                printf("Received request:\n%s\n", buffer);

                HttpRequest req;

                if (parse_http_request(buffer, &req) != 0)
                {
                    log_error("Failed to parse HTTP request\n");
                    send_http_error(current_fd, 400, "Bad Request");
                    goto cleanup;
                }
                req_parsed = true;

                /** Find best backend based on path prefix*/
                Route *backend = find_backend(routes, route_count, req.path);
                if (!backend)
                {
                    log_error("No backend found for path: %s", req.path);
                    send_http_error(current_fd, 502, "Bad Gateway");
                    goto cleanup;
                }

                printf("Routing to backend: %s:%d for prefix: %s\n", backend->host, backend->port, backend->prefix);

                targetfd = connect_to_target(backend->host, backend->port);

                if (targetfd < 0)
                {
                    log_error("Failed to connect to backend %s:%d", backend->host, backend->port);
                    send_http_error(current_fd, 502, "Bad Gateway");
                    goto cleanup;
                }

                /** Get client IP */
                char client_ip[16];

                get_client_ip(current_fd, client_ip, sizeof(client_ip));

                char request[MAX_REQUEST_SIZE];

                int build_request = rebuild_request(&req, request, client_ip, sizeof(request));

                if (build_request < 0)
                {
                    log_error("Failed to rebuild request from client %d", current_fd);
                    send_http_error(current_fd, 500, "Internal Server Error");
                    goto cleanup;
                }

                if (forward_request(targetfd, request, strlen(request)) < 0)
                {
                    log_errno("Failed to forward request to backend %s:%d", backend->host, backend->port);
                    send_http_error(current_fd, 502, "Bad Gateway");
                    goto cleanup;
                }

                /*
                 * Note:
                 * This proxy only handles one request per TCP connection.
                 * It force-closes client_fd and target_fd after relay.
                 * It must add 'Connection: close' to backend request and client response
                 * to avoid keep-alive hangs.
                 * See docs/connection-issues.md for details.
                 */

                if (relay_response(targetfd, current_fd) < 0)
                {
                    log_error("Failed to relay response to client");
                    send_http_error(current_fd, 502, "Bad Gateway");
                    goto cleanup;
                }

                /**
                 * Close backend connection after successful response relay.
                 *
                 * This is critical because:
                 * - Prevents file descriptor leaks (each connect_to_target() opens a new fd)
                 * - Avoids backend connection state issues that can affect subsequent requests
                 * - Prevents local port exhaustion on the proxy side
                 * - Ensures proper cleanup of connection buffers and resources
                 *
                 * Without this, the proxy will work on first request but fail on subsequent
                 * requests due to resource leaks and stale connection state.
                 */

                printf("DEBUG: closing target backend and client socket\n");

            cleanup:
                // Clean up resources for this client
                if (req_parsed)
                {
                    free_http_request(&req);
                }
                if (targetfd != -1)
                {
                    close(targetfd);
                }
                if (current_fd >= 0)
                {
                    close(current_fd);
                }
                /**Continue to next client */
                continue;
            }
        }
    }

    close(epoll_fd);
    close(server_fd);
    return 0;
}