#pragma once
#include <stdio.h>

/**
 * @file debug.h
 * @brief Lightweight compile-time debug logging utility.
 *
 * This header defines the DEBUG_PRINT macro.  
 * 
 * When DEBUG_MODE is defined at compile time:
 *      gcc -DDEBUG_MODE ...
 * DEBUG_PRINT prints formatted debug messages to stderr.
 *
 * When DEBUG_MODE is *not* defined:
 *      DEBUG_PRINT(...) expands to an empty do{}while(0) block,
 *      producing zero runtime overhead and zero binary size cost.
 *
 * This is the correct and production-safe way to conditionally
 * include debug logging in high-performance C applications.
 */

#ifdef DEBUG_MODE

/**
 * @brief Print a debug message when DEBUG_MODE is enabled.
 *
 * Usage:
 *     DEBUG_PRINT("Client connected fd=%d", fd);
 *
 * Output example:
 *     [DEBUG] Client connected fd=12
 */
#define DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

#else

/**
 * @brief When DEBUG_MODE is disabled, remove debug logs entirely.
 *
 * DEBUG_PRINT(...) becomes an empty statement, allowing the compiler
 * to optimize away all debug logging code with zero overhead.
 */
#define DEBUG_PRINT(fmt, ...) \
    do {} while (0)

#endif
