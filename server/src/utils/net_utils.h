#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <cjson/cJSON.h>

void send_json_response(int sockfd, int status, const char *message);

#endif