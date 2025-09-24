#pragma once

/**
 * @brief Represents the lifecycle states of a single proxy connection.
 * Each connection moves through these states as it is processed by the proxy.
 * They help the event loop (epoll) decide what action to perform next.
 */

/**
 *  These happen instantly in memory - NO epoll events needed:
 *   CONN_PARSING_REQUEST,     // ❌ Remove - parse_http_request() is immediate
 *   CONN_SELECTING_BACKEND,   // ❌ Remove - find_backend() is immediate
 *   CONN_REBUILDING_REQUEST,  // ❌ Remove - rebuild_request() is immediate
 *  Why? These are CPU-bound operations that complete in microseconds.
 *  Adding states for them adds complexity without async benefit.
 *
 * Only if // If any of these operations could block or fail:
 * - DNS lookups for backend hostnames (if not using IPs)
 * - Database lookups for routing decisions
 * - Authentication calls to external services
 * - Rate limiting checks with external systems
 */

typedef enum
{
    /**---Client Side--- */
    CONN_LISTENING,
    // CONN_IDLE,             /**< Waiting for next HTTP request on an open connection (Keep-Alive only). */
    CONN_READING_REQUEST,  /**< Reading HTTP request bytes from client. */
    CONN_REQUEST_COMPLETE, /**< Full HTTP request received, ready to parse. */

    /**--- BACKEND SIDE --- */
    CONN_CONNECTING_BACKEND, /**< Establishing connection to backend (non-blocking). */
    CONN_SENDING_REQUEST,    /**< Forwarding the parsed request to backend. */
    CONN_READING_RESPONSE,   /**< Receiving HTTP response from backend. */
    CONN_BACKEND_EOF,        /**< To detect backend is closed */

    /**--- RESPONSE TO CLIENT ---*/
    CONN_SENDING_RESPONSE, /**< Sending backend response back to client. */

    /**--- FINAL STATES ---*/
    CONN_ERROR, /**< Error occurred — cleanup required. */
    CONN_DONE   /**< Transaction complete — ready for cleanup/free. */
} connection_state_t;

//                 ┌───────────────────┐
//                 │   CONN_IDLE       │ (slot available / new accept)
//                 └───────┬───────────┘
//                         │ accept()
//                         ▼
//               ┌─────────────────────────┐
//               │ CONN_READING_REQUEST    │  (EPOLLIN from client)
//               └───────────┬─────────────┘
//                           │ full request parsed
//                           ▼
//               ┌─────────────────────────┐
//               │ CONN_CONNECTING_BACKEND │  (non-blocking connect)
//               └───────────┬─────────────┘
//                           │ EPOLLOUT fired, connect complete
//                           ▼
//               ┌─────────────────────────┐
//               │ CONN_SENDING_REQUEST    │  (EPOLLOUT to backend)
//               └───────────┬─────────────┘
//                           │ request fully sent
//                           ▼
//               ┌─────────────────────────┐
//               │ CONN_READING_RESPONSE   │  (EPOLLIN from backend)
//               └───────────┬─────────────┘
//                           │ response buffered
//                           ▼
//               ┌─────────────────────────┐
//               │ CONN_SENDING_RESPONSE   │  (EPOLLOUT to client)
//               └───────────┬─────────────┘
//                           │ response fully sent
//                           ▼
//               ┌─────────────────────────┐
//               │ CONN_DONE               │  (transaction finished)
//               └───────────┬─────────────┘
//            keep-alive?    │             no keep-alive
//              ┌────────────┘
//              │ yes
//              ▼
//     ┌─────────────────────────┐
//     │ CONN_READING_REQUEST    │  (loop for next request)
//     └─────────────────────────┘

//   Error at any stage → ───────────────────────► CONN_ERROR → CONN_CLOSING
//   Timeout / disconnect → ─────────────────────► CONN_CLOSING
