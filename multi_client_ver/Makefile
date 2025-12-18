obj-m += room_driver.o

# ===== Kernel module build =====
# 預設給你 VM 用的 kernel tree；若在 RPi 本機編譯，建議 make 時覆蓋：
#   make LINUX_KERNEL_PATH=/lib/modules/$(uname -r)/build
LINUX_KERNEL_PATH ?= /home/stu/linux

ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

# ===== User-space build (server/client) =====
USER_CC ?= gcc
USER_CFLAGS ?= -O2 -Wall -Wextra -pthread -Iroom_server_

SRC_client = room_client.c
SRC_test   = test_driver.c

SRC_server = room_server_/room_server.c  \
             room_server_/room_action.c  \
             room_server_/globe_var.c    \
             room_server_/room_timer.c   \
             room_server_/wait_queue.c   \
             room_server_/user_db.c      \
             room_server_/shm_state.c

.PHONY: all user clean module

# 建 module（需要 LINUX_KERNEL_PATH 指到 kernel build dir）
all: module

module:
	@if [ -d "$(LINUX_KERNEL_PATH)" ]; then \
		$(MAKE) -C "$(LINUX_KERNEL_PATH)" M="$(PWD)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" modules; \
	else \
		echo "[WARN] LINUX_KERNEL_PATH not found: $(LINUX_KERNEL_PATH)"; \
		echo "       Skip kernel module build. (Use: make module LINUX_KERNEL_PATH=/lib/modules/\$$(uname -r)/build)"; \
	fi

# 只編 user-space
user: room_client test_driver room_server

room_server: $(SRC_server)
	$(USER_CC) $(USER_CFLAGS) -o $@ $^

test_driver: $(SRC_test)
	$(USER_CC) $(USER_CFLAGS) -o $@ $^

room_client: $(SRC_client)
	$(USER_CC) $(USER_CFLAGS) -o $@ $^

ncurse: ncurse.c
	$(USER_CC) $(USER_CFLAGS) -o $@ $^ -lncurses

clean:
	@rm -f room_client room_server test_driver ncurse
	@if [ -d "$(LINUX_KERNEL_PATH)" ]; then \
		$(MAKE) -C "$(LINUX_KERNEL_PATH)" M="$(PWD)" ARCH="$(ARCH)" CROSS_COMPILE="$(CROSS_COMPILE)" clean; \
	else \
		echo "[WARN] LINUX_KERNEL_PATH not found: $(LINUX_KERNEL_PATH)"; \
		echo "       Skip kernel module clean."; \
	fi
