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