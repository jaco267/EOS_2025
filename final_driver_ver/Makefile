obj-m += room_driver.o

# ===== Kernel module build (for Raspberry Pi / cross-compile) =====
LINUX_KERNEL_PATH ?= /home/ncrl/rpi-linux-6.1

ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

# ===== User-space build (server/client) =====
CFLAGS += -O2 -Wall -Wextra -pthread
USER_CC ?= gcc

SRC_client = room_client.c
SRC_test   = test_driver.c

SRC_server = room_server_/room_server.c  \
             room_server_/room_action.c  \
             room_server_/globe_var.c    \
             room_server_/room_timer.c   \
             room_server_/wait_queue.c   \
             room_server_/user_db.c

.PHONY: all user clean

all:
	$(MAKE) -C $(LINUX_KERNEL_PATH) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

user: room_client test_driver room_server

room_server: $(SRC_server)
	$(USER_CC) $(CFLAGS) -o $@ $^

test_driver: $(SRC_test)
	$(USER_CC) $(CFLAGS) -o $@ $^

room_client: $(SRC_client)
	$(USER_CC) $(CFLAGS) -o $@ $^

clean:
	$(MAKE) -C $(LINUX_KERNEL_PATH) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f room_client room_server test_driver

