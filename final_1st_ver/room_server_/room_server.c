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

// system config
#define MAX_ROOMS 3
#define PORT 8080

// durations are still specified in seconds for easy reading.
// We'll convert to ticks using TICK_MS.
#define SLOT_DURATION 30        // seconds (one slot)
#define CHECKIN_TIMEOUT 5       // seconds to check-in after reservation

#define TICK_MS 100             // tick granularity (milliseconds)
#define TICKS_PER_SEC (1000 / TICK_MS)

// Derived tick counts
#define SLOT_TICKS (SLOT_DURATION * TICKS_PER_SEC)
#define CHECKIN_TICKS (CHECKIN_TIMEOUT * TICKS_PER_SEC)

typedef enum { FREE, RESERVED, IN_USE } room_status_t;

typedef struct {
    int id;
    room_status_t status;
    uint64_t reserve_tick;   // tick when reserved / checked-in
    int extend_used;         // 0: not extended, 1: extended
} room_t;

// shared resources
room_t rooms[MAX_ROOMS];
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;
int room_reservations_today[MAX_ROOMS] = {0};

// global tick counter (tick increments in signal handler)
// use sig_atomic_t for signal-safety; worker will read into a wider type
static volatile sig_atomic_t g_tick = 0;

// helper: status -> string
const char* get_status_str(room_status_t status) {
    switch(status) {
        case FREE: return "FREE (🟢)";
        case RESERVED: return "RESERVED (🔴)";
        case IN_USE: return "IN_USE (🔴)";
        default: return "UNKNOWN";
    }
}

// get current tick (make a snapshot)
// returns as 64-bit unsigned to avoid overflow in calculations
static inline uint64_t get_current_tick_snapshot() {
    // read volatile sig_atomic_t and cast to uint64_t
    return (uint64_t)g_tick;
}

/**
 * return formatted string with all room status.
 * Caller must free returned buffer.
 */
char* get_all_status() {
    pthread_mutex_lock(&room_mutex);
    size_t required_size = MAX_ROOMS * 120 + 200;
    char *resp = (char*)malloc(required_size);
    if (!resp) {
        pthread_mutex_unlock(&room_mutex);
        return strdup("ERROR: malloc failed.");
    }
    strcpy(resp, "--- Room Status ---\n");
    uint64_t now_tick = get_current_tick_snapshot();
    for (int i = 0; i < MAX_ROOMS; i++) {
        char tmp[200];
        uint64_t elapsed_ticks = 0;
        if (rooms[i].status != FREE) {
            if (now_tick >= rooms[i].reserve_tick)
                elapsed_ticks = now_tick - rooms[i].reserve_tick;
            else
                elapsed_ticks = 0; // unlikely
        }
        long elapsed_sec = (elapsed_ticks * TICK_MS) / 1000;
        snprintf(tmp, sizeof(tmp),
                 "Room %d | Status: %s%s | Reserve Count: %d | Time Elapsed: %lds\n",
                 rooms[i].id,
                 get_status_str(rooms[i].status),
                 rooms[i].extend_used ? " (Extended)" : "",
                 room_reservations_today[i],
                 (rooms[i].status != FREE) ? elapsed_sec : 0L);
        strncat(resp, tmp, required_size - strlen(resp) - 1);
    }
    strncat(resp, "-------------------\n", required_size - strlen(resp) - 1);
    pthread_mutex_unlock(&room_mutex);
    return resp;
}

// ------ operations (reserve/checkin/release/extend) ------

int reserve_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    pthread_mutex_lock(&room_mutex);
    if (room_reservations_today[room_id] >= 2) {
        pthread_mutex_unlock(&room_mutex);
        return -3;
    }
    room_t *r = &rooms[room_id];
    if (r->status == FREE) {
        r->status = RESERVED;
        r->reserve_tick = get_current_tick_snapshot();
        r->extend_used = 0;
        room_reservations_today[room_id]++;
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d reserved at tick %llu.\n", room_id, (unsigned long long)r->reserve_tick);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

int check_in(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status == RESERVED) {
        r->status = IN_USE;
        r->reserve_tick = get_current_tick_snapshot(); // start of session
        r->extend_used = 0;
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d checked in at tick %llu.\n", room_id, (unsigned long long)r->reserve_tick);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

int release_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status != FREE) {
        r->status = FREE;
        r->extend_used = 0;
        r->reserve_tick = 0;
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d released.\n", room_id);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

int extend_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    pthread_mutex_lock(&room_mutex);
    // check for other pending reservations (simplified: any RESERVED except self)
    int has_other_reserved = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (i == room_id) continue;
        if (rooms[i].status == RESERVED) { has_other_reserved = 1; break; }
    }
    room_t *r = &rooms[room_id];
    if (r->status == IN_USE && r->extend_used == 0 && !has_other_reserved) {
        r->extend_used = 1;
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d extended at tick %llu.\n", room_id, (unsigned long long)get_current_tick_snapshot());
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

// ---------- soft-timer worker & tick handler ----------

// signal handler: invoked every TICK_MS ms; do minimal work
static void tick_handler(int sig) {
    // Increment the global tick counter (signal-safe)
    (void)sig; // unused
    g_tick++;
}

// soft-timer worker: scans rooms, handles timeouts and auto-release
void* timer_worker(void* arg) {
    (void)arg;
    printf("[TIMER] Worker thread started. Tick ms = %d\n", TICK_MS);
    uint64_t last_seen_tick = get_current_tick_snapshot();
    while (1) {
        uint64_t now_tick = get_current_tick_snapshot();
        if (now_tick == last_seen_tick) {
            // sleep a little to avoid busy loop; use nanosleep for sub-second
            struct timespec req = {0, 50 * 1000 * 1000}; // 50ms
            nanosleep(&req, NULL);
            continue;
        }
        // process every new tick since last_seen_tick (if any)
        // but we don't need to iterate tick-by-tick — just use now_tick snapshot
        last_seen_tick = now_tick;

        pthread_mutex_lock(&room_mutex);
        for (int i = 0; i < MAX_ROOMS; i++) {
            room_t *r = &rooms[i];
            if (r->status == RESERVED) {
                uint64_t elapsed = 0;
                if (now_tick >= r->reserve_tick) elapsed = now_tick - r->reserve_tick;
                if (elapsed >= CHECKIN_TICKS) {
                    // reservation timeout -> auto-release
                    printf("[TIMER] Room %d reservation timeout at tick %llu (elapsed %llu ticks). Auto-release.\n",
                           i, (unsigned long long)now_tick, (unsigned long long)elapsed);
                    r->status = FREE;
                    r->extend_used = 0;
                    r->reserve_tick = 0;
                } else if (elapsed >= CHECKIN_TICKS - (5 * TICKS_PER_SEC)) {
                    // countdown reminder (5 seconds before timeout)
                    // print every tick in that window could be noisy; we print once when entering window
                    // We check if elapsed == CHECKIN_TICKS - 5s to avoid repeated prints
                    if (elapsed == CHECKIN_TICKS - (5 * TICKS_PER_SEC)) {
                        printf("[TIMER] Room %d RESERVED: Check-in deadline approaching! (%llu/%d sec)\n",
                               i, (unsigned long long)(elapsed / TICKS_PER_SEC), CHECKIN_TIMEOUT);
                    }
                }
            } else if (r->status == IN_USE) {
                uint64_t allowed = SLOT_TICKS + (r->extend_used ? SLOT_TICKS : 0);
                uint64_t elapsed = 0;
                if (now_tick >= r->reserve_tick) elapsed = now_tick - r->reserve_tick;
                if (elapsed >= allowed) {
                    printf("[TIMER] Room %d session ended at tick %llu (elapsed %llu ticks). Auto-release.\n",
                           i, (unsigned long long)now_tick, (unsigned long long)elapsed);
                    r->status = FREE;
                    r->extend_used = 0;
                    r->reserve_tick = 0;
                } else if (elapsed == allowed - (5 * TICKS_PER_SEC)) {
                    printf("[TIMER] Room %d IN_USE: Session ending soon! (%llu/%llu sec)\n",
                           i, (unsigned long long)(elapsed / TICKS_PER_SEC), (unsigned long long)(allowed / TICKS_PER_SEC));
                }
            }
        }
        pthread_mutex_unlock(&room_mutex);
        // small sleep to yield CPU (worker loop will continue on next tick)
        struct timespec req = {0, 10 * 1000 * 1000}; // 10 ms
        nanosleep(&req, NULL);
    }
    return NULL;
}

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
    buffer[valread] = '\0';
    buffer[strcspn(buffer, "\r\n")] = 0;

    char *token = strtok(buffer, " ");
    char *cmd = token;
    int room_id = -1;
    if (cmd != NULL && strcmp(cmd, "status") != 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            room_id = atoi(token);
        }
    }

    if (cmd == NULL) {
        snprintf(response, sizeof(response), "ERROR Please provide a command.");
    } else if (strcmp(cmd, "status") == 0) {
        char *s = get_all_status();
        snprintf(response, sizeof(response), "OK\n%s", s);
        free(s);
    } else if (room_id == -1 && strcmp(cmd, "status") != 0) {
        snprintf(response, sizeof(response), "ERROR Invalid or missing Room ID.");
    } else if (room_id < 0 || room_id >= MAX_ROOMS) {
        snprintf(response, sizeof(response), "ERROR Room ID %d is out of range (0-%d).", room_id, MAX_ROOMS-1);
    } else if (strcmp(cmd, "reserve") == 0) {
        int res = reserve_room(room_id);
        if (res == 0) {
            snprintf(response, sizeof(response), "OK Room %d reserved successfully. Check-in in %d seconds.", room_id, CHECKIN_TIMEOUT);
        } else if (res == -3) {
            snprintf(response, sizeof(response), "ERROR Room %d reservation failed. Daily limit reached.", room_id);
        } else {
            snprintf(response, sizeof(response), "ERROR Room %d reservation failed. Room is not free.", room_id);
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
    } else {
        snprintf(response, sizeof(response), "ERROR Unknown command: %s.", cmd);
    }

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
        rooms[i].extend_used = 0;
        rooms[i].reserve_tick = 0;
        room_reservations_today[i] = 0;
    }

    // setup SIGALRM handler for tick
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tick_handler;
    // block other signals while in handler
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }

    // configure timer: TICK_MS milliseconds interval
    struct itimerval itv;
    itv.it_value.tv_sec = TICK_MS / 1000;
    itv.it_value.tv_usec = (TICK_MS % 1000) * 1000;
    itv.it_interval = itv.it_value; // periodic
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
