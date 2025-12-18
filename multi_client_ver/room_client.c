#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
// [CLIENT-HW] 新增：client 本機控制麵包板
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#define SERVER_IP "127.0.0.1"
//#define SERVER_IP "192.168.0.2"
#define PORT 8080
#define BUFFER_SIZE 2048
#define DEVICE_FILE "/dev/etx_device"

// client 端用來輪詢切換房號（可先寫死 12；若不一致，下面會自動從 server 回覆修正）
#define DEFAULT_MAX_ROOMS 12

static pthread_mutex_t net_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_selected_room = 0;
static int g_max_rooms = DEFAULT_MAX_ROOMS;


static void print_help() {
    printf("--- Smart Room Reservation Client ---\n");
    printf("Commands:\n");
    printf("  status [room_id]\n");
    printf("     - status            : show all rooms\n");
    printf("     - status 0          : show room 0 and update 7seg/LED\n");
    printf("\n");
    printf("  register <user_id> <name>\n");
    printf("     - register 312513129 Hong\n");
    printf("\n");
    printf("  reserve <room_id> <user_id> [name]\n");
    printf("     - reserve 0 312513129\n");
    printf("     - reserve 0 312513129 Hong   (auto register/update name)\n");
    printf("\n");
    printf("  checkin <room_id> [user_id]\n");
    printf("  release <room_id> [user_id]\n");
    printf("  extend  <room_id> [user_id]\n");
    printf("     - if user_id is provided, server will verify ownership\n");
    printf("\n");
    printf("  exit\n");
    printf("-------------------------------------\n");
}

static char room_to_7seg_char(int room_id) {
    if (room_id >= 0 && room_id <= 9) return (char)('0' + room_id);
    if (room_id == 10) return 'A';
    if (room_id == 11) return 'B';
    return 'G';
}

static void hw_apply_from_response(const char *resp) {
    if (!resp) return;

    int rid = -1, st = -1, uid = -1;

    char *dup = strdup(resp);
    if (!dup) return;

    for (char *line = strtok(dup, "\n"); line; line = strtok(NULL, "\n")) {
        int tr, ts, tu;
        if (sscanf(line, "HW room=%d status=%d user=%d", &tr, &ts, &tu) == 3) {
            rid = tr; st = ts; uid = tu;
        }
    }
    free(dup);

    if (rid < 0 || st < 0) return;

    int fd = open(DEVICE_FILE, O_WRONLY);
    if (fd < 0) {
    perror("open /dev/etx_device (O_WRONLY) failed");
    return;
}

    char cmd[64];
    char c = room_to_7seg_char(rid);
    snprintf(cmd, sizeof(cmd), "7seg %c", c);
    (void)write(fd, cmd, strlen(cmd));

    int led_id = st + 1;              // FREE/RESERVED/IN_USE -> 1/2/3
    if (led_id < 1) led_id = 1;
    if (led_id > 3) led_id = 3;
    snprintf(cmd, sizeof(cmd), "led %d", led_id);
    (void)write(fd, cmd, strlen(cmd));

    close(fd);
}

static void maybe_update_max_rooms_from_error(const char *resp) {
    // 解析 "ERROR Room ID out of range (0-%d)."
    int max_id = -1;
    if (resp && sscanf(resp, "ERROR Room ID out of range (0-%d).", &max_id) == 1) {
        if (max_id >= 0) g_max_rooms = max_id + 1;
        if (g_max_rooms <= 0) g_max_rooms = DEFAULT_MAX_ROOMS;
        g_selected_room = 0;
    }
}

static int send_cmd_and_process(const char *cmd, int print_response) {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    // ====== [SYNC] 用指令裡的 room_id 更新 g_selected_room ======
    // 注意：外面呼叫 send_cmd_and_process() 前已經鎖 net_mutex，所以這裡不要再鎖
    {
        char op[16] = {0};
        int rid = -1;

        // 只處理有帶 room_id 的指令 (status/select/reserve/checkin/release/extend)
        if (sscanf(cmd, "%15s %d", op, &rid) == 2) {
            if (!strcmp(op, "status") || !strcmp(op, "select") ||
                !strcmp(op, "reserve") || !strcmp(op, "checkin") ||
                !strcmp(op, "release") || !strcmp(op, "extend")) {
                if (rid >= 0 && rid < g_max_rooms) {
                    g_selected_room = rid;
                }
            }
        }
    }
    // ==========================================================
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address\n");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    (void)send(sock, cmd, strlen(cmd), 0);

    memset(buffer, 0, sizeof(buffer));
    int total = 0;
    while (1) {
        int n = read(sock, buffer + total, (int)sizeof(buffer) - 1 - total);
        if (n <= 0) break;
        total += n;
        if (total >= (int)sizeof(buffer) - 1) break;
    }
    buffer[total] = '\0';
    close(sock);

    if (total <= 0) {
        if (print_response) printf("Error: server disconnected.\n");
        return -1;
    }

    if (print_response) {
        printf("\n--- Server Response ---\n%s\n-----------------------\n", buffer);
    }

    // 外面呼叫 send_cmd_and_process() 前已經鎖 net_mutex，所以這裡不要再鎖
    maybe_update_max_rooms_from_error(buffer);
    hw_apply_from_response(buffer);

    return 0;
}

static void* button_listener_client(void *arg) {
    (void)arg;

    while (1) {
        int fd = open(DEVICE_FILE, O_RDONLY);
        if (fd < 0) {
            perror("open /dev/etx_device failed (client button)");
            sleep(1);
            continue;
        }

        char buf[64];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (n <= 0) continue;

        buf[n] = '\0';

        if (strstr(buf, "BTN:1")) {
            int next;

            pthread_mutex_lock(&net_mutex);
            if (g_max_rooms <= 0) g_max_rooms = DEFAULT_MAX_ROOMS;
            next = (g_selected_room + 1) % g_max_rooms;
            g_selected_room = next;
            pthread_mutex_unlock(&net_mutex);

            char cmd[64];
            // 用 select：回覆短、但仍會附 HW line（你 server 要記得在 select 分支 append_hw_line）
            snprintf(cmd, sizeof(cmd), "status %d", next);

            // 按鈕觸發不想洗版：print_response=0
            pthread_mutex_lock(&net_mutex);
            send_cmd_and_process(cmd, 0);
            pthread_mutex_unlock(&net_mutex);
        }
    }
    return NULL;
}

static void* status_poller_client(void *arg) {
    (void)arg;
    while (1) {
        int rid;
        pthread_mutex_lock(&net_mutex);
        rid = g_selected_room;
        pthread_mutex_unlock(&net_mutex);

        char cmd[64];
        snprintf(cmd, sizeof(cmd), "status %d", rid);

        pthread_mutex_lock(&net_mutex);
        send_cmd_and_process(cmd, 0);   // 不洗版，但會更新 HW
        pthread_mutex_unlock(&net_mutex);

        usleep(300 * 1000); // 300ms 可自行調 200~1000ms
    }
    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char command[256];
    print_help();
    pthread_t tid_btn;
if (pthread_create(&tid_btn, NULL, button_listener_client, NULL) != 0) {
    perror("pthread_create button_listener_client");
} else {
    pthread_detach(tid_btn);
}

pthread_t tid_poll;
if (pthread_create(&tid_poll, NULL, status_poller_client, NULL) != 0) {
    perror("pthread_create status_poller_client");
} else {
    pthread_detach(tid_poll);
}
pthread_mutex_lock(&net_mutex);
send_cmd_and_process("status 9999", 0);  // 讓 server 回 out-of-range，client 解析到 (0-2) -> g_max_rooms=3
send_cmd_and_process("status 0", 0);     // 立刻刷新一次顯示
pthread_mutex_unlock(&net_mutex);
    while (1) {
        printf("\nEnter command: ");
        if (!fgets(command, sizeof(command), stdin)) break;
        command[strcspn(command, "\n")] = 0;
        if (strcasecmp(command, "exit") == 0) {printf("Exiting client.\n");break;}
        if (strcasecmp(command, "help") == 0) {print_help();continue;}
        if (command[0] == '\0') continue;
        //*-------socket-----------------
        pthread_mutex_lock(&net_mutex);
        send_cmd_and_process(command, 1);
        pthread_mutex_unlock(&net_mutex);
    }
    return 0;
}
