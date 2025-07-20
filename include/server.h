#pragma once

/**
 * @file server.h
 * @brief Declares functions for setting up a server socket and accepting client connections.
 */

/**
 * @brief Creates and binds a TCP server socket to the specified port.
 *
 * @param port The port number to bind the server socket to.
 * @return Server socket file descriptor on success, -1 on failure.
 */
int setup_server(int port);

/**
 * @brief Accepts a new client connection on the server socket.
 *
 * @param server_id File descriptor of the server socket returned by setup_server.
 * @return Client socket file descriptor on success, -1 on failure.
 */
int accept_client(int server_id);