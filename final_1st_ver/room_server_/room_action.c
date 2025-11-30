#include "room_action.h"

// helper: status -> string
const char* get_status_str(room_status_t status) {
    switch(status) {
        case FREE: return "FREE (🟢)";
        case RESERVED: return "RESERVED (🟡)";
        case IN_USE: return "IN_USE (🔴)";
        default: return "UNKNOWN";
    }
}


/** return formatted string with all room status.
 * Caller must free returned buffer.*/
char* get_all_status() {
    pthread_mutex_lock(&room_mutex);
    size_t required_size = MAX_ROOMS * 120 + 200;
    char *resp = (char*)malloc(required_size);
    if (!resp) {pthread_mutex_unlock(&room_mutex); return strdup("ERROR: malloc failed.");}
    strcpy(resp, "--- Room Status ---\n");
    uint64_t now_tick = get_current_tick_snapshot();
    for (int i = 0; i < MAX_ROOMS; i++) {
        char tmp[200];
        uint64_t elapsed_ticks = 0; //* reserve or checkin 狀態過了多久
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
        pthread_mutex_unlock(&room_mutex); return -3;
    }
    room_t *r = &rooms[room_id];
    if (r->status == FREE) {
        r->status = RESERVED;
        r->reserve_tick = get_current_tick_snapshot();
        r->extend_used = 0;
        room_reservations_today[room_id]++;
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d reserved at tick %llu.\n", room_id, 
            (unsigned long long)r->reserve_tick);
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
    //todo 這有點怪 為什麼其他房間有reserve 就不能reserve?
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
