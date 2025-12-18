@ -0,0 +1,81 @@
#include "shm_state.h"
#include "globe_var.h"
#include "user_db.h"
#include "wait_queue.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "room_timer.h"

static int g_shm_fd = -1;
static shm_state_t *g_shm = NULL;

int shm_init_server(void){
    g_shm_fd = shm_open(SHM_NAME, O_CREAT|O_RDWR, 0666);
    if (g_shm_fd < 0) return -1;
    if (ftruncate(g_shm_fd, SHM_SIZE) != 0) return -2;
    g_shm = (shm_state_t*)mmap(NULL, SHM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shm == MAP_FAILED) return -3;

    memset(g_shm, 0, SHM_SIZE);
    g_shm->magic = 0x52554D53; // 'RUMS'
    g_shm->version = 1;
    g_shm->seq = 0;
    return 0;
}

static int calc_remain_sec_locked(int i, uint64_t now_tick){
    room_t *r = &rooms[i];
    if (r->status == FREE) return 0;

    uint64_t elapsed = (now_tick >= r->reserve_tick) ? (now_tick - r->reserve_tick) : 0;
    if (r->status == RESERVED){
        int remain_ticks = (int)CHECKIN_TICKS - (int)elapsed;
        if (remain_ticks < 0) remain_ticks = 0;
        return (remain_ticks * TICK_MS) / 1000;
    }
    if (r->status == IN_USE){
        uint64_t allowed = SLOT_TICKS + (r->extend_used ? SLOT_TICKS : 0);
        int remain_ticks = (int)allowed - (int)elapsed;
        if (remain_ticks < 0) remain_ticks = 0;
        return (remain_ticks * TICK_MS) / 1000;
    }
    return 0;
}

// room_mutex 已經被 caller lock 住時呼叫
void shm_publish_locked(void){
    if (!g_shm) return;

    uint64_t now_tick = get_current_tick_snapshot();
    g_shm->selected_room = g_selected_room;

    for (int i=0;i<MAX_ROOMS;i++){
        room_t *r = &rooms[i];
        room_snap_t *s = &g_shm->rooms[i];
        s->status = (int)r->status;
        s->user_id = r->user_id;
        s->reserve_count_today = r->reserve_count_today;
        s->wait_count = r->wait_q.count;
        for (int k=0;k<MAX_WAIT_PREVIEW;k++){
            s->wait_preview[k] = -1;
            if (k < r->wait_q.count){
                int idx = (r->wait_q.head + k) % MAX_WAITING;
                s->wait_preview[k] = r->wait_q.users[idx];
            }
        }
        s->remain_sec = calc_remain_sec_locked(i, now_tick);
    }
    g_shm->seq++;
}

void shm_close_server(void){
    if (g_shm && g_shm != MAP_FAILED) munmap(g_shm, SHM_SIZE);
    if (g_shm_fd >= 0) close(g_shm_fd);
    // server 結束時要不要 shm_unlink 看你；demo 常保留、或結束時清掉都可以
    // shm_unlink(SHM_NAME);
}

