// room_server.c
// Soft-timer (tick-based) version of your room server.
// Tick resolution: 100 ms (10 ticks/sec) using setitimer + SIGALRM.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <stdint.h>
#include <errno.h>

#include "globe_var.h"
#include "room_action.h"
#include "room_timer.h"
#include "user_db.h"

// 你應該會在某個 .c 定義它（例如 globe_var.c）
extern int g_selected_room;

// --------- helpers ---------

static void send_text(int sock, const char *msg) {
    if (!msg) msg = "ERROR internal\n";
    send(sock, msg, strlen(msg), 0);
}

static void usage(char *out, size_t n) {
    snprintf(out, n,
        "Commands:\n"
        "  status [room_id]\n"
        "  register <user_id> <name_no_space>\n"
        "  select <room_id>\n"
        "  reserve <room_id> <user_id>\n"
        "  checkin <room_id> <user_id>\n"
        "  release <room_id> <user_id>\n"
        "  extend <room_id> <user_id>\n");
}

// ---------- client handler ----------

void* client_handler(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);

    char buffer[1024] = {0};
    char response[2048] = {0};

    ssize_t valread = read(client_sock, buffer, sizeof(buffer) - 1);
    if (valread <= 0) goto cleanup;

    buffer[valread] = '\0';
    buffer[strcspn(buffer, "\r\n")] = 0;

    // quick cmd peek
    char cmd[32] = {0};
    sscanf(buffer, "%31s", cmd);

    // -------- register <user_id> <name...> --------
    if (strcmp(cmd, "register") == 0) {
        int uid = -1, n = 0;
        char name[128] = {0};

        if (sscanf(buffer, "register %d %n", &uid, &n) >= 1) {
            if (n > 0 && buffer[n]) {
                snprintf(name, sizeof(name), "%s", buffer + n);
            }
            pthread_mutex_lock(&room_mutex);
            int rc = user_register_locked(uid, name);
            pthread_mutex_unlock(&room_mutex);

            if (rc == 0) snprintf(response, sizeof(response), "OK registered uid=%d name=%s", uid, (name[0] ? name : "UNKNOWN"));
            else snprintf(response, sizeof(response), "ERROR register failed (uid must >0, max users=%d)", MAX_USERS);
        } else {
            snprintf(response, sizeof(response), "ERROR usage: register <user_id> <name>");
        }

        send_text(client_sock, response);
        goto cleanup;
    }

    // ---------- parse common tokens ----------
    char *dup = strdup(buffer);
    if (!dup) {
        send_text(client_sock, "ERROR strdup failed");
        goto cleanup;
    }

    char *t0 = strtok(dup, " ");
    (void)t0;
    char *tok_room = strtok(NULL, " ");
    char *tok_user = strtok(NULL, " ");

    int room_id = (tok_room ? atoi(tok_room) : -1);
    int user_id = (tok_user ? atoi(tok_user) : -1);

    // name remainder (for reserve)
    char name[128] = {0};
    if (strcmp(cmd, "reserve") == 0) {
        int rid = -1, uid = -1, n = 0;
        if (sscanf(buffer, "reserve %d %d %n", &rid, &uid, &n) >= 2) {
            if (n > 0 && buffer[n]) snprintf(name, sizeof(name), "%s", buffer + n);
            room_id = rid;
            user_id = uid;
        }
    }

    // ---------- commands ----------
    if (strcmp(cmd, "status") == 0) {
        // allow: status  OR  status <room_id>
        int rid = -1;
        if (tok_room) rid = atoi(tok_room);

        char *s = get_all_status(tok_room ? rid : -1);
        snprintf(response, sizeof(response), "OK\n%s", s);
        free(s);

        // [AUTO-HW] 新增：status <room_id> 視為「選取房」並立即更新顯示
        if (rid >= 0 && rid < MAX_ROOMS) {
            pthread_mutex_lock(&room_mutex);
            g_selected_room = rid;
            update_display_selected_locked();
            pthread_mutex_unlock(&room_mutex);
        }

    } else if (strcmp(cmd, "select") == 0) {
        // [AUTO-HW] 新增：select 指令用來指定「哪一間房」顯示在 LED/7seg
        if (room_id < 0 || room_id >= MAX_ROOMS) {
            snprintf(response, sizeof(response), "ERROR usage: select <room_id> (0-%d).", MAX_ROOMS - 1);
        } else {
            pthread_mutex_lock(&room_mutex);
            g_selected_room = room_id;
            update_display_selected_locked();
            pthread_mutex_unlock(&room_mutex);
            snprintf(response, sizeof(response), "OK selected room %d.", room_id);
        }

    } else if (room_id < 0 || room_id >= MAX_ROOMS) {
        snprintf(response, sizeof(response), "ERROR Room ID out of range (0-%d).", MAX_ROOMS - 1);

    } else if (strcmp(cmd, "reserve") == 0) {
        if (user_id <= 0) {
            snprintf(response, sizeof(response), "ERROR usage: reserve <room_id> <user_id> [name]");
        } else {
            int res = reserve_room(room_id, user_id, name[0] ? name : NULL);
            if (res == 0) snprintf(response, sizeof(response), "OK Room %d reserved by %d. Check-in in %d seconds.", room_id, user_id, CHECKIN_TIMEOUT);
            else if (res == -3) snprintf(response, sizeof(response), "ERROR Room %d reservation failed. Daily limit reached.", room_id);
            else if (res == -4) snprintf(response, sizeof(response), "WAIT Room %d busy. Added to wait queue.", room_id);
            else if (res == -5) snprintf(response, sizeof(response), "ERROR Room %d wait queue full.", room_id);
            else if (res == -6) snprintf(response, sizeof(response), "ERROR user %d already has an active room.", user_id);
            else if (res == -7) snprintf(response, sizeof(response), "ERROR user %d already in wait queue for room %d.", user_id, room_id);
            else if (res == -10) snprintf(response, sizeof(response), "ERROR user %d not registered. Use: register <user_id> <name> OR reserve ... <name>", user_id);
            else snprintf(response, sizeof(response), "ERROR reserve failed (%d).", res);
        }

    } else if (strcmp(cmd, "checkin") == 0) {
        int res = check_in(room_id, user_id);
        if (res == 0) snprintf(response, sizeof(response), "OK Room %d checked in. Session duration: %d seconds.", room_id, SLOT_DURATION);
        else if (res == -8) snprintf(response, sizeof(response), "ERROR checkin denied: not the room owner.");
        else snprintf(response, sizeof(response), "ERROR Room %d check-in failed.", room_id);

    } else if (strcmp(cmd, "release") == 0) {
        int res = release_room(room_id, user_id);
        if (res == 0) snprintf(response, sizeof(response), "OK Room %d released.", room_id);
        else if (res == -8) snprintf(response, sizeof(response), "ERROR release denied: not the room owner.");
        else snprintf(response, sizeof(response), "ERROR Room %d release failed.", room_id);

    } else if (strcmp(cmd, "extend") == 0) {
        int res = extend_room(room_id, user_id);
        if (res == 0) snprintf(response, sizeof(response), "OK Room %d extended by %d seconds.", room_id, SLOT_DURATION);
        else if (res == -8) snprintf(response, sizeof(response), "ERROR extend denied: not the room owner.");
        else snprintf(response, sizeof(response), "ERROR Room %d extension failed.", room_id);

    } else {
        snprintf(response, sizeof(response), "ERROR Unknown command: %s", cmd);
    }

    free(dup);
    send_text(client_sock, response);

cleanup:
    close(client_sock);
    return NULL;
}



// ---------- main ----------

int main() {
    // initialize rooms
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].id = i;
        rooms[i].status = FREE;
        rooms[i].reserve_tick = 0;
        rooms[i].extend_used = 0;
        rooms[i].user_id = -1;

        rooms[i].wait_q.head = 0;
        rooms[i].wait_q.tail = 0;
        rooms[i].wait_q.count = 0;

        rooms[i].reserve_count_today = 0;
    }

    // default selected room (optional)
    g_selected_room = 0;

    // init simulated "day"
    time_t now = time(NULL);
    g_last_reset_day = now / SIM_DAY_SECONDS;

    // setup SIGALRM handler for tick
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tick_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // accept() 仍可能 EINTR，你下面有處理
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }

    // configure timer: TICK_MS milliseconds interval
    struct itimerval itv;
    itv.it_value.tv_sec = TICK_MS / 1000;
    itv.it_value.tv_usec = (TICK_MS % 1000) * 1000;
    itv.it_interval = itv.it_value;
    if (setitimer(ITIMER_REAL, &itv, NULL) != 0) {
        perror("setitimer");
        return 1;
    }

    // start timer worker thread
    pthread_t tid_timer;
    if (pthread_create(&tid_timer, NULL, timer_worker, NULL) != 0) {
        perror("pthread_create timer_worker");
        return 1;
    }
    pthread_detach(tid_timer);

    // setup network server
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) != 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 8) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server (soft-timer) listening on port %d with %d rooms.\n", PORT, MAX_ROOMS);
    printf("TICK_MS=%d (ticks/sec=%d), SLOT_DURATION=%d sec (%d ticks), CHECKIN_TIMEOUT=%d sec (%d ticks)\n",
           TICK_MS, TICKS_PER_SEC, SLOT_DURATION, SLOT_TICKS, CHECKIN_TIMEOUT, CHECKIN_TICKS);
    printf("--- Waiting for clients ---\n");

    while (1) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        int *pclient = malloc(sizeof(int));
        if (!pclient) {
            perror("malloc");
            close(new_socket);
            continue;
        }
        *pclient = new_socket;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, pclient) != 0) {
            perror("pthread_create client");
            free(pclient);
            close(new_socket);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    pthread_mutex_destroy(&room_mutex);
    return 0;
}
