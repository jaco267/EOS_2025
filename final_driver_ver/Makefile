obj-m += room_driver.o

LINUX_KERNEL_PATH ?= /home/elton/Desktop/emb_linux/linux


SRC_client = room_client.c 
driver_test = test_driver.c
SRC_server = room_server_/room_server.c   room_server_/room_action.c  \
  room_server_/globe_var.c room_server_/room_timer.c room_server_/wait_queue.c


all:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(LINUX_KERNEL_PATH) M=$(PWD) modules

user: room_client test_driver  room_server

room_server: $(SRC_server)
	gcc $(CFLAGS) -o $@ $^

test_driver: $(driver_test)
	gcc -o $@ $^  

room_client: $(SRC_client)
	gcc -o $@ $^


clean:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(LINUX_KERNEL_PATH) M=$(PWD) clean
	rm -f room_client room_server test_driver
