#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include "server.h"
#include "request_parser.h"
#include "proxy.h"
#include "route_config.h"
#include "rebuild_request.h"

#define PORT 8000
#define BUFFER_SIZE 16384

int main()
{
    // Ignore SIGPIPE globally so the server doesn't crash on client disconnects
    signal(SIGPIPE, SIG_IGN);

    int server_id, client_id;
    char buffer[BUFFER_SIZE];
    server_id = setup_server(PORT);
    if (server_id < 0)
    {
        perror("Failed to start server");
        return 1;
    }
    printf("Server is listening on port %d\n", PORT);

    // load routes

    Route routes[MAX_ROUTES];
    int route_count = load_routes("routes.conf", routes, MAX_ROUTES);
    if (route_count <= 0)
    {
        perror("No routes loaded");
        return 1;
    }
    while (1)
    {
        client_id = accept_client(server_id);
        if (client_id < 0)
            continue;

        memset(buffer, 0, BUFFER_SIZE);

        // Initialize variables for this client iteration
        int targetfd = -1;
        bool req_parsed = false;

        int bytes_read = read(client_id, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0)
        {
            goto cleanup;
        }

        printf("Received request:\n%s\n", buffer);
        HttpRequest req;
        if (parse_http_request(buffer, &req) != 0)
        {
            fprintf(stderr, "Failed to parse request\n");
            goto cleanup;
        }
        req_parsed = true;
        // if (strcmp(req.methode, "GET") == 0 && strcmp(req.path, "/") == 0)
        // {
        //     const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 14\r\n\r\nHello, world!\n";
        //     write(client_id, response, strlen(response));
        // }
        // else if (strcmp(req.methode, "POST") == 0 && strcmp(req.path, "/login") == 0)
        // {
        //     // Example: You could parse form data in req.body
        //     const char *response = "HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\nLogged!\n";
        //     write(client_id, response, strlen(response));
        // }
        // else
        // {
        //     const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
        //     write(client_id, response, strlen(response));
        // }
        // printf("KEY %s\n",req.Headers[0].key);
        // printf("VALUE %s\n",req.Headers[0].value);

        // header based proxy

        // char *host_value = NULL;
        // for (int i = 0; i < req.header_count; i++)
        // {
        //     if (strcasecmp(req.Headers[i].key, "Host") == 0)
        //     {
        //         host_value = req.Headers[i].value;
        //         break;
        //     }
        // }

        // if (!host_value)
        // {
        //     fprintf(stderr, "No Host header found!\n");
        //     close(client_id);
        //     continue;
        // }

        // Find best backend based on path prefix
        Route *backend = find_backend(routes, route_count, req.path);
        if (!backend)
        {
            const char *error_response =
                "HTTP/1.1 502 Bad Gateway\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Bad Gateway\n";

            write(client_id, error_response, strlen(error_response));
            goto cleanup;
        }

        printf("Routing to backend: %s:%d for prefix: %s\n", backend->host, backend->port, backend->prefix);

        targetfd = connect_to_target(backend->host, backend->port);

        if (targetfd < 0)
        {
            const char *error_response =
                "HTTP/1.1 502 Bad Gateway\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Bad Gateway\n";

            write(client_id, error_response, strlen(error_response));
            goto cleanup;
        }

        // Get client IP
        char client_ip[16];

        get_client_ip(client_id, client_ip, sizeof(client_ip));

        // // Build the corrected request
        // char request[4096];
        // int len = snprintf(request, sizeof(request),
        //                    "%s %s HTTP/1.1\r\n"
        //                    "Host: %s:%d\r\n"
        //                    "Connection: close\r\n" // Addedd connection close for time being to work properly untill implenting keep-alive
        //                    "\r\n",
        //                    req.methode,   // "GET"
        //                    req.path,      // "/_next/static/css/app/page.css?v=1751791044675"
        //                    backend->host, // "localhost"
        //                    backend->port  // 3000
        // );

        // // Add all original headers except Host
        // for (int i = 0; i < req.header_count; i++)
        // {
        //     if (strcasecmp(req.Headers[i].key, "Host") != 0)
        //     {
        //         len += snprintf(request + len, sizeof(request) - len,
        //                         "%s: %s\r\n", req.Headers[i].key, req.Headers[i].value);
        //     }
        // }

        // // Add terminating \r\n
        // len += snprintf(request + len, sizeof(request) - len, "\r\n");

        // Buffer to build correct request
        char request[MAX_REQUEST_SIZE];

        int build_request = rebuild_request(&req, request, client_ip, sizeof(request));

        if (build_request < 0)
        {
            const char *error_response =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 21\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Internal Server Error";

            write(client_id, error_response, strlen(error_response));
            goto cleanup;
        }

        if (forward_request(targetfd, request, strlen(request)) < 0)
        {
            // Send 502 Bad Gateway back to the client
            const char *error_response =
                "HTTP/1.1 502 Bad Gateway\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Bad Gateway\n";

            write(client_id, error_response, strlen(error_response));
            goto cleanup;
        }

        /*
         * Note:
         * This proxy only handles one request per TCP connection.
         * It force-closes client_fd and target_fd after relay.
         * It must add 'Connection: close' to backend request and client response
         * to avoid keep-alive hangs.
         * See docs/connection-issues.md for details.
         */

        if (relay_response(targetfd, client_id) < 0)
        {
            fprintf(stderr, "Failed to relay response to client\n");
            goto cleanup;
        }

        /**
         * Close backend connection after successful response relay.
         *
         * This is critical because:
         * - Prevents file descriptor leaks (each connect_to_target() opens a new fd)
         * - Avoids backend connection state issues that can affect subsequent requests
         * - Prevents local port exhaustion on the proxy side
         * - Ensures proper cleanup of connection buffers and resources
         *
         * Without this, the proxy will work on first request but fail on subsequent
         * requests due to resource leaks and stale connection state.
         */

        printf("DEBUG: closing backend and client socket\n");

    cleanup:
        // Clean up resources for this client
        if (req_parsed)
        {
            free_http_request(&req);
        }
        if (targetfd != -1)
        {
            close(targetfd);
        }
        if (client_id >= 0)
        {
            close(client_id);
        }
        // Continue to next client
        continue;
    }
    close(server_id);
    return 0;
}
