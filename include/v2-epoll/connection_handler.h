#pragma once
#include <v2-epoll/connection.h>
#include <common/route_config.h>

/**
 * @brief Possible return status of a connection handler.
 */

/**
 * In HTTP, EOF (connection close) handling depends on whether it's a keep-alive
 * connection or a connection that should be closed
 */
typedef enum
{
  HANDLER_OK,     /**< Operation succeeded, keep connection alive */
  HANDLER_CLOSED, /**< Peer closed connection (EOF) */
  HANDLER_ERROR   /**< Fatal error occurred */
} handler_status_t;

/**
 * @brief Handle incoming data from client
 *
 * Reads and processes HTTP request from client. Handles request parsing,
 * backend connection establishment, and state transitions.
 *
 * @param conn Pointer to connection structure
 * @return HANDLER_OK if write successful and connection can remain open.
 *         HANDLER_CLOSED if backend closed the connection.
 *         HANDLER_ERROR if a fatal error occurred.
 */
handler_status_t handle_client_readable(connection_t *conn, Route *routes, int route_count, int epoll_fd);

/**
 * @brief Handle outgoing data to client
 *
 * Sends HTTP response data to client. Manages response forwarding
 * from backend server to client.
 *
 * @param conn Pointer to connection structure
 * @return HANDLER_OK if write successful and connection can remain open.
 *         HANDLER_CLOSED if backend closed the connection.
 *         HANDLER_ERROR if a fatal error occurred.
 */
handler_status_t handle_client_writable(connection_t *conn, int epoll_fd);

/**
 * @brief Handle incoming data from backend server
 *
 * Reads HTTP response from backend server and processes it for
 * forwarding to client. Handles response parsing and buffering.
 *
 * @param conn Pointer to connection structure
 * @return HANDLER_OK if write successful and connection can remain open.
 *         HANDLER_CLOSED if backend closed the connection.
 *         HANDLER_ERROR if a fatal error occurred.
 */
handler_status_t handle_backend_readable(connection_t *conn, int epoll_fd);

/**
 * @brief Handle outgoing data to backend server
 *
 * Sends HTTP request to backend server. Manages the rebuilt HTTP
 * request transmission to the upstream server.
 *
 * @param conn Pointer to connection structure
 * @return HANDLER_OK if write successful and connection can remain open.
 *         HANDLER_CLOSED if backend closed the connection.
 *         HANDLER_ERROR if a fatal error occurred.
 */
handler_status_t handle_backend_writable(connection_t *conn, int epoll_fd);

/**
 * @brief Handle connection errors and cleanup
 *
 * Called when EPOLLERR or EPOLLHUP events occur on either socket.
 * Performs complete cleanup of connection resources.
 * @return HANDLER_ERROR always, since this represents a fatal state.
 */
handler_status_t handle_connection_error(connection_t *conn, int epoll_fd);
