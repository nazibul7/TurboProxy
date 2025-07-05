#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "server.h"
#include "request_parser.h"
#include "proxy.h"

#define PORT 7000
#define BUFFER_SIZE 1024

int main()
{
    int server_id, client_id;
    char buffer[BUFFER_SIZE];
    server_id = setup_server(PORT);
    if (server_id < 0)
    {
        perror("Failed to start server");
        return 1;
    }
    printf("Server is listening on port %d\n", PORT);

    while (1)
    {
        client_id = accept_client(server_id);
        if (client_id < 0)
            continue;
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client_id, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0)
        {
            close(client_id);
            continue;
        }

        printf("Received request:\n%s\n", buffer);
        HttpRequest req;
        if (parse_http_request(buffer, &req) != 0)
        {
            fprintf(stderr, "Failed to parse request\n");
            close(client_id);
            continue;
        }
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

        char *host_value = NULL;
        for (int i = 0; i < req.header_count; i++)
        {
            if (strcasecmp(req.Headers[i].key, "Host") == 0)
            {
                host_value = req.Headers[i].value;
                break;
            }
        }

        if (!host_value)
        {
            fprintf(stderr, "No Host header found!\n");
            close(client_id);
            continue;
        }

        int targetfd = connect_to_target(host_value, 80);
        
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
            close(client_id);
            continue;
        }
        char request[1024];
        snprintf(request, sizeof(request),
                 "GET / HTTP/1.1\r\n"
                 "Host: httpbin.org\r\n"
                 "User-Agent: curl/7.81.0\r\n"
                 "Accept: */*\r\n"
                 "Connection: close\r\n"
                 "\r\n");

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

            // Close both sockets
            close(targetfd);
            close(client_id);
            continue; // Skip to next client
        }
        if (relay_response(targetfd, client_id) < 0)
        {
            fprintf(stderr, "Failed to relay response to client\n");
            close(targetfd);
            close(client_id);
            continue;
        }

        close(client_id);
    }
    close(server_id);
    return 0;
}
