#include "room_action.h"
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include "wait_queue.h"
#include "user_db.h"

const char* get_status_str(room_status_t status) {
    switch(status) {
        case FREE: return "FREE (🟢)";
        case RESERVED: return "RESERVED (🟡)";
        case IN_USE: return "IN_USE (🔴)";
        default: return "UNKNOWN";
    }
}

static void hw_update_selected_room_locked(int room_id) {
    // caller already holds room_mutex
    int fd = open(DEVICE_FILE, O_WRONLY);
    if (fd < 0) return;

    // [AUTO-HW] 修改：7seg 改顯示「房號」(00~11)，不再顯示 user_id 末兩碼
	char cmd[64];
	snprintf(cmd, sizeof(cmd), "7seg %02d", rooms[room_id].id);  // 或直接用 room_id
	(void)write(fd, cmd, strlen(cmd));


    // LED: show room status (FREE=1, RESERVED=2, IN_USE=3)
    int led_id = (int)rooms[room_id].status + 1;
    snprintf(cmd, sizeof(cmd), "led %d", led_id);
    (void)write(fd, cmd, strlen(cmd));

    close(fd);
}
// [AUTO-HW] 新增：統一入口，更新「目前選取房間 g_selected_room」的 LED/7seg
// caller 必須已經持有 room_mutex
int update_display_selected_locked(void) {
    // caller holds room_mutex
    if (g_selected_room < 0 || g_selected_room >= MAX_ROOMS) return -1;
    hw_update_selected_room_locked(g_selected_room);
    return 0;
}
char* get_all_status(int room_id) {
    pthread_mutex_lock(&room_mutex);

    size_t required_size = MAX_ROOMS * 220 + 256;
    char *resp = (char*)malloc(required_size);
    if (!resp) {
        pthread_mutex_unlock(&room_mutex);
        return strdup("ERROR: malloc failed.");
    }
    strcpy(resp, "--- Room Status ---\n");

    uint64_t now_tick = get_current_tick_snapshot();

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (room_id != -1 && i != room_id) continue;

        char tmp[512];
        uint64_t elapsed_ticks = 0;
        if (rooms[i].status != FREE) {
            if (now_tick >= rooms[i].reserve_tick) elapsed_ticks = now_tick - rooms[i].reserve_tick;
        }
        long elapsed_sec = (elapsed_ticks * TICK_MS) / 1000;

        // wait queue -> string
        char wait_buf[128]; wait_buf[0] = '\0';
        if (rooms[i].wait_q.count == 0) {
            strcpy(wait_buf, "[]");
        } else {
            strcat(wait_buf, "[");
            for (int j = 0; j < rooms[i].wait_q.count; j++) {
                int idx = (rooms[i].wait_q.head + j) % MAX_WAITING;
                char num[16];
                snprintf(num, sizeof(num), "%d", rooms[i].wait_q.users[idx]);
                strcat(wait_buf, num);
                if (j != rooms[i].wait_q.count - 1) strcat(wait_buf, ",");
            }
            strcat(wait_buf, "]");
        }

        int uid = rooms[i].user_id;
        const char* uname = (uid > 0) ? user_name_locked(uid) : "-";

        snprintf(tmp, sizeof(tmp),
            "Room %d | %s%s | Uid: %d (%s) | Reserve Count: %d | Time Elapsed: %lds\n"
            "       | wait count: %d | wait queue: %s\n",
            rooms[i].id,
            get_status_str(rooms[i].status),
            rooms[i].extend_used ? " (Extended)" : "",
            uid, uname,
            rooms[i].reserve_count_today,
            (rooms[i].status != FREE) ? elapsed_sec : 0L,
            rooms[i].wait_q.count,
            wait_buf
        );
        strncat(resp, tmp, required_size - strlen(resp) - 1);
    }

    strncat(resp, "-------------------\n", required_size - strlen(resp) - 1);
    // [AUTO-HW] 修改：不在 status() 裡直接更新硬體
    // 硬體更新改由「狀態變更事件」(reserve/checkin/release/timeout/promote) 自動觸發
    // only when selecting a room, update HW to avoid pin shortage
    //if (room_id != -1 && room_id >= 0 && room_id < MAX_ROOMS) {
    //    hw_update_selected_room_locked(room_id);
    //}

    pthread_mutex_unlock(&room_mutex);
    return resp;
}

// return codes:
//  0 ok
// -2 bad room
// -3 daily limit
// -6 user already has active room
// -7 already in wait queue
// -4 added to wait queue
// -5 wait queue full
int reserve_room(int room_id, int user_id, const char* name) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    if (user_id <= 0) return -9;

    pthread_mutex_lock(&room_mutex);

    // auto register/update name if provided
    if (name && name[0]) {
        (void)user_register_locked(user_id, name);
    }

    // must be registered
    if (!user_get_locked(user_id)) {
        pthread_mutex_unlock(&room_mutex);
        return -10; // not registered
    }

    // one user can hold only one active room at a time
    if (!user_can_reserve_locked(user_id)) {
        pthread_mutex_unlock(&room_mutex);
        return -6;
    }

    room_t *r = &rooms[room_id];

    if (r->reserve_count_today >= 2) {
        pthread_mutex_unlock(&room_mutex);
        return -3;
    }

    if (r->status == FREE) {
        r->status = RESERVED;
        r->reserve_tick = get_current_tick_snapshot();
        r->extend_used = 0;
        r->user_id = user_id;
        r->warn_5s_sent = 0; // [AUTO-WARN] 新增：新預約先清提醒
        r->reserve_count_today++;

        user_mark_reserved_locked(user_id, room_id);
        // [AUTO-HW] 新增：若本房是選取房，狀態變更後立刻更新 LED/7seg
        //if (room_id == g_selected_room) update_display_selected_locked();
        // [AUTO-HW] 修改：reserve 成功後，直接把顯示切到這間房
        g_selected_room = room_id;
        update_display_selected_locked();
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d reserved by user %d.\n", room_id, user_id);
        return 0;
        //pthread_mutex_unlock(&room_mutex);
        //printf("[SERVER LOG] Room %d reserved by user %d.\n", room_id, user_id);
        //return 0;
    }

    // room busy -> wait queue (avoid duplicates)
    if (wait_contains(&r->wait_q, user_id)) {
        pthread_mutex_unlock(&room_mutex);
        return -7;
    }

    if (wait_enqueue(&r->wait_q, user_id) == 0) {
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d busy. User %d added to wait queue.\n", room_id, user_id);
        return -4;
    }

    pthread_mutex_unlock(&room_mutex);
    return -5;
}

// if user_id != -1, enforce ownership
int check_in(int room_id, int user_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];

    if (r->status != RESERVED) {
        pthread_mutex_unlock(&room_mutex);
        return -1;
    }

    // if (user_id != -1 && r->user_id != user_id) {
    //     pthread_mutex_unlock(&room_mutex);
    //     return -8; // not owner
    // }

    r->status = IN_USE;
    r->warn_5s_sent = 0; // [AUTO-WARN] 新增：開始使用，清提醒
    r->reserve_tick = get_current_tick_snapshot();
    r->extend_used = 0;

    if (r->user_id > 0) user_mark_inuse_locked(r->user_id, room_id);
    // [AUTO-HW] 新增：checkin 狀態變更後自動更新硬體
    //if (room_id == g_selected_room) update_display_selected_locked();
    // [AUTO-HW] 修改：checkin 成功後，直接把顯示切到這間房
    g_selected_room = room_id;
    update_display_selected_locked();

    pthread_mutex_unlock(&room_mutex);
    printf("[SERVER LOG] Room %d checked in.\n", room_id);
    return 0;
    //pthread_mutex_unlock(&room_mutex);
    //printf("[SERVER LOG] Room %d checked in.\n", room_id);
    //return 0;
}

int release_room(int room_id, int user_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];

    if (r->status == FREE) {
        pthread_mutex_unlock(&room_mutex);
        return -1;
    }

    if (user_id != -1 && r->user_id != user_id) {
        pthread_mutex_unlock(&room_mutex);
        return -8; // not owner
    }

    int old_user = r->user_id;

    r->status = FREE;
    r->extend_used = 0;
    r->reserve_tick = 0;
    r->warn_5s_sent = 0; // [AUTO-WARN] 新增：釋放後清提醒
    r->user_id = -1;

    if (old_user > 0) user_clear_active_locked(old_user);

    int next_user;
    if (wait_dequeue(&r->wait_q, &next_user) == 0) {
        // assign to next waiting user (also obey daily limit)
        if (r->reserve_count_today < 2 && user_can_reserve_locked(next_user)) {
            r->status = RESERVED;
            r->reserve_tick = get_current_tick_snapshot();
            r->extend_used = 0;
            r->warn_5s_sent = 0; // [AUTO-WARN] 新增：候補接手，清提醒
            r->user_id = next_user;
            r->reserve_count_today++;

            user_mark_reserved_locked(next_user, room_id);

            printf("[SERVER LOG] Room %d assigned to waiting user %d.\n", room_id, next_user);
        } else {
            // if can't assign, clear their active (they shouldn't have one, but be safe)
            user_clear_active_locked(next_user);
            printf("[SERVER LOG] Room %d: dequeued user %d but cannot assign.\n", room_id, next_user);
        }
    }
// [AUTO-HW] 新增：release/遞補造成狀態變更後，自動更新硬體
    //if (room_id == g_selected_room) update_display_selected_locked();
    // [AUTO-HW] 修改：release/遞補完成後，直接把顯示切到這間房
    g_selected_room = room_id;
    update_display_selected_locked();

    pthread_mutex_unlock(&room_mutex);
    return 0;
}

int extend_room(int room_id, int user_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];

    if (r->status != IN_USE || r->extend_used != 0) {
        pthread_mutex_unlock(&room_mutex);
        return -1;
    }

    if (user_id != -1 && r->user_id != user_id) {
        pthread_mutex_unlock(&room_mutex);
        return -8;
    }

    r->extend_used = 1;
    r->warn_5s_sent = 0; // [AUTO-WARN] 新增：延長後，允許在最終 5 秒再提醒一次
    pthread_mutex_unlock(&room_mutex);

    printf("[SERVER LOG] Room %d extended.\n", room_id);
    return 0;
}
