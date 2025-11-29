#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 2048

/**
 * @brief 格式化時間戳並印出訊息。
 * @param msg 要印出的訊息
 */
void print_log(const char* msg) {
    time_t timer;
    char buffer[26];
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 26, "[%H:%M:%S]", tm_info);
    printf("%s %s\n", buffer, msg);
}

/**
 * @brief 連接到伺服器，發送命令，接收回應，並印出。
 * @param command 要發送的命令字串。
 * @return 0 成功, -1 失敗。
 */
int send_command(const char* command) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // 1. 創建 socket 檔案描述符
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        print_log("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 轉換 IP 位址
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        print_log("Invalid address/ Address not supported");
        close(sock);
        return -1;
    }

    // 2. 連接到伺服器
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        print_log("Connection Failed (Is server running?)");
        close(sock);
        return -1;
    }
    
    print_log("------------------------------------------");
    printf("[SENT] %s\n", command);

    // 3. 發送命令
    send(sock, command, strlen(command), 0);
    
    // 4. 接收回應
    int valread = read(sock, buffer, BUFFER_SIZE - 1);
    
    if (valread > 0) {
        printf("[RECV]\n%s\n", buffer);
    } else {
        print_log("Error: Server disconnected or failed to read response.");
    }

    // 5. 關閉 socket
    close(sock);
    return 0;
}


int main() {
    print_log("Automated Client starting sequence...");
    int room_id = 0;
    char cmd_buffer[100];
    int result;

    // 模擬 tmux send-keys -t $SESSION:0.1 "status" C-m
    print_log("Step 1: Get initial STATUS.");
    result = send_command("status");
    if (result != 0) return 1;
    
    // 模擬 tmux send-keys -t $SESSION:0.1 "reserve 0" C-m
    print_log("Step 2: Reserve Room 0.");
    snprintf(cmd_buffer, sizeof(cmd_buffer), "reserve %d", room_id);
    result = send_command(cmd_buffer);
    if (result != 0) return 1;

    // 模擬 sleep 3 (等待 3 秒，模擬使用者前往報到)
    print_log("Pausing for 3 seconds (Simulate arrival to check-in).");
    sleep(3);
    
    // 模擬 tmux send-keys -t $SESSION:0.1 "checkin 0" C-m
    print_log("Step 3: Check-in Room 0.");
    snprintf(cmd_buffer, sizeof(cmd_buffer), "checkin %d", room_id);
    result = send_command(cmd_buffer);
    if (result != 0) return 1;

    // 模擬 sleep 1
    print_log("Pausing for 1 second.");
    sleep(1); 

    // 模擬 tmux send-keys -t $SESSION:0.1 "status" C-m
    print_log("Step 4: Check STATUS after check-in.");
    result = send_command("status");
    if (result != 0) return 1;
    
    // 繼續其他自動化步驟...
    
    print_log("Automated Client sequence finished.");
    return 0;
}