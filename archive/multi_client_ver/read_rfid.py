#!/usr/bin/env python3
import time
import socket
import RPi.GPIO as GPIO
from pirc522 import RFID

# 關閉 GPIO 警告：避免 "This channel is already in use" 類型提示
GPIO.setwarnings(False)

# 不用 IRQ，避免 RuntimeError: Failed to add edge detection
rdr = RFID(pin_irq=None)
SERVER_IP = "127.0.0.1"
SERVER_PORT = 8080

room_0_UID = 538838369608  # 小白卡
room_1_UID = 954533143448  # elton  悠遊卡  
room_3_UID = 48665919977   # 磁扣


def uid_to_hex(uid_bytes):
    return ''.join(f'{b:02X}' for b in uid_bytes)

def send_reserve(uid_hex):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((SERVER_IP, SERVER_PORT))
            if uid_hex==room_0_UID:
              cmd = f"checkin 0"
            elif uid_hex == room_1_UID:
              cmd = f"checkin 1"
            elif uid_hex == room_3_UID:
              cmd = f"checkin 2"
            else:
              cmd = "nononono"
            print("command: ", cmd)
            sock.sendall(cmd.encode())
            resp = sock.recv(1024).decode().strip()
            print(f"[server] {resp}")
    except Exception as e:
        print(f"[socket error] {e}")

DEBOUNCE_SEC = 5
last_time = 0

try:
    print("把悠遊卡靠近 RC522（Ctrl+C 結束）")
    last = None

    while True:
        err, _ = rdr.request()
        if not err:
            now = time.time()
            err, uid = rdr.anticoll()
            if not err and uid:
                uid_hex = uid_to_hex(uid)

                # 去抖：同一張卡不要一直狂刷輸出
                if (uid_hex != last)  or (now - last_time) >= DEBOUNCE_SEC:
                    # print(f"UID bytes: {uid}")
                    # print(f"UID hex  : {uid_hex}")
                    print(f"UID dec  : {int(uid_hex, 16)}")
                    send_reserve(int(uid_hex, 16))
                    print("-" * 30)
                    last = uid_hex
                    last_time = now
                time.sleep(0.6)

        time.sleep(0.05)

except KeyboardInterrupt:
    pass
finally:
    rdr.cleanup()
