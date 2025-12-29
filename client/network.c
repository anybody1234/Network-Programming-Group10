#include "network.h"
#include <unistd.h>
#include <arpa/inet.h>
#include "logic.h"
#define BUFF_SIZE 4096
#define RECV_BUF_SIZE 32768

int connect_to_server()
{
    struct sockaddr_in serv_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        return -1;
    }
    return 0;
}

void send_json_request(cJSON *req)
{
    if (sockfd == -1)
        return;
    char *json_str = cJSON_PrintUnformatted(req);
    char packet[BUFF_SIZE];
    snprintf(packet, sizeof(packet), "%s\r\n", json_str);
    send(sockfd, packet, strlen(packet), 0);
    free(json_str);
}

void *recv_thread_func(void *arg)
{
    char buffer[BUFF_SIZE];
    static char acc_buffer[RECV_BUF_SIZE];
    static int acc_len = 0;

    while (1)
    {
        memset(buffer, 0, BUFF_SIZE);
        int n = recv(sockfd, buffer, BUFF_SIZE - 1, 0);
        if (n <= 0)
        {
            printf("\n[DISCONNECT TO SERVER]\n");
            exit(0);
        }

        if (acc_len + n >= RECV_BUF_SIZE)
        {
            acc_len = 0;
            continue;
        }

        memcpy(acc_buffer + acc_len, buffer, n);
        acc_len += n;
        acc_buffer[acc_len] = '\0';

        char *start = acc_buffer;
        char *end;
        while ((end = strstr(start, "\r\n")) != NULL)
        {
            *end = '\0';
            if (strlen(start) > 0)
            {
                process_server_response(start);
            }
            start = end + 2;
        }

        int remaining = acc_len - (start - acc_buffer);
        if (remaining > 0)
        {
            memmove(acc_buffer, start, remaining);
        }
        acc_len = remaining;
        acc_buffer[acc_len] = '\0';
    }
    return NULL;
}