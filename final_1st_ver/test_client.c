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

/*** @brief 格式化時間戳並印出訊息。
 * @param msg 要印出的訊息*/
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
int send_command(const char* command, char* response_buffer, size_t buffer_size) {
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
void status(char* recv_buffer, size_t recv_size){
    if (send_command("status", recv_buffer, recv_size) != 0) {
        print_log("FATAL: get_status failed. Exiting program.");
        exit(EXIT_FAILURE);
    }
}
void reserve(char* cmd_buffer, size_t cmd_size, int room_id, char* recv_buffer, size_t recv_size){
    
    snprintf(cmd_buffer, cmd_size, "reserve %d", room_id);
    if (send_command(cmd_buffer, recv_buffer, recv_size) != 0) {
        print_log("FATAL: reserve_room failed. Exiting program.");
        exit(EXIT_FAILURE);
    }
}
void checkin(char* cmd_buffer, size_t cmd_size, int room_id, char* recv_buffer, size_t recv_size) {
    snprintf(cmd_buffer, cmd_size, "checkin %d", room_id);
    if (send_command(cmd_buffer, recv_buffer, recv_size) != 0) {
        print_log("FATAL: checkin_room failed. Exiting program.");
        exit(EXIT_FAILURE);
    }
}
void release(char* cmd_buffer, size_t cmd_size, int room_id, char* recv_buffer, size_t recv_size) {
    snprintf(cmd_buffer, cmd_size, "release %d", room_id);
    int res =  send_command(cmd_buffer, recv_buffer, recv_size);
    if (res != 0){
        print_log("FATAL: release_room failed. Exiting program.");
        exit(EXIT_FAILURE);
    }
}
int main() {
    print_log("Automated Client starting sequence...");
    int room_id = 0;
    char cmd_buffer[100]; char recv_buffer[BUFFER_SIZE]; 
    // -----status
    status(recv_buffer,sizeof(recv_buffer));  
    // **檢查接收到的訊息**
    printf("recev-----%s-----\n",recv_buffer);
    //* ----reserve room 0----
    reserve(cmd_buffer,sizeof(cmd_buffer),room_id,recv_buffer,sizeof(recv_buffer));
    sleep(3); // 模擬 sleep 3 (等待 3 秒，模擬使用者前往報到)
    checkin(cmd_buffer, sizeof(cmd_buffer), room_id, recv_buffer, sizeof(recv_buffer)); 
    sleep(1); 
    status(recv_buffer,sizeof(recv_buffer));
    sleep(1);   
    release(cmd_buffer, sizeof(cmd_buffer), room_id, recv_buffer, sizeof(recv_buffer)); 


    print_log("Automated Client sequence finished.");
    return 0;
}