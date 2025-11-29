#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 系統配置
#define MAX_ROOMS 3             // 房間總數
#define PORT 8080               // 伺服器監聽埠號
#define SLOT_DURATION 30        // 每個時段長度（秒），模擬 30 分鐘
#define CHECKIN_TIMEOUT 5       // 預約後報到時限（秒），模擬 5 分鐘

// 房間狀態
typedef enum {FREE, RESERVED, IN_USE} room_status_t;

// 房間結構
typedef struct {
    int id;
    room_status_t status;
    time_t reserve_time;        // 預約或開始使用 (Check-in) 的時間戳
    int extend_used;            // 0: 未延長, 1: 已延長
} room_t;

// 共享資源
room_t rooms[MAX_ROOMS];
pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;
int room_reservations_today[MAX_ROOMS] = {0}; // 簡化：記錄當日預約次數，實際應用需每日重置

// 輔助函式：將房間狀態轉換為字串
const char* get_status_str(room_status_t status) {
    switch (status) {
        case FREE: return "FREE (🟢)";
        case RESERVED: return "RESERVED (🔴)";
        case IN_USE: return "IN_USE (🔴)";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 取得所有房間的當前狀態。
 * @return 包含所有房間狀態的格式化字串。
 */
char* get_all_status() {
    pthread_mutex_lock(&room_mutex);
    // 預估狀態字串長度
    size_t required_size = MAX_ROOMS * 60 + 200;
    char *response = (char*)malloc(required_size);
    if (!response) {
        pthread_mutex_unlock(&room_mutex);
        return strdup("ERROR: Memory allocation failed.");
    }

    strcpy(response, "--- Room Status ---\n");
    for (int i = 0; i < MAX_ROOMS; i++) {
        char room_info[100];
        long time_elapsed = time(NULL) - rooms[i].reserve_time;
        
        snprintf(room_info, sizeof(room_info),
                 "Room %d | Status: %s%s | Reserve Count: %d | Time Elapsed: %lds\n",
                 rooms[i].id,
                 get_status_str(rooms[i].status),
                 rooms[i].extend_used ? " (Extended)" : "",
                 room_reservations_today[i],
                 (rooms[i].status != FREE) ? time_elapsed : 0L);
        strcat(response, room_info);
    }
    strcat(response, "-------------------\n");
    pthread_mutex_unlock(&room_mutex);
    return response;
}

/**
 * @brief 預約房間
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (已被預約或次數超限)
 */
int reserve_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    // 檢查單日使用上限（簡化：不實際檢查使用者 ID，僅檢查房間次數）
    if (room_reservations_today[room_id] >= 2) {
        pthread_mutex_unlock(&room_mutex);
        return -3; // 使用次數超限
    }
    // 檢查是否已存在預約（簡化：不檢查使用者 ID，假設每個客戶端代表一個唯一使用者）
    for(int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].status == RESERVED) { // 實際應用中，需檢查是否為同一個使用者
             // 假設簡化規則：不得同時持有多筆預約 (在此不做複雜檢查)
        }
    }
    if (r->status == FREE) {
        r->status = RESERVED;
        r->reserve_time = time(NULL);
        r->extend_used = 0;
        room_reservations_today[room_id]++;
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d reserved.\n", room_id);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1; // 房間不可用
}

/**
 * @brief 報到/Check-in
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (狀態不對)
 */
int check_in(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status == RESERVED) {
        // 更新 reserve_time 作為 session start time
        r->status = IN_USE;
        r->reserve_time = time(NULL); 
        printf("[SERVER LOG] Room %d checked in.\n", room_id);
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1; // 無法報到
}

/**
 * @brief 釋放房間
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (狀態不對)
 */
int release_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    
    if (r->status != FREE) {
        r->status = FREE;
        printf("[SERVER LOG] Room %d released.\n", room_id);
        // 實際應用中，應通知候補清單的下一位使用者
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }

    pthread_mutex_unlock(&room_mutex);
    return -1; // 無法釋放
}

/**
 * @brief 延長使用
 * @param room_id 房間 ID
 * @return 0 成功, -1 失敗 (已延長過或狀態不對)
 */
int extend_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2; // 無效 ID

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    
    // 檢查是否有人候補（簡化：沒有人 RESERVED 就視為無人候補）
    int has_reservation = 0;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].status == RESERVED && i != room_id) {
            has_reservation = 1;
            break;
        }
    }

    if (r->status == IN_USE && r->extend_used == 0 && !has_reservation) {
        r->extend_used = 1;
        printf("[SERVER LOG] Room %d extended.\n", room_id);
        pthread_mutex_unlock(&room_mutex);
        return 0;
    }
    
    pthread_mutex_unlock(&room_mutex);
    // 實際應用中，需區分已延長過或有人候補
    return -1; 
}


/**
 * @brief 定時器執行緒：負責掃描房間狀態並執行超時/時段結束的自動釋放。
 */
void* timer_thread(void* arg) {
    printf("[TIMER] Timer thread started.\n");
    while(1) {
        pthread_mutex_lock(&room_mutex);
        time_t now = time(NULL);
        for (int i=0; i<MAX_ROOMS; i++) {
            room_t *r = &rooms[i];       
            // 處理 RESERVED 狀態
            if (r->status == RESERVED) {
                if (now - r->reserve_time >= CHECKIN_TIMEOUT) {
                    // 超時自動取消
                    printf("[TIMER] Room %d reservation timeout! (Auto-release)\n", i);
                    r->status = FREE;
                    // Note: 不減少 room_reservations_today[i] 因為預約已計數
                } else if (now - r->reserve_time >= CHECKIN_TIMEOUT - 5) {
                    // 倒數提醒 (5秒前，LED 應轉黃，但此處僅印出文字)
                    printf("[TIMER] Room %d RESERVED: Check-in deadline approaching! (%ld/%d sec)\n", 
                           i, now - r->reserve_time, CHECKIN_TIMEOUT);
                }
            }
            // 處理 IN_USE 狀態
            if (r->status == IN_USE) {
                time_t allowed_duration = SLOT_DURATION;
                if (r->extend_used) {
                    allowed_duration += SLOT_DURATION; // 延長後為 60 秒
                }
                if (now - r->reserve_time >= allowed_duration) {
                    // 時段結束自動釋放
                    printf("[TIMER] Room %d session ended! (Auto-release)\n", i);
                    r->status = FREE;
                } else if (now - r->reserve_time >= allowed_duration - 5) {
                    // 倒數提醒 (5秒前，LED 應轉黃)
                    printf("[TIMER] Room %d IN_USE: Session ending soon! (%ld/%ld sec)\n", 
                           i, now - r->reserve_time, allowed_duration);
                }
            }
        }
        pthread_mutex_unlock(&room_mutex);
        sleep(1); // 每秒掃描一次
    }
    return NULL;
}


/**
 * @brief 客戶端連接處理執行緒
 * @param arg 客戶端 socket 描述符
 */
void* client_handler(void* arg) {
    int client_sock = *(int*)arg;
    free(arg); // 釋放主執行緒分配的記憶體
    char buffer[1024] = {0};
    char response[1024];
    printf("[SERVER] New client connected on socket %d.\n", client_sock);
    // 接收客戶端命令
    int valread = read(client_sock, buffer, 1024);
    if (valread <= 0) {
        printf("[SERVER] Client %d disconnected or error.\n", client_sock);
        goto cleanup;
    }
    // 移除換行符
    buffer[strcspn(buffer, "\n")] = 0;
    // 解析命令: CMD ROOM_ID
    char *token = strtok(buffer, " ");
    char *cmd = token;
    int room_id = -1;
    if (cmd != NULL && strcmp(cmd, "status") != 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            room_id = atoi(token);
        }
    }
    // 處理命令
    if (cmd == NULL) {
        snprintf(response, sizeof(response), "ERROR Please provide a command.");
    } else if (strcmp(cmd, "status") == 0) {
        char *status_data = get_all_status();
        strcpy(response, "OK\n");
        strcat(response, status_data);
        free(status_data);
    } else if (room_id == -1 && strcmp(cmd, "status") != 0) {
        snprintf(response, sizeof(response), "ERROR Invalid or missing Room ID.");
    } else if (room_id < 0 || room_id >= MAX_ROOMS) {
        snprintf(response, sizeof(response), "ERROR Room ID %d is out of range (0-%d).", room_id, MAX_ROOMS-1);
    } else if (strcmp(cmd, "reserve") == 0) {
        int res = reserve_room(room_id);
        if (res == 0) {
            snprintf(response, sizeof(response), "OK Room %d reserved successfully. Check-in in %d seconds.", room_id, CHECKIN_TIMEOUT);
        } else if (res == -3) {
            snprintf(response, sizeof(response), "ERROR Room %d reservation failed. Daily limit reached.", room_id);
        } else {
            snprintf(response, sizeof(response), "ERROR Room %d reservation failed. Room is not free.", room_id);
        }
    } else if (strcmp(cmd, "checkin") == 0) {
        int res = check_in(room_id);
        if (res == 0) {
            snprintf(response, sizeof(response), "OK Room %d checked in. Session duration: %d seconds.", room_id, SLOT_DURATION);
        } else {
            snprintf(response, sizeof(response), "ERROR Room %d check-in failed. Status must be RESERVED.", room_id);
        }
    } else if (strcmp(cmd, "release") == 0) {
        int res = release_room(room_id);
        if (res == 0) {
            snprintf(response, sizeof(response), "OK Room %d released successfully.", room_id);
        } else {
            snprintf(response, sizeof(response), "ERROR Room %d release failed. Room is already FREE.", room_id);
        }
    } else if (strcmp(cmd, "extend") == 0) {
        int res = extend_room(room_id);
        if (res == 0) {
            snprintf(response, sizeof(response), "OK Room %d extended by %d seconds.", room_id, SLOT_DURATION);
        } else {
            snprintf(response, sizeof(response), "ERROR Room %d extension failed. Room is not IN_USE, already extended, or there is a pending reservation.", room_id);
        }
    } else {
        snprintf(response, sizeof(response), "ERROR Unknown command: %s.", cmd);
    }
    // 將回應發送給客戶端
    send(client_sock, response, strlen(response), 0);
cleanup:
    close(client_sock);
    printf("[SERVER] Client %d handler finished.\n", client_sock);
    return NULL;
}


int main() {
    // 1. 初始化房間狀態
    for (int i=0; i<MAX_ROOMS; i++) {
        rooms[i].id = i;
        rooms[i].status = FREE;
        rooms[i].extend_used = 0;
        rooms[i].reserve_time = 0;
    }

    // 2. 啟動定時器執行緒
    pthread_t t_timer;
    if (pthread_create(&t_timer, NULL, timer_thread, NULL) != 0) {
        perror("Could not create timer thread");
        return 1;
    }
    pthread_detach(t_timer); // 使執行緒在結束後自動釋放資源

    // 3. 設置網路伺服器
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    // 創建 socket 檔案描述符
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 設置 socket 選項，允許重用位址和埠號
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // 監聽所有網路介面
    address.sin_port = htons(PORT);

    // 綁定 socket 到指定的埠號
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 開始監聽連線
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d with %d rooms.\n", PORT, MAX_ROOMS);
    printf("SLOT_DURATION: %d sec, CHECKIN_TIMEOUT: %d sec\n", SLOT_DURATION, CHECKIN_TIMEOUT);
    printf("--- Waiting for clients ---\n");


    // 4. 接受連線並為每個客戶端建立執行緒
    while (1) {
        printf("Waiting for a new connection...\n");
        // new_socket 將用於後續與客戶端的通訊
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        // 為客戶端 socket 描述符分配記憶體，以便傳遞給新執行緒
        int *new_sock_ptr = (int*)malloc(sizeof(int));
        if (new_sock_ptr == NULL) {
            perror("malloc failed");
            close(new_socket);
            continue;
        }
        *new_sock_ptr = new_socket;
        
        pthread_t client_tid;
        // 建立執行緒來處理客戶端請求
        if (pthread_create(&client_tid, NULL, client_handler, (void*)new_sock_ptr) != 0) {
            perror("Could not create client handler thread");
            free(new_sock_ptr);
            close(new_socket);
        }
        pthread_detach(client_tid); // 分離執行緒
    }

    // 雖然這裡永遠不會執行，但還是加上清理程式碼
    close(server_fd);
    pthread_mutex_destroy(&room_mutex);
    return 0;
}