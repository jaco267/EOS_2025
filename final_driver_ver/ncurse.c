#include <ncurses.h>
#include <string.h>

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
          const char *action = menus[selected_room].actions[selected_action];
          /* 只有 reserve 才要求輸入 user id */
          if (      strcmp(action, "status")  == 0){
            mvprintw(ROW_OFFSET+7, 2, "status %d", selected_room);
          }else if (strcmp(action, "reserve") == 0) {
            char user_id[32];
            mvprintw(ROW_OFFSET+5, 2, "Enter user id: ");
            refresh();  echo(); curs_set(1);
            mvgetnstr(ROW_OFFSET+5, 18, user_id, sizeof(user_id) - 1);
            noecho();  curs_set(0);
            mvprintw(ROW_OFFSET+7, 2, "reserve %d %s", selected_room, user_id);
          }else if (strcmp(action, "checkin") == 0){
            mvprintw(ROW_OFFSET+5, 2, "checkin %d", selected_room);
          }else if (strcmp(action, "release") == 0){
            mvprintw(ROW_OFFSET+5, 2, "release %d", selected_room);
          }else if (strcmp(action, "extend") == 0){
            mvprintw(ROW_OFFSET+5, 2, "extend %d", selected_room);
          }else{mvprintw(ROW_OFFSET+5, 2, "Execute unknown action %s ...", action);}
          mvprintw(ROW_OFFSET+9, 2, "Press any key to continue...");
          refresh(); getch();
          dropdown = 0;
          break;
        }
      }
    }
    endwin();
    return 0;
}
