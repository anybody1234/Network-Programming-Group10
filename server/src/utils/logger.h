#ifndef LOGGER_H
#define LOGGER_H

void protocol_set_current_request_for_log(const char *request_line);
void protocol_clear_current_request_for_log(void);
void protocol_log_result(int client_sock, const char *result);

#endif
