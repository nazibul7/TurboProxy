#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include "common/error_handler.h"

/**
 * void log_error(const char *msg) {
 *  fprintf(stderr, "[ERROR] %s\n", msg);
 * }
 *
 * int main() {
 *  int config_loaded = 0;
 *  if (!config_loaded) {
 *      log_error("Configuration file missing or invalid");
 *   }
 *  return 0;
 * }
 *
 * @output-
 * - [ERROR] Configuration file missing or invalid
 *
 * Logic errors
 * Internal program failures
 * Not related to any errno or system call.
 * for application-level logic or sanity checks.
 */

void log_error(const char *fmt, ...)
{
    va_list args;
    fprintf(stderr, "[ERROR] ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

/**
 * Difference between errno vs error
 *
 * void log_errno(const char *msg) {
 *   fprintf(stderr, "[ERRNO] %s: %s\n", msg, strerror(errno));
 * }
 *
 * int main() {
 *   FILE *f = fopen("nonexistent.txt", "r");
 *   if (!f) {
 *       log_errno("Failed to open file");
 *   }
 *   return 0;
 * }
 *
 * @Output
 * - [ERRNO] Failed to open file: No such file or directory
 *
 * errno is a global variable set automatically by system calls on failure.
 * strerror(errno) turns it into a readable string.
 * This tells u what & why of the error
 */

void log_errno(const char *fmt, ...)
{
    va_list args;
    fprintf(stderr, "[ERROR] ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, ": %s\n", strerror(errno));
}

void send_http_error(int client_fd, int status_code, const char *message)
{
    char response[512];
    int len = snprintf(response, sizeof(response),
                       "HTTP/1.1 %d %s\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       status_code, message, strlen(message), message);

    // Check for buffer overflow
    if (len < 0 || (size_t)len >= sizeof(response))
    {
        log_error("send_http_error: HTTP response too large for buffer");
        return;
    }

    ssize_t total_sent = 0;
    while (total_sent < len)
    {
        ssize_t bytes_sent = send(client_fd, response + total_sent, len - total_sent, 0);
        if (bytes_sent < 0)
        {
            if (errno == EINTR) // Interrupted, retry
                continue;
            if (errno == EPIPE || errno == ECONNRESET)
            {
                log_errno("send_http_error: Client %d disconnected (EPIPE or ECONNRESET)", client_fd);
            }
            else
            {
                log_errno("send_http_error: Failed to send response to client %d", client_fd);
            }
            return;
        }
        /**
         *  According to POSIX and common socket programming practice, documented in StackOverflow, a read() or recv() returning 0 means:
         *  - Remote peer closed connection normally
         *  - No error, just that no bytes were read" because the remote socket was closed gracefully
         *  - Do not consider it an error; treat it as connection end.
         */
        if (bytes_sent == 0)
        {
            log_error("send_http_error: send() returned 0 bytes, connection closed by client %d", client_fd);
            return;
        }
        total_sent += bytes_sent;
    }
}