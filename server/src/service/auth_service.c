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
        send_json_response(sockfd, 400, "Bad Request: Missing username or password");
        return;
    }

    const char *username = user_item->valuestring;
    const char *password = pass_item->valuestring;
    if (!username || !password || username[0] == '\0' || password[0] == '\0')
    {
        send_json_response(sockfd, 400, "Bad Request: Username and password cannot be empty");
        return;
    }

    int reg_res = db_register_account(db_conn, username, password);
    if (reg_res == 1)
    {
        send_json_response(sockfd, 201, "Registration successful");
        return;
    }
    if (reg_res == -2)
    {
        send_json_response(sockfd, 409, "Username exists");
        return;
    }
    send_json_response(sockfd, 500, "Database error");
}

void handle_login(struct ClientSession *session, cJSON *json_req, MYSQL *db_conn)
{
    cJSON *user_item = cJSON_GetObjectItem(json_req, "username");
    cJSON *pass_item = cJSON_GetObjectItem(json_req, "password");
    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item))
    {
        send_json_response(session->sockfd, 400, "Bad Request: Missing username or password");
        return;
    }

    const char *username = user_item->valuestring;
    const char *password = pass_item->valuestring;
    if (!username || !password || username[0] == '\0' || password[0] == '\0')
    {
        send_json_response(session->sockfd, 400, "Bad Request: Username and password cannot be empty");
        return;
    }

    if (session->is_logged_in)
    {
        send_json_response(session->sockfd, 409, "You are already logged in");
        return;
    }

    int result = db_login_account(db_conn, username, password);
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
        strncpy(session->username, username, sizeof(session->username) - 1);
        session->username[sizeof(session->username) - 1] = '\0';
        send_json_response(session->sockfd, 200, "Login successful");
        return;
    }

    if (result == -1)
    {
        send_json_response(session->sockfd, 423, "Account is locked");
        return;
    }

    if (result < 0)
    {
        send_json_response(session->sockfd, 500, "Database error");
        return;
    }

    session->failed_login_count++;
    if (session->failed_login_count < 5)
    {
        send_json_response(session->sockfd, 401, "Invalid username or password");
        return;
    }

    int lock_res = db_lock_account(db_conn, username);
    if (lock_res == 1)
    {
        send_json_response(session->sockfd, 423, "Account has been locked after too many failed attempts");
        return;
    }
    send_json_response(session->sockfd, 500, "Database error while locking account");
}

void handle_client_disconnection(ClientSession *session)
{
    if (session && session->sockfd != -1)
    {
        printf("Client disconnected: socket %d, user: %s\n", session->sockfd,
               session->is_logged_in ? session->username : "(not logged in)");
    }
}