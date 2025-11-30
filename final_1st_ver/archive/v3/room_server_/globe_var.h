#if !defined(GLOBEVAR)
#define GLOBEVAR
#include <time.h>
#include <pthread.h>
// 系統配置
#define MAX_ROOMS 3             // 房間總數
#define PORT 8080               // 伺服器監聽埠號
#define SLOT_DURATION 30        // 每個時段長度（秒），模擬 30 分鐘
#define CHECKIN_TIMEOUT 5       // 預約後報到時限（秒），模擬 5 分鐘
// 房間狀態
typedef enum {FREE, RESERVED, IN_USE} room_status_t;
// 房間結構
typedef struct {
    int id;
    room_status_t status;
    time_t reserve_time;        // 預約或開始使用 (Check-in) 的時間戳
    int extend_used;            // 0: 未延長, 1: 已延長
} room_t;
// 共享資源
extern room_t rooms[MAX_ROOMS];
extern pthread_mutex_t room_mutex;
extern int room_reservations_today[MAX_ROOMS]; // 簡化：記錄當日預約次數，實際應用需每日重置
#endif // GLOBEVAR
