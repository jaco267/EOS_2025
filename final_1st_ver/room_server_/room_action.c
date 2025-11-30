#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globe_var.h"
#include "room_action.h"
// 輔助函式：將房間狀態轉換為字串
const char* get_status_str(room_status_t status) {
    switch (status) {
        case FREE: return "FREE (🟢)";
        case RESERVED: return "RESERVED (🟡)";
        case IN_USE: return "IN_USE (🔴)";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 取得所有房間的當前狀態。
 * @return 包含所有房間狀態的格式化字串。
 */
char* get_all_status() {
    pthread_mutex_lock(&room_mutex);
    // 預估狀態字串長度
    size_t required_size = MAX_ROOMS * 60 + 200;
    char *response = (char*)malloc(required_size);
    if (!response) {
        pthread_mutex_unlock(&room_mutex);
        return strdup("ERROR: Memory allocation failed.");
    }
    strcpy(response, "--- Room Status ---\n");
    for (int i = 0; i < MAX_ROOMS; i++) {
        char room_info[100];
        long time_elapsed = time(NULL) - rooms[i].reserve_time;   
        snprintf(room_info, sizeof(room_info),
                 "Room %d | Status: %s%s | Reserve Count: %d | Time Elapsed: %lds\n",
                 rooms[i].id,
                 get_status_str(rooms[i].status),
                 rooms[i].extend_used ? " (Extended)" : "",
                 room_reservations_today[i],
                 (rooms[i].status != FREE) ? time_elapsed : 0L);
        strcat(response, room_info);
    }
    strcat(response, "-------------------\n");
    pthread_mutex_unlock(&room_mutex);
    return response;
}
/**
 * @brief 預約房間
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (已被預約或次數超限)
 */
int reserve_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    // 檢查單日使用上限（簡化：不實際檢查使用者 ID，僅檢查房間次數）
    if (room_reservations_today[room_id] >= 2) {
        pthread_mutex_unlock(&room_mutex);
        return -3; // 使用次數超限
    }
    // 檢查是否已存在預約（簡化：不檢查使用者 ID，假設每個客戶端代表一個唯一使用者）
    for(int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].status == RESERVED) { // 實際應用中，需檢查是否為同一個使用者
             // 假設簡化規則：不得同時持有多筆預約 (在此不做複雜檢查)
        }
    }
    if (r->status == FREE) {
        r->status = RESERVED;
        r->reserve_time = time(NULL);
        r->extend_used = 0;
        room_reservations_today[room_id]++;
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d reserved.\n", room_id);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1; // 房間不可用
}
/**
 * @brief 報到/Check-in
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (狀態不對)
 */
int check_in(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status == RESERVED) {
        // 更新 reserve_time 作為 session start time
        r->status = IN_USE;
        r->reserve_time = time(NULL); 
        printf("[SERVER LOG] Room %d checked in.\n", room_id);
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1; // 無法報到
}
/**
 * @brief 釋放房間
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (狀態不對)
 */
int release_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    
    if (r->status != FREE) {
        r->status = FREE;
        printf("[SERVER LOG] Room %d released.\n", room_id);
        // 實際應用中，應通知候補清單的下一位使用者
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }

    pthread_mutex_unlock(&room_mutex);
    return -1; // 無法釋放
}
/**
 * @brief 延長使用
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (已延長過或狀態不對)
 */
int extend_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    
    // 檢查是否有人候補（簡化：沒有人 RESERVED 就視為無人候補）
    int has_reservation = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].status == RESERVED && i != room_id) {
            has_reservation = 1;
            break;
        }
    }

    if (r->status == IN_USE && r->extend_used == 0 && !has_reservation) {
        r->extend_used = 1;
        printf("[SERVER LOG] Room %d extended.\n", room_id);
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }
    
    pthread_mutex_unlock(&room_mutex);
    // 實際應用中，需區分已延長過或有人候補
    return -1; 
}



