#include "net_utils.h"

#include "logger.h"

void send_json_response(int sockfd, int status, const char *message)
{
    cJSON *resp_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp_json, "status", status);
    cJSON_AddStringToObject(resp_json, "message", message);

    char *json_str = cJSON_PrintUnformatted(resp_json);
    size_t len = strlen(json_str);
    char *msg_buffer = (char *)malloc(len + 3);
    if (msg_buffer)
    {
        sprintf(msg_buffer, "%s\r\n", json_str);
        send(sockfd, msg_buffer, strlen(msg_buffer), 0);
        free(msg_buffer);
    }

    char result[768];
    snprintf(result, sizeof(result), "%d|%s", status, message ? message : "");
    protocol_log_result(sockfd, result);

    free(json_str);
    cJSON_Delete(resp_json);
}

void send_cjson_packet(int sockfd, cJSON *json)
{
    int status = 0;
    const char *message = "";
    if (json)
    {
        cJSON *status_item = cJSON_GetObjectItem(json, "status");
        if (cJSON_IsNumber(status_item))
            status = status_item->valueint;
        cJSON *msg_item = cJSON_GetObjectItem(json, "message");
        if (cJSON_IsString(msg_item) && msg_item->valuestring)
            message = msg_item->valuestring;
    }

    char *json_str = cJSON_PrintUnformatted(json);
    if (!json_str)
        return;

    size_t len = strlen(json_str);
    char *packet = (char *)malloc(len + 3);
    if (packet)
    {
        sprintf(packet, "%s\r\n", json_str);
        send(sockfd, packet, strlen(packet), MSG_NOSIGNAL);
        free(packet);
    }
    free(json_str);

    char result[768];
    if (message && message[0] != '\0')
        snprintf(result, sizeof(result), "%d|%s", status, message);
    else
        snprintf(result, sizeof(result), "%d", status);
    protocol_log_result(sockfd, result);
}