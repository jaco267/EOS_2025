#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <unistd.h>
#include "glob_var.h"
#include "soft_timer_api.h"

/* ====== Soft timer API ====== */
int st_start(int ms, int periodic, void (*cb)(void *), void *arg){
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
  
int st_cancel(int id){
  int i;
  for (i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].active && timers[i].id == id) {
        timers[i].active = 0;
        return 0;
    }
  }
  return -1;
}