#include "logger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#ifndef LOG_FILE_NAME
#define LOG_FILE_NAME "server_log.txt"
#endif

static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static __thread const char *tls_current_request = NULL;

void protocol_set_current_request_for_log(const char *request_line)
{
    tls_current_request = request_line;
}

void protocol_clear_current_request_for_log(void)
{
    tls_current_request = NULL;
}

static void sanitize_for_log(char *s)
{
    if (!s)
        return;
    for (char *p = s; *p; p++)
    {
        if (*p == '\n' || *p == '\r')
            *p = ' ';
        if (*p == '$')
            *p = '_';
    }
}

static void get_client_ip_str(int sock, char *out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return;

    struct sockaddr_storage addr;
    socklen_t len = (socklen_t)sizeof(addr);
    out[0] = '\0';

    if (getpeername(sock, (struct sockaddr *)&addr, &len) != 0)
    {
        snprintf(out, out_sz, "unknown");
        return;
    }

    if (addr.ss_family != AF_INET)
    {
        snprintf(out, out_sz, "unknown");
        return;
    }

    struct sockaddr_in *a = (struct sockaddr_in *)&addr;
    if (!inet_ntop(AF_INET, &a->sin_addr, out, out_sz))
        snprintf(out, out_sz, "unknown");
}

static void write_activity_log_line(int client_sock, const char *request, const char *result)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%d/%m/%Y %H:%M:%S", &tm_now);

    char ip[64];
    get_client_ip_str(client_sock, ip, sizeof(ip));

    char req_buf[512];
    char res_buf[512];
    snprintf(req_buf, sizeof(req_buf), "%s", request ? request : "");
    snprintf(res_buf, sizeof(res_buf), "%s", result ? result : "");

    sanitize_for_log(req_buf);
    sanitize_for_log(res_buf);

    pthread_mutex_lock(&g_log_mutex);

    FILE *f = fopen(LOG_FILE_NAME, "a");
    if (f)
    {
        fprintf(f, "[%s]$%s$%s$%s\n", time_buf, ip, req_buf, res_buf);
        fclose(f);
    }

    pthread_mutex_unlock(&g_log_mutex);
}

void protocol_log_result(int client_sock, const char *result)
{
    write_activity_log_line(client_sock, tls_current_request, result);
}
