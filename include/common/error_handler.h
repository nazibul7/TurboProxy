#pragma once

/**
 * @file error_handler.h
 * @brief Send an HTTP error response to the client.
 * @param client_fd file descriptor of client
 * @param status_code HTTP status code to send(e.g., 404,500,502)
 * @param message Describing the error
 */
void send_http_error(int client_fd, int status_code, const char *message);

/**
 * @brief Log an error message with the corresponding errno description.
 * @param fmt Format string (like printf).
 * @param ... Additional arguments matching the format string.
 */
void log_errno(const char *fmt, ...);

/**
 * @brief Log a general error message to stderr.
 * @param fmt Format string (like printf).
 * @param ... Additional arguments matching the format string.
 */
void log_error(const char *fmt, ...);
