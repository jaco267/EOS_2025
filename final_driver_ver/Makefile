obj-m += room_driver.o
LINUX_KERNEL_PATH ?= /home/elton/Desktop/emb_linux/linux


all:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(LINUX_KERNEL_PATH) M=$(PWD) modules

room_client: room_client.c
	gcc -o room_client room_client.c


clean:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(LINUX_KERNEL_PATH) M=$(PWD) clean
	rm -f room_client
