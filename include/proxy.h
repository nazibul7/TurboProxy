#pragma once

int connect_to_target(char *host, int port);
int forward_request(int targetFd, char *request_data,int length);
int relay_response(int targetFd, int clientFd);