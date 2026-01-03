/* 
 * timer_diff2.c  (Q9 版示意)
 * 比較三種 itimer 在 CPU-bound / IO-bound 下的差異
 */
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/* counters */
int SIGALRM_count   = 0;
int SIGVTALRM_count = 0;
int SIGPROF_count   = 0;

/* handlers */
void SIGALRM_handler (int signum)   { SIGALRM_count++; }
void SIGVTALRM_handler (int signum) { SIGVTALRM_count++; }
void SIGPROF_handler (int signum)   { SIGPROF_count++; }

/* from講義：IO-bound 工作 */
void IO_WORKS(void)
{
    int fd, ret;
    char buffer[100];
    int i;
    for (i = 0; i < 10000; i++) {
        sleep(0.01); // 10ms sleep，CPU 不用，profile timer 不累計
    }
    for (i = 0; i < 1600000; i++) {
	if ((fd = open("/etc/hosts", O_RDONLY)) < 0) {
	    perror("Open /etc/hosts");
	    exit(EXIT_FAILURE);
	}
        do {
            ret = read(fd, buffer, 100);
        } while(ret > 0);
        close(fd);
    }
}

/* 新增：CPU-bound 工作（一直 busy loop） */
void CPU_WORKS(void)
{
    volatile unsigned long dummy = 0;
    unsigned long i;

    /* 跑一大輪算術運算，大概會吃一段時間的 CPU */
    for (i = 0; i < 500000000UL; i++) {
        dummy += i;
    }
}

/* 封裝：設定三個 itimer 都為 100ms 週期 */
void start_all_timers(void)
{
    struct itimerval timer;

    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_sec     = 0;
    timer.it_value.tv_usec    = 100000;  /* 第一次 100ms 後觸發 */
    timer.it_interval.tv_sec  = 0;
    timer.it_interval.tv_usec = 100000;  /* 之後每 100ms 一次 */

    setitimer(ITIMER_REAL,   &timer, NULL);
    setitimer(ITIMER_VIRTUAL,&timer, NULL);
    setitimer(ITIMER_PROF,   &timer, NULL);
}

/* 歸零 counters */
void reset_counters(void)
{
    SIGALRM_count   = 0;
    SIGVTALRM_count = 0;
    SIGPROF_count   = 0;
}

int main (int argc, char **argv)
{
    struct sigaction SA_SIGALRM, SA_SIGVTALRM, SA_SIGPROF;

    /* 安裝三個 signal handler */
    memset(&SA_SIGALRM, 0, sizeof(SA_SIGALRM));
    SA_SIGALRM.sa_handler = SIGALRM_handler;
    sigaction(SIGALRM, &SA_SIGALRM, NULL);

    memset(&SA_SIGVTALRM, 0, sizeof(SA_SIGVTALRM));
    SA_SIGVTALRM.sa_handler = SIGVTALRM_handler;
    sigaction(SIGVTALRM, &SA_SIGVTALRM, NULL);

    memset(&SA_SIGPROF, 0, sizeof(SA_SIGPROF));
    SA_SIGPROF.sa_handler = SIGPROF_handler;
    sigaction(SIGPROF, &SA_SIGPROF, NULL);

    /* ===== 測試一：IO-bound ===== */
    printf("=== IO-bound test ===\n");
    reset_counters();
    start_all_timers();
    IO_WORKS();
    printf("[IO] SIGALRM_count   = %d\n", SIGALRM_count);
    printf("[IO] SIGVTALRM_count = %d\n", SIGVTALRM_count);
    printf("[IO] SIGPROF_count   = %d\n\n", SIGPROF_count);

    /* ===== 測試二：CPU-bound ===== */
    printf("=== CPU-bound test ===\n");
    reset_counters();
    start_all_timers();
    CPU_WORKS();
    printf("[CPU] SIGALRM_count   = %d\n", SIGALRM_count);
    printf("[CPU] SIGVTALRM_count = %d\n", SIGVTALRM_count);
    printf("[CPU] SIGPROF_count   = %d\n", SIGPROF_count);

    return 0;
}

