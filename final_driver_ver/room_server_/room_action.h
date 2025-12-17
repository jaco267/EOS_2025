#if !defined(ROOMACTION)
#define ROOMACTION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globe_var.h"
#include "room_timer.h"


// helper: status -> string
const char* get_status_str(room_status_t status);

/**
 * return formatted string with all room status.
 * Caller must free returned buffer.
 */
char* get_all_status(int room_id);
// [SUGGEST-FREE] 新增：回傳目前 FREE 房清單字串（malloc，caller free）
char* get_free_rooms_hint(void);
// ------ operations (reserve/checkin/release/extend) ------

int reserve_room(int room_id, int user_id, const char* name);

int check_in(int room_id, int user_id);
int release_room(int room_id, int user_id);
int extend_room(int room_id, int user_id);
int update_display_selected_locked(void); // caller holds room_mutex


#endif // ROOMACTION
