```sh

scp -r ../RFID_test   elton@192.168.222.222:~/Desktop

ssh elton@192.168.222.222
sudo apt install -y libgpiod-dev
sudo apt install wiringpi
make
./main
```

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
