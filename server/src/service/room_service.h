#ifndef ROOM_SERVICE_H
#define ROOM_SERVICE_H
#include <cjson/cJSON.h>
#include <mysql/mysql.h>
#include "auth_service.h"

void handle_create_room(ClientSession *session, cJSON *req_json, MYSQL *db_conn);
void handle_list_rooms(ClientSession *session, MYSQL *db_conn);
void handle_update_answers(ClientSession *session, cJSON *req_json, MYSQL *db_conn);
void handle_join_room(ClientSession *session, cJSON *req_json, MYSQL *db_conn);
void handle_submit_exam(ClientSession *session, cJSON *req_json, MYSQL *db_conn);

void handle_list_score_rooms(ClientSession *session, cJSON *req_json, MYSQL *db_conn);
void handle_get_room_scores(ClientSession *session, cJSON *req_json, MYSQL *db_conn);

void handle_practice_start(ClientSession *session, cJSON *req, MYSQL *conn);
void handle_practice_submit(ClientSession *session, cJSON *req, MYSQL *conn);
#endif