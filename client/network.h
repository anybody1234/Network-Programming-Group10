#ifndef NETWORK_H
#define NETWORK_H
#include "globals.h"

int connect_to_server();
void send_json_request(cJSON *req);
void *recv_thread_func(void *arg);

#endif