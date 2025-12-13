#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <fcntl.h>

#include <errno.h> // 新增：用於 perror 顯示錯誤
#include <sys/types.h> // 新增：用於 ssize_t
#define MAX_NAME_LEN 30 
#define DEVICE_FILE "/dev/etx_device"
#define READ_BUFFER_SIZE 2048 // 讀取狀態的緩衝區大小

int main(int argc, char* argv[]){
  printf("--- Phase 1: Sending Command via WRITE ---\n");
  int fd_write = open(DEVICE_FILE, O_WRONLY);
  if (fd_write < 0) {perror("Failed to open /dev/etx_device");return 1;}
  
  char room_id_str[MAX_NAME_LEN];  
  int room_id = 2; //* 0,1,2  
  //* command 7seg 2  
  snprintf(room_id_str, sizeof(room_id_str), "7seg %d", room_id);
  printf("Sending command: [%s]\n", room_id_str);


  ssize_t ret = write(fd_write, room_id_str, strlen(room_id_str));
  if (ret < 0) {perror("Failed to write to the device");close(fd_write);return 1;}

  close(fd_write);
  usleep(2000000);  // 延遲 2 秒再顯示下一個
  
  printf("\n--- Phase 2: Requesting Status via READ ---\n");
  int fd_read = open(DEVICE_FILE, O_RDONLY);
  if (fd_read < 0) {
      perror("Failed to open device for READ");
      return 1;
  }

  char read_buffer[READ_BUFFER_SIZE] = {0};
  // read()，執行driver 的 etx_read()
  ssize_t bytes_read = read(fd_read, read_buffer, READ_BUFFER_SIZE - 1);
  
  if (bytes_read > 0) {
      read_buffer[bytes_read] = '\0';
      printf("Read successful. Received %zd bytes:\n", bytes_read);
      printf("------------------------------------------\n");
      printf("%s\n", read_buffer); // 印出驅動程式返回的狀態
      printf("------------------------------------------\n");
  } else if (bytes_read == 0) {
      printf("Read 0 bytes. End of file/buffer, or driver returned nothing.\n");
  } else {
      perror("Failed to read from the device");
      close(fd_read);
      return 1;
  }
  
  close(fd_read);
  return 0; 
}