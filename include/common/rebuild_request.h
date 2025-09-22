#pragma once
#include "common/http_types.h" // For HttpRequest struct

// Maximum size for rebuilt request
#define MAX_REQUEST_SIZE 8192

/**
 * @file rebuild_request.h
 * @brief Rebuild HTTP request for forwarding to target backend
 *
 * @param req Parsed HTTPrequest from client
 * @param buffer Buffer to store rebuilt request
 * @param client_ip - Client IP address for X-Forwarded-For header
 * @param buffer_size Size of buffer
 * @return 0 on success & -1 on failure
 */

ssize_t rebuild_request(HttpRequest *req, char *buffer,char *client_ip ,size_t buffer_size);

/**
 * @brief Function to get client IP from socket
 * 
 * @param client_fd Client socket file descriptor
 * @param ip_buffer Buffer to store IP address
 * @param buffer_size Size of IP buffer
 * @return 0 on success & -1 on failure
 */

int get_client_ip(int client_fd, char *ip_buffer, size_t buffer_size);