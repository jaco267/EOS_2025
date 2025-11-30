#if !defined(ROOMACTION)
#define ROOMACTION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globe_var.h"

// get current tick (make a snapshot)
// returns as 64-bit unsigned to avoid overflow in calculations
static inline uint64_t get_current_tick_snapshot() {
    // read volatile sig_atomic_t and cast to uint64_t
    return (uint64_t)g_tick;
}


// helper: status -> string
const char* get_status_str(room_status_t status);

/**
 * return formatted string with all room status.
 * Caller must free returned buffer.
 */
char* get_all_status();
// ------ operations (reserve/checkin/release/extend) ------

int reserve_room(int room_id);

int check_in(int room_id);
int release_room(int room_id);

int extend_room(int room_id);
#endif // ROOMACTION
