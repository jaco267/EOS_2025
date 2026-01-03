#include "user_db.h"
#include <string.h>
#include <stdio.h>

static user_t g_users[MAX_USERS];
static int g_user_count = 0;

user_t* user_get_locked(int user_id) {
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i].id == user_id) return &g_users[i];
    }
    return NULL;
}

int user_register_locked(int user_id, const char* name) {
    if (user_id <= 0) return -1;
    user_t* u = user_get_locked(user_id);
    if (u) {
        // update name
        if (name && name[0]) {
            strncpy(u->name, name, USER_NAME_LEN - 1);
            u->name[USER_NAME_LEN - 1] = '\0';
        }
        return 0;
    }
    if (g_user_count >= MAX_USERS) return -2;

    user_t* nu = &g_users[g_user_count++];
    nu->id = user_id;
    nu->active_room = -1;
    nu->active_state = 0;
    if (name && name[0]) {
        strncpy(nu->name, name, USER_NAME_LEN - 1);
        nu->name[USER_NAME_LEN - 1] = '\0';
    } else {
        strncpy(nu->name, "UNKNOWN", USER_NAME_LEN - 1);
        nu->name[USER_NAME_LEN - 1] = '\0';
    }
    return 0;
}

int user_can_reserve_locked(int user_id) {
    user_t* u = user_get_locked(user_id);
    if (!u) return 0;                // must register first
    return (u->active_room == -1);
}

int user_mark_reserved_locked(int user_id, int room_id) {
    user_t* u = user_get_locked(user_id);
    if (!u) return -1;
    u->active_room = room_id;
    u->active_state = 1;
    return 0;
}

int user_mark_inuse_locked(int user_id, int room_id) {
    user_t* u = user_get_locked(user_id);
    if (!u) return -1;
    u->active_room = room_id;
    u->active_state = 2;
    return 0;
}

void user_clear_active_locked(int user_id) {
    user_t* u = user_get_locked(user_id);
    if (!u) return;
    u->active_room = -1;
    u->active_state = 0;
}

const char* user_name_locked(int user_id) {
    user_t* u = user_get_locked(user_id);
    return u ? u->name : "UNREGISTERED";
}

