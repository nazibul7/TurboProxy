#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include "proxy.h"
#include "error_handler.h"

int connect_to_target(char *host, int port)
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
        log_error("No such host: %s", host);
        printf("Error: No such host: %s\n", host);
        return -1;
    }
    struct sockaddr_in target_addr;
    int socket_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0)
    {
        log_errno("Failed to create socket");
        return -1;
    }

    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    // target_addr.sin_addr.s_addr = INADDR_ANY;   INADDR_ANY (value 0.0.0.0) is not valid for client connections. It's used on servers to listen on all interfaces, not to connect.
    memcpy(&target_addr.sin_addr, server->h_addr_list[0], server->h_length);
    printf("%s\n", server->h_addr_list[0]);
    printf("%d\n", server->h_length);
    printf("%s\n", server->h_name);
    if (connect(socket_fd, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0)
    {
        log_errno("Failed to connect to target %s:%d", host, port);
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

int forward_request(int targetFd, char *request_data, int length)
{
    // return send(targetFd, request_data, strlen(request_data), 0);

    // Why not just only send because--

    // i- Kernel socket buffer may not have enough space.

    // ii-TCP flow control may limit sending rate.

    // iii-Signal interruption (EINTR) might stop it.

    // iv- Non-blocking sockets might return early with EAGAIN / EWOULDBLOCK.

    ssize_t total_sent = 0;           // Track how many bytes we’ve sent
    char *buf = (char *)request_data; // Cast to a pointer for byte-by-byte math becaue void *buffer = malloc(1024); buffer is a const void *, You can't do arithmetic on a void * in standard C

    while (total_sent < length) // Keep sending until we've sent everything
    {
        ssize_t sent = send(targetFd, buf + total_sent, length - total_sent, 0);
        if (sent < 0)
        {
            if (errno == EINTR) // Interrupted by a signal? Retry
            {
                continue;
            }
            if (errno == EPIPE || errno == ECONNRESET)
            {
                log_error("forward_request: connection closed by peer (errno=%d)", errno);
                return -1;
            }

            log_errno("forward_request: send failed");
            return -1;
        }
        if (sent == 0) // No progress? Connection broken
        {
            log_error("forward_request: send returned 0 bytes (connection possibly closed)");
            return -1;
        }
        total_sent += sent; // Update how much we've sent
    }
    return total_sent;
}

// int relay_response(int targetFd, int clientFd)
// {
//     char buffer[4096];
//     ssize_t bytes_read;

//     while ((bytes_read = recv(targetFd, buffer, sizeof(buffer), 0)) > 0)
//     {
//         send(clientFd, buffer, bytes_read, 0);
//     }

//     return 0;
// }

// int forward_request(int targetFd, char *request_data, int length)
// {
//     int total_sent = 0;
//     while (total_sent < length)
//     {
//         int sent = send(targetFd, request_data + total_sent, length - total_sent, 0);
//         if (sent < 0)
//         {
//             perror("Failed to forward request");
//             return -1;
//         }
//         total_sent += sent;
//     }
//     return total_sent;
// }

int relay_response(int targetFd, int clientFd)
{
    char buffer[16384];
    ssize_t bytes_read;

    printf("DEBUG: relay_response started\n");
    while (1)
    {
        bytes_read = recv(targetFd, buffer, sizeof(buffer), 0);
        printf("DEBUG: relay_response read %zd bytes\n", bytes_read);

        // bytes_read < 0 (-1)	Error occurred during read	Check errno; retry or handle error
        if (bytes_read < 0)
        {
            /**
             * EINTR (Interrupted system call)
             * - It means a system call (like read(), recv(), or send()) was interrupted by a signal before it could complete.
             * - The usual response is to retry the system call,
             *   because this interruption is not a true error in reading or writing data,
             *   just an interruption.
             */
            if (errno == EINTR)
                continue;
            else
            {
                log_errno("relay_response: recv failed");
                return -1;
            }
        }
        else if (bytes_read == 0)
        {
            // Connection closed by target server
            log_error("relay_response: target server closed connection");
            break;
        }

        ssize_t total_sent = 0;
        while (total_sent < bytes_read)
        {
            ssize_t bytes_sent = send(clientFd, buffer + total_sent, bytes_read - total_sent, 0);
            if (bytes_sent < 0)
            {
                if (errno == EINTR)
                    continue;
                /**
                 * EPIPE (Broken pipe)
                 * Indicates that a write to a pipe or socket failed because the reading end has been closed.
                 * In networking, this typically happens when you try to send() data to a socket for which the other side (client or server) has closed the connection.
                 * By default, the OS sends a SIGPIPE signal to your process when this happens, which if unhandled causes your program to terminate.
                 * If you ignore SIGPIPE (e.g., with signal(SIGPIPE, SIG_IGN)), the send() call returns -1 with errno set to EPIPE, allowing your program to handle the error gracefully without crashing.
                 */

                /**
                 * ECONNRESET (Connection reset by peer)
                 * Means that the connection was forcibly closed by the remote side.
                 * It usually occurs when the remote client TCP stack abruptly closes the connection or crashes.
                 * When you try to read or write on such a socket, the kernel informs you by returning an error with errno set to ECONNRESET.
                 * This is a common error in network programming and indicates that the peer closed the socket unexpectedly.
                 */
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    log_errno("relay_response: client disconnected (errno=%d)", errno);
                    return -1;
                }
                log_errno("relay_response: send failed");
                return -1;
            }
            /**
             *  According to POSIX and common socket programming practice, documented in StackOverflow, a read() or recv() returning 0 means:
             *  - Remote peer closed connection normally
             *  - No error, just that no bytes were read" because the remote socket was closed gracefully
             *  - Do not consider it an error; treat it as connection end.
             */
            if (bytes_sent == 0)
            {
                log_error("relay_response: send returned 0 bytes");
                return -1;
            }

            total_sent += bytes_sent;
        }
    }
    printf("DEBUG: relay_response done\n");
    return 0;
}