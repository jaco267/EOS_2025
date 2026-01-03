#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ncurses.h>
#include <string.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 2048

#define MENU_ROOM_COUNT 3
#define MAX_ACTIONS 5
#define ROW_OFFSET 3
typedef struct {
    const char *room_id;
    const char *actions[MAX_ACTIONS];
    int action_count;
} Menu;

Menu menus[MENU_ROOM_COUNT] = {
    {"Room0", {"status","reserve", "checkin", "release","extend"}, 5},
    {"Room1", {"status","reserve", "checkin", "release","extend"}, 5},
    {"Room2", {"status","reserve", "checkin", "release","extend"}, 5}
};
int main() {
    //*-------room_client----------
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char command[256];
    //------------------------------
    int ch;
    int selected_room = 0;   int selected_action = 0;
    int dropdown = 0;
    initscr(); noecho(); cbreak(); keypad(stdscr, TRUE);
    curs_set(0);
    while (1) {
      clear();
      /* ===== draw menu bar ===== */
      int x = 2;
      for (int i = 0; i < MENU_ROOM_COUNT; i++) {
        if (i == selected_room && !dropdown) attron(A_REVERSE);
        mvprintw(0, x, "%s", menus[i].room_id);
        if (i == selected_room && !dropdown) attroff(A_REVERSE);
        x += strlen(menus[i].room_id) + 4;
      }

       mvprintw(ROW_OFFSET+10, 2, "[S] Status All    [Q] Quit");
      /* ===== draw dropdown ===== */
      if (dropdown) {
        int start_x = 2;
        for (int i = 0; i < selected_room; i++)
          start_x += strlen(menus[i].room_id) + 4;
        for (int i = 0; i < menus[selected_room].action_count; i++) {
          if (i == selected_action)  attron(A_REVERSE);
          mvprintw(i + 1, start_x, "%s",  menus[selected_room].actions[i]);
          if (i == selected_action)  attroff(A_REVERSE);
        }
      }
      refresh();          ch = getch();
      //*----status all-----
      if (ch == 's' || ch == 'S') {
        clear();
        snprintf(command, sizeof(command), "status");
        mvprintw(ROW_OFFSET+2, 2, "CMD: %s", command);
        /* socket */
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {mvprintw(ROW_OFFSET+4,2,"socket error");refresh(); getch(); continue;}
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            mvprintw(ROW_OFFSET+4,2,"connect failed");
            close(sock);
            refresh(); getch();
            continue;
        }
        send(sock, command, strlen(command), 0);
        memset(buffer, 0, sizeof(buffer));
        int n = read(sock, buffer, sizeof(buffer)-1);
        if (n > 0) {
            mvprintw(ROW_OFFSET+4, 2, "%s", buffer);
        } else {
            mvprintw(ROW_OFFSET+4, 2, "No response");
        }
    
        close(sock);
        mvprintw(LINES-2, 2, "Press any key to continue...");
        refresh();
        getch();
        continue;   // ⭐ 非常重要：跳過下面 menu 邏輯
    }
    
      if (ch == 'q') break;
      if (!dropdown) {
        switch (ch) {
          case KEY_LEFT:
            selected_room = (selected_room - 1 + MENU_ROOM_COUNT) % MENU_ROOM_COUNT;
            break;
          case KEY_RIGHT: selected_room = (selected_room + 1) % MENU_ROOM_COUNT; break;
          case KEY_DOWN:
          case '\n':  dropdown = 1; selected_action = 0; break;
        }
      } else {
        switch (ch) {
        case KEY_UP:
          selected_action = (selected_action - 1 + menus[selected_room].action_count) %
              menus[selected_room].action_count;
          break;
        case KEY_DOWN:
          selected_action = (selected_action + 1) % menus[selected_room].action_count;
          break;
        case 27: /* ESC */ dropdown = 0; break;
        case '\n':
          memset(command, 0, sizeof(command));

          //--------------------
          const char *action = menus[selected_room].actions[selected_action];
          /* 只有 reserve 才要求輸入 user id */
          if (      strcmp(action, "status")  == 0){
            snprintf(command, sizeof(command), "status %d", selected_room);
            mvprintw(ROW_OFFSET+5, 2, "CMD: %s", command);
          }else if (strcmp(action, "reserve") == 0) {
            char user_id[32];
            char user_name[32];
            mvprintw(ROW_OFFSET+4, 2, "Enter user id: ");
            refresh();  echo(); curs_set(1);
            mvgetnstr(ROW_OFFSET+4, 18, user_id, sizeof(user_id) - 1);
            mvprintw(ROW_OFFSET+5, 2, "Enter user name: ");
            refresh();  echo(); curs_set(1);
            mvgetnstr(ROW_OFFSET+5, 20, user_name, sizeof(user_name) - 1);
            noecho();  curs_set(0);
            snprintf(command, sizeof(command), "reserve %d %s %s", selected_room, user_id, user_name);
            mvprintw(ROW_OFFSET+5, 2, "CMD: %s", command);
          }else if (strcmp(action, "checkin") == 0){
            snprintf(command, sizeof(command), "checkin %d", selected_room);
            mvprintw(ROW_OFFSET+5, 2, "CMD: %s", command);
          }else if (strcmp(action, "release") == 0){
            snprintf(command, sizeof(command), "release %d", selected_room);
            mvprintw(ROW_OFFSET+5, 2, "CMD: %s", command);
          }else if (strcmp(action, "extend") == 0){
            snprintf(command, sizeof(command), "extend %d", selected_room);
            mvprintw(ROW_OFFSET+5, 2, "CMD: %s", command);
          }else{mvprintw(ROW_OFFSET+5, 2, "Execute unknown action %s ...", action);}
          //*----socket----------
          sock = socket(AF_INET, SOCK_STREAM, 0);
          if (sock < 0) { perror("socket"); continue; }
          serv_addr.sin_family = AF_INET;
          serv_addr.sin_port = htons(PORT);
          if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
              mvprintw(ROW_OFFSET+7,2,"Invalid address\n");close(sock);continue;
          }
          if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
              perror("connect");close(sock);sleep(1);continue;
          }
          //*-------send command to server------------
          send(sock, command, strlen(command), 0);
          //*--------socket receive------------------
          memset(buffer, 0, sizeof(buffer));
          int n = read(sock, buffer, sizeof(buffer) - 1);
          if (n > 0) {
              mvprintw(ROW_OFFSET+7,2,"\n--- Server Response ---\n%s\n-----------------------\n", buffer);
          } else {
              mvprintw(ROW_OFFSET+7,2,"Error: server disconnected.\n");
          }
          close(sock);
          //*-----------------------------------------------
          mvprintw(ROW_OFFSET+15, 2, "Press any key to continue...");
          refresh(); getch();
          dropdown = 0;
          break;
        }
      }
    }
    endwin();
    return 0;
}
