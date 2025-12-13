
- makefile 記得 linux kernel path 要改成自己電腦上的路徑   (不是 /home/elton/Desktop...)
- scp 的路徑也要改成自己的設定 (不是 elton@192...)  

```sh
make clean
make 
scp -r ../final_driver_ver   elton@192.168.222.222:~/Desktop
pi$ cd ~/Desktop/final_driver_ver
pi$ sudo insmod room_driver.ko  
#    sudo rmmod room_driver.ko
pi$ lsmod | grep room_driver
pi$ dmesg |tail -n 10

#### user space program   
pi$ make user
pi$ sudo su 
pi$ ./test_driver    
##### run server 
pi$ ./room_server
pi$ ./room_client   
#* status
#* reserve 0 
#* checkin 0  
#* status 

```