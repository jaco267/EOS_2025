//@ -0,0 +1,32 @@
#pragma once
#include <stdint.h>

#define SHM_NAME "/eos_room_state"
#define SHM_SIZE 4096

#ifndef MAX_ROOMS
#define MAX_ROOMS 3
#endif

#define MAX_WAIT_PREVIEW 4

typedef struct {
    int status;                 // 0 FREE, 1 RESERVED, 2 IN_USE
    int user_id;
    int reserve_count_today;
    int wait_count;
    int wait_preview[MAX_WAIT_PREVIEW];
    int remain_sec;             // 對 RESERVED: checkin剩餘; 對 IN_USE: session剩餘; FREE: 0
} room_snap_t;

typedef struct {
    uint32_t magic;             // 0x52554D53 "RUMS" 類似
    uint32_t version;           // 1
    uint64_t seq;               // server 每次更新 +1
    int selected_room;          // g_selected_room
    room_snap_t rooms[MAX_ROOMS];
} shm_state_t;

int  shm_init_server(void);
void shm_publish_locked(void);
void shm_close_server(void);
