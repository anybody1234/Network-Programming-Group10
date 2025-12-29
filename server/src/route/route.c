#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "route.h"
#include "../utils/net_utils.h"
#include <cjson/cJSON.h>
#include "../service/auth_service.h"
#include "../service/room_service.h"
void route_request(ClientSession *client, char *json_str, MYSQL *db_conn)
{
    printf("[DEBUG] User '%s'| Id '%d': %s\n", client->is_logged_in ? client->username : "Guest", client->user_id, json_str);
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        send_json_response(client->sockfd, 400, "Invalid JSON format");
        return;
    }
    cJSON *type_item = cJSON_GetObjectItem(json, "type");
    if (!cJSON_IsString(type_item))
    {
        send_json_response(client->sockfd, 400, "Missing request type");
        cJSON_Delete(json);
        return;
    }
    char *type = type_item->valuestring;
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
    else {
        send_json_response(client->sockfd, 400, "Unknown request type");
    }
    cJSON_Delete(json);
}