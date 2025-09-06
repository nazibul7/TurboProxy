#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

#define SMALL_BUFFER_SIZE 1024

/**
 * @brief Dynamic + small inline buffer abstraction
 *
 * This buffer implementation uses hybrid approach: small data stored inline in
 * the structure to avoid heap allocation, while larger data automatically move
 * to dynamic allocation for better performance.
 *
 * Reason for this Hybrid approach --
 * - For small data instead of doing malloca() for every connection , we just store data in
 *   the fixed-size inline buffer (on the stack/inside the struct).
 *   This avoids heap allocations → faster, less memory fragmentation.
 * - If the inline buffer isn’t enough, you allocate (malloc/realloc) and mark is_dynamic = true
 *   This gives flexibility without being limited by the inline size.
 *
 * Automatically adapts to the workload.
 */

typedef struct buffer
{
    char *data;                        /**< Pointer to actual buffer memory (either small_buf or malloc'd) */
    size_t size;                       /**< Total capacity of buffer */
    size_t len;                        /**< Number of bytes currently stored */
    size_t offset;                     /**< Number of bytes already consumed (start of unread data) */
    bool is_dynamic;                   /**< True if malloc was used */
    char small_buf[SMALL_BUFFER_SIZE]; /**< Inline buffer for small data */
} buffer_t;

/* -------------------------------------------------------------------------
 * Buffer Management
 * ------------------------------------------------------------------------- */

/**
 * @brief Initalize a buffer.
 *
 * Starts with inline storage (small_buf). No allocation is done initially.
 *
 * @param buf Pointer to buffer object.
 * @return 0 on success, -1 on error.
 */
int buffer_init(buffer_t *buf);

/**
 * @brief Free any dynamically allocated memory.
 *
 * Resets the buffer back to an uninitialized state.
 * @param buf Pointer to buffer object.
 */
void buffer_cleanup(buffer_t *buf);

/* -------------------------------------------------------------------------
 * Capacity Management
 * ------------------------------------------------------------------------- */

/**
 * @brief Ensure buffer has enough free space for upcoming data.
 *
 * Expands dynamically if needed.
 *
 * @param buf Pointer to buffer object.
 * @param needed Minimum free space required (bytes).
 * @return 0 on success, -1 on allocation failure.
 */
int buffer_ensure_space(buffer_t *buf, size_t needed);

/**
 * @brief Compact the buffer by moving unread data to the front.
 *
 * Moves unread data to the beginning of the buffer and resets offset to 0.
 * @param buf Pointer to buffer object.
 */

/**
 * Before compact (after consuming 6 bytes):
 *
 * data: [H][e][l][l][o][ ][W][o][r][l][d][?][?][?]
 *                      ^               ^           ^
 *                   offset=6        len=11      size=14
 *
 * After buffer_compact():
 * data: [W][o][r][l][d][?][?][?][?][?][?][?][?][?]
 *       ^               ^                       ^
 *    offset=0        len=5                   size=14
 */
void buffer_compact(buffer_t *buf);

/* -------------------------------------------------------------------------
 * Data Operations
 * ------------------------------------------------------------------------- */

/**
 * @brief Append data to buffer.
 *
 * Expands buffer if necessary.
 *
 * @param buf Pointer to buffer object.
 * @param data Data source pointer.
 * @param len Length of data to append.
 * @return 0 on success, -1 on allocation failure.
 */
int buffer_append(buffer_t *buf, const void *data, size_t len);

/**
 * @brief Mark bytes as consumed. Advances the offset to mark data as consumed. Does not actually
 * remove data from memory until buffer_compact() is called.
 *
 * @param buf Pointer to buffer object.
 * @param bytes Number of bytes to consume.
 */

/**
 * Before compact (after consuming 6 bytes):
 *
 * data: [H][e][l][l][o][ ][W][o][r][l][d]
 *        ^                               ^
 *     offset=0                        len=11
 *
 *
 * After buffer_consume(buf, 6):
 * data: [H][e][l][l][o][ ][W][o][r][l][d]
 *                         ^               ^
 *                      offset=6        len=11
 *                      (consumed)    (available data: "World")
 */
void buffer_consume(buffer_t *buf, size_t bytes);

/* -------------------------------------------------------------------------
 * Query Functions
 * ------------------------------------------------------------------------- */

/**
 * @brief Get number of bytes available for reading.
 *
 * @param buf Pointer to buffer object.
 * @return Count of readable bytes. Returns the amount of data that can be read from the buffer
 * (i.e., len - offset).
 */
size_t buffer_available_data(const buffer_t *buf);

/**
 * @brief Get number of bytes of free space in buffer
 *
 * @param buf Pointer to buffer object.
 * @return Count of free space (size - len).
 */
size_t buffer_available_space(const buffer_t *buf);

/**
 * @brief Get pointer to unread data in buffer.
 *
 * @param buf Pointer to buffer object.
 * @return Returns a pointer to the start of unread data in the buffer.
 */
char *buffer_read_ptr(const buffer_t *buf);