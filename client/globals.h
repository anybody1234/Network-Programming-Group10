#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>
typedef enum
{
    SCREEN_AUTH,
    SCREEN_MENU,
    SCREEN_ROOM_LIST,
    SCREEN_WAITING,
    SCREEN_EXAM,

    SCREEN_SCORE_ROLE,
    SCREEN_SCORE_ROOM_LIST,
    SCREEN_SCORE_VIEW
} ScreenState;

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5500
#define MAX_QUESTIONS 100

extern int sockfd;
extern ScreenState current_screen;
extern char current_username[50];
extern int current_room_id;

extern int exam_duration;
extern int exam_remaining;
extern time_t local_start_time;
extern cJSON *exam_questions;
extern char user_answers[MAX_QUESTIONS];
extern int current_q_idx;
extern int total_questions;
extern int needs_redraw;

extern char score_role[16];
extern cJSON *score_rooms;
extern cJSON *score_items;
extern int score_selected_room_id;
extern int score_self_value;

#endif