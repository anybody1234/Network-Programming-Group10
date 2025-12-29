#include "globals.h"
#include "network.h"
#include "ui.h"
#include <ctype.h>
#include <pthread.h>
#include <sys/select.h>
#include <unistd.h>

int sockfd = -1;
ScreenState current_screen = SCREEN_AUTH;
char current_username[50] = "";
int current_room_id = -1;
int exam_duration = 0;
int exam_remaining = 0;
time_t local_start_time = 0;
cJSON *exam_questions = NULL;
char user_answers[MAX_QUESTIONS];
int current_q_idx = 0;
int total_questions = 0;
int needs_redraw = 1;
int current_practice_id = -1;
char score_role[16] = "";
cJSON *score_rooms = NULL;
cJSON *score_items = NULL;
int score_selected_room_id = -1;
int score_self_value = 0;

int main()
{
    if (connect_to_server() < 0)
    {
        printf("Khong the ket noi den Server!\n");
        return 1;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread_func, NULL);

    char line[256];

    while (1)
    {
        if (needs_redraw)
        {
            ui_render();
            needs_redraw = 0;
        }

        if (current_screen == SCREEN_EXAM)
        {
            static time_t last_tick = 0;
            if (time(NULL) > last_tick)
            {
                ui_update_timer_only();
                last_tick = time(NULL);
            }
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = {0, 100000};

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

        if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds))
        {
            if (fgets(line, sizeof(line), stdin))
            {
                line[strcspn(line, "\n")] = 0;

                if (current_screen == SCREEN_EXAM)
                {
                    if (strlen(line) == 0 || strcasecmp(line, "N") == 0)
                    {
                        if (current_q_idx < total_questions - 1)
                            current_q_idx++;
                        else
                            printf("\nDa la cau cuoi cung!\n");
                        needs_redraw = 1;
                    }
                    else
                    {
                        char cmd = toupper(line[0]);
                        if (cmd == 'P') {
                            if (current_q_idx > 0)
                                current_q_idx--;
                            needs_redraw = 1;
                        }
                        else if (cmd >= 'A' && cmd <= 'D')
                        {
                            user_answers[current_q_idx] = cmd;
                            cJSON *q = cJSON_GetArrayItem(exam_questions, current_q_idx);
                            if (q && q->string)
                            {
                                cJSON *req = cJSON_CreateObject();
                                cJSON_AddStringToObject(req, "type", "UPDATE_ANSWER");
                                cJSON_AddNumberToObject(req, "room_id", current_room_id);
                                cJSON_AddStringToObject(req, "question_id", q->string);
                                char ans_str[2] = {cmd, '\0'};
                                cJSON_AddStringToObject(req, "answer", ans_str);
                                send_json_request(req);
                                cJSON_Delete(req);
                            }
                            needs_redraw = 1;
                        }
                        else if (cmd == 'S')
                        {
                            ui_handle_submit_exam();
                            current_screen = SCREEN_MENU;
                            needs_redraw = 1;
                        }
                        else if (cmd == 'Q')
                        {
                            current_screen = SCREEN_MENU;
                            needs_redraw = 1;
                        }
                    }
                }
                else
                {
                    if (current_screen == SCREEN_AUTH)
                    {
                        char u[50], p[50];
                        if (sscanf(line, "login %s %s", u, p) == 2)
                        {
                            cJSON *req = cJSON_CreateObject();
                            cJSON_AddStringToObject(req, "type", "LOGIN");
                            cJSON_AddStringToObject(req, "username", u);
                            cJSON_AddStringToObject(req, "password", p);
                            send_json_request(req);
                            cJSON_Delete(req);
                            strcpy(current_username, u);
                        }
                        else if (strcmp(line, "1") == 0)
                        {
                            ui_handle_login_input();
                        }
                        else if (strcmp(line, "2") == 0)
                        {
                            ui_handle_register_input();
                        }
                        else if (strcmp(line, "0") == 0)
                        {
                            break;
                        }
                        else
                        {
                            needs_redraw = 1;
                        }
                    }
                    else if (current_screen == SCREEN_MENU)
                    {
                        if (strcmp(line, "1") == 0)
                        {
                            cJSON *req = cJSON_CreateObject();
                            cJSON_AddStringToObject(req, "type", "LIST_ROOMS");
                            send_json_request(req);
                            cJSON_Delete(req);
                            current_screen = SCREEN_ROOM_LIST;
                            needs_redraw = 1;
                        }
                        else if (strcmp(line, "2") == 0)
                        {
                            ui_handle_create_room();
                            needs_redraw = 1;
                        }
                        else if (strcmp(line, "3") == 0)
                        {
                            ui_handle_join_room();
                        }
                        else if (strcmp(line, "4") == 0)
                        {
                            current_screen = SCREEN_AUTH;
                            needs_redraw = 1;
                        }
                        else if (strcmp(line, "5") == 0)
                        {
                            current_screen = SCREEN_SCORE_ROLE;
                            needs_redraw = 1;
                        }
                        else if (strcmp(line, "6") == 0) { 
                            int n, m;
                            printf("\n--- CAU HINH LUYEN TAP ---\n");
                            printf("Nhap so luong cau hoi: "); 
                            if (scanf("%d", &n) != 1) n = 10;
                            
                            printf("Nhap thoi gian lam bai (phut): ");
                            if (scanf("%d", &m) != 1) m = 15;
                            
                            while(getchar()!='\n'); 
                            cJSON *req = cJSON_CreateObject();
                            cJSON_AddStringToObject(req, "type", "PRACTICE_START");
                            cJSON_AddNumberToObject(req, "num_questions", n);
                            cJSON_AddNumberToObject(req, "duration", m);
                            
                            send_json_request(req);
                            cJSON_Delete(req);
                            
                            printf("Dang tai de thi..."); fflush(stdout);
                        }
                        else
                        {
                            needs_redraw = 1;
                        }
                    }
                    else if (current_screen == SCREEN_SCORE_ROLE)
                    {
                        if (strcmp(line, "1") == 0)
                        {
                            strcpy(score_role, "HOST");
                            if (score_rooms)
                            {
                                cJSON_Delete(score_rooms);
                                score_rooms = NULL;
                            }
                            if (score_items)
                            {
                                cJSON_Delete(score_items);
                                score_items = NULL;
                            }
                            cJSON *req = cJSON_CreateObject();
                            cJSON_AddStringToObject(req, "type", "LIST_SCORE_ROOMS");
                            cJSON_AddStringToObject(req, "role", score_role);
                            send_json_request(req);
                            cJSON_Delete(req);
                            current_screen = SCREEN_SCORE_ROOM_LIST;
                            needs_redraw = 1;
                        }
                        else if (strcmp(line, "2") == 0)
                        {
                            strcpy(score_role, "PARTICIPANT");
                            if (score_rooms)
                            {
                                cJSON_Delete(score_rooms);
                                score_rooms = NULL;
                            }
                            if (score_items)
                            {
                                cJSON_Delete(score_items);
                                score_items = NULL;
                            }
                            cJSON *req = cJSON_CreateObject();
                            cJSON_AddStringToObject(req, "type", "LIST_SCORE_ROOMS");
                            cJSON_AddStringToObject(req, "role", score_role);
                            send_json_request(req);
                            cJSON_Delete(req);
                            current_screen = SCREEN_SCORE_ROOM_LIST;
                            needs_redraw = 1;
                        }
                        else if (strcmp(line, "0") == 0 || toupper(line[0]) == 'Q')
                        {
                            current_screen = SCREEN_MENU;
                            needs_redraw = 1;
                        }
                        else
                        {
                            needs_redraw = 1;
                        }
                    }
                    else if (current_screen == SCREEN_SCORE_ROOM_LIST)
                    {
                        if (toupper(line[0]) == 'Q')
                        {
                            current_screen = SCREEN_MENU;
                            needs_redraw = 1;
                        }
                        else
                        {
                            int rid = 0;
                            if (sscanf(line, "%d", &rid) == 1 && rid > 0)
                            {
                                score_selected_room_id = rid;
                                score_self_value = 0;
                                if (score_items)
                                {
                                    cJSON_Delete(score_items);
                                    score_items = NULL;
                                }
                                cJSON *req = cJSON_CreateObject();
                                cJSON_AddStringToObject(req, "type", "GET_ROOM_SCORES");
                                cJSON_AddStringToObject(req, "role", score_role);
                                cJSON_AddNumberToObject(req, "room_id", rid);
                                send_json_request(req);
                                cJSON_Delete(req);
                                current_screen = SCREEN_SCORE_VIEW;
                                needs_redraw = 1;
                            }
                            else
                            {
                                needs_redraw = 1;
                            }
                        }
                    }
                    else if (current_screen == SCREEN_SCORE_VIEW)
                    {
                        if (toupper(line[0]) == 'Q')
                        {
                            current_screen = SCREEN_MENU;
                            needs_redraw = 1;
                        }
                        else
                        {
                            current_screen = SCREEN_SCORE_ROOM_LIST;
                            needs_redraw = 1;
                        }
                    }
                    else if (current_screen == SCREEN_ROOM_LIST || current_screen == SCREEN_WAITING)
                    {
                        if (line[0] == '\0' || toupper(line[0]) == 'Q')
                        {
                            current_screen = SCREEN_MENU;
                            needs_redraw = 1;
                        }
                    }
                }
            }
        }
    }
    close(sockfd);
    return 0;
}