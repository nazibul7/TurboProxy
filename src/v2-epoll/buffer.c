#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <v2-epoll/buffer.h>
#include <common/error_handler.h>

int buffer_init(buffer_t *buf)
{
    if (!buf)
    {
        log_error("buffer_init: buf is NULL");
        return -1;
    }
    buf->data = buf->small_buf;
    buf->size = SMALL_BUFFER_SIZE;
    buf->len = 0;
    buf->offset = 0;
    buf->is_dynamic = false;

    memset(buf->small_buf, 0, sizeof(buf->small_buf));

    return 0;
}

void buffer_cleanup(buffer_t *buf)
{
    if (!buf)
    {
        log_error("buffer_cleanup: buf is NULL");
        return;
    }
    if (buf->is_dynamic && buf->data && buf->data != buf->small_buf)
    {
        free(buf->data);
        buf->data = NULL;
    }

    buf->size = 0;
    buf->len = 0;
    buf->offset = 0;
    buf->is_dynamic = false;
}

int buffer_ensure_space(buffer_t *buf, size_t needed)
{
    if (!buf)
    {
        log_error("buffer_ensure_space: buf is NULL");
        return -1;
    }

    size_t free_space = buf->size - buf->len;

    /**Already enough free space → nothing to do */
    if (free_space >= needed)
    {
        return 0;
    }

    size_t new_size = buf->size * 2;

    /**Compute new capacity (grow exponentially, minimum = len + needed) */

    if (new_size < buf->len + needed)
    {
        new_size = buf->len + needed;
    }

    if (!buf->is_dynamic)
    {
        /**Case 1: Currently using inline small_buf → migrate to heap */

        /**
         * Here, new_size is a number of bytes you want, but sizeof(new_size) is just the size of the type size_t (usually 8 on 64-bit).
         * So instead of allocating new_size bytes, you always allocate 8 bytes, which causes buffer overflows.
         * I should wrote char *new_data = malloc(new_size); not char *new_data = malloc(sizeof(new_size));
         */
        char *new_data = malloc(new_size);
        if (!new_data)
        {
            log_errno("buffer_ensure_space: Dynamic allocation failed");
            return -1;
        }

        /**Copy existing data from inline buffer */
        memcpy(new_data, buf->data, buf->len);
        memset(new_data + buf->len, 0, new_size - buf->len);

        buf->data = new_data;
        buf->size = new_size;
        buf->is_dynamic = true;
    }
    else
    {
        /**Case 2: Already dynamic → realloc */
        char *new_data = realloc(buf->data, new_size);
        if (!new_data)
        {
            log_errno("buffer_ensure_space: Dynamic reallocation failed");
            return -1;
        }
        buf->data = new_data;
        buf->size = new_size;
    }
    return 0;
}

size_t buffer_available_space(const buffer_t *buf)
{
    if (buf->size > buf->len)
    {
        return buf->size - buf->len;
    }
    return 0;
}

char *buffer_write_ptr(const buffer_t *buf)
{
    return buf->data + buf->len;
}

size_t buffer_available_data(const buffer_t *buf)
{
    return buf->len - buf->offset;
}

char *buffer_read_ptr(const buffer_t *buf)
{
    return buf->data + buf->offset;
}