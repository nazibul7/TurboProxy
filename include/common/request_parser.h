#pragma once
#include "common/http_types.h" // For HTTP type struct

/**
 * @file request_parser.h
 * @brief Parses a raw HTTP request from a buffer into an HttpRequest structure.
 *
 * @param buffer Raw HTTP request
 * @param req Pointer to a HttpRequest struct where the parsed request fields will be stored or to be populated.
 * @return 0 on success, -1 on failure
 */

int parse_http_request(char *buffer, HttpRequest *req);

/**
 * @brief Frees any dynamically allocated memory in an HttpRequest and resets its fields.
 * @param req Pointer to a HttpRequest struct where the already parsed request fields will cleared.
 */
void free_http_request(HttpRequest *req);