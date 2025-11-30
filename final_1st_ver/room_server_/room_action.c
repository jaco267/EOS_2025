#include "room_action.h"

// helper: status -> string
const char* get_status_str(room_status_t status) {
    switch(status) {
        case FREE: return "FREE (🟢)";
        case RESERVED: return "RESERVED (🔴)";
        case IN_USE: return "IN_USE (🔴)";
        default: return "UNKNOWN";
    }
}