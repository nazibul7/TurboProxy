#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <common/server.h>
#include <v2-epoll/epoll_server.h>
#include <common/error_handler.h>
#include <common/route_config.h>
#include <common/request_parser.h>
#include <common/proxy.h>
#include <common/rebuild_request.h>
#include <v2-epoll/epoll_proxy.h>
#include <v2-epoll/connection.h>
#include <v2-epoll/buffer_io.h>
#include <v2-epoll/connection_handler.h>
#include <common/debug.h>

#define PORT 8000
#define BUFFER_SIZE 16384
#define MAX_PENDING_FREE 1024

int main()
{
    /**
     * OPTIMIZATION 1: Ignore SIGPIPE globally
     * WHY: Prevents server crash when client closes connection unexpectedly.
     * Without this, writing to closed socket sends SIGPIPE which terminates process.
     */
    signal(SIGPIPE, SIG_IGN);

    /**
     * OPTIMIZATION 2: Batch cleanup of connections
     * WHY: Freeing connections immediately during event processing can cause
     * issues if the same connection has multiple events pending. Instead,
     * mark for deletion and batch cleanup after all events processed.
     */

    connection_t *pending_free[MAX_PENDING_FREE];
    int pending_free_count = 0;

    int server_fd, client_fd, epoll_fd;

    server_fd = setup_server(PORT);

    if (server_fd < 0)
    {
        log_error("Failed to start server");
        return 1;
    }

    /**
     * OPTIMIZATION 3: Non-blocking server socket
     * WHY: Prevents accept() from blocking the entire event loop.
     * In edge-triggered mode, this allows us to accept multiple connections
     * in a loop without blocking.
     */
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

    static connection_t listener;
    memset(&listener, 0, sizeof(listener));
    listener.client_fd = server_fd;
    listener.state = CONN_LISTENING;

    struct epoll_event event, events[EPOLL_MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.ptr = &listener;

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

    handler_status_t status = HANDLER_OK;

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

        /**
         * Reset pending free counter per iteration
         * WHY: Process all events first, then cleanup. This ensures
         * connections aren't freed while they might still have events
         * in the current batch.
         */
        pending_free_count = 0;

        /**
         * Process all events in batch
         * WHY: epoll_wait returns multiple ready events. Processing them
         * all before next epoll_wait reduces syscall overhead.
         */
        for (int i = 0; i < nfds; i++)
        {
            connection_t *conn = (connection_t *)events[i].data.ptr;

            DEBUG_PRINT("Event %d: events=0x%x, conn=%p\n", i, events[i].events, conn);

            if (!conn || conn->should_free_conn)
            {
                DEBUG_PRINT("  WARNING: Skipping event for freed connection\n");
                continue;
            }

            if (conn)
            {
                DEBUG_PRINT("  client_fd=%d, backend_fd=%d, state=%d\n",
                            conn->client_fd, conn->backend_fd, conn->state);
            }

            if (conn->state == CONN_LISTENING)
            {
                /**
                 * Loop accept for multiple pending connections
                 * WHY: When server is under load, multiple clients may connect
                 * between epoll_wait calls. The listen backlog can hold multiple
                 * pending connections. Accepting in a loop handles bursts better.
                 *
                 * In non-blocking mode:
                 * - accept() returns -1 with errno=EAGAIN when no more connections
                 * - This is EXPECTED, not an error
                 *
                 * NOTE: In level-triggered mode (current), this isn't strictly needed
                 * since we'll get EPOLLIN again if connections remain. But it's still
                 * better for handling bursts efficiently.
                 */
                while (1)
                {
                    /**
                     * accept() never returns EINPROGRESS, because accepting a connection is different:
                     * - either a connection is ready to accept, or it isn’t.
                     * - There’s no “in-progress” state like a TCP handshake.
                     */
                    // accepts the new connections which came to connects
                    client_fd = accept_client(listener.client_fd);
                    if (client_fd == -1)
                    {
                        break; // No connection - move to next epoll event
                    }
                    if (client_fd == -2)
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

                    connection_t *new_conn = connection_create(client_fd);
                    if (!new_conn)
                    {
                        close(client_fd);
                        log_error("connection_create failed for fd=%d", client_fd);
                        continue;
                    }

                    /**
                     * Here for Event flag for epoll that tells you:
                     * "This file descriptor (socket) has data
                     * you can read *without blocking*."
                     */
                    event.events = EPOLLIN;
                    event.data.ptr = new_conn;

                    if (epoll_server_add(epoll_fd, client_fd, &event) < 0)
                    {
                        log_error("Failed to add client fd in epoll watchlist");
                        connection_free(new_conn, epoll_fd);
                        continue;
                    }

                    DEBUG_PRINT("✅ New client connection created: fd=%d, state=%d\n",
                                new_conn->client_fd, new_conn->state);
                    continue;
                }
            }
            else
            {
                /**----------------------2--handle readable events---------------------- */
                if (events[i].events & EPOLLIN)
                {
                    if (conn->state == CONN_READING_REQUEST)
                    {
                        status = handle_client_readable(conn, routes, route_count, epoll_fd);
                    }
                    else if (conn->backend_fd >= 0 && conn->state == CONN_READING_RESPONSE)
                    {
                        status = handle_backend_readable(conn, epoll_fd);
                    }

                    if (status == HANDLER_ERROR || status == HANDLER_CLOSED)
                    {
                        conn->should_free_conn = true;
                    }
                    else if (status == HANDLER_OK && conn->state == CONN_DONE)
                    {
                        conn->should_free_conn = true;
                    }
                }

                /**-----------------------handle writable events---------------------- */
                if (!conn->should_free_conn && events[i].events & EPOLLOUT)
                {
                    if (conn->state == CONN_CONNECTING_BACKEND || conn->state == CONN_SENDING_REQUEST)
                    {
                        status = handle_backend_writable(conn, epoll_fd);
                    }
                    else if (conn->state == CONN_SENDING_RESPONSE)
                    {
                        status = handle_client_writable(conn, epoll_fd);
                    }
                    if (status == HANDLER_ERROR || status == HANDLER_CLOSED)
                    {
                        conn->should_free_conn = true;
                    }
                    else if (status == HANDLER_OK && conn->state == CONN_DONE)
                    {
                        conn->should_free_conn = true;
                    }
                }

                if (conn->should_free_conn)
                {
                    if (pending_free_count < MAX_PENDING_FREE)
                        pending_free[pending_free_count++] = conn;
                    else
                        log_error("Too many connections pending free!");
                }
            }
        }
        for (int i = 0; i < pending_free_count; i++)
        {
            connection_free(pending_free[i], epoll_fd);
        }
    }

    close(epoll_fd);
    close(server_fd);
    return 0;
}