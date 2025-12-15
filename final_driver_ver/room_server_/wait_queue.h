#if !defined(WAITQUEUE)
#define WAITQUEUE
// #include "globe_var.h"
// 最大候補人數
#define MAX_WAITING 8

// waiting queue（FIFO）
typedef struct {
    int users[MAX_WAITING];
    int head;
    int tail;
    int count;
} wait_queue_t;

int wait_enqueue(wait_queue_t *q, int user_id) ;
int wait_dequeue(wait_queue_t *q, int *user_id);

#endif // WAITQUEUE