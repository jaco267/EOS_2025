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

#include "glob_var.h"
#include "call_back.h"
#include "soft_timer_api.h"

int pipe_fds[2];   /* pipe_fds[0] = read, pipe_fds[1] = write */

/* ====== Timer ISR: SIGALRM handler，只能寫 pipe ====== */
void sigalrm_handler(int signum){
    unsigned char b = 1;
    ssize_t n = write(pipe_fds[1], &b, 1);
    (void)n;   // 明示「我故意不使用 n」
}

int tick_count = 0;
int ms_time = 0;
/* ====== Worker loop：在 main thread 裡跑 ====== */
void worker_loop(int a_id,int b_id, int c_id){
    unsigned char buf[64];
    while (1) {
        ssize_t n = read(pipe_fds[0], buf, sizeof(buf));
        if (n <= 0) continue;

        
        ms_time = tick_count*10;
        // if (tick_count%5 == 0){  //every 5*10 = 50 ms
        //   printf("tick_count: %d = %d ms\n",tick_count, ms_time);
        // }
        if (ms_time >= 2000){
          int cancel_code = st_cancel(b_id);
          if (cancel_code >= 0){
            printf("cancel timer b sucessufully\n");
          }
        }
        tick_count++;
        // if (ms_time > )
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



int main(void){
    struct sigaction sa;
    struct itimerval it;
    struct timeval tv_start;
    /* 建立 pipe：ISR 寫、worker 讀 */
    if (pipe(pipe_fds) < 0) {perror("pipe");exit(EXIT_FAILURE);}
    /* 安裝 SIGALRM handler */
    memset(&sa, 0, sizeof(sa));
    //* 使用 setitimer(ITIMER_REAL) 產生固定 tick（例如 10ms）
    //* every 10 ms 執行 handler write short msg into pipe 
    sa.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &sa, NULL);
    //** 設定 ITIMER_REAL：每 10ms 一次 tick ------
    memset(&it, 0, sizeof(it));
    it.it_value.tv_sec     = 0;
    it.it_value.tv_usec    = TICK_MS * 1000;
    it.it_interval.tv_sec  = 0;
    it.it_interval.tv_usec = TICK_MS * 1000;
    setitimer(ITIMER_REAL, &it, NULL);
    
    //*------------------------------------
    
    gettimeofday(&tv_start, NULL);
    tv_start_ = tv_start.tv_sec;
    tv_start_u = tv_start.tv_usec; 
    printf("Soft timers started at %ld.%06ld\n",
           (long)tv_start.tv_sec, (long)tv_start.tv_usec);
    /* 註冊 3 個 soft timers */
    int a_id = st_start(100, 0, cb_A, NULL);   /* 100ms, one-shot */
    int b_id = st_start(250, 1, cb_B, NULL);   /* 250ms, periodic */
    int c_id = st_start(700, 1, cb_C, NULL);   /* 700ms, periodic */

    /* 進入 worker_loop：等 tick、更新軟體計時器 */
    worker_loop(a_id,b_id,c_id);

    return 0;
}

