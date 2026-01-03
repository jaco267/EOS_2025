#if !defined(ROOMTIMER)
#define ROOMTIMER
#include <stdio.h>
#include <stdlib.h>
#include "globe_var.h"
#include "room_action.h"
// get current tick (make a snapshot)
// returns as 64-bit unsigned to avoid overflow in calculations
static inline uint64_t get_current_tick_snapshot() {
    // read volatile sig_atomic_t and cast to uint64_t
    return (uint64_t)g_tick;
}

// ---------- soft-timer worker & tick handler ----------

// signal handler: invoked every TICK_MS ms; do minimal work
void tick_handler(int sig);

// soft-timer worker: scans rooms, handles timeouts and auto-release
void* timer_worker(void* arg);

#endif // ROOMTIMER
