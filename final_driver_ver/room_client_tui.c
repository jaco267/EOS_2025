// room_client_tui.c  (ncurses TUI client for your room_server)
// Keys: q quit | s status(room) | p select(room) | r reserve | i checkin | o release | e extend
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef DEFAULT_SERVER_IP
#define DEFAULT_SERVER_IP "127.0.0.1"
#endif
#ifndef DEFAULT_SERVER_PORT
#define DEFAULT_SERVER_PORT 8080
#endif

static const char *g_server_ip = DEFAULT_SERVER_IP;
static int g_server_port = DEFAULT_SERVER_PORT;

static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

// one-shot request: connect -> send line -> read all -> close
static int send_cmd(const char *line, char *out, size_t out_sz) {
    if (!line || !out || out_sz == 0) return -1;
    out[0] = '\0';

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -2;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)g_server_port);
    if (inet_pton(AF_INET, g_server_ip, &addr.sin_addr) != 1) {
        close(fd);
        return -3;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -4;
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "%s\n", line);
    if (send(fd, buf, strlen(buf), 0) < 0) {
        close(fd);
        return -5;
    }

    size_t used = 0;
    while (1) {
        ssize_t n = recv(fd, out + used, out_sz - 1 - used, 0);
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        used += (size_t)n;
        if (used >= out_sz - 1) break;
    }
    out[used] = '\0';
    close(fd);
    return 0;
}

static void draw_help(WINDOW *w) {
    werase(w);
    box(w, 0, 0);
    mvwprintw(w, 1, 2, "Keys: q quit | s status(room) | p select(room) | r reserve | i checkin | o release | e extend");
    mvwprintw(w, 2, 2, "Note: status <room_id> 會回傳該房狀態；若你 server 有實作 select，可用 p.");
    wrefresh(w);
}

static void prompt_line(WINDOW *w, const char *prompt, char *out, size_t out_sz) {
    werase(w);
    box(w, 0, 0);
    mvwprintw(w, 1, 2, "%s", prompt);
    wmove(w, 2, 2);
    echo();
    curs_set(1);
    wgetnstr(w, out, (int)out_sz - 1);
    noecho();
    curs_set(0);
    wrefresh(w);
}

static void show_text(WINDOW *w, const char *title, const char *text) {
    werase(w);
    box(w, 0, 0);
    mvwprintw(w, 1, 2, "%s", title ? title : "Output");

    int maxy, maxx;
    getmaxyx(w, maxy, maxx);

    int row = 2;
    const char *p = text ? text : "";
    while (*p && row < maxy - 1) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > maxx - 4) len = maxx - 4;
        mvwprintw(w, row, 2, "%.*s", len, p);
        row++;
        p = nl ? nl + 1 : p + strlen(p);
    }
    wrefresh(w);
}

int main(int argc, char **argv) {
    if (argc >= 2) g_server_ip = argv[1];
    if (argc >= 3) g_server_port = atoi(argv[2]);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(100); // non-blocking getch every 100ms

    int H, W;
    getmaxyx(stdscr, H, W);
    int h_help = 4, h_cmd = 4;
    int h_main = H - h_help - h_cmd;
    if (h_main < 8) h_main = 8;

    WINDOW *w_main = newwin(h_main, W, 0, 0);
    WINDOW *w_help = newwin(h_help, W, h_main, 0);
    WINDOW *w_cmd  = newwin(h_cmd,  W, h_main + h_help, 0);

    draw_help(w_help);

    char status_buf[8192];
    char out_buf[8192];
    long long last_refresh = 0;

    (void)send_cmd("status", status_buf, sizeof(status_buf));
    show_text(w_main, "STATUS", status_buf);

    bool running = true;
    while (running) {
        long long t = now_ms();
        if (t - last_refresh >= 500) {
            if (send_cmd("status", status_buf, sizeof(status_buf)) == 0) {
                show_text(w_main, "STATUS", status_buf);
            }
            last_refresh = t;
        }

        int ch = getch();
        if (ch == ERR) continue;

        if (ch == 'q' || ch == 'Q') {
            running = false;
        } else if (ch == 's' || ch == 'S') {
            char rid[32];
            prompt_line(w_cmd, "status room_id = ?", rid, sizeof(rid));
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "status %s", rid);
            (void)send_cmd(cmd, out_buf, sizeof(out_buf));
            show_text(w_main, "STATUS", out_buf);

        } else if (ch == 'p' || ch == 'P') {
            char rid[32];
            prompt_line(w_cmd, "select room_id = ?", rid, sizeof(rid));
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "select %s", rid);
            (void)send_cmd(cmd, out_buf, sizeof(out_buf));
            show_text(w_main, "OUTPUT", out_buf);

        } else if (ch == 'r' || ch == 'R') {
            char rid[32], uid[32], name[128];
            prompt_line(w_cmd, "reserve room_id = ?", rid, sizeof(rid));
            prompt_line(w_cmd, "reserve user_id = ?", uid, sizeof(uid));
            prompt_line(w_cmd, "reserve name(no_space or empty) = ?", name, sizeof(name));
            char cmd[256];
            if (name[0]) snprintf(cmd, sizeof(cmd), "reserve %s %s %s", rid, uid, name);
            else         snprintf(cmd, sizeof(cmd), "reserve %s %s", rid, uid);
            (void)send_cmd(cmd, out_buf, sizeof(out_buf));
            show_text(w_main, "OUTPUT", out_buf);

        } else if (ch == 'i' || ch == 'I') {
            char rid[32], uid[32];
            prompt_line(w_cmd, "checkin room_id = ?", rid, sizeof(rid));
            prompt_line(w_cmd, "checkin user_id = ?", uid, sizeof(uid));
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "checkin %s %s", rid, uid);
            (void)send_cmd(cmd, out_buf, sizeof(out_buf));
            show_text(w_main, "OUTPUT", out_buf);

        } else if (ch == 'o' || ch == 'O') {
            char rid[32], uid[32];
            prompt_line(w_cmd, "release room_id = ?", rid, sizeof(rid));
            prompt_line(w_cmd, "release user_id = ?", uid, sizeof(uid));
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "release %s %s", rid, uid);
            (void)send_cmd(cmd, out_buf, sizeof(out_buf));
            show_text(w_main, "OUTPUT", out_buf);

        } else if (ch == 'e' || ch == 'E') {
            char rid[32], uid[32];
            prompt_line(w_cmd, "extend room_id = ?", rid, sizeof(rid));
            prompt_line(w_cmd, "extend user_id = ?", uid, sizeof(uid));
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "extend %s %s", rid, uid);
            (void)send_cmd(cmd, out_buf, sizeof(out_buf));
            show_text(w_main, "OUTPUT", out_buf);
        }
    }

    delwin(w_main);
    delwin(w_help);
    delwin(w_cmd);
    endwin();
    return 0;
}
