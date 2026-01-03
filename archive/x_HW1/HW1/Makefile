obj-m += hw1.o

all:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /home/elton/Desktop/emb_linux/linux M=$(PWD) modules

hw1_writer: hw1_writer.c
	gcc -o hw1_writer hw1_writer.c


clean:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C /home/elton/Desktop/emb_linux/linux M=$(PWD) clean
	rm -f hw1_writer
