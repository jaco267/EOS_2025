```sh
make clean
make  
scp -r ../final_driver_ver   elton@192.168.222.222:~/Desktop
pi$ cd ~/Desktop/final_driver_ver
pi$ sudo insmod room_driver.ko  
#    sudo rmmod room_driver.ko
pi$ lsmod | grep room_driver
pi$ dmesg |tail -n 10

#### usser space program   
pi$ make room_client
pi$ ./room_client 
```