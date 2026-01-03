#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <unistd.h>
#include "call_back.h"
#include "glob_var.h"
/* ====== Demo 用的 callback ====== */
void cb_A(void *arg){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // printf("[A] one-shot 100ms fired at %ld.%06ld\n",
        //    (long)tv.tv_sec, (long)tv.tv_usec);
    
        // printf("[A] one-shot 100ms fired at %ld.%06ld\n",
        //     (long)tv.tv_sec- tv_start_, (long)tv.tv_usec-tv_start_u);
    
    long ttt = ((long)tv.tv_sec-tv_start_)*1000000 + ((long)tv.tv_usec-tv_start_u);
    float ttt2 = (float)ttt/1000;
    printf("[A] one-shot 100ms fired at  %f ms\n", ttt2);
    /* 故意在 callback 裡 sleep，觀察 bounded inaccuracy */
    usleep(50 * 1000);  /* 50ms */
}

void cb_B(void *arg){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // printf("[B] periodic 250ms fired at %ld.%06ld\n",
    //        (long)tv.tv_sec, (long)tv.tv_usec);
    // printf("[B] periodic 250ms fired at %ld.%06ld\n",
    //     (long)tv.tv_sec-tv_start_, (long)tv.tv_usec-tv_start_u);
    long ttt = ((long)tv.tv_sec-tv_start_)*1000000 + ((long)tv.tv_usec-tv_start_u);
    float ttt2 = (float)ttt/1000;
    printf("[B] periodic 250ms fired at  %f ms\n", ttt2);
}
void cb_C(void *arg){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // printf("[C] periodic 700ms fired at %ld.%06ld\n",
    //        (long)tv.tv_sec, (long)tv.tv_usec);

    long ttt = ((long)tv.tv_sec-tv_start_)*1000000 + ((long)tv.tv_usec-tv_start_u);
    float ttt2 = (float)ttt/1000;
    printf("[C] periodic 700ms fired at  %f ms\n", ttt2);
}
