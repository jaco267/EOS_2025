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
    printf("status - Get all room statuses.\n");
    printf("reserve <id> - Reserve a room (e.g., reserve 0).\n");
    printf("checkin <id> - Check-in to a reserved room.\n");
    printf("release <id> - Manually release a room.\n");
    printf("extend <id> - Extend an IN_USE room once.\n");
    printf("exit - Exit the client.\n");
    printf("-------------------------------------\n");
    while (1) {
        printf("\nEnter command: ");
        if (fgets(command, sizeof(command), stdin) == NULL) {   break;}
        command[strcspn(command, "\n")] = 0;  // 移除換行符 
        if (strcasecmp(command, "exit") == 0) {printf("Exiting client.\n");break;}
        // 1. 創建 client socket fd
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("\nSocket creation error");continue;}
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        // 轉換 IP 位址從文字到二進制格式
        if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            printf("\nInvalid address/ Address not supported \n"); close(sock);
            continue;
        }
        // 2. connect to server
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection Failed"); close(sock);
            sleep(1); // 由於伺服器可能還未啟動，這裡不應直接退出 
            continue;
        }
        send(sock, command, strlen(command), 0); // 3. 發送命令
        // 4. 接收回應
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {printf("\n--- Server Response ---\n%s\n-----------------------\n", buffer);
        } else {          printf("\nError: Server disconnected or failed to read response.\n");}
        close(sock);// 5. 關閉 socket
    }
}

int main() {
    run_client();
    return 0;
}