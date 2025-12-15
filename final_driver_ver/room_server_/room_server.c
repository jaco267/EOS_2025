// server_softtimer.c
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

// ---------- client handler ----------

void* client_handler(void* arg) {
  int client_sock = *(int*)arg;
  free(arg);
  char buffer[1024] = {0};
  char response[2048];
  printf("[SERVER] New client connected on socket %d.\n", client_sock);

  ssize_t valread = read(client_sock, buffer, sizeof(buffer)-1);
  if (valread <= 0) {
      printf("[SERVER] Client %d disconnected or read error.\n", client_sock);
      goto cleanup;
  }
  buffer[valread] = '\0';     buffer[strcspn(buffer, "\r\n")] = 0;

  char *cmd =  strtok(buffer, " ");
  char* tok_room = strtok(NULL, " ");
  char* tok_user = strtok(NULL, " ");
  int room_id = -1;
  int user_id = -1;
  if (tok_room) room_id = atoi(tok_room);
  if (tok_user) user_id = atoi(tok_user);
  
  if (cmd == NULL) {   snprintf(response, sizeof(response), "ERROR Please provide a command.");
  } else if (strcmp(cmd, "status") == 0) {
      char *s = get_all_status(room_id);
      snprintf(response, sizeof(response), "OK\n%s", s);
      free(s);
  } else if (room_id == -1 && strcmp(cmd, "status") != 0) { 
    snprintf(response, sizeof(response), "ERROR Invalid or missing Room ID.");
  } else if (room_id < 0 || room_id >= MAX_ROOMS) {         snprintf(response, sizeof(response), "ERROR Room ID %d is out of range (0-%d).", room_id, MAX_ROOMS-1);
  } else if (strcmp(cmd, "reserve") == 0) {
      if (user_id == -1){
        snprintf(response, sizeof(response), "ERROR: user_id should >0, got %d , usage : reserve <room_id> <user_id>", user_id);
      }else{
        int res = reserve_room(room_id, user_id);
        if (res == 0) {
            snprintf(response, sizeof(response), "OK Room %d reserved successfully. Check-in in %d seconds.", room_id, CHECKIN_TIMEOUT);
        } else if (res == -3) {
            snprintf(response, sizeof(response), "ERROR Room %d reservation failed. Daily limit reached.", room_id);
        } else if (res == -4) {
        snprintf(response, sizeof(response),
                 "WAIT Room %d is currently not free.\n"
                 "You have been added to the waiting list.\n"
                 "When the current session ends, the room will be reserved for a waiting client automatically.",
                 room_id);
        }else {
            snprintf(response, sizeof(response), "ERROR Room %d reservation failed. Room is not free.", room_id);
        }
      }

  } else if (strcmp(cmd, "checkin") == 0) {
      int res = check_in(room_id);
      if (res == 0) {
          snprintf(response, sizeof(response), "OK Room %d checked in. Session duration: %d seconds.", room_id, SLOT_DURATION);
      } else {
          snprintf(response, sizeof(response), "ERROR Room %d check-in failed. Status must be RESERVED.", room_id);
      }
  } else if (strcmp(cmd, "release") == 0) {
      int res = release_room(room_id);
      if (res == 0) {
          snprintf(response, sizeof(response), "OK Room %d released successfully.", room_id);
      } else {
          snprintf(response, sizeof(response), "ERROR Room %d release failed. Room is already FREE.", room_id);
      }
  } else if (strcmp(cmd, "extend") == 0) {
      int res = extend_room(room_id);
      if (res == 0) {
          snprintf(response, sizeof(response), "OK Room %d extended by %d seconds.", room_id, SLOT_DURATION);
      } else {
          snprintf(response, sizeof(response), "ERROR Room %d extension failed. Room is not IN_USE, already extended, or there is a pending reservation.", room_id);
      }
  } else {snprintf(response, sizeof(response), "ERROR Unknown command: %s.", cmd);}

  send(client_sock, response, strlen(response), 0);

cleanup:
  close(client_sock);
  printf("[SERVER] Client %d handler finished.\n", client_sock);
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
        rooms[i].user_id     = -1;
        rooms[i].wait_count  = 0;
        //*  wait queue 
          rooms[i].wait_q.head = 0;
          rooms[i].wait_q.tail = 0;
          rooms[i].wait_q.count = 0;
        rooms[i].reserve_count_today = 0; 
    }
    // 取得「今天」的 day index
    time_t now = time(NULL);
    g_last_reset_day = now / SIM_DAY_SECONDS;
    // setup SIGALRM handler for tick
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tick_handler;
    // block other signals while in handler
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) != 0) {perror("sigaction");return 1;}
    // configure timer: TICK_MS milliseconds interval
    struct itimerval itv;
    itv.it_value.tv_sec = TICK_MS / 1000;
    itv.it_value.tv_usec = (TICK_MS % 1000) * 1000;
    itv.it_interval = itv.it_value; // periodic
    if (setitimer(ITIMER_REAL, &itv, NULL) != 0) {perror("setitimer");return 1;}
    // start timer worker thread
    pthread_t tid_timer;
    if (pthread_create(&tid_timer, NULL, timer_worker, NULL) != 0) {perror("pthread_create timer_worker");return 1;}
    pthread_detach(tid_timer);

    // setup network server (same as original)
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 8) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server (soft-timer) listening on port %d with %d rooms.\n", PORT, MAX_ROOMS);
    printf("TICK_MS=%d (ticks/sec=%d), SLOT_DURATION=%d sec (%d ticks), CHECKIN_TIMEOUT=%d sec (%d ticks)\n",
           TICK_MS, TICKS_PER_SEC, SLOT_DURATION, SLOT_TICKS, CHECKIN_TIMEOUT, CHECKIN_TICKS);
    printf("--- Waiting for clients ---\n");

    while (1) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            if (errno == EINTR) continue; // interrupted by signal, retry
            perror("accept");
            continue;
        }
        int *pclient = malloc(sizeof(int));
        if (!pclient) { perror("malloc"); close(new_socket); continue; }
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

    // cleanup (unreachable in this server design)
    close(server_fd);
    pthread_mutex_destroy(&room_mutex);
    return 0;
}
