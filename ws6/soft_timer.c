/*
 * soft_timer_demo.c  (Q10 範例)
 * 使用 ITIMER_REAL + SIGALRM + pipe 模擬硬體 timer + soft timer worker
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_TIMERS  16
#define TICK_MS     10  /* 一個 tick = 10ms */

typedef struct {
    int  id;
    int  active;
    int  periodic;           /* 0: one-shot, 1: periodic */
    int  timeout_ticks;      /* 原始 timeout */
    int  remaining_ticks;    /* 倒數用 */
    void (*cb)(void *);
    void *arg;
} soft_timer_t;

soft_timer_t timers[MAX_TIMERS];
int next_id = 1;
int pipe_fds[2];   /* pipe_fds[0] = read, pipe_fds[1] = write */

/* ====== Timer ISR: SIGALRM handler，只能寫 pipe ====== */
void sigalrm_handler(int signum)
{
    unsigned char b = 1;
    ssize_t n = write(pipe_fds[1], &b, 1);
    (void)n;   // 明示「我故意不使用 n」
}

/* ====== Soft timer API ====== */
int st_start(int ms, int periodic, void (*cb)(void *), void *arg)
{
    int i;
    int ticks = ms / TICK_MS;
    if (ticks <= 0) ticks = 1;

    for (i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) {
            timers[i].id              = next_id++;
            timers[i].active          = 1;
            timers[i].periodic        = periodic;
            timers[i].timeout_ticks   = ticks;
            timers[i].remaining_ticks = ticks;
            timers[i].cb              = cb;
            timers[i].arg             = arg;
            return timers[i].id;
        }
    }
    return -1; /* no space */
}

int st_cancel(int id)
{
    int i;
    for (i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].active && timers[i].id == id) {
            timers[i].active = 0;
            return 0;
        }
    }
    return -1;
}

/* ====== Worker loop：在 main thread 裡跑 ====== */
void worker_loop(void)
{
    unsigned char buf[64];

    while (1) {
        ssize_t n = read(pipe_fds[0], buf, sizeof(buf));
        if (n <= 0) continue;

        /* 每個 byte = 一個 tick（10ms） */
        for (ssize_t k = 0; k < n; k++) {
            int i;
            for (i = 0; i < MAX_TIMERS; i++) {
                if (!timers[i].active) continue;

                timers[i].remaining_ticks--;
                if (timers[i].remaining_ticks <= 0) {
                    /* timeout → 呼叫 callback */
                    timers[i].cb(timers[i].arg);

                    if (timers[i].periodic) {
                        timers[i].remaining_ticks = timers[i].timeout_ticks;
                    } else {
                        timers[i].active = 0;
                    }
                }
            }
        }
    }
}

/* ====== Demo 用的 callback ====== */
void cb_A(void *arg)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("[A] one-shot 100ms fired at %ld.%06ld\n",
           (long)tv.tv_sec, (long)tv.tv_usec);

    /* 故意在 callback 裡 sleep，觀察 bounded inaccuracy */
    usleep(50 * 1000);  /* 50ms */
}

void cb_B(void *arg)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("[B] periodic 250ms fired at %ld.%06ld\n",
           (long)tv.tv_sec, (long)tv.tv_usec);
}

void cb_C(void *arg)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("[C] periodic 700ms fired at %ld.%06ld\n",
           (long)tv.tv_sec, (long)tv.tv_usec);
}

int main(void)
{
    struct sigaction sa;
    struct itimerval it;
    struct timeval tv_start;

    /* 建立 pipe：ISR 寫、worker 讀 */
    if (pipe(pipe_fds) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* 安裝 SIGALRM handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sa, NULL);

    /* 設定 ITIMER_REAL：每 10ms 一次 tick */
    memset(&it, 0, sizeof(it));
    it.it_value.tv_sec     = 0;
    it.it_value.tv_usec    = TICK_MS * 1000;
    it.it_interval.tv_sec  = 0;
    it.it_interval.tv_usec = TICK_MS * 1000;
    setitimer(ITIMER_REAL, &it, NULL);

    gettimeofday(&tv_start, NULL);
    printf("Soft timers started at %ld.%06ld\n",
           (long)tv_start.tv_sec, (long)tv_start.tv_usec);

    /* 註冊 3 個 soft timers */
    st_start(100, 0, cb_A, NULL);   /* 100ms, one-shot */
    st_start(250, 1, cb_B, NULL);   /* 250ms, periodic */
    st_start(700, 1, cb_C, NULL);   /* 700ms, periodic */

    /* 進入 worker_loop：等 tick、更新軟體計時器 */
    worker_loop();

    return 0;
}

