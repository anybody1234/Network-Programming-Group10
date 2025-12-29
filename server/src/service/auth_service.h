#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <cjson/cJSON.h>
#include <mysql/mysql.h>
#define BUFFER_SIZE 16384

typedef struct ClientSession
{
    int sockfd;
    int user_id;
    int is_logged_in;
    char username[50];
    char buffer[BUFFER_SIZE];
    int buf_len;
    int failed_login_count;
} ClientSession;

void handle_register(int sockfd, cJSON *json_req, MYSQL *db_conn);
void handle_login(ClientSession *session, cJSON *json_req, MYSQL *db_conn);
void handle_client_disconnection(ClientSession *session);
#endif