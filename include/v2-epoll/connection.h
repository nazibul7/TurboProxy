#include <stdbool.h>
#include "buffer.h"
#include "common/http_types.h"
#include "common/route_config.h"
#include "v2-epoll/connection_state.h"

/**
 * Represents a single proxied connection (client <-> proxy <-> backend).
 * Tracks all state, buffers, and metadata required for async event-driven
 * processing via epoll.
 */
typedef struct connection
{
    /* ---------------- File Descriptors ---------------- */
    int client_fd;  /**< Client socket fd (accepted from listen socket). */
    int backend_fd; /**< Backend socket fd (-1 if not yet connected). */

    /* ---------------- Connection State ---------------- */
    connection_state_t state; /**< Current connection state (e.g. RECV_REQ, SEND_BACKEND, RELAY_RESP). */

    /* ---------------- Request Processing ---------------- */
    buffer_t request_buffer;    /**< Accumulates raw HTTP request from client. */
    HttpRequest parsed_request; /**< Parsed HTTP request (populated once parsing succeeds). */
    bool request_parsed;        /**< True once request has been fully parsed. */
    Route selected_backend;     /**< Routing decision for backend (after parsing request). */

    /* ---------------- Backend Communication ---------------- */
    char *rebuilt_request;       /**< Reconstructed/normalized request for backend (malloc’ed). */
    size_t rebuilt_request_size; /**< Total size of rebuilt request. */
    size_t request_sent_bytes;   /**< How many bytes already sent to backend. */

    /* ---------------- Response Handling ---------------- */
    buffer_t response_buffer;   /**< Buffer holding backend response data. */
    size_t response_sent_bytes; /**< Bytes of response already sent to client. */

    /* ---------------- Error Handling ---------------- */
    int last_error; /**< Last errno or internal error code. */

    /* ---------------- Client Metadata ---------------- */
    char client_ip[16]; /**< Client IPv4 string ("xxx.xxx.xxx.xxx"). */

} connection_t;

/**
 * @brief Allocate and initialize a new proxy connection object.
 *
 * @param client_fd File descriptor of the accepted client socket.
 * @return Pointer to a newly allocated connection_t on success,
 *         or NULL on allocation/initialization failure.
 */
connection_t *connection_create(int client_fd);


/**
 * @brief Clean up and release all resources associated with a proxy connection.
 * 
 * @param conn Pointer to the connection_t object to free (may be NULL).
 */
void connection_free(connection_t *conn);