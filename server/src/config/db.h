#ifndef DB_H
#define DB_H

#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#define DB_SERVER "127.0.0.1"
#define DB_USER "sinhvien"
#define DB_PASS "123456"
#define DB_NAME "quiz_app"
typedef struct Room
{
    int id;
    char room_name[100];
    int num_questions;
    int duration_minutes;
    char start_time[64];
    int status; // 0: Chưa bắt đầu, 1: Đang thi, 2: Kết thúc
    char host_name[50];
    char *questions_data;
} Room;

typedef struct RoomBrief
{
    int id;
    char room_name[100];
} RoomBrief;

typedef struct RoomScoreItem
{
    char username[50];
    int score;
    int status; // 0: in progress/not submitted, 1: submitted
} RoomScoreItem;
MYSQL *db_connect();
void db_close(MYSQL *conn);

int db_register_account(MYSQL *conn, const char *username, const char *password);
int db_login_account(MYSQL *conn, const char *username, const char *password);
int db_lock_account(MYSQL *conn, const char *username);

int db_total_questions(MYSQL *conn);
int db_create_room(MYSQL *conn, int user_id, const char *room_name, int num_questions, int duration, const char *start_time_str);
int db_generate_questions_for_room(MYSQL *conn, int room_id, int num_questions);

int db_get_all_rooms(MYSQL *conn, Room **rooms);

Room db_get_room_info(MYSQL *conn, int room_id);
int db_check_is_participant(MYSQL *conn, int room_id, int user_id);
int db_join_room(MYSQL *conn, int user_id, int room_id);
char *db_get_user_answers_json(MYSQL *conn, int room_id, int user_id);

int db_get_starting_rooms(MYSQL *conn, Room **rooms);
void db_update_room_status(MYSQL *conn, int room_id, int new_status);
int db_get_room_participants(MYSQL *conn, int room_id, int **user_ids);

int db_update_user_answers(MYSQL *conn, int room_id, int user_id, const char *question_id, const char *answer);
int db_recalculate_score(MYSQL *conn, int room_id, int user_id);

int db_get_ending_rooms(MYSQL *conn, Room **rooms);

int db_finalize_user_exam(MYSQL *conn, int room_id, int user_id);
int db_check_is_participant_score(MYSQL *conn, int room_id, int user_id);

int db_get_rooms_by_host(MYSQL *conn, int host_id, RoomBrief **rooms);
int db_get_rooms_taken_by_user(MYSQL *conn, int user_id, RoomBrief **rooms);
int db_is_room_host(MYSQL *conn, int room_id, int host_id);
int db_get_user_score_in_room(MYSQL *conn, int room_id, int user_id, int *score_out);
int db_get_room_scores(MYSQL *conn, int room_id, RoomScoreItem **items);

int db_validate_question_id(MYSQL *conn, int room_id, const char *question_id);

int db_generate_questions_for_practice(MYSQL *conn, int user_id, int num_questions, int duration, cJSON **out_questions);
int db_submit_practice_result(MYSQL *conn, int history_id, int user_id, cJSON *user_answers, int *out_score, int *out_total, int *out_is_late);

#endif