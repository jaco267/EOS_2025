#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 2048
static void send_command(const char *cmd){
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    printf("\n>>> Send: %s\n", cmd);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return;
    }

    send(sock, cmd, strlen(cmd), 0);

    memset(buffer, 0, sizeof(buffer));
    int n = read(sock, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        printf("--- Server Response ---\n%s\n-----------------------\n",
               buffer);
    } else {
        printf("Server disconnected or no response\n");
    }

    close(sock);
}
int main() {
    send_command("reserve 0 777 Tom");
    sleep(1);
    send_command("reserve 1 888 Alice");
    sleep(1);
    send_command("reserve 2 999 Bob");
    sleep(1);
    send_command("checkin 0");
    sleep(1);
    send_command("checkin 1");
    sleep(1);
    send_command("checkin 2");
    sleep(1);

    send_command("extend 0");
    sleep(1);
    send_command("extend 1");
    sleep(1);
    send_command("extend 2");
    sleep(1);
    /* step 3: status 0 */
    send_command("status 0");

    return 0;
}