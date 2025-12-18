#### Please branch before commit 
- [rpi 連接無線網路](https://hackmd.io/@vin0719/HyycBxnjn)
- button 不按下  gpio16 是 1  按下變成  0 
- makefile 記得 linux kernel path 要改成自己電腦上的路徑   (不是 /home/elton/Desktop...)
- scp 的路徑也要改成自己的設定 (不是 elton@192...)  
- 目前有新增加 user_id  所以 reserve cmd 變成   

```sh
reserve <room_id> <user_id>
```

```sh
# export LINUX_KERNEL_PATH=/home/elton/Desktop/emb_linux/linux
make clean
#* sudo apt install libncurses-dev
#* make ncurse
#* ./ncurse
# make clean LINUX_KERNEL_PATH=/home/stu/linux
# make clean LINUX_KERNEL_PATH=/home/ncrl/rpi-linux-6.1
# make clean LINUX_KERNEL_PATH=/home/elton/Desktop/emb_linux/linux
make 
# make LINUX_KERNEL_PATH=/home/stu/linux
# make LINUX_KERNEL_PATH=/home/ncrl/rpi-linux-6.1
# make LINUX_KERNEL_PATH=/home/elton/Desktop/emb_linux/linux
scp -r ../final_driver_ver   elton@192.168.222.222:~/Desktop
# scp -r ../final_driver_ver user@192.168.222.222:~/Desktop/final_driver_ver/


ssh elton@192.168.222.222
pi$ cd ~/Desktop/final_driver_ver
pi$ sudo insmod room_driver.ko  
#    sudo rmmod room_driver.ko
pi$ lsmod | grep room_driver
pi$ dmesg |tail -n 10

#### user space program   
pi$ make user
pi$ make ncurse 
pi$ make test_client
pi$ sudo su 
pi$ ./test_driver    
# turn off led 
pi$ echo "led 0" > /dev/etx_device
##### run server 
pi$ ./room_server
pi$ ./room_client   
pi$ python read_rfid.py
#* status
#* status  0    #只單純查看房間0 的狀態 led1 亮起 代表 free  
#* reserve 0 312513129
#* status  0    五秒內  可以看到 led 2 亮起 代表 reserved  
#* checkin 0    五秒內
#* status  0    可以看到 led 3 亮起
#* status 

pi$ echo "led 1" > /dev/etx_device
pi$ echo "led 2" > /dev/etx_device
pi$ echo "led 3" > /dev/etx_device
#* 0 turn off 
pi$ echo "led 0" > /dev/etx_device

pi$ echo "7seg 7" > /dev/etx_device
#* 7seg turn off 
pi$ echo "7seg g" > /dev/etx_device

# 可以透過 gpio 看 各個gpio 和 按鈕狀態 
pi$ cat /dev/etx_device
# leds state: GPIO_14:0, GPIO_15:0, GPIO_18:0, GPIO_17:0, GPIO_27:0, GPIO_22:0, GPIO_10:0, GPIO_9:0, GPIO_11:0, GPIO_0:0
# BTN:1

# rpi$ 關機  
# pi$ sudo shutdown -h now
```

### todo 
#### 重要  


- todo : [佳鴻] rfid  
  - [github](https://github.com/wzyy2/My_Linux_Driver/tree/master)
  - [csdn blog](https://www.cnblogs.com/jzcn/p/17171811.html)
  - 搜尋 rfid rc522 linux driver 
  - 搜尋 rfid rc522 linux driver github
- todo : [elton] ncurse
- todo : [嘉詳]倒數時間     
- todo : [嘉詳]status 顯示 register 的 資訊
#### 次要
-  todo  release：只允許使用者本人釋放
-  todo  check_in：只允許原本預約的人
-  todo 現在好像有一個小bug 就是reserve limitation 好像如果 wait count = 4 的時候可以略過 (在wait 的時候 reserve wait count 可以累積到 4)
- todo waiting user timeout 等太久自動移除
- user-based daily limit (不是 room-based)


#### 已完成
- [v][嘉詳] led 7seg 目前只有status 的時候會更新狀態, 能否在 timeout 時自動更新led (used->timeout release, reserve -> timeout relase)
- [v][elton] button 之後 直接 checkin
- [v][elton] checkout 取消輸入 id  因為時間不夠   
#### led pin,  三個 led 接上 gpio14,15,18  (當成 state(reserve, used, free))
- RFID 還沒弄
```
                                  RPi gpio pin  
   gnd                      3.3 v---1  2    5v   
g f | a b                 GPIO  2   3  4    5v          
| | | | |                 GPIO  3   5  6 ---gnd         
 -------                  GPIO  4   7  8 ---GPIO 14 (led1)
|   a   |                     gnd   9  10---GPIO 15 (led2)
| f   b |             (a) GPIO 17---11 12---GPIO 18 (led3)
|   G   |             (b) GPIO 27---13 14---gnd          
| E   C |             (c) GPIO 22---15 16---gpio23      
|   D . |      (RFID:咖啡)   3.3v ---17 18---gpio24           
---------      (RFID?藍色)GPIO 10---19 20---gnd      (RFID-橘色)
| | | | |      (RFID?綠)  GPIO  9---21 22---gpio25  (RFID-RST?紅)   
e d | c Dp     (RFID?紫)  GPIO 11---23 24---gpio8   (RFID-SDA,灰)       
   gnd                        gnd   25 26---gpio7       
                      (d) GPIO  0---27 28---gpio1 
                      (e) GPIO  5---29 30---gnd 
                      (f) GPIO  6---31 32---gpio12
                      (g) GPIO 13   33 34---gnd
                          GPIO 19   35 36---GPIO 16 (button) 
                          GPIO 26---37 38---GPIO 20
                             gnd    39 40---GPIO 21 
```

```
RFID-RC522  
pin    0   1   2   3     4  5   6   7
      SDA SCK MOSI MISO RQ GND RST 3x3
      灰  紫   藍   綠   黃  橘  紅  咖啡
GPIO  8   11  10   9    X      25      
```


- [button 接線](https://raspberrypihq.com/use-a-push-button-with-raspberry-pi-gpio/)

