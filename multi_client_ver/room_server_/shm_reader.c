#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "room_server_/shm_state.h"

int main() {
    int fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (fd < 0) { perror("shm_open"); return 1; }

    shm_state_t *shm = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }

    printf("magic=0x%08X ver=%u seq=%llu selected=%d\n",
           shm->magic, shm->version,
           (unsigned long long)shm->seq, shm->selected_room);

    for (int i=0;i<MAX_ROOMS;i++){
        printf("room%d st=%d uid=%d remain=%d wait=%d\n",
               i, shm->rooms[i].status, shm->rooms[i].user_id,
               shm->rooms[i].remain_sec, shm->rooms[i].wait_count);
    }
    return 0;
}
