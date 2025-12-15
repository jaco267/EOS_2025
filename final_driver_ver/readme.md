- makefile 記得 linux kernel path 要改成自己電腦上的路徑   (不是 /home/elton/Desktop...)
- scp 的路徑也要改成自己的設定 (不是 elton@192...)  
- 目前有新增加 user_id  所以 reserve cmd 變成   

```sh
reserve <room_id> <user_id>
```

```sh

make clean
make 
scp -r ../final_driver_ver   elton@192.168.222.222:~/Desktop

ssh elton@192.168.222.222
pi$ cd ~/Desktop/final_driver_ver
pi$ sudo insmod room_driver.ko  
#    sudo rmmod room_driver.ko
pi$ lsmod | grep room_driver
pi$ dmesg |tail -n 10

#### user space program   
pi$ make user
pi$ sudo su 
pi$ ./test_driver    
# turn off led 
pi$ echo "led 0" > /dev/etx_device
##### run server 
pi$ ./room_server
pi$ ./room_client   
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

pi$ cat /dev/etx_device

```

### todo 
#### 重要  
- todo : button  
- todo : rfid  
- todo : ncurse
- todo : led 7seg 目前只有status 的時候會更新狀態, 能否在 timeout 時自動更新led (used->timeout release, reserve -> timeout relase)   
#### 次要
- todo : user 現在還只是一個 count 應該要 加入 user information (在 reserve 時輸入名字)
  -  todo  release：只允許使用者本人釋放
  -  todo  check_in：只允許原本預約的人
  -  todo  room wait list 紀錄 id
  -  todo 現在好像有一個小bug 就是reserve limitation 好像如果 wait count = 4 的時候可以略過 (在wait 的時候 reserve wait count 可以累積到 4)
  - todo waiting user timeout 等太久自動移除
  - user-based daily limit (不是 room-based)




#### led pin,  現在只要把 三個 led 接上 gpio14,15,18 就好了 (之後會當成 state(reserve, used, free), 但我還沒寫)
```
pin 腳                          RPi gpio pin  
           gnd           3.3 v   ---1  2                                
        g f | a b                   3  4                           
        | | | | |                   5  6 ---gnd                       
         -------                    7  8 ---GPIO 14 (led1)         
        |   a   |                   9  10---GPIO 15 (led2)         
        | f   b |    GPIO 17  (a)---11 12---GPIO 18 (led3)       
        |   G   |    GPIO 27  (b)---13 14                     
        | E   C |    GPIO 22  (c)---15 16                    
        |   D . |                   17 18                         
        ---------    GPIO 10  (d)---19 20                     
        | | | | |    GPIO  9  (e)---21 22                   
        e d | c Dp   GPIO 11  (f)---23 24             
           gnd                      25 26             
                     GPIO  0  (g)---27 28               
                     GPIO  5     ---29 30               
                     GPIO  6     ---31 32               
                                    33 34               
                                    35 36 ---GPIO 16 (button)               
                     GPIO 26     ---37 38               
                             gnd    39 40 ---GPIO 21 
```

- [button 接線](https://raspberrypihq.com/use-a-push-button-with-raspberry-pi-gpio/)