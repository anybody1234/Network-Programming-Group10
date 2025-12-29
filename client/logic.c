#include "logic.h"
#include "globals.h"

void process_server_response(char *json_str)
{
    cJSON *json = cJSON_Parse(json_str);
    if (!json)
        return;

    cJSON *type = cJSON_GetObjectItem(json, "type");
    cJSON *status = cJSON_GetObjectItem(json, "status");
    cJSON *msg = cJSON_GetObjectItem(json, "message");

    int code = status ? status->valueint : 0;
    char *type_str = type ? type->valuestring : "";
    char *message = msg ? msg->valuestring : "";
    if (current_screen == SCREEN_AUTH)
    {
        if (code == 200 && strstr(message, "Login successful"))
        {
            current_screen = SCREEN_MENU;
            needs_redraw = 1;
        }
        else if (code == 201)
        {
            printf("\n[THANH CONG] %s. Hay dang nhap.\n", message);
            printf("Nhan Enter de tiep tuc...");
            fflush(stdout);
        }
        else if (code >= 400)
        {
            printf("\n[LOI] %s\n", message);
            printf("Nhan Enter de thu lai...");
            fflush(stdout);
        }
    }
    else if (code == 200 && strcmp(type_str, "JOIN_SUCCESS") == 0)
    {
        current_room_id = cJSON_GetObjectItem(json, "room_id")->valueint;
        current_screen = SCREEN_WAITING;
        needs_redraw = 1;
    }
    else if (strcmp(type_str, "EXAM_START") == 0)
    {
        cJSON *r_id_item = cJSON_GetObjectItem(json, "room_id");
        if (r_id_item)
        {
            current_room_id = r_id_item->valueint;
        }

        current_screen = SCREEN_EXAM;
        exam_duration = cJSON_GetObjectItem(json, "duration")->valueint;
        exam_remaining = cJSON_GetObjectItem(json, "remaining")->valueint;
        local_start_time = time(NULL);
        if (exam_questions)
            cJSON_Delete(exam_questions);
        exam_questions = cJSON_Duplicate(cJSON_GetObjectItem(json, "questions"), 1);
        total_questions = cJSON_GetArraySize(exam_questions);
        memset(user_answers, 0, sizeof(user_answers));
        current_q_idx = 0;
        cJSON *saved = cJSON_GetObjectItem(json, "saved_answers");
        if (saved && cJSON_GetArraySize(saved) > 0)
        {
            printf("\n[DEBUG] Server gui ve %d dap an da luu. Dang khoi phuc...\n", cJSON_GetArraySize(saved));

            cJSON *ans_item = NULL;
            cJSON_ArrayForEach(ans_item, saved)
            {
                if (ans_item->string && ans_item->valuestring)
                {
                    char *q_id_saved_str = ans_item->string;
                    char ans_char = ans_item->valuestring[0];
                    int found_match = 0;
                    for (int i = 0; i < total_questions; i++)
                    {
                        cJSON *q = cJSON_GetArrayItem(exam_questions, i);
                        if (q && q->string)
                        {
                            if (strcmp(q->string, q_id_saved_str) == 0)
                            {
                                user_answers[i] = ans_char;
                                found_match = 1;
                                break;
                            }
                        }
                    }
                    if (!found_match)
                    {
                        printf("[DEBUG] Canh bao: Khong tim thay cau hoi ID %s trong de thi hien tai!\n", q_id_saved_str);
                    }
                }
            }
        }
        else
        {
            if (message && strlen(message) > 0)
            {
                if (strstr(message, "Final Score recorded") || strstr(message, "Time is up"))
                {
                    printf("\n\n>>> [THONG BAO] %s <<<\n\n", message);
                    if (exam_questions)
                    {
                        cJSON_Delete(exam_questions);
                        exam_questions = NULL;
                    }
                    total_questions = 0;
                    current_q_idx = 0;
                    exam_remaining = 0;
                    if (current_screen != SCREEN_MENU)
                    {
                        current_screen = SCREEN_MENU;
                        needs_redraw = 1;
                    }
                    if (current_screen == SCREEN_MENU)
                    {
                        printf("Nhap lua chon > ");
                        fflush(stdout);
                    }
                }
                else
                {
                    printf("\n[SERVER] %s\n", message);
                }
            }
        }
        needs_redraw = 1;
    }
    else if (strcmp(type_str, "ROOM_LIST") == 0)
    {
        printf("\n\n--- DANH SACH PHONG ---\n");
        cJSON *rooms = cJSON_GetObjectItem(json, "rooms");
        cJSON *r;
        cJSON_ArrayForEach(r, rooms)
        {
            int status_val = cJSON_GetObjectItem(r, "status")->valueint;
            const char *status_text = "";
            if (status_val == 0)
                status_text = "Dang cho phong thi bat dau";
            else if (status_val == 1)
                status_text = "Phong thi dang bat dau";
            else if (status_val == 2)
                status_text = "Phong thi da ket thuc";
            else
                status_text = "Khong ro";

            printf("ID: %d | %-15s | %d cau | Trang thai: %s\n",
                   cJSON_GetObjectItem(r, "id")->valueint,
                   cJSON_GetObjectItem(r, "room_name")->valuestring,
                   cJSON_GetObjectItem(r, "num_questions")->valueint,
                   status_text);
        }
        printf("\nNhan Enter de quay lai menu lenh...");
        fflush(stdout);
    }
    else if (strcmp(type_str, "SCORE_ROOM_LIST") == 0)
    {
        cJSON *rooms = cJSON_GetObjectItem(json, "rooms");
        if (score_rooms)
        {
            cJSON_Delete(score_rooms);
            score_rooms = NULL;
        }
        if (rooms && cJSON_IsArray(rooms))
        {
            score_rooms = cJSON_Duplicate(rooms, 1);
        }
        needs_redraw = 1;
    }
    else if (strcmp(type_str, "SCORE_SELF") == 0)
    {
        cJSON *rid = cJSON_GetObjectItem(json, "room_id");
        cJSON *sc = cJSON_GetObjectItem(json, "score");
        if (rid && cJSON_IsNumber(rid))
            score_selected_room_id = rid->valueint;
        if (sc && cJSON_IsNumber(sc))
            score_self_value = sc->valueint;
        needs_redraw = 1;
    }
    else if (strcmp(type_str, "SCORE_ALL") == 0)
    {
        cJSON *rid = cJSON_GetObjectItem(json, "room_id");
        cJSON *arr = cJSON_GetObjectItem(json, "scores");
        if (rid && cJSON_IsNumber(rid))
            score_selected_room_id = rid->valueint;
        if (score_items)
        {
            cJSON_Delete(score_items);
            score_items = NULL;
        }
        if (arr && cJSON_IsArray(arr))
        {
            score_items = cJSON_Duplicate(arr, 1);
        }
        needs_redraw = 1;
    }
    else if (strcmp(type_str, "PRACTICE_START_OK") == 0) {
        current_practice_id = cJSON_GetObjectItem(json, "history_id")->valueint;
        
        exam_duration = cJSON_GetObjectItem(json, "duration")->valueint;
        exam_remaining = cJSON_GetObjectItem(json, "remaining")->valueint; // = duration
        local_start_time = time(NULL);
        if (exam_questions) cJSON_Delete(exam_questions);
        exam_questions = cJSON_Duplicate(cJSON_GetObjectItem(json, "questions"), 1);
        total_questions = cJSON_GetArraySize(exam_questions);
    
        memset(user_answers, 0, sizeof(user_answers));
        current_q_idx = 0;
        
        current_screen = SCREEN_PRACTICE;
        needs_redraw = 1;
    }
    else if (strcmp(type_str, "PRACTICE_RESULT") == 0) {
        int score = cJSON_GetObjectItem(json, "score")->valueint;
        int total = cJSON_GetObjectItem(json, "total")->valueint;
        int is_late = cJSON_GetObjectItem(json, "is_late")->valueint;

        printf("\n\n==========================\n");
        printf(" KET QUA LUYEN TAP\n");
        printf(" Diem so: %d / %d\n", score, total);
        
        if (is_late == 1) {
            printf(" [!] CANH BAO: Ban nop muon so voi quy dinh!\n");
        } else {
            printf(" [OK] Ban nop bai dung gio.\n");
        }
        printf("==========================\n");
        printf("Nhan Enter de quay ve Menu..."); fflush(stdout);
        
        // Logic để user nhấn Enter rồi mới về Menu xử lý ở main.c
        current_screen = SCREEN_MENU; 
    }
    else
    {
        if (message && strlen(message) > 0)
            printf("\n[SERVER] %s\n", message);

        if (code >= 400)
        {
            if (current_screen == SCREEN_SCORE_VIEW)
            {
                current_screen = SCREEN_SCORE_ROOM_LIST;
                needs_redraw = 1;
            }
            else if (current_screen == SCREEN_SCORE_ROOM_LIST)
            {
                current_screen = SCREEN_SCORE_ROLE;
                needs_redraw = 1;
            }
        }
    }

    cJSON_Delete(json);
}