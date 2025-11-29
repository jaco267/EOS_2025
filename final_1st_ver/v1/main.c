#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define MAX_ROOMS 2
#define MAX_RESERVES 2
#define SLOT_DURATION 30 // 30 min, for simulation we use seconds
#define CHECKIN_TIMEOUT 5 // 5 min -> seconds for simulation

typedef enum {FREE, RESERVED, IN_USE} room_status_t;

typedef struct {
    int id;
    room_status_t status;
    time_t reserve_time;   // timestamp of reservation
    int extend_used;       // 0: not extended, 1: extended
} room_t;

room_t rooms[MAX_ROOMS];
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t room_sem; // 用於等待房間可用

// 模擬 LED 顯示
void display_room_status(room_t *r) {
    const char *state[] = {"FREE", "RESERVED", "IN_USE"};
    printf("[Room %d] Status: %s%s\n", r->id, state[r->status], r->extend_used ? " (Extended)" : "");
}

// 預約房間
int reserve_room(int room_id) {
    pthread_mutex_lock(&room_mutex);

    room_t *r = &rooms[room_id];
    if (r->status == FREE) {
        r->status = RESERVED;
        r->reserve_time = time(NULL);
        r->extend_used = 0;
        printf("Room %d reserved.\n", room_id);
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }

    pthread_mutex_unlock(&room_mutex);
    return -1; // 已被預約
}

// Check-in
int check_in(int room_id) {
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status == RESERVED) {
        r->status = IN_USE;
        printf("Room %d checked in.\n", room_id);
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1; // 不能報到
}

// Release
int release_room(int room_id) {
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status != FREE) {
        r->status = FREE;
        printf("Room %d released.\n", room_id);
        sem_post(&room_sem); // 喚醒等待預約的使用者
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

// Extend
int extend_room(int room_id) {
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status == IN_USE && r->extend_used == 0) {
        r->extend_used = 1;
        printf("Room %d extended.\n", room_id);
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

// Timer thread
void* timer_thread(void* arg) {
    while(1) {
        pthread_mutex_lock(&room_mutex);
        time_t now = time(NULL);
        for (int i=0; i<MAX_ROOMS; i++) {
            room_t *r = &rooms[i];
            if (r->status == RESERVED && now - r->reserve_time >= CHECKIN_TIMEOUT) {
                printf("Room %d reservation timeout!\n", i);
                r->status = FREE;
                sem_post(&room_sem);
            }
            if (r->status == IN_USE && now - r->reserve_time >= SLOT_DURATION) {
                printf("Room %d session ended.\n", i);
                r->status = FREE;
                sem_post(&room_sem);
            }
        }
        pthread_mutex_unlock(&room_mutex);
        sleep(1); // 每秒掃描一次
    }
    return NULL;
}

// 模擬使用者行為
void* user_thread(void* arg) {
    // int id = *(int*)arg;
    while(1) {
        sem_wait(&room_sem); // 等待有空房
        for (int i=0; i<MAX_ROOMS; i++) {
            if (reserve_room(i) == 0) {
                sleep(2); // 模擬到現場報到
                check_in(i);
                sleep(3); // 使用中
                if (rand()%2) extend_room(i); // 隨機延長
                release_room(i);
                break;
            }
        }
        sleep(1);
    }
    return NULL;
}

int main() {
    srand(time(NULL));

    for (int i=0; i<MAX_ROOMS; i++) {
        rooms[i].id = i;
        rooms[i].status = FREE;
        rooms[i].extend_used = 0;
    }

    sem_init(&room_sem, 0, MAX_ROOMS);

    pthread_t t_timer, t_user1, t_user2;
    pthread_create(&t_timer, NULL, timer_thread, NULL);

    int u1=0, u2=1;
    pthread_create(&t_user1, NULL, user_thread, &u1);
    pthread_create(&t_user2, NULL, user_thread, &u2);

    pthread_join(t_timer, NULL);
    pthread_join(t_user1, NULL);
    pthread_join(t_user2, NULL);

    sem_destroy(&room_sem);
    pthread_mutex_destroy(&room_mutex);
    return 0;
}
