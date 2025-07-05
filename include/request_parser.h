#pragma once
#include "http_types.h"

int parse_http_request(char *buffer, HttpRequest *req);