#pragma once

/**
 * @file proxy.h
 * @brief Connects to specified target backend host & port
 * 
 * @param host The host name or IP address of backend server
 * @param port Port number to connect to
 * @return Socket file descriptor 0 on success & -1
 */

int connect_to_target(char *host, int port);


/**
 * @brief Sends client request data to connected target backend
 * 
 * @param targetFd File descriptor of target backend
 * @param request_data Buffer containing the HTTP request 
 * @param length Length of request data in Bytes
 * @return Number of bytes sent or -1 on failure
 */
int forward_request(int targetFd, char *request_data,int length);

/**
 * @brief Relay the response to client
 * 
 * @param targetFd File descriptor of target backend
 * @param clientFd File descriptor of client
 * @return Number of bytes relayed or -1 on fialure.
 */
int relay_response(int targetFd, int clientFd);