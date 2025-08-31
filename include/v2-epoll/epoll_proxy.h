
/**
 * @brief Connects to specified non-blockin target backend host & port
 * 
 * @param host The host name or IP address of backend server
 * @param port Port number to connect to
 * @return Socket file descriptor success & -1 on error
 */

int connect_to_target_nb(char *host, int port, int epoll_fd);