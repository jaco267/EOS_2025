#include "wait_queue.h"

int wait_enqueue(wait_queue_t *q, int user_id) {
    if (q->count >= MAX_WAITING)
        return -1;

    q->users[q->tail] = user_id;
    q->tail = (q->tail + 1) % MAX_WAITING;
    q->count++;
    return 0;
}

int wait_dequeue(wait_queue_t *q, int *user_id) {
    if (q->count == 0)
        return -1;

    *user_id = q->users[q->head];
    q->head = (q->head + 1) % MAX_WAITING;
    q->count--;
    return 0;
}

int wait_contains(wait_queue_t *q, int user_id) {
    for (int i = 0; i < q->count; i++) {
        int idx = (q->head + i) % MAX_WAITING;
        if (q->users[idx] == user_id) return 1;
    }
    return 0;
}
