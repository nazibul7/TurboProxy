#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <common/proxy.h>
#include <common/error_handler.h>
#include <v2-epoll/epoll_server.h>

int connect_to_target_nb(char *host, int port)
{
    char *colon = strchr(host, ':');
    if (colon)
    {
        *colon = '\0'; // terminate string at ':'
    }
    // gethostbyname → resolve backend hostname (example: "backend.local" → 10.0.0.5).
    struct hostent *server = gethostbyname(host);
    if (server == NULL)
    {
        log_error("connect_to_target_nb: No such host: %s", host);
        return -1;
    }
    struct sockaddr_in target_addr;
    int socket_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0)
    {
        log_errno("connect_to_target_nb: Failed to create socket");
        return -1;
    }

    /**Set it to non-blocking for  */
    if (set_non_blocking(socket_fd))
    {
        log_error("connect_to_target_nb: failed to set server fd non-blocking");
        close(socket_fd);
        return -1;
    }

    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);

    /** target_addr.sin_addr.s_addr = INADDR_ANY;   INADDR_ANY (value 0.0.0.0) is not valid for client connections. It's used on servers to listen on all interfaces, not to connect.*/
    memcpy(&target_addr.sin_addr, server->h_addr_list[0], server->h_length);
    printf("%s\n", server->h_addr_list[0]);
    printf("%d\n", server->h_length);
    printf("%s\n", server->h_name);

    /*
     * 1. What happens during connect():
     *
     * When a TCP client calls connect():
     *   - The kernel starts a TCP handshake by sending a SYN to the server.
     *   - Waits for the server to respond with SYN-ACK.
     *   - Then the client sends ACK, and the connection is established.
     *
     * Note: This handshake usually takes some time (network latency, server processing, etc.).
     *
     * 2. Blocking vs Non-blocking socket:
     *
     * Blocking socket:
     *   - connect() waits until the handshake completes (or fails).
     *   - The function call does not return until the connection is ready.
     *   - Problem: the program is stuck here if the server is slow or unreachable.
     *
     * Non-blocking socket:
     *   - connect() cannot wait, because the socket is non-blocking.
     *   - If the handshake cannot complete immediately (almost always), the kernel cannot finish the connection instantly.
     *   - Instead, connect() returns -1 and sets errno = EINPROGRESS.
     *   - Meaning: "The connection is in progress; you can continue doing other work.
     *     Later, you can check if it succeeded using select()/poll()/epoll() and getsockopt(SO_ERROR)".
     */

    /**Initiate connection with target backend */
    int ret = connect(socket_fd, (struct sockaddr *)&target_addr, sizeof(target_addr));
    if (ret < 0 && errno != EINPROGRESS)
    {
        log_errno("connect_to_target_nb: Failed to connect to target %s:%d", host, port);
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}