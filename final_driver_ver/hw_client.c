// hw_client.c
// Hardware display client: reads SHM (/eos_room_state) and writes to /dev/etx_device
// Shows selected_room on 7seg, and selected_room status on LED.
//
// LED mapping (same as your driver expects):
//   led 1 => FREE
//   led 2 => RESERVED
//   led 3 => IN_USE
//
// 7seg mapping: "7seg <digit>" where digit is room_id (0..2)
//
// Run this ONLY on the Raspberry Pi with the kernel driver inserted.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <signal.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "room_server_/shm_state.h"

#ifndef DEVICE_FILE
#define DEVICE_FILE "/dev/etx_device"
#endif

static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static int write_device_cmd(int fd, const char *cmd) {
    size_t n = strlen(cmd);
    ssize_t w = write(fd, cmd, n);
    if (w < 0) return -1;
    return 0;
}

static int hw_apply(int room_id, int status) {
    // status: 0 FREE, 1 RESERVED, 2 IN_USE
    int fd = open(DEVICE_FILE, O_WRONLY);
    if (fd < 0) {
        perror("open /dev/etx_device");
        return -1;
    }

    char cmd[64];

    // 7seg: show room id
    snprintf(cmd, sizeof(cmd), "7seg %d", room_id);
    if (write_device_cmd(fd, cmd) != 0) {
        perror("write 7seg");
        close(fd);
        return -1;
    }

    // LED: show status (FREE->1, RESERVED->2, IN_USE->3)
    int led_id = status + 1;
    if (led_id < 1) led_id = 1;
    if (led_id > 3) led_id = 3;

    snprintf(cmd, sizeof(cmd), "led %d", led_id);
    if (write_device_cmd(fd, cmd) != 0) {
        perror("write led");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        fprintf(stderr, "Hint: start room_server first (it creates %s)\n", SHM_NAME);
        return 1;
    }

    shm_state_t *shm = (shm_state_t*)mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return 1;
    }

    // Optional sanity check
    if (shm->magic != 0x52554D53u) { // 'RUMS'
        fprintf(stderr, "WARN: SHM magic mismatch: 0x%08x (expected 0x52554D53)\n", shm->magic);
    }
    if (shm->version != 1) {
        fprintf(stderr, "WARN: SHM version mismatch: %u (expected 1)\n", shm->version);
    }

    printf("[HW_CLIENT] Connected to SHM %s. Writing display to %s\n", SHM_NAME, DEVICE_FILE);
    printf("[HW_CLIENT] Ctrl+C to stop.\n");

    uint64_t last_seq = 0;
    int last_room = -999;
    int last_status = -999;

    while (!g_stop) {
        // Snapshot fields (no locks here; rely on seq monotonic updates)
        uint64_t seq = shm->seq;
        int sel = shm->selected_room;

        if (sel < 0 || sel >= MAX_ROOMS) {
            // No valid selection; don't spam hardware
            usleep(100 * 1000);
            continue;
        }

        int st = shm->rooms[sel].status;

        // Update on seq change OR selection/status change
        if (seq != last_seq || sel != last_room || st != last_status) {
            if (hw_apply(sel, st) == 0) {
                const char *st_str = (st==0)?"FREE":(st==1)?"RESERVED":"IN_USE";
                printf("[HW_CLIENT] seq=%llu selected=%d status=%s\n",
                       (unsigned long long)seq, sel, st_str);
                fflush(stdout);
                last_seq = seq;
                last_room = sel;
                last_status = st;
            } else {
                // If driver not ready or permission issues, retry later
                fprintf(stderr, "[HW_CLIENT] apply failed; will retry...\n");
            }
        }

        // Poll interval
        usleep(100 * 1000); // 100ms
    }

    printf("\n[HW_CLIENT] Stopping...\n");
    munmap(shm, SHM_SIZE);
    close(shm_fd);
    return 0;
}
