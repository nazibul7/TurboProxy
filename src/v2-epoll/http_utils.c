#define _GNU_SOURCE 
#include <string.h>
#include <stdlib.h>
#include <v2-epoll/http_utils.h>

bool http_request_complete(const buffer_t *buf)
{
    if (!buf || buffer_available_data(buf) == 0)
    {
        return false;
    }

    /**Get pointer of readbale data */
    char *data = buffer_read_ptr(buf);

    /**Find end of headers (double CRLF) */
    const char *headers_end = strstr(data, "\r\n\r\n");
    if (!headers_end)
    {
        printf("Headers is not completed\n");
        return false;
    }

    size_t header_len = (headers_end - data) + 4; // include "\r\n\r\n"

    /**If request has body, check Content-Length */

    const char *content_length_start = strcasestr(data, "content-length:");
    if (content_length_start)
    {
        /**
         * atoi just converts a string to int
         * "27" → 27 ✅
         * " 27" → 27 ✅
         * "abc27" → 0 ❌ (hard to debug!)
         *
         * Since the string begins with "Content-Length", not digits, atoi will stop immediately.
         * That's why add strlen(content-length:). Which is 15
         */
        const char *p = content_length_start + 15;
        while (*p == ' ' || *p == '\t')
            p++;
        size_t content_length = atoi(p);
        
        size_t body_received = strlen(data) - header_len;

        /**Check if we have all the body data */
        if (body_received < content_length)
        {
            return false;
        }
    }

    return true;
}
