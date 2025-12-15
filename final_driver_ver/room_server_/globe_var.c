#include "globe_var.h"

// shared resources
room_t rooms[MAX_ROOMS];
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;

// global tick counter (tick increments in signal handler)
// use sig_atomic_t for signal-safety; worker will read into a wider type
volatile sig_atomic_t g_tick = 0;
// 新增
long g_last_reset_day = -1;   // 尚未初始化
