#if !defined(GLOBEVAR)
#define GLOBEVAR
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
// system config
#define MAX_ROOMS 3
#define PORT 8080

#define DEVICE_FILE "/dev/etx_device"
#define MAX_NAME_LEN 30 
// durations are still specified in seconds for easy reading.
// We'll convert to ticks using TICK_MS.
#define SLOT_DURATION 30        // seconds (one slot)
#define CHECKIN_TIMEOUT 10       // seconds to check-in after reservation

#define TICK_MS 100             // tick granularity (milliseconds)
#define TICKS_PER_SEC (1000 / TICK_MS)

// Derived tick counts
#define SLOT_TICKS (SLOT_DURATION * TICKS_PER_SEC)
#define CHECKIN_TICKS (CHECKIN_TIMEOUT * TICKS_PER_SEC)
typedef enum { FREE, RESERVED, IN_USE } room_status_t;

//* 新增欄位後請記得去room_server.c main() 修改 initialize room
typedef struct {
    int id;
    room_status_t status;
    uint64_t reserve_tick;   // tick when reserved / checked-in
    int extend_used;         // 0: not extended, 1: extended
    int user_id;             //* 輸入學號  
    int wait_count;          //* 等待人數
} room_t;

// shared resources
extern room_t rooms[MAX_ROOMS];
extern pthread_mutex_t room_mutex;
#define SIM_DAY_SECONDS 180    // 模擬：1 天 = 180 秒
extern int room_reservations_today[MAX_ROOMS];
// 新增：記錄上次重設是在第幾天（UNIX epoch / 一天的 index）
extern time_t g_last_reset_day;
// 新增：每間房有多少人在候補隊列
extern int room_waiting_count[MAX_ROOMS];
// global tick counter (tick increments in signal handler)
// use sig_atomic_t for signal-safety; worker will read into a wider type
//* dont put static in header file
extern volatile sig_atomic_t g_tick;

#endif // GLOBEVAR

