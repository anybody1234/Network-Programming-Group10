#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "route.h"
#include "../utils/net_utils.h"
#include "../utils/logger.h"
#include <cjson/cJSON.h>
#include "../service/auth_service.h"
#include "../service/room_service.h"
void route_request(ClientSession *client, char *json_str, MYSQL *db_conn)
{
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        protocol_set_current_request_for_log("INVALID_JSON");
        send_json_response(client->sockfd, 400, "Invalid JSON format");
        protocol_clear_current_request_for_log();
        return;
    }

    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (!cJSON_IsString(type_item))
    {
        send_json_response(client->sockfd, 400, "Missing request type");
        cJSON_Delete(json);
        protocol_clear_current_request_for_log();
        return;
    }
    char *type = type_item->valuestring;

    /* build a sanitized request string for logging (avoid sensitive fields) */
    char san[256];
    san[0] = '\0';
    snprintf(san, sizeof(san), "%s", type);
    cJSON *u = cJSON_GetObjectItem(json, "username");
    if (cJSON_IsString(u))
    {
        strncat(san, "|user:", sizeof(san) - strlen(san) - 1);
        strncat(san, u->valuestring, sizeof(san) - strlen(san) - 1);
    }
    cJSON *r = cJSON_GetObjectItem(json, "room_id");
    if (cJSON_IsNumber(r))
    {
        char numbuf[32];
        snprintf(numbuf, sizeof(numbuf), "|room:%d", r->valueint);
        strncat(san, numbuf, sizeof(san) - strlen(san) - 1);
    }
    cJSON *q = cJSON_GetObjectItem(json, "question_id");
    if (cJSON_IsNumber(q))
    {
        char numbuf[32];
        snprintf(numbuf, sizeof(numbuf), "|q:%d", q->valueint);
        strncat(san, numbuf, sizeof(san) - strlen(san) - 1);
    }
    cJSON *h = cJSON_GetObjectItem(json, "history_id");
    if (cJSON_IsNumber(h))
    {
        char numbuf[32];
        snprintf(numbuf, sizeof(numbuf), "|history:%d", h->valueint);
        strncat(san, numbuf, sizeof(san) - strlen(san) - 1);
    }

    protocol_set_current_request_for_log(san);

    printf("[DEBUG] User '%s'| Id '%d': %s\n", client->is_logged_in ? client->username : "Guest", client->user_id, san);
    if (strcmp(type, "REGISTER") == 0)
    {
        handle_register(client->sockfd, json, db_conn);
    }
    else if (strcmp(type, "LOGIN") == 0)
    {
        handle_login(client, json, db_conn);
    }
    else if (strcmp(type, "LOGOUT") == 0)
    {
        if (client->is_logged_in)
        {
            client->is_logged_in = 0;
            client->user_id = -1;
            client->username[0] = '\0';
            send_json_response(client->sockfd, 202, "Logout successful");
        }
        else
        {
            send_json_response(client->sockfd, 401, "Not logged in");
        }
    }
    else if (strcmp(type, "CREATE_ROOM") == 0)
    {
        handle_create_room(client, json, db_conn);
    }
    else if (strcmp(type, "LIST_ROOMS") == 0)
    {
        handle_list_rooms(client, db_conn);
    }
    else if (strcmp(type, "UPDATE_ANSWER") == 0)
    {
        handle_update_answers(client, json, db_conn);
    }
    else if (strcmp(type, "JOIN_ROOM") == 0)
    {
        handle_join_room(client, json, db_conn);
    }
    else if (strcmp(type, "SUBMIT_EXAM") == 0)
    {
        handle_submit_exam(client, json, db_conn);
    }
    else if (strcmp(type, "LIST_SCORE_ROOMS") == 0)
    {
        handle_list_score_rooms(client, json, db_conn);
    }
    else if (strcmp(type, "GET_ROOM_SCORES") == 0)
    {
        handle_get_room_scores(client, json, db_conn);
    }
    else if (strcmp(type, "PRACTICE_START") == 0)
    {
        handle_practice_start(client, json, db_conn);
    }
    else if (strcmp(type, "PRACTICE_SUBMIT") == 0)
    {
        handle_practice_submit(client, json, db_conn);
    }
    else
    {
        send_json_response(client->sockfd, 400, "Unknown request type");
    }
    cJSON_Delete(json);
    protocol_clear_current_request_for_log();
}