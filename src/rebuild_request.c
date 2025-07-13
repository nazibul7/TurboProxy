#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "rebuild_request.h"

int rebuild_request(HttpRequest *req, char *buffer, char *client_ip, size_t buffer_size)
{
    // validate input parameter
    if (!req || !buffer || buffer_size == 0)
    {
        return -1;
    }
    size_t write_pos = 0;
    int written = 0;
    
    // snprint_f return The number of characters that would have been written if the buffer were big enough, excluding the terminating \0.
    
    // Write request line: METHOD PATH VERSION
    written = snprintf(buffer, buffer_size, "%s %s HTTP/1.1\r\n", req->methode, req->path);
    if (written < 0 || (size_t)written >= buffer_size - write_pos)
    return -1;
    
    write_pos += written;
    
    // Adding all headers
    for (int i = 0; i < req->header_count; i++)
    {
        written = snprintf(buffer + write_pos, buffer_size - write_pos, "%s: %s\r\n", req->Headers[i].key, req->Headers[i].value);
        if (written < 0 || (size_t)written >= buffer_size - write_pos)
        return -1;
        write_pos += written;
    }
    
    // Add Connection header either close || keep-alive
    written = snprintf(buffer + write_pos, buffer_size - write_pos, "Connection: close\r\n");
    if (written < 0 || (size_t)written >= buffer_size - write_pos)
    return -1;

    write_pos += written;
    
    // Add X-Forwarded-For header
    written = snprintf(buffer + write_pos, buffer_size - write_pos, "X-Forwarded-For: %s\r\n", client_ip);
    if (written < 0 || (size_t)written >= buffer_size - write_pos)
    {
        return -1;
    }
    write_pos += written;
    
    // End header
    written = snprintf(buffer + write_pos, buffer_size - write_pos, "\r\n");
    if (written < 0 || (size_t)written >= buffer_size - write_pos)
    {
        return -1;
    }
    write_pos += written;
    
    // Add body
    
    if (req->body && req->body_length)
    {
        if (write_pos + req->body_length >= buffer_size)
        {
            return -1;
        }
        memcpy(buffer + write_pos, req->body, req->body_length);
        write_pos += req->body_length;
    }
    return 0;
}

int get_client_ip(int client_fd, char *ip_buffer, size_t buffer_size)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    // Fill addr with actual client address info
    if (getpeername(client_fd, (struct sockaddr *)&addr, &addr_len) != 0)
    {
        // fallback if getpeername fails
        strncpy(ip_buffer, "127.0.0.1", buffer_size - 1);
        ip_buffer[buffer_size - 1] = '\0';
        return -1;
    }
    const char *ip = inet_ntoa(addr.sin_addr);
    strncpy(ip_buffer, ip, buffer_size - 1);
    ip_buffer[buffer_size - 1] = '\0';
    return 0;
}