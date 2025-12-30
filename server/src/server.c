#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include "route/route.h"
#include "config/db.h"
#include <pthread.h>
#include "service/auth_service.h"
#include "utils/net_utils.h"
#include <cjson/cJSON.h>

#define PORT 5500
#define BUFF_SIZE 16384
#define BACKLOG 20
#define MAX_CLIENTS FD_SETSIZE

ClientSession client_sessions[MAX_CLIENTS];
MYSQL *db_conn;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
void init_sessions()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_sessions[i].sockfd = -1;
        client_sessions[i].buf_len = 0;
        client_sessions[i].is_logged_in = 0;
        client_sessions[i].failed_login_count = 0;
        client_sessions[i].username[0] = '\0';
        client_sessions[i].user_id = -1;
        memset(client_sessions[i].buffer, 0, BUFF_SIZE);
    }
}
void *room_monitor_thread(void *arg)
{
    MYSQL *thread_db = db_connect();
    if (!thread_db)
    {
        fprintf(stderr, "[Monitor] DB Connection failed inside thread\n");
        return NULL;
    }
    printf("[Monitor] Room monitor thread started.\n");
    while (1)
    {
        Room *starting_rooms = NULL;
        int start_count = db_get_starting_rooms(thread_db, &starting_rooms);
        for (int i = 0; i < start_count; i++)
        {
            int r_id = starting_rooms[i].id;
            printf("[Monitor] Starting Room %d (%s)...\n", r_id, starting_rooms[i].room_name);
            db_update_room_status(thread_db, r_id, 1);

            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "type", "EXAM_START");
            cJSON_AddNumberToObject(resp, "status", 300);
            cJSON_AddNumberToObject(resp, "room_id", r_id);
            cJSON_AddStringToObject(resp, "message", "The exam has started!");

            int duration_sec = starting_rooms[i].duration_minutes * 60;
            cJSON_AddNumberToObject(resp, "duration", duration_sec);
            cJSON_AddNumberToObject(resp, "remaining", duration_sec);

            if (starting_rooms[i].questions_data)
            {
                cJSON *q_json = cJSON_Parse(starting_rooms[i].questions_data);
                if (q_json)
                {
                    cJSON_AddItemToObject(resp, "questions", q_json);
                }
                free(starting_rooms[i].questions_data);
            }
            char *msg = cJSON_PrintUnformatted(resp);
            char packet[BUFF_SIZE * 2];
            snprintf(packet, sizeof(packet), "%s\r\n", msg);
            printf("[Monitor] Sending to room %d: %s\n", r_id, packet);
            int *user_ids = NULL;
            int u_count = db_get_room_participants(thread_db, r_id, &user_ids, 0);
            pthread_mutex_lock(&clients_mutex);
            for (int u = 0; u < u_count; u++)
            {
                int target_uid = user_ids[u];
                for (int s = 0; s < MAX_CLIENTS; s++)
                {
                    if (client_sessions[s].sockfd != -1 &&
                        client_sessions[s].is_logged_in &&
                        client_sessions[s].user_id == target_uid)
                    {

                        send(client_sessions[s].sockfd, packet, strlen(packet), 0);
                        printf(" -> Sent exam to UserID %d\n", target_uid);
                    }
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            free(msg);
            cJSON_Delete(resp);
            if (user_ids)
                free(user_ids);
        }
        if (starting_rooms)
            free(starting_rooms);
        Room *ending_rooms = NULL;
        int end_count = db_get_ending_rooms(thread_db, &ending_rooms);
        for (int i = 0; i < end_count; i++)
        {
            int r_id = ending_rooms[i].id;
            printf("[Monitor] Ending Room %d (%s)...\n", r_id, ending_rooms[i].room_name);
            db_update_room_status(thread_db, r_id, 2);
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "type", "EXAM_END");
            cJSON_AddNumberToObject(resp, "status", 310);
            cJSON_AddNumberToObject(resp, "room_id", r_id);
            cJSON_AddStringToObject(resp, "message", "Time is up! Exam finished. Access the room to see your score.");
            char *msg = cJSON_PrintUnformatted(resp);
            char packet[512];
            snprintf(packet, sizeof(packet), "%s\r\n", msg);

            int *user_ids = NULL;
            int u_count = db_get_room_participants(thread_db, r_id, &user_ids, 0);

            pthread_mutex_lock(&clients_mutex);
            for (int u = 0; u < u_count; u++)
            {
                int target_uid = user_ids[u];
                for (int s = 0; s < MAX_CLIENTS; s++)
                {
                    if (client_sessions[s].sockfd != -1 &&
                        client_sessions[s].is_logged_in &&
                        client_sessions[s].user_id == target_uid)
                    {
                        send(client_sessions[s].sockfd, packet, strlen(packet), 0);
                    }
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            free(msg);
            cJSON_Delete(resp);
            if (user_ids)
                free(user_ids);
        }
        if (ending_rooms)
            free(ending_rooms);
        sleep(2);
    }
    mysql_close(thread_db);
    return NULL;
}
void remove_client(int i, fd_set *all_fds)
{
    handle_client_disconnection(&client_sessions[i]);
    FD_CLR(client_sessions[i].sockfd, all_fds);
    close(client_sessions[i].sockfd);
    client_sessions[i].sockfd = -1;
    client_sessions[i].buf_len = 0;
    client_sessions[i].is_logged_in = 0;
    client_sessions[i].failed_login_count = 0;
    client_sessions[i].username[0] = '\0';
    memset(client_sessions[i].buffer, 0, BUFF_SIZE);
}
void add_new_client(int connfd, struct sockaddr_in addr)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sessions[i].sockfd == -1)
        {
            client_sessions[i].sockfd = connfd;
            client_sessions[i].is_logged_in = 0;
            client_sessions[i].buf_len = 0;
            client_sessions[i].buffer[0] = '\0';
            client_sessions[i].username[0] = '\0';
            client_sessions[i].user_id = -1;
            client_sessions[i].failed_login_count = 0;
            printf("New connection from %s:%d (fd: %d)\n",
                   inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), connfd);
            send_json_response(connfd, 100, "Connected to the server");
            return;
        }
    }
    fprintf(stderr, "Max clients reached. Rejecting %s\n", inet_ntoa(addr.sin_addr));
    send_json_response(connfd, 503, "Server is full, try again later");
    close(connfd);
}

void handle_client_data(int i, fd_set *all_fds)
{
    ClientSession *s = &client_sessions[i];
    int room_left = BUFF_SIZE - 1 - s->buf_len;
    if (room_left <= 0)
    {
        fprintf(stderr, "Client %d buffer overflow. Disconnecting.\n", s->sockfd);
        remove_client(i, all_fds);
        return;
    }
    int n = recv(s->sockfd, s->buffer + s->buf_len, room_left, 0);
    if (n <= 0)
    {
        if (n == 0)
        {
            printf("Client %d hung up.\n", s->sockfd);
        }
        else
        {
            perror("recv");
        }
        remove_client(i, all_fds);
        return;
    }
    s->buf_len += n;
    s->buffer[s->buf_len] = '\0';
    char *msg_end;
    while ((msg_end = strstr(s->buffer, "\r\n")) != NULL)
    {
        *msg_end = '\0';
        if (strlen(s->buffer) > 0)
        {
            route_request(s, s->buffer, db_conn);
        }
        char *remaining_data = msg_end + 2;
        int remaining_len = s->buf_len - (remaining_data - s->buffer);
        if (remaining_len > 0)
        {
            memmove(s->buffer, remaining_data, remaining_len);
        }
        s->buf_len = remaining_len;
        s->buffer[s->buf_len] = '\0';
    }
}

int main(int argc, char *argv[])
{
    int port = PORT;
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }

    int listenfd, connfd, max_fd, i, nready;
    fd_set all_set, read_set;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len;
    db_conn = db_connect();
    if (!db_conn)
        return 1;
    printf("Database connected.\n");
    init_sessions();

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind failed");
        return 1;
    }

    listen(listenfd, BACKLOG);
    printf("Server running on port %d using IO Multiplexing...\n", port);
    pthread_t monitor_tid;
    if (pthread_create(&monitor_tid, NULL, room_monitor_thread, NULL) != 0)
    {
        perror("Failed to create monitor thread");
    }
    else
    {
        pthread_detach(monitor_tid);
    }
    printf("Server running on port %d using IO Multiplexing + Monitor Thread...\n", port);
    max_fd = listenfd;
    FD_ZERO(&all_set);
    FD_SET(listenfd, &all_set);
    while (1)
    {
        read_set = all_set;
        nready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
        if (nready < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }
        if (FD_ISSET(listenfd, &read_set))
        {
            cli_len = sizeof(cli_addr);
            connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
            if (connfd < 0)
            {
                perror("accept");
            }
            else
            {
                add_new_client(connfd, cli_addr);
                FD_SET(connfd, &all_set);
                if (connfd > max_fd)
                    max_fd = connfd;
            }
            if (--nready <= 0)
                continue;
        }
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            int sockfd = client_sessions[i].sockfd;

            if (sockfd == -1)
                continue;
            if (FD_ISSET(sockfd, &read_set))
            {
                handle_client_data(i, &all_set);
                if (client_sessions[i].sockfd == -1)
                {
                    if (sockfd == max_fd)
                    {
                        while (max_fd > listenfd && !FD_ISSET(max_fd, &all_set))
                        {
                            max_fd--;
                        }
                    }
                }
                if (--nready <= 0)
                    break;
            }
        }
    }
    db_close(db_conn);
    close(listenfd);
    return 0;
}