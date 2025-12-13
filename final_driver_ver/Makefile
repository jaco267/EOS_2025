obj-m += room_driver.o

all:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /home/elton/Desktop/emb_linux/linux M=$(PWD) modules

room_client: room_client.c
	gcc -o room_client room_client.c


clean:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /home/elton/Desktop/emb_linux/linux M=$(PWD) clean
	rm -f room_client
