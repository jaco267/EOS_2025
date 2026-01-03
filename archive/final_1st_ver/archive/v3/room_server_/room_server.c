#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "globe_var.h"
#include "room_action.h"

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