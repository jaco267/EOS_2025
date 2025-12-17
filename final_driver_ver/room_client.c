#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 2048

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

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char command[256];
    print_help();
    while (1) {
        printf("\nEnter command: ");
        if (!fgets(command, sizeof(command), stdin)) break;
        command[strcspn(command, "\n")] = 0;
        if (strcasecmp(command, "exit") == 0) {printf("Exiting client.\n");break;}
        if (strcasecmp(command, "help") == 0) {print_help();continue;}
        if (command[0] == '\0') continue;
        //*-------socket-----------------
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { perror("socket"); continue; }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
            printf("Invalid address\n");close(sock);continue;
        }
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("connect");close(sock);sleep(1);continue;
        }
        send(sock, command, strlen(command), 0);
        //*--------socket receive------------------
        memset(buffer, 0, sizeof(buffer));
        int n = read(sock, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            printf("\n--- Server Response ---\n%s\n-----------------------\n", buffer);
        } else {
            printf("Error: server disconnected.\n");
        }
        close(sock);
    }
    return 0;
}
