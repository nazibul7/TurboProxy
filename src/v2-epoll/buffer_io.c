#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <v2-epoll/buffer_io.h>
#include <common/error_handler.h>

/**
 * Here I have used while loop to read all data at once In LT mode because,
 * I knew in LT mode it will give me notification about data available,
 * but with while loop it will reduce epoll wakeups call & sys call by draining the buffer socket in one go.
 *
 *
 * IMPORTANT CHANGE: Return -2 for EOF to distinguish from errors
 * -2 = EOF (graceful connection close)
 * -1 = Actual error
 * >= 0 = Bytes read
 */

ssize_t buffer_read_from_fd(buffer_t *buf, int fd)
{
    ssize_t total_bytes_read = 0;
    ssize_t bytes_read;
    while (1)
    {
        /**Ensure buffer has space available */
        if (buffer_ensure_space(buf, 4096) != 0)
        {
            log_error("buffer_read_from_fd: Failed to ensure buffer space or out of memory");
            return total_bytes_read > 0 ? total_bytes_read : -1;
        }

        /**Get pointer to writable in buffer */
        char *write_ptr = buffer_write_ptr(buf);
        size_t available_space = buffer_available_space(buf);
        if (!write_ptr || available_space == 0)
        {
            log_error("buffer_read_from_fd: no write space in buffer");
            return 0;
        }

        bytes_read = recv(fd, write_ptr, available_space, 0);

        if (bytes_read < 0)
        {
            /**Interrupted, retry */
            if (errno == EINTR)
                continue;

            /**For READ, you get: ECONNRESET, but not EPIPE */
            else if (errno == ECONNRESET)
            {
                log_errno("buffer_read_from_fd: Client disconnected (ECONNRESET)");
                return -1;
            }

            /**Non-blocking: no data available */
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /**Return total bytes read, or 0 if nothing was read */
                return total_bytes_read > 0 ? total_bytes_read : 0;
            }
            else
            {
                log_errno("buffer_read_from_fd: recv failed");
                return -1;
            }
        }

        /**
         *  According to POSIX and common socket programming practice, documented in StackOverflow, a read() or recv() returning 0 means:
         *  - Remote peer closed connection normally
         *  - No error, just that no bytes were read" because the remote socket was closed gracefully
         *  - Do not consider it an error; treat it as connection end. Just cleanup the connection.
         *  - In HTTP, EOF (connection close) handling depends on whether it's a keep-alive
         *  - connection or a connection that should be closed.
         */
        if (bytes_read == 0)
        {
            printf("buffer_read_from_fd: Client fd %d sent EOF\n", fd);
            return total_bytes_read > 0 ? total_bytes_read : -2;
        }

        /**Continue loop to read more data until EAGAIN */
        buf->len += bytes_read;
        total_bytes_read += bytes_read;
    }
}

ssize_t buffer_write_to_fd(buffer_t *buf, int fd)
{
    ssize_t total_bytes_sent = 0;

    while (1)
    {
        /**Get readable data from buffer */
        char *read_ptr = buffer_read_ptr(buf);
        size_t data_to_send = buffer_available_data(buf);

        if (!read_ptr || data_to_send == 0)
        {
            return total_bytes_sent > 0 ? total_bytes_sent : 0;
        }

        ssize_t sent = send(fd, read_ptr, data_to_send, 0);
        if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /** Kernel send buffer is full → wait for EPOLLOUT*/
                return total_bytes_sent > 0 ? total_bytes_sent : 0;
            }
            else if (errno == EINTR)
            {
                /** Interrupted, let epoll handle retry*/
                return total_bytes_sent > 0 ? total_bytes_sent : 0;
            }
            else
            {
                if (errno == ECONNRESET)
                {
                    log_errno("send_to_backend: Backend disconnected(ECONNRESET)");
                }
                else if (errno == EPIPE)
                {
                    log_error("send_to_backend: backend closed connection (EPIPE)");
                }
                else
                {
                    log_errno("send_to_backend: fatal send() error");
                }
                return -1;
            }
        }
        /**
         * recv() == 0 → EOF, close connection gracefully.
         * send() == 0 → treat as error, close and cleanup socket.
         */
        else if (sent == 0)
        {
            log_errno("send returned 0 - connection may be closed or problematic");
            return total_bytes_sent > 0 ? total_bytes_sent : -2;
        }
        buf->offset += sent;
        total_bytes_sent += sent;
    }
}