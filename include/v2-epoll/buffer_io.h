#pragma once

#include <v2-epoll/buffer.h>

/* -------------------------------------------------------------------------
 * File Descriptor I/O
 * ------------------------------------------------------------------------- */

/**
 *@brief Read from a file descriptor into into available buffer space.
 *
 * Automatically expands if space is insufficient.
 *
 * @param buf Pointer to buffer object.
 * @param fd File descriptor to read from.
 * @return Number of bytes read, 0 for EOF, or -1 on error.
 */
ssize_t buffer_read_from_fd(buffer_t *buf, int fd);


/**
 * @brief Write buffered data to a file descriptor.
 *
 * Marks written data as consumed.
 *
 * @param buf Pointer to buffer object.
 * @param fd File descriptor to write to.
 * @return Number of bytes written, 0 if nothing to write, or -1 on error.
 */
ssize_t buffer_write_to_fd(buffer_t *buf, int fd);
