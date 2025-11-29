#if !defined(GLOBVAR)
#define GLOBVAR
#define MAX_TIMERS  16  //* timer_table size
#define TICK_MS     10  /* 一個 tick = 10ms */
typedef struct {
    int  id;
    int  active;
    int  periodic;           /* 0: one-shot, 1: periodic */
    int  timeout_ticks;      /* 原始 timeout（以 tick 為單位）、 */
    int  remaining_ticks;    /* 倒數用 */
    void (*cb)(void *);  //timeout 時需呼叫的 callback func
    void *arg;
} soft_timer_t;

//* timer table 
extern soft_timer_t timers[MAX_TIMERS];
extern int next_id;
extern long tv_start_;
extern long tv_start_u; 
#endif // GLOBVAR

