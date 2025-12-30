#include "ui.h"
#include "globals.h"
#include "network.h"

// Hàm dọn dẹp bộ nhớ đệm bàn phím
void flush_input()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

void clear_screen()
{
    printf("\033[H\033[J");
}

void draw_auth()
{
    clear_screen();
    printf("=== HE THONG THI TRAC NGHIEM ===\n");
    printf("1. Dang nhap\n");
    printf("2. Dang ky\n");
    printf("0. Thoat\n");
    printf("----------------------------\n");
    printf("Nhap lua chon > ");
    fflush(stdout);
}

void draw_menu()
{
    clear_screen();
    printf("=== MENU CHINH (%s) ===\n", current_username);
    printf("1. Xem danh sach phong\n");
    printf("2. Tao phong moi\n");
    printf("3. Vao phong (Join)\n");
    printf("4. Dang xuat\n");
    printf("5. Xem diem\n");
    printf("6. Luyen tap\n");
    printf("----------------------------\n");
    printf("Nhap lua chon > ");
    fflush(stdout);
}

static void draw_score_role()
{
    clear_screen();
    printf("=== XEM DIEM ===\n");
    printf("1. Xem voi tu cach chu phong\n");
    printf("2. Xem voi tu cach nguoi thi\n");
    printf("0. Quay lai\n");
    printf("----------------------------\n");
    printf("Nhap lua chon > ");
    fflush(stdout);
}

static void draw_score_room_list()
{
    clear_screen();
    printf("=== DANH SACH PHONG (%s) ===\n", (strlen(score_role) ? score_role : ""));
    printf("----------------------------\n");

    if (score_rooms && cJSON_IsArray(score_rooms) && cJSON_GetArraySize(score_rooms) > 0)
    {
        cJSON *r = NULL;
        cJSON_ArrayForEach(r, score_rooms)
        {
            cJSON *id = cJSON_GetObjectItem(r, "id");
            cJSON *name = cJSON_GetObjectItem(r, "room_name");
            if (id && name && cJSON_IsNumber(id) && cJSON_IsString(name))
            {
                printf("ID: %d | %s\n", id->valueint, name->valuestring);
            }
        }
    }
    else
    {
        printf("(Khong co phong nao)\n");
    }

    printf("----------------------------\n");
    printf("Nhap ID phong de xem diem\n");
    printf("Go 'q' roi Enter de quay lai menu\n");
    printf("> ");
    fflush(stdout);
}

static void draw_score_view()
{
    clear_screen();
    printf("=== KET QUA PHONG %d ===\n", score_selected_room_id);
    printf("----------------------------\n");

    if (strcmp(score_role, "HOST") == 0)
    {
        if (score_items && cJSON_IsArray(score_items) && cJSON_GetArraySize(score_items) > 0)
        {
            cJSON *it = NULL;
            cJSON_ArrayForEach(it, score_items)
            {
                cJSON *uname = cJSON_GetObjectItem(it, "username");
                cJSON *score = cJSON_GetObjectItem(it, "score");
                cJSON *st = cJSON_GetObjectItem(it, "submit_status");
                if (uname && score && cJSON_IsString(uname) && cJSON_IsNumber(score))
                {
                    int submitted = (st && cJSON_IsNumber(st)) ? st->valueint : 0;
                    printf("%-15s | Diem: %d | %s\n", uname->valuestring, score->valueint,
                           submitted == 1 ? "Da nop" : "Chua nop");
                }
            }
        }
        else
        {
            printf("(Chua co du lieu diem)\n");
        }
    }
    else
    {
        printf("Diem cua ban: %d\n", score_self_value);
    }

    printf("----------------------------\n");
    printf("Nhan Enter de quay lai danh sach phong\n");
    printf("Go 'q' roi Enter de ve menu\n");
    printf("> ");
    fflush(stdout);
}

void draw_waiting()
{
    clear_screen();
    printf("=== PHONG CHO %d ===\n", current_room_id);
    printf("Dang cho chu phong bat dau...\n");
    printf("Go 'q' roi Enter de thoat.\n");
}

void draw_exam()
{
    clear_screen();
    int elapsed = (int)(time(NULL) - local_start_time);
    int remain = exam_remaining - elapsed;
    if (remain < 0)
        remain = 0;

    printf("=== THI TRAC NGHIEM | Con lai: %02d:%02d ===\n", remain / 60, remain % 60);

    if (exam_questions && total_questions > 0)
    {
        cJSON *q = cJSON_GetArrayItem(exam_questions, current_q_idx);
        printf("\nCau %d/%d: %s\n", current_q_idx + 1, total_questions,
               cJSON_GetObjectItem(q, "question_text")->valuestring);

        cJSON *opts = cJSON_GetObjectItem(q, "options");
        char my_ans = user_answers[current_q_idx];

        printf(" %s A. %s\n", (my_ans == 'A' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "A")->valuestring);
        printf(" %s B. %s\n", (my_ans == 'B' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "B")->valuestring);
        printf(" %s C. %s\n", (my_ans == 'C' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "C")->valuestring);
        printf(" %s D. %s\n", (my_ans == 'D' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "D")->valuestring);
    }
    printf("\n--------------------------------\n");
    printf("Nhap dap an [A/B/C/D] roi Enter.\n");
    printf("Dieu huong: [N]ext, [P]rev\n");
    printf("Chuc nang: [S]ubmit (Nop bai), [Q]uit (Thoat)\n");
    printf("> ");
    fflush(stdout);
}

static void draw_practice()
{
    clear_screen();
    int elapsed = (int)(time(NULL) - local_start_time);
    int remain = exam_remaining - elapsed;
    if (remain < 0)
        remain = 0;

    printf("=== LUYEN TAP | Con lai: %02d:%02d ===\n", remain / 60, remain % 60);

    if (exam_questions && total_questions > 0)
    {
        cJSON *q = cJSON_GetArrayItem(exam_questions, current_q_idx);
        printf("\nCau %d/%d: %s\n", current_q_idx + 1, total_questions,
               cJSON_GetObjectItem(q, "question_text")->valuestring);

        cJSON *opts = cJSON_GetObjectItem(q, "options");
        char my_ans = user_answers[current_q_idx];

        printf(" %s A. %s\n", (my_ans == 'A' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "A")->valuestring);
        printf(" %s B. %s\n", (my_ans == 'B' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "B")->valuestring);
        printf(" %s C. %s\n", (my_ans == 'C' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "C")->valuestring);
        printf(" %s D. %s\n", (my_ans == 'D' ? "[X]" : "[ ]"), cJSON_GetObjectItem(opts, "D")->valuestring);
    }
    printf("\n--------------------------------\n");
    printf("Nhap dap an [A/B/C/D] roi Enter.\n");
    printf("Dieu huong: [N]ext, [P]rev\n");
    printf("Chuc nang: [S]ubmit (Nop bai), [Q]uit (Thoat)\n");
    printf("> ");
    fflush(stdout);
}

static void draw_practice_result()
{
    clear_screen();
    printf("=== KET QUA LUYEN TAP ===\n");
    printf("Diem so: %d / %d\n", practice_last_score, practice_last_total);
    if (practice_last_is_late)
        printf("[!] Ban nop muon so voi quy dinh!\n");
    else
        printf("[OK] Ban nop dung gio.\n");
    printf("----------------------------\n");
    printf("Nhan Enter de quay ve Menu...\n");
    fflush(stdout);
}
void ui_handle_submit_exam()
{
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "SUBMIT_EXAM");
    cJSON_AddNumberToObject(req, "room_id", current_room_id);
    send_json_request(req);
    cJSON_Delete(req);
    printf("\nDang nop bai... Vui long cho ket qua...\n");
}

void ui_handle_submit_practice()
{
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "PRACTICE_SUBMIT");
    cJSON_AddNumberToObject(req, "history_id", current_practice_id);

    cJSON *ans_obj = cJSON_CreateObject();
    if (exam_questions && ans_obj)
    {
        for (int i = 0; i < total_questions; i++)
        {
            char ans = user_answers[i];
            if (ans)
            {
                cJSON *q = cJSON_GetArrayItem(exam_questions, i);
                if (q && q->string)
                {
                    char ans_str[2] = {ans, '\0'};
                    cJSON_AddStringToObject(ans_obj, q->string, ans_str);
                }
            }
        }
    }
    cJSON_AddItemToObject(req, "answers", ans_obj);
    send_json_request(req);
    cJSON_Delete(req);
    printf("\nDang nop ket qua luyen tap... Vui long cho...\n");
}
void ui_update_timer_only()
{
    int elapsed = (int)(time(NULL) - local_start_time);
    int remain = exam_remaining - elapsed;
    if (remain < 0)
        remain = 0;
    const char *title = (current_screen == SCREEN_PRACTICE) ? "LUYEN TAP" : "THI TRAC NGHIEM";
    printf("\033[s\033[1;1H\033[K=== %s | Con lai: %02d:%02d ===\033[u", title, remain / 60, remain % 60);
    fflush(stdout);
}

void ui_render()
{
    switch (current_screen)
    {
    case SCREEN_AUTH:
        draw_auth();
        break;
    case SCREEN_MENU:
        draw_menu();
        break;
    case SCREEN_WAITING:
        draw_waiting();
        break;
    case SCREEN_EXAM:
        draw_exam();
        break;
    case SCREEN_PRACTICE:
        draw_practice();
        break;
    case SCREEN_PRACTICE_RESULT:
        draw_practice_result();
        break;
    case SCREEN_SCORE_ROLE:
        draw_score_role();
        break;
    case SCREEN_SCORE_ROOM_LIST:
        draw_score_room_list();
        break;
    case SCREEN_SCORE_VIEW:
        draw_score_view();
        break;
    default:
        break;
    }
}

void ui_handle_login_input()
{
    char user[50], pass[50];
    printf("\n--- DANG NHAP ---\n");
    printf("Username: ");
    scanf("%s", user);
    printf("Password: ");
    scanf("%s", pass);
    flush_input(); 

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "LOGIN");
    cJSON_AddStringToObject(req, "username", user);
    cJSON_AddStringToObject(req, "password", pass);
    send_json_request(req);
    cJSON_Delete(req);

    strcpy(current_username, user);
}

void ui_handle_register_input()
{
    char user[50], pass[50];
    printf("\n--- DANG KY TAI KHOAN ---\n");
    printf("Username moi: ");
    scanf("%s", user);
    printf("Password moi: ");
    scanf("%s", pass);
    flush_input(); 
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "REGISTER");
    cJSON_AddStringToObject(req, "username", user);
    cJSON_AddStringToObject(req, "password", pass);
    send_json_request(req);
    cJSON_Delete(req);
}

void ui_handle_create_room()
{
    char name[50];
    int n, m;
    char start_time[100];

    printf("\n--- TAO PHONG ---\n");
    printf("Ten phong: ");
    scanf(" %[^\n]", name);
    printf("So luong cau hoi: ");
    scanf("%d", &n);
    printf("Thoi gian (phut): ");
    scanf("%d", &m);
    printf("Thoi gian bat dau (YYYY-MM-DD HH:MM:SS): ");
    scanf(" %[^\n]", start_time);
    flush_input(); 

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "CREATE_ROOM");
    cJSON_AddStringToObject(req, "room_name", name);
    cJSON_AddNumberToObject(req, "num_questions", n);
    cJSON_AddNumberToObject(req, "duration", m);
    cJSON_AddStringToObject(req, "start_time", start_time);

    send_json_request(req);
    cJSON_Delete(req);
}

void ui_handle_join_room()
{
    int id;
    printf("\nNhap ID phong: ");
    scanf("%d", &id);
    flush_input();

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "JOIN_ROOM");
    cJSON_AddNumberToObject(req, "room_id", id);
    send_json_request(req);
    cJSON_Delete(req);
}