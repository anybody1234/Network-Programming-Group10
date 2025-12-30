#include "db.h"
#include <string.h>

MYSQL *db_connect()
{
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL)
    {
        fprintf(stderr, "mysql_init() failed\n");
        return NULL;
    }
    if (mysql_real_connect(conn, DB_SERVER, DB_USER, DB_PASS, DB_NAME, 3306, NULL, 0) == NULL)
    {
        fprintf(stderr, "mysql_real_connect() failed\n");
        mysql_close(conn);
        return NULL;
    }
    printf("Connected to MySQL OK\n");
    return conn;
}
// Function to close the database connection
void db_close(MYSQL *conn)
{
    if (conn != NULL)
    {
        mysql_close(conn);
    }
}

// Function to login an account
int db_login_account(MYSQL *conn, const char *username, const char *password)
{
    char queury[256];
    sprintf(queury, "SELECT status, id FROM users WHERE username='%s' AND password='%s'", username, password);
    if (mysql_query(conn, queury))
    {
        fprintf(stderr, "mysql_query() failed %s\n", mysql_error(conn));
        return -2;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
    {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return -2;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    int result = 0;
    if (row)
    {
        int status = atoi(row[0]);
        int id = atoi(row[1]);
        if (status == 1)
        {
            result = id;
        }
        else
        {
            result = -1;
        }
    }
    mysql_free_result(res);
    return result;
}

int db_register_account(MYSQL *conn, const char *username, const char *password)
{
    char query[256];
    sprintf(query, "INSERT INTO users (username, password, status) VALUES ('%s', '%s', 1)", username, password);
    if (mysql_query(conn, query))
    {
        unsigned int err = mysql_errno(conn);
        if (err == 1062)
        {
            fprintf(stderr, "Duplicate username: %s\n", mysql_error(conn));
            return -2;
        }
        fprintf(stderr, "DB error: %s\n", mysql_error(conn));
        return -1;
    }
    return 1;
}

int db_lock_account(MYSQL *conn, const char *username)
{
    char query[256];
    sprintf(query, "UPDATE users SET status = 0 WHERE username = '%s'", username);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "Failed to lock account: %s\n", mysql_error(conn));
        return -1;
    }
    printf("Account '%s' has been locked\n", username);
    return 1;
}

int db_total_questions(MYSQL *conn)
{
    if (mysql_query(conn, "SELECT COUNT(*) FROM questions"))
    {
        fprintf(stderr, "SELECT COUNT(*) failed. Error: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL)
    {
        fprintf(stderr, "No result: %s\n", mysql_error(conn));
        return -1;
    }
    int total = 0;
    MYSQL_ROW row;
    if ((row = mysql_fetch_row(res)))
    {
        if (row[0] != NULL)
        {
            total = atoi(row[0]);
        }
    }
    mysql_free_result(res);
    return total;
}
int db_create_room(MYSQL *conn, int user_id, const char *room_name, int num_questions, int duration_minutes, const char *start_time)
{
    char query[512];
    sprintf(query, "INSERT INTO rooms (room_name, num_questions, duration_minutes, start_time, status, host_id) "
                   "VALUES ('%s', %d, %d, '%s', 0, %d)",
            room_name, num_questions, duration_minutes, start_time, user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "Create room failed: %s\n", mysql_error(conn));
        return -1;
    }
    int room_id = mysql_insert_id(conn);
    return room_id;
}
cJSON *db_select_random_questions(MYSQL *conn, int num_questions)
{
    char query[256];
    sprintf(query, "SELECT id, content FROM questions ORDER BY RAND() LIMIT %d", num_questions);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "SELECT random questions failed: %s\n", mysql_error(conn));
        return NULL;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
    {
        fprintf(stderr, "mysql_store_result() failed: %s\n", mysql_error(conn));
        return NULL;
    }
    cJSON *questions_map = cJSON_CreateObject();
    MYSQL_ROW row;
    int count = 0;
    while ((row = mysql_fetch_row(res)))
    {
        char *q_id = row[0];
        char *q_content = row[1];
        cJSON *json_content = cJSON_Parse(q_content);
        if (json_content)
        {
            cJSON_AddItemToObject(questions_map, q_id, json_content);
            count++;
        }
    }
    mysql_free_result(res);
    if (count == 0)
    {
        cJSON_Delete(questions_map);
        return NULL;
    }
    return questions_map;
}
int db_generate_questions_for_room(MYSQL *conn, int room_id, int num_questions)
{
    cJSON *questions_map = db_select_random_questions(conn, num_questions);
    if (!questions_map)
    {
        printf("[Warning] Could not fetch questions for room %d\n", room_id);
        return -1;
    }
    char *json_str = cJSON_PrintUnformatted(questions_map);
    unsigned long json_len = strlen(json_str);
    char *escaped_json = (char *)malloc(json_len * 2 + 1);
    mysql_real_escape_string(conn, escaped_json, json_str, json_len);
    char *update_query = (char *)malloc(strlen(escaped_json) + 256);
    sprintf(update_query, "UPDATE rooms SET questions_data = '%s' WHERE id = %d", escaped_json, room_id);
    int success = 1;
    if (mysql_query(conn, update_query))
    {
        fprintf(stderr, "[DB Error] Update room snapshot failed: %s\n", mysql_error(conn));
        success = -1;
    }
    else
    {
        printf("Generated snapshot for room %d successfully.\n", room_id);
    }
    free(update_query);
    free(escaped_json);
    free(json_str);
    cJSON_Delete(questions_map);
    return success;
}

int db_get_all_rooms(MYSQL *conn, Room **rooms)
{
    char query[256];
    sprintf(query, "SELECT r.id, r.room_name, r.num_questions, r.duration_minutes, r.start_time, r.status, u.username "
                   "FROM rooms r "
                   "LEFT JOIN users u ON r.host_id = u.id "
                   "ORDER BY r.id DESC");
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_all_rooms failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    int count = mysql_num_rows(res);
    if (count == 0)
    {
        *rooms = NULL;
        mysql_free_result(res);
        return 0;
    }
    *rooms = (Room *)malloc(sizeof(Room) * count);
    MYSQL_ROW row;
    int idx = 0;
    while ((row = mysql_fetch_row(res)))
    {
        (*rooms)[idx].id = atoi(row[0]);
        strncpy((*rooms)[idx].room_name, row[1] ? row[1] : "", sizeof((*rooms)[idx].room_name) - 1);
        (*rooms)[idx].num_questions = atoi(row[2]);
        (*rooms)[idx].duration_minutes = atoi(row[3]);
        strncpy((*rooms)[idx].start_time, row[4] ? row[4] : "", sizeof((*rooms)[idx].start_time) - 1);
        (*rooms)[idx].status = atoi(row[5]);
        strncpy((*rooms)[idx].host_name, row[6] ? row[6] : "Unknown", sizeof((*rooms)[idx].host_name) - 1);
        idx++;
    }
    mysql_free_result(res);
    return count;
}

Room db_get_room_info(MYSQL *conn, int room_id)
{
    Room room;
    memset(&room, 0, sizeof(Room));
    room.id = -1;
    room.questions_data = NULL;
    char query[512];
    sprintf(query,
            "SELECT id, room_name, num_questions, duration_minutes, status, questions_data, UNIX_TIMESTAMP(start_time) "
            "FROM rooms WHERE id = %d",
            room_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_room_info failed: %s\n", mysql_error(conn));
        return room;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return room;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row)
    {
        room.id = atoi(row[0]);
        if (row[1])
            strncpy(room.room_name, row[1], sizeof(room.room_name) - 1);
        room.num_questions = atoi(row[2]);
        room.duration_minutes = atoi(row[3]);
        room.status = atoi(row[4]);
        if (row[5])
        {
            room.questions_data = strdup(row[5]);
        }
        if (row[6])
        {
            snprintf(room.start_time, sizeof(room.start_time), "%s", row[6]);
        }
    }
    mysql_free_result(res);
    return room;
}
int db_check_is_participant(MYSQL *conn, int room_id, int user_id)
{
    char query[256];
    sprintf(query, "SELECT id FROM room_participants WHERE room_id=%d AND user_id=%d", room_id, user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_check_is_participant failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    int exists = 0;
    if (res)
    {
        if (mysql_num_rows(res) > 0)
            exists = 1;
        mysql_free_result(res);
    }
    return exists;
}
int db_check_is_participant_score(MYSQL *conn, int room_id, int user_id)
{
    char query[256];
    sprintf(query, "SELECT id FROM room_participants WHERE room_id=%d AND user_id=%d AND status = 1", room_id, user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_check_is_participant failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (res)
    {
        if (mysql_num_rows(res) > 0)
        {
            char score_query[256];
            sprintf(score_query, "SELECT score FROM room_participants WHERE room_id=%d AND user_id=%d", room_id, user_id);
            if (mysql_query(conn, score_query))
            {
                fprintf(stderr, "db_check_is_participant failed: %s\n", mysql_error(conn));
                return -1;
            }
            int score = 0;
            MYSQL_RES *score_res = mysql_store_result(conn);
            if (score_res)
            {
                MYSQL_ROW row = mysql_fetch_row(score_res);
                if (row && row[0])
                {
                    score = atoi(row[0]);
                }
                mysql_free_result(score_res);
            }
            mysql_free_result(res);
            return score;
        }
        mysql_free_result(res);
    }
    return -1;
}
int db_join_room(MYSQL *conn, int user_id, int room_id)
{
    char query[256];
    sprintf(query, "INSERT INTO room_participants (room_id, user_id) VALUES (%d, %d)", room_id, user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_join_room failed: %s\n", mysql_error(conn));
        return -1;
    }
    return 1;
}
char *db_get_user_answers_json(MYSQL *conn, int room_id, int user_id)
{
    char query[256];
    sprintf(query, "SELECT user_answers FROM room_participants WHERE room_id=%d AND user_id=%d", room_id, user_id);

    if (mysql_query(conn, query))
        return NULL;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return NULL;

    char *ans = NULL;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0])
    {
        ans = strdup(row[0]);
    }
    mysql_free_result(res);
    return ans;
}

int db_get_starting_rooms(MYSQL *conn, Room **rooms)
{
    char query[512];
    sprintf(query,
            "SELECT id, room_name, num_questions, duration_minutes, status, questions_data "
            "FROM rooms WHERE status = 0 AND start_time <= NOW()");
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_starting_rooms failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    int count = mysql_num_rows(res);
    if (count == 0)
    {
        *rooms = NULL;
        mysql_free_result(res);
        return 0;
    }
    *rooms = (Room *)malloc(sizeof(Room) * count);
    MYSQL_ROW row;
    int i = 0;
    while ((row = mysql_fetch_row(res)))
    {
        (*rooms)[i].id = atoi(row[0]);
        if (row[1])
            strcpy((*rooms)[i].room_name, row[1]);
        (*rooms)[i].num_questions = atoi(row[2]);
        (*rooms)[i].duration_minutes = atoi(row[3]);
        (*rooms)[i].status = atoi(row[4]);
        (*rooms)[i].questions_data = row[5] ? strdup(row[5]) : NULL;
        i++;
    }
    mysql_free_result(res);
    return count;
}
void db_update_room_status(MYSQL *conn, int room_id, int new_status)
{
    char query[128];
    sprintf(query, "UPDATE rooms SET status = %d WHERE id = %d", new_status, room_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "Update status failed: %s\n", mysql_error(conn));
    }
}
int db_get_room_participants(MYSQL *conn, int room_id, int **user_ids, int status_room)
{
    char query[128];
    sprintf(query, "SELECT user_id FROM room_participants WHERE room_id = %d AND status = %d", room_id, status_room);

    if (mysql_query(conn, query))
        return 0;

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return 0;

    int count = mysql_num_rows(res);
    if (count == 0)
    {
        mysql_free_result(res);
        return 0;
    }

    *user_ids = (int *)malloc(sizeof(int) * count);
    MYSQL_ROW row;
    int i = 0;
    while ((row = mysql_fetch_row(res)))
    {
        (*user_ids)[i] = atoi(row[0]);
        i++;
    }
    mysql_free_result(res);
    return count;
}

int db_update_user_answers(MYSQL *conn, int room_id, int user_id, const char *question_id, const char *answer)
{
    char query[512];
    sprintf(query,
            "UPDATE room_participants "
            "SET user_answers = JSON_SET(COALESCE(user_answers, '{}'), '$.\"%s\"', '%s') "
            "WHERE room_id = %d AND user_id = %d AND status = 0",
            question_id, answer, room_id, user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_update_user_answers failed: %s\n", mysql_error(conn));
        return -1;
    }
    if (mysql_affected_rows(conn) == 0)
    {
        return -1;
    }
    return 1;
}

int db_recalculate_score(MYSQL *conn, int room_id, int user_id)
{
    char query[256];
    sprintf(query, "SELECT questions_data FROM rooms WHERE id=%d", room_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_recalculate_score failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res_q = mysql_store_result(conn);
    if (!res_q)
    {
        return -1;
    }
    MYSQL_ROW row_q = mysql_fetch_row(res_q);
    char *questions_json_str = row_q && row_q[0] ? strdup(row_q[0]) : NULL;
    mysql_free_result(res_q);
    if (!questions_json_str)
        return -1;
    char a_query[256];
    sprintf(a_query, "SELECT user_answers FROM room_participants WHERE room_id=%d AND user_id=%d", room_id, user_id);
    if (mysql_query(conn, a_query))
    {
        fprintf(stderr, "db_recalculate_score failed: %s\n", mysql_error(conn));
        free(questions_json_str);
        return -1;
    }
    MYSQL_RES *res_a = mysql_store_result(conn);
    if (!res_a)
    {
        free(questions_json_str);
        return -1;
    }
    MYSQL_ROW row_a = mysql_fetch_row(res_a);
    char *answers_json_str = row_a && row_a[0] ? strdup(row_a[0]) : NULL;
    mysql_free_result(res_a);
    if (!answers_json_str)
    {
        free(questions_json_str);
        return 0;
    }
    int total_score = 0;
    cJSON *q_map = cJSON_Parse(questions_json_str);
    cJSON *a_map = cJSON_Parse(answers_json_str);
    if (q_map && a_map)
    {
        cJSON *answer_item = NULL;
        cJSON_ArrayForEach(answer_item, a_map)
        {
            char *q_id = answer_item->string;
            char *user_choice = answer_item->valuestring;
            cJSON *q_obj = cJSON_GetObjectItem(q_map, q_id);
            if (q_obj)
            {
                cJSON *correct_item = cJSON_GetObjectItem(q_obj, "correct_answer");
                cJSON *weight_item = cJSON_GetObjectItem(q_obj, "score_weight");
                if (correct_item && cJSON_IsString(correct_item))
                {
                    if (strcmp(user_choice, correct_item->valuestring) == 0)
                    {
                        int weight = (weight_item && cJSON_IsNumber(weight_item)) ? weight_item->valueint : 1;
                        total_score += weight;
                    }
                }
            }
        }
    }
    if (q_map)
        cJSON_Delete(q_map);
    if (a_map)
        cJSON_Delete(a_map);
    free(questions_json_str);
    free(answers_json_str);
    char update_query[256];
    sprintf(update_query, "UPDATE room_participants SET score = %d WHERE room_id=%d AND user_id=%d",
            total_score, room_id, user_id);

    if (mysql_query(conn, update_query))
    {
        fprintf(stderr, "Failed to update score: %s\n", mysql_error(conn));
        return -1;
    }
    return 1;
}

int db_get_ending_rooms(MYSQL *conn, Room **rooms)
{
    char query[512];
    sprintf(query,
            "SELECT id, room_name, num_questions, duration_minutes, status, questions_data "
            "FROM rooms WHERE status = 1 AND (UNIX_TIMESTAMP(start_time) + duration_minutes * 60) <= UNIX_TIMESTAMP(NOW())");
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_ending_rooms failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    int count = mysql_num_rows(res);
    if (count == 0)
    {
        *rooms = NULL;
        mysql_free_result(res);
        return 0;
    }
    *rooms = (Room *)malloc(sizeof(Room) * count);
    MYSQL_ROW row;
    int i = 0;
    while ((row = mysql_fetch_row(res)))
    {
        (*rooms)[i].id = atoi(row[0]);
        if (row[1])
            strcpy((*rooms)[i].room_name, row[1]);
        (*rooms)[i].num_questions = atoi(row[2]);
        (*rooms)[i].duration_minutes = atoi(row[3]);
        (*rooms)[i].status = atoi(row[4]);
        (*rooms)[i].questions_data = row[5] ? strdup(row[5]) : NULL;
        i++;
    }
    mysql_free_result(res);
    return count;
}

int db_finalize_user_exam(MYSQL *conn, int room_id, int user_id)
{
    char query[256];
    char score_query[256];
    sprintf(query, "UPDATE room_participants SET status = 1 WHERE room_id=%d AND user_id=%d", room_id, user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_finalize_user_exam failed: %s\n", mysql_error(conn));
        return -1;
    }
    if (mysql_affected_rows(conn) == 0)
    {
        return -1;
    }
    sprintf(score_query, "SELECT score FROM room_participants WHERE room_id=%d AND user_id=%d", room_id, user_id);
    if (mysql_query(conn, score_query))
    {
        fprintf(stderr, "db_finalize_user_exam score fetch failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    MYSQL_ROW row = mysql_fetch_row(res);
    int final_score = 0;
    if (row && row[0])
    {
        final_score = atoi(row[0]);
    }
    mysql_free_result(res);
    printf("User %d finalized exam for room %d with score %d\n", user_id, room_id, final_score);
    return final_score;
}

int db_get_rooms_by_host(MYSQL *conn, int host_id, RoomBrief **rooms)
{
    char query[256];
    sprintf(query, "SELECT id, room_name FROM rooms WHERE host_id=%d ORDER BY id DESC", host_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_rooms_by_host failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    int count = mysql_num_rows(res);
    if (count == 0)
    {
        *rooms = NULL;
        mysql_free_result(res);
        return 0;
    }
    *rooms = (RoomBrief *)malloc(sizeof(RoomBrief) * count);
    if (!*rooms)
    {
        mysql_free_result(res);
        return -1;
    }
    MYSQL_ROW row;
    int idx = 0;
    while ((row = mysql_fetch_row(res)))
    {
        (*rooms)[idx].id = row[0] ? atoi(row[0]) : -1;
        strncpy((*rooms)[idx].room_name, row[1] ? row[1] : "", sizeof((*rooms)[idx].room_name) - 1);
        (*rooms)[idx].room_name[sizeof((*rooms)[idx].room_name) - 1] = '\0';
        idx++;
    }
    mysql_free_result(res);
    return count;
}

int db_get_rooms_taken_by_user(MYSQL *conn, int user_id, RoomBrief **rooms)
{
    char query[512];
    sprintf(query,
            "SELECT r.id, r.room_name "
            "FROM rooms r "
            "JOIN room_participants rp ON rp.room_id = r.id "
            "WHERE rp.user_id=%d AND rp.status=1 "
            "ORDER BY r.id DESC",
            user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_rooms_taken_by_user failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    int count = mysql_num_rows(res);
    if (count == 0)
    {
        *rooms = NULL;
        mysql_free_result(res);
        return 0;
    }
    *rooms = (RoomBrief *)malloc(sizeof(RoomBrief) * count);
    if (!*rooms)
    {
        mysql_free_result(res);
        return -1;
    }
    MYSQL_ROW row;
    int idx = 0;
    while ((row = mysql_fetch_row(res)))
    {
        (*rooms)[idx].id = row[0] ? atoi(row[0]) : -1;
        strncpy((*rooms)[idx].room_name, row[1] ? row[1] : "", sizeof((*rooms)[idx].room_name) - 1);
        (*rooms)[idx].room_name[sizeof((*rooms)[idx].room_name) - 1] = '\0';
        idx++;
    }
    mysql_free_result(res);
    return count;
}

int db_is_room_host(MYSQL *conn, int room_id, int host_id)
{
    char query[256];
    sprintf(query, "SELECT id FROM rooms WHERE id=%d AND host_id=%d", room_id, host_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_is_room_host failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    int ok = (mysql_num_rows(res) > 0) ? 1 : 0;
    mysql_free_result(res);
    return ok;
}

int db_get_user_score_in_room(MYSQL *conn, int room_id, int user_id, int *score_out)
{
    if (!score_out)
        return -1;
    char query[256];
    sprintf(query, "SELECT score FROM room_participants WHERE room_id=%d AND user_id=%d", room_id, user_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_user_score_in_room failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row)
    {
        mysql_free_result(res);
        return 0;
    }
    int score = 0;
    if (row[0])
        score = atoi(row[0]);
    *score_out = score;
    mysql_free_result(res);
    return 1;
}

int db_get_room_scores(MYSQL *conn, int room_id, RoomScoreItem **items)
{
    char query[512];
    sprintf(query,
            "SELECT u.username, rp.score, rp.status "
            "FROM room_participants rp "
            "JOIN users u ON u.id = rp.user_id "
            "WHERE rp.room_id=%d "
            "ORDER BY rp.score DESC, u.username ASC",
            room_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_get_room_scores failed: %s\n", mysql_error(conn));
        return -1;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return -1;
    int count = mysql_num_rows(res);
    if (count == 0)
    {
        *items = NULL;
        mysql_free_result(res);
        return 0;
    }
    *items = (RoomScoreItem *)malloc(sizeof(RoomScoreItem) * count);
    if (!*items)
    {
        mysql_free_result(res);
        return -1;
    }
    MYSQL_ROW row;
    int idx = 0;
    while ((row = mysql_fetch_row(res)))
    {
        strncpy((*items)[idx].username, row[0] ? row[0] : "", sizeof((*items)[idx].username) - 1);
        (*items)[idx].username[sizeof((*items)[idx].username) - 1] = '\0';
        (*items)[idx].score = row[1] ? atoi(row[1]) : 0;
        (*items)[idx].status = row[2] ? atoi(row[2]) : 0;
        idx++;
    }
    mysql_free_result(res);
    return count;
}
int db_validate_question_id(MYSQL *conn, int room_id, const char *question_id)
{
    char query[256];
    sprintf(query, "SELECT JSON_CONTAINS_PATH(questions_data, 'one', '$.\"%s\"') FROM rooms WHERE id = %d", question_id, room_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "db_validate_question_id failed: %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    int is_valid = 0;
    if (row && row[0] && atoi(row[0]) == 1)
    {
        is_valid = 1;
    }
    mysql_free_result(res);
    return is_valid;
}

int db_generate_questions_for_practice(MYSQL *conn, int user_id, int num_questions, int duration, cJSON **out_questions)
{
    cJSON *questions_map = db_select_random_questions(conn, num_questions);
    if (!questions_map)
    {
        printf("[Warning] Could not fetch questions for practice session\n");
        return 0;
    }
    char *json_str = cJSON_PrintUnformatted(questions_map);
    if (!json_str)
    {
        cJSON_Delete(questions_map);
        return 0;
    }
    unsigned long json_len = strlen(json_str);
    char *escaped_json = (char *)malloc(json_len * 2 + 1);
    mysql_real_escape_string(conn, escaped_json, json_str, json_len);
    char *update_query = (char *)malloc(strlen(escaped_json) + 256);
    sprintf(update_query,
            "INSERT INTO practice_history (user_id, questions_data, user_answers, num_questions, duration_minutes, score, is_late) "
            "VALUES (%d, '%s', '{}', %d, %d, 0, 0)",
            user_id, escaped_json, num_questions, duration);
    free(escaped_json);
    free(json_str);
    if (mysql_query(conn, update_query))
    {
        fprintf(stderr, "Create practice failed: %s\n", mysql_error(conn));
        cJSON_Delete(questions_map);
        return 0;
    }
    *out_questions = questions_map;
    return (int)mysql_insert_id(conn);
}

int db_submit_practice_result(MYSQL *conn, int history_id, int user_id, cJSON *user_answers, int *out_score, int *out_total, int *out_is_late)
{
    char query[512];
    sprintf(query,
            "SELECT questions_data, "
            "IF(NOW() > DATE_ADD(practiced_at, INTERVAL (duration_minutes + 1) MINUTE), 1, 0) as is_late "
            "FROM practice_history WHERE id=%d AND user_id=%d",
            history_id, user_id);

    if (mysql_query(conn, query))
        return 0;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res)
        return 0;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row)
    {
        mysql_free_result(res);
        return 0;
    }
    cJSON *questions = cJSON_Parse(row[0]);
    int is_late = atoi(row[1]);

    mysql_free_result(res);

    if (!questions)
        return 0;
    int score = 0;
    int total = 0;
    cJSON *q_node;
    cJSON_ArrayForEach(q_node, questions)
    {
        total++;
        char *q_id = q_node->string;
        cJSON *correct = cJSON_GetObjectItem(q_node, "correct_answer");
        cJSON *user_ans = cJSON_GetObjectItem(user_answers, q_id);

        if (user_ans && correct && cJSON_IsString(user_ans) && cJSON_IsString(correct))
        {
            if (strcasecmp(user_ans->valuestring, correct->valuestring) == 0)
            {
                score++;
            }
        }
    }
    *out_score = score;
    *out_total = total;
    *out_is_late = is_late;
    char *ans_str = cJSON_PrintUnformatted(user_answers);
    char *esc_ans = malloc(strlen(ans_str) * 2 + 1);
    mysql_real_escape_string(conn, esc_ans, ans_str, strlen(ans_str));

    char update_query[512 * 2];
    sprintf(update_query,
            "UPDATE practice_history SET user_answers='%s', score=%d, is_late=%d WHERE id=%d",
            esc_ans, score, is_late, history_id);

    if (mysql_query(conn, update_query))
    {
        fprintf(stderr, "Update practice result failed: %s\n", mysql_error(conn));
    }

    free(esc_ans);
    free(ans_str);
    cJSON_Delete(questions);

    return 1;
}