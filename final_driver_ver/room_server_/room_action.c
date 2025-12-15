#include "room_action.h"
#include <sys/types.h> // 新增：用於 ssize_t
#include<unistd.h>
#include <fcntl.h>
#include "wait_queue.h"
// helper: status -> string
const char* get_status_str(room_status_t status) {
    switch(status) {
        case FREE: return "FREE (🟢)";
        case RESERVED: return "RESERVED (🟡)";
        case IN_USE: return "IN_USE (🔴)";
        default: return "UNKNOWN";
    }
}
/** return formatted string with all room status.
 * Caller must free returned buffer.*/
char* get_all_status(int room_id) {
    pthread_mutex_lock(&room_mutex);
    size_t required_size = MAX_ROOMS * 120 + 200;
    char *resp = (char*)malloc(required_size);
    if (!resp) {pthread_mutex_unlock(&room_mutex); return strdup("ERROR: malloc failed.");}
    strcpy(resp, "--- Room Status ---\n");

    uint64_t now_tick = get_current_tick_snapshot();
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (room_id!=-1){
            if (i != room_id){continue;}
        }
        char tmp[512];
        uint64_t elapsed_ticks = 0; //* reserve or checkin 狀態過了多久
        if (rooms[i].status != FREE) {
          if (now_tick >= rooms[i].reserve_tick)  elapsed_ticks = now_tick - rooms[i].reserve_tick;
          else                                    elapsed_ticks = 0; // unlikely
        }
        long elapsed_sec = (elapsed_ticks * TICK_MS) / 1000;
        //*----obtain wait list user ids   [usr1,usr2,...]-------
        char wait_buf[128]; wait_buf[0] = '\0'; 
        if (rooms[i].wait_q.count == 0) {strcpy(wait_buf,"[]"); }
        else{
            strcat(wait_buf,"["); 
            for(int j=0; j<rooms[i].wait_q.count; j++){
                int idx = (rooms[i].wait_q.head + j) % MAX_WAITING; 
                char num[16]; 
                snprintf(num, sizeof(num), "%d", rooms[i].wait_q.users[idx]);
                strcat(wait_buf,num); 
                if (j != rooms[i].wait_q.count - 1) strcat(wait_buf, ",");
            }
            strcat(wait_buf,"]");
        }

        snprintf(tmp, sizeof(tmp),
"Room %d | %s%s | Uid: %d | Reserve Count: %d | Time Elapsed: %lds\n\
       | wait count: %d | wait queue: %s\n",
                 rooms[i].id,
                 get_status_str(rooms[i].status),
                 rooms[i].extend_used ? " (Extended)" : "",
                   rooms[i].user_id, 
                rooms[i].reserve_count_today,
                (rooms[i].status != FREE) ? elapsed_sec : 0L,
                 rooms[i].wait_q.count,
                 wait_buf);
        strncat(resp, tmp, required_size - strlen(resp) - 1);
    }
    strncat(resp, "-------------------\n", required_size - strlen(resp) - 1);
    if (room_id != -1){
        if (room_id >= MAX_ROOMS){
            perror("Failed : room_id >= MAX_ROOMS");
            goto r_unlock;
        }
        int fd_write = open(DEVICE_FILE, O_WRONLY);
        if (fd_write < 0) {
            perror("Failed to open /dev/etx_device");
            goto r_unlock; 
        }
        char led_id_str[MAX_NAME_LEN];
        snprintf(led_id_str, sizeof(led_id_str), "7seg %d", room_id);
        ssize_t ret = write(fd_write, led_id_str, strlen(led_id_str));
        if (ret < 0) {perror("Failed to write to 7seg");}
        //* turn on led status  
        int led_id;
        led_id = rooms[room_id].status+1; //* 3,2,1   for used, reserve , free  
        snprintf(led_id_str, sizeof(led_id_str), "led %d", led_id);
        ret = write(fd_write, led_id_str, strlen(led_id_str));
        if (ret < 0) {
            perror("Failed to write to led");
            close(fd_write);
        }
    }
    
  r_unlock: 
    pthread_mutex_unlock(&room_mutex);
    return resp;
}

// ------ operations (reserve/checkin/release/extend) ------

int reserve_room(int room_id, int user_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->reserve_count_today >= 2) {
        pthread_mutex_unlock(&room_mutex); return -3;
    }
    //todo  這裡有個 bug 就是 一天內如果在 free 的狀態下 reserve , 等到auto release free 再 reserve 的話只能兩次(reserve_count_today++)
    //todo 但是假如 reserve -> checkin-> inuse,  在這種狀態下就可以瘋狂reserve 超過兩次？
    //todo 也許次數限制改成 checkin 而不是 reserve 比較合理？
    if (r->status == FREE) {
        r->status = RESERVED;
        r->reserve_tick = get_current_tick_snapshot();
        r->extend_used = 0;
        r->user_id = user_id;
        r->reserve_count_today++; 
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d reserved at tick %llu.\n", room_id, 
            (unsigned long long)r->reserve_tick);
        return 0;
    } else {
        // 房間不空閒 → 加入候補隊列
        if (wait_enqueue(&r->wait_q, user_id) == 0) { // 代表加入候補成功
            pthread_mutex_unlock(&room_mutex);
            printf("[SERVER LOG] Room %d busy. User %d added to wait queue (count=%d).\n",
                   room_id, user_id, r->wait_q.count);
            return -4;
        } else {
            pthread_mutex_unlock(&room_mutex);
            printf("[SERVER LOG] Room %d busy. Also wait queue is full so cant add to wait list.\n",
                   room_id);
            return -5; // queue full
        }
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

int check_in(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status == RESERVED) {
        r->status = IN_USE;
        r->reserve_tick = get_current_tick_snapshot(); // start of session
        r->extend_used = 0;
        //todo r->user_id
        pthread_mutex_unlock(&room_mutex);
        printf("[SERVER LOG] Room %d checked in at tick %llu.\n", room_id, (unsigned long long)r->reserve_tick);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

int release_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;
    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];
    if (r->status != FREE) {
        r->status = FREE;
        r->extend_used = 0;
        r->reserve_tick = 0;
        r->user_id = -1;
        printf("[SERVER LOG] Room %d released by user.\n", room_id);
        int next_user; 

        // 若有候補 → 立刻讓候補接手
        if (wait_dequeue(&r->wait_q, &next_user) == 0) {
            //todo  這裡應該要有像是 reserve 裡面 r->reserve_count_today >= 2 return 的機制？
            r->status = RESERVED;
            r->reserve_tick = get_current_tick_snapshot();
            r->extend_used  = 0;
            r->user_id = next_user;
            r->reserve_count_today++;
            printf("[SERVER LOG] Room %d assigned to waiting list. "
                "Remaining waiters = %d.\n",
                room_id, r->wait_q.count);
        }
        
        pthread_mutex_unlock(&room_mutex);
        //printf("[SERVER LOG] Room %d released.\n", room_id);
        return 0;
    }
    pthread_mutex_unlock(&room_mutex);
    return -1;
}

int extend_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) return -2;

    pthread_mutex_lock(&room_mutex);
    room_t *r = &rooms[room_id];

    // 條件：房間正在使用中，且尚未延長過
    if (r->status == IN_USE && r->extend_used == 0) {
        r->extend_used = 1;
        pthread_mutex_unlock(&room_mutex);

        printf("[SERVER LOG] Room %d extended at tick %llu.\n",
               room_id,
               (unsigned long long)get_current_tick_snapshot());
        return 0;
    }

    pthread_mutex_unlock(&room_mutex);
    return -1; // 不合法延長
}

