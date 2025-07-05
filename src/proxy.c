#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include "proxy.h"

int connect_to_target(char *host, int port)
{
    char *colon = strchr(host, ':');
    if (colon)
    {
        *colon = '\0'; // terminate string at ':'
    }
    struct hostent *server = gethostbyname(host);
    if (server == NULL)
    {
        printf("Error: No such host: %s\n", host);
        return -1;
    }
    struct sockaddr_in target_addr;
    int socket_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0)
    {
        printf("OK");
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
        perror("Failed to connect to target");
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
            else
            {
                return -1;
            }
        }
        if (sent == 0) // No progress? Connection broken
        {
            break;
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
    char buffer[4096];
    ssize_t bytes_read;

    while ((bytes_read = recv(targetFd, buffer, sizeof(buffer), 0)) > 0)
    {
        if (bytes_read < 0)
        {
            if (errno == EINTR)
                continue;
            else
                return -1;
        }
        ssize_t total_sent = 0;
        while (total_sent < bytes_read)
        {
            ssize_t bytes_sent = send(clientFd, buffer + total_sent, bytes_read - total_sent, 0);
            if (bytes_sent < 0)
            {
                if (errno == EINTR)
                    continue;
                perror("send");
                return -1;
            }

            if (bytes_sent == 0)
            {
                // Unusual: send sent 0 bytes
                fprintf(stderr, "send returned 0 bytes\n");
                return -1;
            }

            total_sent += bytes_sent;
        }
    }
    return 0;
}