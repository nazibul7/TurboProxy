
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <v2-epoll/connection.h>
#include <common/error_handler.h>
#include <v2-epoll/connection_state.h>
#include <v2-epoll/buffer.h>
#include <v2-epoll/epoll_server.h>
#include <common/request_parser.h>

connection_t *connection_create(int client_fd)
{
    /**Allocate the memory for connection object */
    connection_t *conn = malloc(sizeof(connection_t));

    if (!conn)
    {
        log_errno("connection_create: Failed to allocates memory for connection object for client %d", client_fd);
        return NULL;
    }

    /** reset memory to zero */
    memset(conn, 0, sizeof(connection_t));

    /**Initalize fields */
    conn->client_fd = client_fd;
    conn->backend_fd = -1;
    conn->state = CONN_READING_REQUEST;

    if (buffer_init(&conn->request_buffer) != 0)
    {
        log_error("connection_create: Failed to initialize request buffer for client %d", client_fd);
        free(conn);
        return NULL;
    }

    conn->request_parsed = false;

    if (buffer_init(&conn->rebuilt_request_buffer) != 0)
    {
        log_error("connection_create: Failed to initialize rebuilt request buffer for client %d", client_fd);
        free(conn);
        return NULL;
    }

    if (buffer_init(&conn->response_buffer) != 0)
    {
        log_error("connection_create: Failed to initialize response buffer for client %d", client_fd);
        free(conn);
        return NULL;
    }

    conn->last_error = 0;

    /**
     * After using memset(conn, 0, sizeof(connection_t));
     * all below field has become zero
     * client_ip, parsed_request, or selected_backend
     */

    return conn;
}

// void connection_free(connection_t *conn)
// {
//     if (!conn)
//         return;

//     if (conn->backend_fd >= 0)
//     {
//         close(conn->backend_fd);
//         conn->backend_fd = -1;
//     }

//     if (conn->client_fd >= 0)
//     {
//         close(conn->client_fd);
//         conn->client_fd = -1;
//     }

//     buffer_cleanup(&conn->request_buffer);

//     buffer_cleanup(&conn->response_buffer);

//     /**frees the connection_t struct memory */
//     free(conn);
// }

void connection_free(connection_t *conn, int epoll_fd)
{
    if (!conn)
        return;

    // Add this debug logging
    printf("DEBUG: Freeing connection %p (client_fd=%d, backend_fd=%d)\n",
           (void *)conn, conn->client_fd, conn->backend_fd);

    if (conn->request_parsed)
    {
        free_http_request(&conn->parsed_request);
        conn->request_parsed = false;
    }

    if (conn->backend_fd >= 0)
    {
        epoll_server_delete(epoll_fd, conn->backend_fd);
        close(conn->backend_fd);
        conn->backend_fd = -1;
    }

    if (conn->client_fd >= 0)
    {
        epoll_server_delete(epoll_fd, conn->client_fd);
        close(conn->client_fd);
        conn->client_fd = -1;
    }

    buffer_cleanup(&conn->rebuilt_request_buffer);
    buffer_cleanup(&conn->request_buffer);
    buffer_cleanup(&conn->response_buffer);

    printf("DEBUG: About to free connection %p\n", (void *)conn);
    free(conn);
    printf("DEBUG: Freed connection %p\n", (void *)conn);
}