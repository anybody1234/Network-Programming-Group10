#include "auth_service.h"
#include "../config/db.h"
#include "../utils/net_utils.h"
#include <string.h>
#include <sys/select.h>
#ifndef MAX_CLIENTS
#define MAX_CLIENTS FD_SETSIZE
#endif
extern ClientSession client_sessions[MAX_CLIENTS];
void handle_register(int sockfd, cJSON *json_req, MYSQL *db_conn)
{
    cJSON *user_item = cJSON_GetObjectItem(json_req, "username");
    cJSON *pass_item = cJSON_GetObjectItem(json_req, "password");
    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item))
    {
        send_json_response(sockfd, 400, "Missing username or password");
        return;
    }
    if (db_register_account(db_conn, user_item->valuestring, pass_item->valuestring) == 1)
    {
        send_json_response(sockfd, 201, "Registration successful");
    }
    else if (db_register_account(db_conn, user_item->valuestring, pass_item->valuestring) == -1)
    {
        send_json_response(sockfd, 500, "Database error");
    }
    else
    {
        send_json_response(sockfd, 409, "Username exists");
    }
}

void handle_login(struct ClientSession *session, cJSON *json_req, MYSQL *db_conn)
{
    cJSON *user_item = cJSON_GetObjectItem(json_req, "username");
    cJSON *pass_item = cJSON_GetObjectItem(json_req, "password");
    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item))
    {
        send_json_response(session->sockfd, 400, "Missing username or password");
        return;
    }
    if (session->is_logged_in)
    {
        send_json_response(session->sockfd, 409, "You are already logged in");
        return;
    }
    int result = db_login_account(db_conn, user_item->valuestring, pass_item->valuestring);
    if (result > 0)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_sessions[i].sockfd != -1 &&
                client_sessions[i].is_logged_in &&
                client_sessions[i].user_id == result)
            {

                send_json_response(session->sockfd, 409, "Account is already logged in on another device");
                return;
            }
        }
        session->is_logged_in = 1;
        session->user_id = result;
        session->failed_login_count = 0;
        strncpy(session->username, user_item->valuestring, sizeof(session->username) - 1);
        session->username[sizeof(session->username) - 1] = '\0';
        send_json_response(session->sockfd, 200, "Login successful");
    }
    else if (result == -1)
    {
        send_json_response(session->sockfd, 405, "Account is locked");
    }
    else
    {
        session->failed_login_count++;
        if (session->failed_login_count < 5)
        {
            send_json_response(session->sockfd, 407, "Invalid username or password");
        }
        else
        {
            int lock_res = db_lock_account(db_conn, user_item->valuestring);
            if (lock_res == 1)
            {
                send_json_response(session->sockfd, 405, "Account has been permanently locked after too many failed attempts");
            }
            else
            {
                send_json_response(session->sockfd, 500, "Database error while locking account");
            }
        }
    }
}

void handle_client_disconnection(ClientSession *session)
{
    if (session && session->sockfd != -1)
    {
        printf("Client disconnected: socket %d, user: %s\n", session->sockfd,
               session->is_logged_in ? session->username : "(not logged in)");
    }
}