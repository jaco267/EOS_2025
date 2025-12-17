#!/usr/bin/env python3
import time
import RPi.GPIO as GPIO
from pirc522 import RFID

# 關閉 GPIO 警告：避免 "This channel is already in use" 類型提示
GPIO.setwarnings(False)

# 不用 IRQ，避免 RuntimeError: Failed to add edge detection
rdr = RFID(pin_irq=None)

def uid_to_hex(uid_bytes):
    return ''.join(f'{b:02X}' for b in uid_bytes)
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
                # if (now - last_time) >= DEBOUNCE_SEC:
                  print(f"UID bytes: {uid}")
                  print(f"UID hex  : {uid_hex}")
                  print(f"UID dec  : {int(uid_hex, 16)}")
                  print("-" * 30)
                  last = uid_hex
                  last_time = now
                time.sleep(0.6)

        time.sleep(0.05)

except KeyboardInterrupt:
    pass
finally:
    rdr.cleanup()
