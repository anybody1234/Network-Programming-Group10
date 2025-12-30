#define _XOPEN_SOURCE 700

#include "room_service.h"
#include "../config/db.h"
#include "../utils/net_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void handle_create_room(ClientSession *session, cJSON *req_json, MYSQL *db_conn)
{
    if (!session->is_logged_in)
    {
        send_json_response(session->sockfd, 401, "Unauthorized: Please log in first");
        return;
    }
    cJSON *room_name_item = cJSON_GetObjectItem(req_json, "room_name");
    cJSON *num_item = cJSON_GetObjectItem(req_json, "num_questions");
    cJSON *dur_item = cJSON_GetObjectItem(req_json, "duration");
    cJSON *start_item = cJSON_GetObjectItem(req_json, "start_time");

    if (!cJSON_IsString(room_name_item) || !cJSON_IsNumber(num_item) || !cJSON_IsNumber(dur_item) || !cJSON_IsString(start_item))
    {
        send_json_response(session->sockfd, 400, "Bad Request: Missing or invalid parameters");
        return;
    }
    char *room_name = room_name_item->valuestring;
    int num_questions = num_item->valueint;
    int duration = dur_item->valueint;
    char *start_time = start_item->valuestring;
    if (num_questions <= 0 || duration <= 0)
    {
        send_json_response(session->sockfd, 400, "Invalid number of questions or duration");
        return;
    }
    struct tm tm_start = {0};
    if (strptime(start_time, "%Y-%m-%d %H:%M:%S", &tm_start) == NULL)
    {
        send_json_response(session->sockfd, 400, "Invalid date format. Use: YYYY-MM-DD HH:MM:SS");
        return;
    }
    time_t t_start = mktime(&tm_start);
    time_t t_now = time(NULL);

    if (t_start == -1)
    {
        send_json_response(session->sockfd, 400, "Invalid time value");
        return;
    }
    if (t_start < t_now)
    {
        send_json_response(session->sockfd, 400, "Start time cannot be in the past");
        return;
    }
    int total_questions = db_total_questions(db_conn);
    if (total_questions < 0)
    {
        send_json_response(session->sockfd, 500, "Database error checking questions");
        return;
    }
    if (total_questions < num_questions)
    {
        send_json_response(session->sockfd, 400, "Not enough questions in the database");
        return;
    }
    int room_id = db_create_room(db_conn, session->user_id, room_name, num_questions, duration, start_time);
    if (room_id <= 0)
    {
        send_json_response(session->sockfd, 500, "Could not create room (Database error)");
        return;
    }
    if (db_generate_questions_for_room(db_conn, room_id, num_questions) != 1)
    {
        send_json_response(session->sockfd, 500, "Could not generate questions for the room");
        return;
    }
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "CREATE_ROOM_SUCCESS");
    cJSON_AddNumberToObject(resp, "status", 201);
    cJSON_AddNumberToObject(resp, "room_id", room_id);
    cJSON_AddStringToObject(resp, "message", "Room created successfully");
    send_cjson_packet(session->sockfd, resp);
    cJSON_Delete(resp);
}

void handle_list_rooms(ClientSession *session, MYSQL *db_conn)
{
    if (!session->is_logged_in)
    {
        send_json_response(session->sockfd, 401, "Unauthorized");
        return;
    }
    Room *rooms = NULL;
    int count = db_get_all_rooms(db_conn, &rooms);
    if (count < 0)
    {
        send_json_response(session->sockfd, 500, "Database Error");
        return;
    }
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "status", 200);
    cJSON_AddStringToObject(resp, "type", "ROOM_LIST");
    cJSON *rooms_array = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
    {
        cJSON *r_item = cJSON_CreateObject();
        cJSON_AddNumberToObject(r_item, "id", rooms[i].id);
        cJSON_AddStringToObject(r_item, "room_name", rooms[i].room_name);
        cJSON_AddNumberToObject(r_item, "num_questions", rooms[i].num_questions);
        cJSON_AddNumberToObject(r_item, "duration", rooms[i].duration_minutes);
        cJSON_AddStringToObject(r_item, "start_time", rooms[i].start_time);
        cJSON_AddNumberToObject(r_item, "status", rooms[i].status);
        cJSON_AddStringToObject(r_item, "host_name", rooms[i].host_name);

        cJSON_AddItemToArray(rooms_array, r_item);
    }
    cJSON_AddItemToObject(resp, "rooms", rooms_array);
    send_cjson_packet(session->sockfd, resp);
    cJSON_Delete(resp);
    if (rooms)
        free(rooms);
}

void handle_join_room(ClientSession *session, cJSON *req_json, MYSQL *db_conn)
{
    if (!session->is_logged_in)
    {
        send_json_response(session->sockfd, 401, "Unauthorized");
        return;
    }
    cJSON *r_item = cJSON_GetObjectItem(req_json, "room_id");
    if (!cJSON_IsNumber(r_item))
    {
        send_json_response(session->sockfd, 400, "Invalid room ID");
        return;
    }
    int room_id = r_item->valueint;
    Room room = db_get_room_info(db_conn, room_id);
    if (room.id == -1)
    {
        send_json_response(session->sockfd, 404, "Room not found");
        return;
    }
    int is_participant = db_check_is_participant(db_conn, room_id, session->user_id);
    int is_participant_score = db_check_is_participant_score(db_conn, room_id, session->user_id);
    if (room.status == 0)
    {
        if (!is_participant)
        {
            if (db_join_room(db_conn, session->user_id, room_id) != 1)
            {
                send_json_response(session->sockfd, 500, "Could not join room (Database error)");
            }
        }
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "status", 200);
        cJSON_AddStringToObject(resp, "type", "JOIN_SUCCESS");
        cJSON_AddNumberToObject(resp, "room_id", room_id);
        cJSON_AddStringToObject(resp, "message", "Joined room. Waiting for host to start...");
        send_cjson_packet(session->sockfd, resp);
        cJSON_Delete(resp);
    }
    else if (room.status == 1)
    {
        printf("[INFO] User %d entering RUNNING room %d\n", session->user_id, room_id);
        if (!is_participant)
        {
            send_json_response(session->sockfd, 403, "Forbidden: You are not a participant of this room");
            if (room.questions_data)
            {
                free(room.questions_data);
            }
            return;
        }
            if (is_participant_score != -1)
            {
                char msg[128];
                sprintf(msg, "Forbidden: You have already submitted the exam with score recorded with score: %d", is_participant_score);
                send_json_response(session->sockfd, 409, msg);
                if (room.questions_data)
                {
                    free(room.questions_data);
                }
                return;
            }
        time_t start_epoch = (time_t)atoll(room.start_time);
        time_t now = time(NULL);
        int elapsed = (int)(now - start_epoch);
        int remaining = (room.duration_minutes * 60) - elapsed;
        if (remaining <= 0)
        {
            send_json_response(session->sockfd, 400, "Exam time is over");
            if (room.questions_data)
                free(room.questions_data);
            return;
        }
        char *saved_answers_str = db_get_user_answers_json(db_conn, room_id, session->user_id);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", "EXAM_START");
            cJSON_AddNumberToObject(resp, "status", 200);
        cJSON_AddNumberToObject(resp, "room_id", room_id);
        cJSON_AddStringToObject(resp, "message", "Resuming exam...");
        cJSON_AddNumberToObject(resp, "duration", room.duration_minutes * 60);
        cJSON_AddNumberToObject(resp, "remaining", remaining);
        if (room.questions_data)
        {
            cJSON *q_json = cJSON_Parse(room.questions_data);
            if (q_json)
            {
                cJSON_AddItemToObject(resp, "questions", q_json);
            }
        }
        if (saved_answers_str)
        {
            cJSON *ans_json = cJSON_Parse(saved_answers_str);
            cJSON_AddItemToObject(resp, "saved_answers", ans_json);
            free(saved_answers_str);
        }
        else
        {
            cJSON_AddObjectToObject(resp, "saved_answers");
        }
        send_cjson_packet(session->sockfd, resp);
        cJSON_Delete(resp);
    }
    else
    {
        send_json_response(session->sockfd, 410, "The exam has ended");
    }
    if (room.questions_data)
    {
        free(room.questions_data);
    }
}

void handle_update_answers(ClientSession *session, cJSON *req_json, MYSQL *db_conn)
{
    if (!session->is_logged_in)
    {
        send_json_response(session->sockfd, 401, "Unauthorized");
        return;
    }
    cJSON *r_item = cJSON_GetObjectItem(req_json, "room_id");
    cJSON *q_item = cJSON_GetObjectItem(req_json, "question_id");
    cJSON *a_item = cJSON_GetObjectItem(req_json, "answer");
    if (!cJSON_IsNumber(r_item) || !a_item)
    {
        return;
    }
    int room_id = r_item->valueint;
    char *answer = a_item->valuestring;
    char q_id_str[32];
    if (cJSON_IsNumber(q_item))
    {
        sprintf(q_id_str, "%d", q_item->valueint);
    }
    else if (cJSON_IsString(q_item))
    {
        snprintf(q_id_str, sizeof(q_id_str), "%s", q_item->valuestring);
    }
    else
    {
        return;
    }
    if (!db_validate_question_id(db_conn, room_id, q_id_str))
    {
        printf("[WARN] User %d tried to answer invalid Question ID: %s in Room %d\n",
               session->user_id, q_id_str, room_id);
        send_json_response(session->sockfd, 400, "Invalid Question ID");
        return;
    }
    if (db_update_user_answers(db_conn, room_id, session->user_id, q_id_str, answer) == 1)
    {
        printf("[INFO] User %d updated Q%s = %s\n", session->user_id, q_id_str, answer);
        if (db_recalculate_score(db_conn, room_id, session->user_id) == 1)
        {
            send_json_response(session->sockfd, 200, "Answer updated and score recalculated");
        }
        else
        {
            send_json_response(session->sockfd, 500, "Answer updated but score recalculation failed");
        }
    }
    else
    {
        printf("[WARN] Update answer failed for User %d (Maybe exam ended?)\n", session->user_id);
    }
}

void handle_submit_exam(ClientSession *session, cJSON *req_json, MYSQL *db_conn)
{
    if (!session->is_logged_in)
    {
        send_json_response(session->sockfd, 401, "Unauthorized");
        return;
    }
    cJSON *r_item = cJSON_GetObjectItem(req_json, "room_id");
    if (!cJSON_IsNumber(r_item))
    {
        send_json_response(session->sockfd, 400, "Invalid room ID");
        return;
    }
    int room_id = r_item->valueint;
    Room room = db_get_room_info(db_conn, room_id);
    if (room.id == -1)
    {
        send_json_response(session->sockfd, 404, "Room not found");
        return;
    }
    int is_participant = db_check_is_participant(db_conn, room_id, session->user_id);
    if (!is_participant)
    {
        send_json_response(session->sockfd, 403, "Forbidden: You are not a participant of this room");
        return;
    }
    if (room.status != 1)
    {
        send_json_response(session->sockfd, 400, "Cannot submit");
        return;
    }
    int score = db_finalize_user_exam(db_conn, room_id, session->user_id);
    if (score != -1)
    {
        char final_score_msg[100];
        snprintf(final_score_msg, sizeof(final_score_msg), "Exam submitted successfully. Final Score recorded: %d", score);
        send_json_response(session->sockfd, 200, final_score_msg);
        printf("[INFO] User %d submitted exam for Room %d\n", session->user_id, room_id);
    }
    else
    {
        send_json_response(session->sockfd, 500, "Could not submit exam (Database error)");
    }
}

static int is_role_host(const char *role)
{
    if (!role)
        return 0;
    return (strcmp(role, "HOST") == 0 || strcmp(role, "host") == 0);
}

void handle_list_score_rooms(ClientSession *session, cJSON *req_json, MYSQL *db_conn)
{
    if (!session->is_logged_in)
    {
        send_json_response(session->sockfd, 401, "Unauthorized");
        return;
    }
    cJSON *role_item = cJSON_GetObjectItem(req_json, "role");
    if (!cJSON_IsString(role_item))
    {
        send_json_response(session->sockfd, 400, "Bad Request: Missing role");
        return;
    }
    const char *role = role_item->valuestring;

    RoomBrief *rooms = NULL;
    int count = 0;
    if (is_role_host(role))
    {
        count = db_get_rooms_by_host(db_conn, session->user_id, &rooms);
    }
    else
    {
        count = db_get_rooms_taken_by_user(db_conn, session->user_id, &rooms);
    }
    if (count < 0)
    {
        send_json_response(session->sockfd, 500, "Database Error");
        return;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "status", 200);
    cJSON_AddStringToObject(resp, "type", "SCORE_ROOM_LIST");
    cJSON_AddStringToObject(resp, "role", role);
    cJSON *rooms_array = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
    {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddNumberToObject(r, "id", rooms[i].id);
        cJSON_AddStringToObject(r, "room_name", rooms[i].room_name);
        cJSON_AddItemToArray(rooms_array, r);
    }
    cJSON_AddItemToObject(resp, "rooms", rooms_array);
    send_cjson_packet(session->sockfd, resp);
    if (rooms)
        free(rooms);
    cJSON_Delete(resp);
}

void handle_get_room_scores(ClientSession *session, cJSON *req_json, MYSQL *db_conn)
{
    if (!session->is_logged_in)
    {
        send_json_response(session->sockfd, 401, "Unauthorized");
        return;
    }
    cJSON *role_item = cJSON_GetObjectItem(req_json, "role");
    cJSON *r_item = cJSON_GetObjectItem(req_json, "room_id");
    if (!cJSON_IsString(role_item) || !cJSON_IsNumber(r_item))
    {
        send_json_response(session->sockfd, 400, "Bad Request: Missing role or room_id");
        return;
    }
    const char *role = role_item->valuestring;
    int room_id = r_item->valueint;

    if (is_role_host(role))
    {
        int is_host = db_is_room_host(db_conn, room_id, session->user_id);
        if (is_host <= 0)
        {
            send_json_response(session->sockfd, 403, "Forbidden: You are not the host of this room");
            return;
        }

        RoomScoreItem *items = NULL;
        int count = db_get_room_scores(db_conn, room_id, &items);
        if (count < 0)
        {
            send_json_response(session->sockfd, 500, "Database Error");
            return;
        }

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "status", 200);
        cJSON_AddStringToObject(resp, "type", "SCORE_ALL");
        cJSON_AddNumberToObject(resp, "room_id", room_id);
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++)
        {
            cJSON *it = cJSON_CreateObject();
            cJSON_AddStringToObject(it, "username", items[i].username);
            cJSON_AddNumberToObject(it, "score", items[i].score);
            cJSON_AddNumberToObject(it, "submit_status", items[i].status);
            cJSON_AddItemToArray(arr, it);
        }
        cJSON_AddItemToObject(resp, "scores", arr);

        send_cjson_packet(session->sockfd, resp);
        if (items)
            free(items);
        cJSON_Delete(resp);
        return;
    }
    else
    {
        int is_participant = db_check_is_participant(db_conn, room_id, session->user_id);
        if (is_participant <= 0)
        {
            send_json_response(session->sockfd, 403, "Forbidden: You are not a participant of this room");
            return;
        }
        int score = 0;
        int got = db_get_user_score_in_room(db_conn, room_id, session->user_id, &score);
        if (got < 0)
        {
            send_json_response(session->sockfd, 500, "Database Error");
            return;
        }
        if (got == 0)
        {
            send_json_response(session->sockfd, 404, "Score not found");
            return;
        }

        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "status", 200);
        cJSON_AddStringToObject(resp, "type", "SCORE_SELF");
        cJSON_AddNumberToObject(resp, "room_id", room_id);
        cJSON_AddNumberToObject(resp, "score", score);

        send_cjson_packet(session->sockfd, resp);
        cJSON_Delete(resp);
        return;
    }
}

void handle_practice_start(ClientSession *session, cJSON *req, MYSQL *conn)
{
    if (!session->is_logged_in)
        return;
    int num = 10;
    int duration = 15;
    cJSON *n_item = cJSON_GetObjectItem(req, "num_questions");
    if (cJSON_IsNumber(n_item))
        num = n_item->valueint;
    cJSON *d_item = cJSON_GetObjectItem(req, "duration");
    if (cJSON_IsNumber(d_item))
        duration = d_item->valueint;

    cJSON *questions = NULL;
    int history_id = db_generate_questions_for_practice(conn, session->user_id, num, duration, &questions);
    if (history_id > 0)
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", "PRACTICE_START_OK");
        cJSON_AddNumberToObject(resp, "status", 200);
        cJSON_AddNumberToObject(resp, "history_id", history_id);
        cJSON_AddNumberToObject(resp, "duration", duration * 60);
        cJSON_AddNumberToObject(resp, "remaining", duration * 60);
        cJSON_AddItemToObject(resp, "questions", questions);
        send_cjson_packet(session->sockfd, resp);
        cJSON_Delete(resp);

        printf("[INFO] User %s started practice %d\n", session->username, history_id);
    }
    else
    {
        send_json_response(session->sockfd, 500, "Failed to create practice session (Not enough questions?)");
    }
}

void handle_practice_submit(ClientSession *session, cJSON *req, MYSQL *conn)
{
    if (!session->is_logged_in)
        return;

    int h_id = cJSON_GetObjectItem(req, "history_id")->valueint;
    cJSON *answers = cJSON_GetObjectItem(req, "answers");

    int score = 0, total = 0, is_late = 0;
    if (db_submit_practice_result(conn, h_id, session->user_id, answers, &score, &total, &is_late))
    {
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "type", "PRACTICE_RESULT");
        cJSON_AddNumberToObject(resp, "status", 200);
        cJSON_AddNumberToObject(resp, "score", score);
        cJSON_AddNumberToObject(resp, "total", total);
        cJSON_AddNumberToObject(resp, "is_late", is_late);
        send_cjson_packet(session->sockfd, resp);
        cJSON_Delete(resp);
        printf("[INFO] User %s submitted practice %d. Score: %d/%d (Late: %d)\n",
               session->username, h_id, score, total, is_late);
    }
    else
    {
        send_json_response(session->sockfd, 500, "Error submitting practice");
    }
}