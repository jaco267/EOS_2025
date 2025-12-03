#include "globe_var.h"
#include "room_timer.h"



// ---------- soft-timer worker & tick handler ----------

// signal handler: invoked every TICK_MS ms; do minimal work
void tick_handler(int sig) {
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
        
        // ==== 模擬「一天 = 180 秒」的跨日檢查 ====
        time_t now_sec = time(NULL);
        long today = now_sec / SIM_DAY_SECONDS;  // 第幾個模擬日
        if (g_last_reset_day == -1) {
            g_last_reset_day = today;
        } else if (today != g_last_reset_day) {
            // 模擬過了一天 → 歸零
            for (int i = 0; i < MAX_ROOMS; i++) {
                room_reservations_today[i] = 0;
            }
            g_last_reset_day = today;
            printf("[TIMER] New (sim) day. room_reservations_today reset to 0.\n");
        }
        
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
                    // 若有候補 → 重新預約給候補
        	if (room_waiting_count[i] > 0) {
            	room_waiting_count[i]--;
            	r->status       = RESERVED;
            	r->reserve_tick = now_tick;
            	r->extend_used  = 0;
            	room_reservations_today[i]++;

            	printf("[TIMER] Room %d assigned to waiting list after timeout. "
                   "Remaining waiters = %d.\n",
                   i, room_waiting_count[i]);
        	}
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
                //** extended =1 : 2 SLOT = 60s,  extended=0: 1 SLOT = 30s   */
                uint64_t allowed = SLOT_TICKS + (r->extend_used ? SLOT_TICKS : 0);
                uint64_t elapsed = 0;
                if (now_tick >= r->reserve_tick) elapsed = now_tick - r->reserve_tick;
                if (elapsed >= allowed) {
                    printf("[TIMER] Room %d session ended at tick %llu (elapsed %llu ticks). Auto-release.\n",
                           i, (unsigned long long)now_tick, (unsigned long long)elapsed);
                    r->status = FREE;
                    r->extend_used = 0;
                    r->reserve_tick = 0;
                    //候補發生
                    if (room_waiting_count[i] > 0) {
            	room_waiting_count[i]--;
            	r->status       = RESERVED;
            	r->reserve_tick = now_tick;
            	r->extend_used  = 0;
            	room_reservations_today[i]++;

            	printf("[TIMER] Room %d assigned to waiting list after session end. "
                   "Remaining waiters = %d.\n",
                   i, room_waiting_count[i]);
        }
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
