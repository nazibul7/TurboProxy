#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "common/server.h"
#include "common/error_handler.h"

int setup_server(int port)
{
    int server_id;
    struct sockaddr_in address;

    // creating a socket

    server_id = socket(AF_INET, SOCK_STREAM, 0);
    if (server_id < 0)
    {
        log_errno("Socket creation failed");
        return -1;
    }

    // Add this block here to reuse the port immediately
    int opt = 1;
    if (setsockopt(server_id, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        log_errno("setsockopt failed");
        close(server_id);
        return -1;
    }

    // initalizing memory for address variable

    memset(&address, 0, sizeof(address));

    // intialize address structer

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    // despit define address as specific structure struct sockaddr_in for IPv4 address, bind fucntion expect to work with multiple protocols, they work with the generic struct sockaddr type.thats why type casted in bind function
    if (bind(server_id, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        log_errno("Bind failed on port %d", port);
        close(server_id);
        return -1;
    }

    // listening function
    /**
     * listen(fd, backlog) and net.core.somaxconn:
     *
     * The `listen(fd, n)` system call sets up a socket to accept incoming connections.
     * Its second parameter, `backlog`, defines the maximum number of connections
     * that can be queued for acceptance.
     * Tells kernel to allow up to n fully established connections waiting in Accept Queue or backlog queue.
     *
     * ─────────────────────────────────────────────────────────────────────
     * However, the actual queue length is capped by the kernel's limit:
     *   → /proc/sys/net/core/somaxconn
     *
     * 📌 Formula:
     *   actual_backlog = min(backlog_passed_by_app, somaxconn_limit)
     *
     * 🔍 Example:
     *   - If you set:
     *       listen(fd, 4096);
     *   - And somaxconn is:
     *       128  → then actual backlog = 128
     *       1024 → then actual backlog = 1024
     *       8192 → then actual backlog = 4096 (since app asked only 4096)
     *
     * 🔧 Check or update `somaxconn`:
     *   Temporary:
     *       sysctl -w net.core.somaxconn=1024
     *
     *   Permanent (edit /etc/sysctl.conf):
     *       net.core.somaxconn = 1024
     *
     *   Check current:
     *       cat /proc/sys/net/core/somaxconn
     *
     * ⚠️ Note:
     *   Setting a high backlog in `listen()` without increasing `somaxconn`
     *   will NOT increase the actual queue length.
     *
     *
     * If your system's somaxconn = 1024, but you call:
     * listen(fd, 128);
     * Then your server can only hold 128 connections in the accept queue.
     * The kernel uses the minimum of the two.
     * you must increase both
     */

    if (listen(server_id, 512) < 0)
    {
        log_errno("Listening failed");
        close(server_id);
        return -1;
    }
    return server_id;
}

int accept_client(int server_id)
{
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int client_id = accept(server_id, (struct sockaddr *)&client_addr, (socklen_t *)&len);
    if (client_id < 0)
    {

        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return -1; //"No connection available" - not an error
        }
        log_errno("accept failed");
        return -1;
    }
    else
    {
        printf("Connection from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    }
    return client_id;
}