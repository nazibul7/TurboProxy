#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <v2-epoll/connection_handler.h>
#include <v2-epoll/buffer_io.h>
#include <common/request_parser.h>
#include <v2-epoll/http_utils.h>
#include <common/error_handler.h>
#include <common/rebuild_request.h>
#include <v2-epoll/epoll_proxy.h>
#include <v2-epoll/epoll_server.h>
#include <common/debug.h>

handler_status_t handle_client_readable(connection_t *conn, Route *routes, int route_count, int epoll_fd)
{
    conn->state = CONN_READING_REQUEST;
    ssize_t bytes_read = buffer_read_from_fd(&conn->request_buffer, conn->client_fd);

    if (bytes_read == -1)
    {
        conn->state = CONN_ERROR;
        return HANDLER_ERROR;
    }
    else if (bytes_read == -2)
    {
        return HANDLER_CLOSED;
    }
    else if (bytes_read == 0)
    {
        return HANDLER_OK;
    }
    else if (bytes_read > 0)
    {
        if (http_request_complete(&conn->request_buffer))
        {
            conn->state = CONN_REQUEST_COMPLETE;
            DEBUG_PRINT("Received request:\n%s\n", conn->request_buffer.data);

            if (parse_http_request(conn->request_buffer.data, &conn->parsed_request) != 0)
            {
                log_error("handle_client_readable: Failed to parse HTTP request\n");
                send_http_error(conn->client_fd, 400, "Bad Request");
                conn->state = CONN_ERROR;
                return HANDLER_ERROR;
            }

            conn->request_parsed = true;

            conn->selected_backend = find_backend(routes, route_count, conn->parsed_request.path);
            if (!conn->selected_backend)
            {
                log_error("handle_client_readable: No backend found for path: %s\n", conn->parsed_request.path);
                send_http_error(conn->client_fd, 502, "Bad Gateway");
                conn->state = CONN_ERROR;
                return HANDLER_ERROR;
            }

            DEBUG_PRINT("Routing to backend: %s:%d for prefix: %s\n", conn->selected_backend->host, conn->selected_backend->port, conn->selected_backend->prefix);

            /** Get client IP */

            get_client_ip(conn->client_fd, conn->client_ip, sizeof(conn->client_ip));

            /**Ensure buffer has space available for rebuild request */
            if (buffer_ensure_space(&conn->rebuilt_request_buffer, 4096) != 0)
            {
                log_error("Failed to ensure buffer space for rebuilt request");
                send_http_error(conn->client_fd, 500, "Internal Server Error");
                conn->state = CONN_ERROR;
                return HANDLER_ERROR;
            }

            char *data_ptr = buffer_write_ptr(&conn->rebuilt_request_buffer);
            size_t available_space = buffer_available_space(&conn->rebuilt_request_buffer);

            ssize_t rebuilt_request_size = rebuild_request(&conn->parsed_request, data_ptr, conn->client_ip, available_space);

            /**
             * Update buffer metadata after external function wrote data directly to buffer memory.
             *
             * CRITICAL: This manual update is required because rebuild_request() operates on raw
             * memory (char*) and has no knowledge of our buffer_t wrapper structure. The buffer's
             * len field tracks how much valid data exists, but it can only know what we tell it.
             *
             * Without this update:
             * - buffer_available_data() returns 0 (thinks buffer is empty)
             * - buffer_write_to_fd() has no data to send
             * - Connection gets stuck in infinite EPOLLOUT loop
             *
             * @param rebuilt_request_size: Number of bytes rebuild_request() wrote to buffer memory
             */
            if (rebuilt_request_size > 0)
            {
                conn->rebuilt_request_buffer.len += rebuilt_request_size;
            }
            else
            {
                log_error("handle_client_readable: Failed to rebuild request from client %d\n", conn->client_fd);
                send_http_error(conn->client_fd, 500, "Internal Server Error");
                conn->state = CONN_ERROR;
                return HANDLER_ERROR;
            }

            conn->backend_fd = connect_to_target_nb(conn->selected_backend->host, conn->selected_backend->port);

            if (conn->backend_fd < 0)
            {
                log_error("handle_client_readable: Failed to connect to backend %s:%d\n", conn->selected_backend->host, conn->selected_backend->port);
                send_http_error(conn->client_fd, 502, "Bad Gateway");
                conn->state = CONN_ERROR;
                return HANDLER_ERROR;
            }

            conn->state = CONN_CONNECTING_BACKEND;

            struct epoll_event event;
            event.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
            event.data.ptr = conn;

            /**
             * epoll_server_add → Add an fd from the kernel’s watchlist.
             */
            if (epoll_server_add(epoll_fd, conn->backend_fd, &event))
            {
                close(conn->backend_fd);
                log_error("handle_client_readable: Could not add backend fd %d to epoll watchlist\n", conn->backend_fd);
                conn->state = CONN_ERROR;
                return HANDLER_ERROR;
            }
            return HANDLER_OK;
        }
        else
        {
            DEBUG_PRINT("Waiting for more data\n");
            return HANDLER_OK;
        }
    }
    return HANDLER_OK;
}

handler_status_t handle_backend_writable(connection_t *conn, int epoll_fd)
{
    DEBUG_PRINT("DEBUG: handle_backend_writable called, state=%d\n", conn->state);
    if (conn->state == CONN_CONNECTING_BACKEND)
    {

        int err = 0;
        socklen_t len = sizeof(err);

        /**Check if non-blocking connect() succeeded or failed */
        if (getsockopt(conn->backend_fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        {
            log_errno("handle_backend_writable: getsockopt failed\n");
            conn->state = CONN_ERROR;
            return HANDLER_ERROR;
        }
        if (err != 0)
        {
            log_error("handle_backend_writable: Backend connect failed: %d\n", err);
            conn->state = CONN_ERROR;
            return HANDLER_ERROR;
        }

        DEBUG_PRINT("Connected to backend %s:%d\n",
                    conn->selected_backend->host, conn->selected_backend->port);

        conn->state = CONN_SENDING_REQUEST;
    }

    if (conn->state == CONN_SENDING_REQUEST)
    {
        /**epoll: "now watch for writable, so we can send the request" */
        ssize_t request_sent_to_backend = buffer_write_to_fd(&conn->rebuilt_request_buffer, conn->backend_fd);

        if (request_sent_to_backend == -1)
        {
            log_error("handle_backend_writable: Failed to forward request to backend %s:%d\n",
                      conn->selected_backend->host, conn->selected_backend->port);
            send_http_error(conn->client_fd, 502, "Bad Gateway");
            conn->state = CONN_ERROR;
            return HANDLER_ERROR;
        }
        else if (request_sent_to_backend == -2)
        {
            DEBUG_PRINT("Backend closed connection during request send");
            return HANDLER_CLOSED;
        }
        else if (request_sent_to_backend == 0)
        {
            return HANDLER_OK;
        }

        if (buffer_available_data(&conn->rebuilt_request_buffer) == 0)
        {
            /** Switch backend socket to EPOLLIN so we can read the response next */
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            event.data.ptr = conn;
            if (epoll_server_modify(epoll_fd, conn->backend_fd, &event) < 0)
            {
                log_error("handle_backend_writable: Failed to modify backend fd to EPOLLIN\n");
                conn->state = CONN_ERROR;
                return HANDLER_ERROR;
            }

            conn->state = CONN_READING_RESPONSE;
        }

        /**else part---> Keep EPOLLOUT, will try again on next event */
    }
    return HANDLER_OK;
}

handler_status_t handle_backend_readable(connection_t *conn, int epoll_fd)
{
    if (conn->state != CONN_READING_RESPONSE)
    {
        return HANDLER_OK;
    }
    DEBUG_PRINT("DEBUG: About to read from backend fd=%d\n", conn->backend_fd);
    ssize_t bytes = buffer_read_from_fd(&conn->response_buffer, conn->backend_fd);
    DEBUG_PRINT("DEBUG: buffer_read_from_fd returned %zd\n", bytes);

    if (bytes == -1)
    {
        log_error("handle_backend_readable: Backend read error");
        conn->state = CONN_ERROR;
        return HANDLER_ERROR;
    }
    else if (bytes == -2)
    {
        DEBUG_PRINT("handle_backend_readable: Backend sent EOF");
        conn->state = CONN_BACKEND_EOF;
        epoll_server_delete(epoll_fd, conn->backend_fd);
    }
    else if (bytes == 0)
    {
        DEBUG_PRINT("DEBUG: No data available (EAGAIN) - why so many?\n");
        return HANDLER_OK;
    }
    else if (bytes > 0)
    {
        DEBUG_PRINT("DEBUG: Read %zd bytes from backend\n", bytes);
    }
    /**Always check for data to send, even after EOF */
    if (buffer_available_data(&conn->response_buffer) > 0)
    {
        conn->state = CONN_SENDING_RESPONSE;
        ssize_t sent = buffer_write_to_fd(&conn->response_buffer, conn->client_fd);
        if (sent == -1)
        {
            log_error("handle_backend_readable: Client send error");
            conn->state = CONN_ERROR;
            return HANDLER_ERROR;
        }
        else if (sent == -2)
        {
            DEBUG_PRINT("handle_backend_readable: Client closed connection");
            return HANDLER_CLOSED;
        }
        else if (sent >= 0)
        {
            if (buffer_available_data(&conn->response_buffer) == 0)
            {
                if (conn->state == CONN_BACKEND_EOF)
                {
                    DEBUG_PRINT("handle_backend_readable: Backend EOF and all data sent - closing");
                    epoll_server_delete(epoll_fd, conn->backend_fd);
                    return HANDLER_CLOSED;
                }
                else
                {
                    /**Continue reading from backend */
                    conn->state = CONN_READING_RESPONSE;
                }
            }
            else
            {
                /**Partial send - enable EPOLLOUT for client */
                conn->state = CONN_SENDING_RESPONSE;
                struct epoll_event event;
                event.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
                event.data.ptr = conn;
                if (epoll_server_modify(epoll_fd, conn->client_fd, &event) < 0)
                {
                    log_error("handle_backend_readable: Failed to modify client fd to EPOLLIN\n");
                    conn->state = CONN_ERROR;
                    return HANDLER_ERROR;
                };
            }
        }
    }
    else
    {
        if (conn->state == CONN_BACKEND_EOF)
        {
            DEBUG_PRINT("Backend EOF with no data - closing connection");
            epoll_server_delete(epoll_fd, conn->backend_fd);
            return HANDLER_CLOSED;
        }
    }
    return HANDLER_OK;
}

handler_status_t handle_client_writable(connection_t *conn, int epoll_fd)
{
    if (conn->state == CONN_DONE)
    {
        return HANDLER_CLOSED;
    }
    else if (conn->state != CONN_SENDING_RESPONSE)
    {
        return HANDLER_OK;
    }

    /**Always try to send when in SENDING_RESPONSE state */
    ssize_t sent = buffer_write_to_fd(&conn->response_buffer, conn->client_fd);

    if (sent == -1)
    {
        log_error("handle_client_writable: Client write error");
        conn->state = CONN_ERROR;
        return HANDLER_ERROR;
    }
    else if (sent == -2)
    {
        DEBUG_PRINT("handle_client_writable: Client closed connection during write");
        return HANDLER_CLOSED;
    }
    else if (sent >= 0)
    {
        if (buffer_available_data(&conn->response_buffer) == 0)
        {
            if (conn->state == CONN_BACKEND_EOF)
            {
                DEBUG_PRINT("handle_client_writable: Backend closed and all data sent - closing connection");
                epoll_server_delete(epoll_fd, conn->backend_fd);
                return HANDLER_CLOSED;
            }
            else
            {
                /**Switch back to reading from backend */
                conn->state = CONN_READING_RESPONSE;
                struct epoll_event backend_event;
                backend_event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
                backend_event.data.ptr = conn;
                if (epoll_server_modify(epoll_fd, conn->backend_fd, &backend_event) < 0)
                {
                    log_error("handle_backend_readable: Failed to modify backend fd to EPOLLIN\n");
                    conn->state = CONN_ERROR;
                    return HANDLER_ERROR;
                }

                /**Remove client from EPOLLOUT monitoring */

                struct epoll_event client_event;
                client_event.events = EPOLLERR | EPOLLHUP;
                client_event.data.ptr = conn;

                if (epoll_server_modify(epoll_fd, conn->client_fd, &client_event) < 0)
                {
                    log_error("handle_client_writable: failed to modify client fd %d", conn->client_fd);
                    conn->state = CONN_ERROR;
                    return HANDLER_ERROR;
                }
            }
        }
        else
        {
            DEBUG_PRINT("handle_client_writable: Still have %zu bytes to send", buffer_available_data(&conn->response_buffer));
            // Keep waiting for next EPOLLOUT - state remains CONN_SENDING_RESPONSE
        }
    }
    return HANDLER_OK;
}