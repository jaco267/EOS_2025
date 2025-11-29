#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 2048

void run_client() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char command[100];
    
    printf("--- Smart Room Reservation Client ---\n");
    printf("Commands:\n");
    printf("STATUS - Get all room statuses.\n");
    printf("RESERVE <id> - Reserve a room (e.g., RESERVE 0).\n");
    printf("CHECKIN <id> - Check-in to a reserved room.\n");
    printf("RELEASE <id> - Manually release a room.\n");
    printf("EXTEND <id> - Extend an IN_USE room once.\n");
    printf("EXIT - Exit the client.\n");
    printf("-------------------------------------\n");
    
    while (1) {
        printf("\nEnter command: ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        // 移除換行符
        command[strcspn(command, "\n")] = 0; 
        
        if (strcasecmp(command, "EXIT") == 0) {
            printf("Exiting client.\n");
            break;
        }
        
        // 1. 創建 socket 檔案描述符
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("\nSocket creation error");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        // 轉換 IP 位址從文字到二進制格式
        if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            printf("\nInvalid address/ Address not supported \n");
            close(sock);
            continue;
        }

        // 2. 連接到伺服器
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection Failed");
            close(sock);
            // 由於伺服器可能還未啟動，這裡不應直接退出
            sleep(1); 
            continue;
        }
        
        // 3. 發送命令
        send(sock, command, strlen(command), 0);
        
        // 4. 接收回應
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE - 1);
        
        if (valread > 0) {
            printf("\n--- Server Response ---\n%s\n-----------------------\n", buffer);
        } else {
            printf("\nError: Server disconnected or failed to read response.\n");
        }

        // 5. 關閉 socket
        close(sock);
    }
}

int main() {
    run_client();
    return 0;
}