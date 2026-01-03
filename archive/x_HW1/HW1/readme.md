```sh
## in host  
make  
host$ scp -r ../HW1   elton@192.168.222.222:~/Desktop
#make clean
# in pi 
cd ~/Desktop/HW1  
sudo insmod hw1.ko  
make hw1_writer
./hw1_writer
```