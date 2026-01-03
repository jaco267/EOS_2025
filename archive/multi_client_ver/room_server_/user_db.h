#if !defined(USERDB)
#define USERDB

#include "globe_var.h"

#define MAX_USERS 64
#define USER_NAME_LEN 32

typedef struct {
    int id;
    char name[USER_NAME_LEN];
    int active_room;   // -1 if none, else room_id (RESERVED or IN_USE)
    int active_state;  // 0 none, 1 reserved, 2 in_use
} user_t;

// All functions assume caller already holds room_mutex
user_t* user_get_locked(int user_id);
int user_register_locked(int user_id, const char* name);

int user_can_reserve_locked(int user_id);         // user not holding any room
int user_mark_reserved_locked(int user_id, int room_id);
int user_mark_inuse_locked(int user_id, int room_id);
void user_clear_active_locked(int user_id);

const char* user_name_locked(int user_id);

#endif

