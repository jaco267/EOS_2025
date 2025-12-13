#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <fcntl.h>
#define MAX_NAME_LEN 30 

int main(int argc, char* argv[]){
  int fd = open("/dev/etx_device", O_WRONLY);
  if (fd < 0) {perror("Failed to open /dev/etx_device");return 1;}
  char room_id_str[MAX_NAME_LEN];  
  int room_id = 43;
  snprintf(room_id_str, sizeof(room_id_str), "7seg %d", room_id);
  ssize_t ret = write(fd, room_id_str, strlen(room_id_str));
  if (ret < 0) {perror("Failed to write to the device");close(fd);return 1;}
  usleep(2000000);  // 延遲 2 秒再顯示下一個
  close(fd);
  return 0; 
}