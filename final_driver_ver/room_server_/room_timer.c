#include "globe_var.h"
#include "room_timer.h"
#include "user_db.h"
#include "wait_queue.h"

void tick_handler(int sig) {
    (void)sig;
    g_tick++;
}

void* timer_worker(void* arg) {
    (void)arg;
    printf("[TIMER] Worker thread started. Tick ms = %d\n", TICK_MS);
    uint64_t last_seen_tick = get_current_tick_snapshot();

    while (1) {
        uint64_t now_tick = get_current_tick_snapshot();
        if (now_tick == last_seen_tick) {
            struct timespec req = {0, 50 * 1000 * 1000};
            nanosleep(&req, NULL);
            continue;
        }
        last_seen_tick = now_tick;

        pthread_mutex_lock(&room_mutex);
        // [AUTO-HW] 新增：只要選取房間的狀態有變更，最後統一更新一次硬體
        int hw_dirty = 0;

        // reset daily counters (sim day)
        time_t now_sec = time(NULL);
        long today = now_sec / SIM_DAY_SECONDS;
        if (g_last_reset_day == -1) {
            g_last_reset_day = today;
        } else if (today != g_last_reset_day) {
            for (int i = 0; i < MAX_ROOMS; i++) {
                rooms[i].reserve_count_today = 0;
            }
            g_last_reset_day = today;
            printf("[TIMER] New (sim) day. reserve_count_today reset.\n");
        }

        for (int i = 0; i < MAX_ROOMS; i++) {
            room_t *r = &rooms[i];

            if (r->status == RESERVED) {
                uint64_t elapsed = (now_tick >= r->reserve_tick) ? (now_tick - r->reserve_tick) : 0;

                if (elapsed >= CHECKIN_TICKS) {
                    int old_user = r->user_id;
                    printf("[TIMER] Room %d reservation timeout. Auto-release.\n", i);

                    r->status = FREE;
                    r->extend_used = 0;
                    r->reserve_tick = 0;
                    r->user_id = -1;

                    if (old_user > 0) user_clear_active_locked(old_user);

                    int next_user;
                    if (wait_dequeue(&r->wait_q, &next_user) == 0) {
                        if (r->reserve_count_today < 2 && user_can_reserve_locked(next_user)) {
                            r->status = RESERVED;
                            r->reserve_tick = now_tick;
                            r->extend_used = 0;
                            r->user_id = next_user;
                            r->reserve_count_today++;

                            user_mark_reserved_locked(next_user, i);

                            printf("[TIMER] Room %d assigned to waiting user %d after timeout.\n", i, next_user);
                        } else {
                            user_clear_active_locked(next_user);
                        }
                    }
                     // [AUTO-HW] 新增：timeout 導致狀態變更，若影響到選取房就標記
                    if (i == g_selected_room) hw_dirty = 1;

                }
            } else if (r->status == IN_USE) {
                uint64_t allowed = SLOT_TICKS + (r->extend_used ? SLOT_TICKS : 0);
                uint64_t elapsed = (now_tick >= r->reserve_tick) ? (now_tick - r->reserve_tick) : 0;

                if (elapsed >= allowed) {
                    int old_user = r->user_id;
                    printf("[TIMER] Room %d session ended. Auto-release.\n", i);

                    r->status = FREE;
                    r->extend_used = 0;
                    r->reserve_tick = 0;
                    r->user_id = -1;

                    if (old_user > 0) user_clear_active_locked(old_user);

                    int next_user;
                    if (wait_dequeue(&r->wait_q, &next_user) == 0) {
                        if (r->reserve_count_today < 2 && user_can_reserve_locked(next_user)) {
                            r->status = RESERVED;
                            r->reserve_tick = now_tick;
                            r->extend_used = 0;
                            r->user_id = next_user;
                            r->reserve_count_today++;

                            user_mark_reserved_locked(next_user, i);

                            printf("[TIMER] Room %d assigned to waiting user %d after session.\n", i, next_user);
                        } else {
                            user_clear_active_locked(next_user);
                        }
                    }
                     // [AUTO-HW] 新增：timeout 導致狀態變更，若影響到選取房就標記
                    if (i == g_selected_room) hw_dirty = 1;

                }
            }
        }
        // [AUTO-HW] 新增：只在需要時更新一次（避免每個 room timeout 都寫硬體）
        if (hw_dirty) update_display_selected_locked();

        pthread_mutex_unlock(&room_mutex);

        struct timespec req = {0, 10 * 1000 * 1000};
        nanosleep(&req, NULL);
    }
    return NULL;
}
