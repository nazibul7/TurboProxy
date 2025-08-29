#include <stdio.h>

#define SMALL_BUFFER_SIZE 1024
/**
 *
 */

typedef struct buffer
{
    char *data;                        /**< Pointer to actual buffer memory */
    size_t size;                       /**< Total capacity of buffer */
    size_t len;                        /**< Number of bytes currently stored */
    size_t offset;                     /**< Number of bytes already consumed */
    bool is_dynamic;                   /**< True if malloc was used */
    char small_buf[SMALL_BUFFER_SIZE]; /**< Inline buffer for small data */
} buffer_t;

/* Buffer Management */
int buffer_init(buffer_t *buf);
void buffer_cleanup(buffer_t *buf);

/* Capacity Management */
int buffer_ensure_space(buffer_t *buf, size_t needed);
void buffer_compact(buffer_t *buf);

/* Data Operations */
int buffer_append(buffer_t *buf, const void *data, size_t len);
void buffer_consume(buffer_t *buf, size_t bytes);

/* File Descriptor I/O */
ssize_t buffer_read_from_fd(buffer_t *buf, int fd);
ssize_t buffer_write_to_fd(buffer_t *buf, int fd);

/* Query Functions */
size_t buffer_available_data(const buffer_t *buf);  /**< Bytes ready to read */
size_t buffer_available_space(const buffer_t *buf); /**< Free space left */
char *buffer_read_ptr(const buffer_t *buf);         /**< Pointer to unread data */