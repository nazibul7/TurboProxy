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
        log_error("rebuild_request: invalid arguments (req=%p, buffer=%p, size=%zu)",
                  req, buffer, buffer_size);
        return -1;
    }
    size_t write_pos = 0;
    int written = 0;

    /**
     * snprint_f return The number of characters that would have been written if the buffer were big enough, excluding the terminating \0.
     *
     * Why perfect for HTTP Header-
     * - Headers are always text strings
     * - Need formatting: "Host: %s\r\n" becomes "Host: example.com\r\n"
     * - Safe bounds checking prevents overflow
     */

    // Write request line: METHOD PATH VERSION
    written = snprintf(buffer, buffer_size, "%s %s HTTP/1.1\r\n", req->methode, req->path);
    if (written < 0)
    {
        log_errno("rebuild_request: snprintf failed at request line");
        return -1;
    }
    if ((size_t)written >= buffer_size - write_pos)
    {
        log_error("rebuild_request: buffer too small while writing request line (needed=%zu, available=%zu)",
                  (size_t)written, buffer_size - write_pos);
        return -1;
    }

    write_pos += written;

    // Adding all headers
    for (int i = 0; i < req->header_count; i++)
    {
        written = snprintf(buffer + write_pos, buffer_size - write_pos, "%s: %s\r\n", req->Headers[i].key, req->Headers[i].value);
        if (written < 0 || (size_t)written >= buffer_size - write_pos)
        {

            log_error("rebuild_request: buffer too small while adding header '%s: %s' (needed=%d, available=%zu)",
                      req->Headers[i].key, req->Headers[i].value, written, buffer_size - write_pos);
            return -1;
        }
        write_pos += written;
    }

    // Add Connection header either close || keep-alive
    written = snprintf(buffer + write_pos, buffer_size - write_pos, "Connection: close\r\n");
    if (written < 0 || (size_t)written >= buffer_size - write_pos)
    {
        log_error("rebuild_request: buffer too small while adding 'Connection: close' header");
        return -1;
    }

    write_pos += written;

    // Add X-Forwarded-For header
    written = snprintf(buffer + write_pos, buffer_size - write_pos, "X-Forwarded-For: %s\r\n", client_ip);
    if (written < 0 || (size_t)written >= buffer_size - write_pos)
    {
        log_error("rebuild_request: buffer too small while adding 'X-Forwarded-For' header");
        return -1;
    }
    write_pos += written;

    // Add Content-Length header,if body is present
    if (req->body && req->body_length)
    {
        written = snprintf(buffer + write_pos, buffer_size - write_pos,
                           "Content-Length: %zu\r\n", req->body_length);
        if (written < 0 || (size_t)written >= buffer_size - write_pos)
        {
            log_error("rebuild_request: buffer too small while adding 'Content-Length' header");
            return -1;
        }
        write_pos += written;
    }

    // End header
    written = snprintf(buffer + write_pos, buffer_size - write_pos, "\r\n");
    if (written < 0 || (size_t)written >= buffer_size - write_pos)
    {
        log_error("rebuild_request: buffer too small while ending headers");
        return -1;
    }
    write_pos += written;

    // Add body
    /*
     * WHY memcpy() INSTEAD OF snprintf() FOR BODY:
     *
     * 1. BINARY DATA SAFETY:
     *    - HTTP body can contain images, files, binary data
     *    - These may have embedded null bytes (\0) that would truncate snprintf
     *    - Example: JPEG file data contains many \0 bytes
     *
     * 2. FORMAT SPECIFIER PROTECTION:
     *    - User input might contain "%s", "%d", etc.
     *    - snprintf would try to interpret these as format specifiers
     *    - Example: JSON like {"message": "Hello %s"} would cause undefined behavior
     *
     * 3. EXACT BYTE PRESERVATION:
     *    - memcpy copies exactly req->body_length bytes
     *    - No interpretation, no null-termination issues
     *    - What goes in is exactly what comes out
     *
     * 4. PERFORMANCE:
     *    - memcpy is optimized for bulk data copying
     *    - No format parsing overhead like snprintf
     */
    if (req->body && req->body_length)
    {
        if (write_pos + req->body_length >= buffer_size)
        {
            log_error("rebuild_request: buffer too small while copying body (needed=%zu, available=%zu)",
                      write_pos + req->body_length, buffer_size);
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
        log_errno("get_client_ip: getpeername failed for fd=%d", client_fd);
        strncpy(ip_buffer, "127.0.0.1", buffer_size - 1);
        ip_buffer[buffer_size - 1] = '\0';
        return -1;
    }
    const char *ip = inet_ntoa(addr.sin_addr);
    if (!ip)
    {
        log_error("get_client_ip: inet_ntoa returned NULL for fd=%d", client_fd);
        strncpy(ip_buffer, "127.0.0.1", buffer_size - 1);
        ip_buffer[buffer_size - 1] = '\0';
        return -1;
    }
    strncpy(ip_buffer, ip, buffer_size - 1);
    ip_buffer[buffer_size - 1] = '\0';
    return 0;
}