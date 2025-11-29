#include "glob_var.h"

//* soft timer table 
soft_timer_t timers[MAX_TIMERS];
int next_id = 1; //* soft timer table id  
long tv_start_;
long tv_start_u; 