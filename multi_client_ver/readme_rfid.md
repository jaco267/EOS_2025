
RFID-python測試
1. 連到樹莓派
2. sudo raspi-config
# Interface Options -> SPI -> Enable
3. sudo reboot
4. ls -l /dev/spidev*
#預期會看到：
  /dev/spidev0.0
  /dev/spidev0.1
5. 裝套件
sudo apt update
sudo apt install -y git python3-spidev python3-rpi.gpio python3-pip python3-dev
sudo python3 -m pip install pi-rc522 --break-system-packages
6. sudo python3 read_ezcard_uid.py 
