#ifndef LOGGER_H
#define LOGGER_H

// Simple project-wide activity log.
// Each line format:
// [dd/mm/yyyy HH:MM:SS]$<ip>$<request>$<result>
// Example:
// [20/12/2025 15:00:11]$127.0.0.1$LOGIN|huyen|1234$200|Login successful

void protocol_set_current_request_for_log(const char *request_line);
void protocol_clear_current_request_for_log(void);

// Log a result string for the current request set via protocol_set_current_request_for_log.
void protocol_log_result(int client_sock, const char *result);

#endif
