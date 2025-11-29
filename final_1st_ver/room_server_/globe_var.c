#include "globe_var.h"
room_t rooms[MAX_ROOMS];
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;
int room_reservations_today[MAX_ROOMS] = {0}; 