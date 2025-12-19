#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define SPI_DEV "/dev/spidev0.0"
#define SPI_SPEED 250000
#define RST_GPIO 25
#define DEBOUNCE_SEC 5

// MFRC522 Registers
#define CommandReg     0x01
#define ComIrqReg      0x04
#define ModeReg        0x11
#define REG_TX_CONTROL   0x14
#define TxASKReg       0x15
#define TReloadRegH    0x2C
#define TReloadRegL    0x2D
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define BitFramingReg  0x0D
#define VersionReg     0x37

// Commands
#define MODE_IDLE       0x00
#define MODE_TRANSREC 0x0C
#define MODE_RESET  0x0F
//*---------
#define ANTENNA_GAIN 0x04


// GPIO RST
void gpio_rst(){
    FILE *f;
    f=fopen("/sys/class/gpio/export","w"); if(f){ fprintf(f,"%d",RST_GPIO); fclose(f); }
    f=fopen("/sys/class/gpio/gpio25/direction","w"); if(f){ fprintf(f,"out"); fclose(f); }
    f=fopen("/sys/class/gpio/gpio25/value","w"); if(f){ fprintf(f,"0"); fclose(f); }
    usleep(100000);
    f=fopen("/sys/class/gpio/gpio25/value","w"); if(f){ fprintf(f,"1"); fclose(f); }
    usleep(150000);
}
int spi_transfer(int fd,uint8_t *tx,uint8_t *rx,size_t len);
int rc522_write(int fd,uint8_t reg,uint8_t val); 
int rc522_read(int fd,uint8_t reg,uint8_t *val);
void rc522_reset(int fd);
void rc522_init(int fd);
int rc522_wait(int fd,uint8_t mask);
int rc522_request(int fd);
int rc522_anticoll(int fd,uint8_t *uid);
unsigned long uid_to_dec(uint8_t *uid);
void set_bitmask(int fd, uint8_t address, uint8_t mask); 

void card_write(int fd, uint8_t* data, int data_len, 
    int* error, uint8_t* back_data, int* back_len){  
    *back_len = 0;  
    *error = 0; 
    uint8_t irq = 0x77;  uint8_t irq_wait = 0x30;  

    int last_bits; 
    uint8_t n = 0;  
    //*--------------------------------
    rc522_write(fd, 0x02, irq | 0x80); 
    //todo  clear bit

    //*** set_bitmask  
    rc522_write(fd,0x0A,0x80);  //FIFOLevelReg   0x0A
    //*** REQA   
    rc522_write(fd,0x09,0x26); // REQA  // FIFODataReg    0x09
    //*** mode trans */
    rc522_write(fd,0x01,MODE_TRANSREC); //*CommandReg     0x01 //*MODE_TRANSREC 0x0C
    rc522_write(fd,0x0D,0x80);  //*BitFramingReg  0x0D
    // printf("card write---\n");
}
// 初始化 RC522
void rc522_init(int fd){
    rc522_reset(fd);
    rc522_write(fd,0x2A,0x8D); //0x2A TModeReg
    rc522_write(fd,0x2B,0x3E); //0x2B TPrescalerReg 
    rc522_write(fd,0x2D,30); //TReloadRegL    0x2D
    rc522_write(fd,0x2C,0); //TReloadRegH    0x2C
    rc522_write(fd,0x15,0x40); //TxASKReg       0x15
    rc522_write(fd,0x11,0x3D); // ModeReg        0x11
    rc522_write(fd,0x26,ANTENNA_GAIN<<4); // 打開天線  //TxControlReg   0x14
    //*----set_antenna
    uint8_t current; 
    int ret = rc522_read(fd, REG_TX_CONTROL, &current);
    printf("current = 0x%02X\n", current);
    if (!(current & 0x03)){
      set_bitmask(fd, REG_TX_CONTROL, 0x03);
    }
}
void set_bitmask(int fd, uint8_t address, uint8_t mask){
    uint8_t current;
    int ret = rc522_read(fd, address, &current);
    rc522_write(fd,address,current|mask); 
}
// SPI full-duplex transfer
int spi_transfer(int fd,uint8_t *tx,uint8_t *rx,size_t len){
    struct spi_ioc_transfer tr;
    memset(&tr,0,sizeof(tr));
    tr.tx_buf=(unsigned long)tx;
    tr.rx_buf=(unsigned long)rx;
    tr.len=len;
    tr.speed_hz=SPI_SPEED;
    tr.bits_per_word=8;
    return ioctl(fd,SPI_IOC_MESSAGE(1),&tr);
}
// RC522 寫寄存器
int rc522_write(int fd,uint8_t address,uint8_t val){
    uint8_t tx[2]={ (address << 1) & 0x7E, val }, rx[2]={0,0};
    return spi_transfer(fd,tx,rx,2);
}
// RC522 讀寄存器
int rc522_read(int fd,uint8_t address,uint8_t *val){
    uint8_t tx[2]={ ((address<<1) & 0x7E) | 0x80,0x00 }, rx[2]={0,0};
    if(spi_transfer(fd,tx,rx,2)<1) return -1;
    *val=rx[1];
    return 0;
}
// SoftReset
void rc522_reset(int fd){
    rc522_write(fd,CommandReg,MODE_RESET);
    usleep(200000);
}

// 等待 RX / TX 完成
int rc522_wait(int fd,uint8_t mask){
    uint8_t val;
    int timeout=200;
    do{
        rc522_read(fd,ComIrqReg,&val);
        if(val & mask) return 1;
        usleep(1000);
        timeout--;
    }while(timeout>0);
    return 0;
}
// Request
int rc522_request(int fd){
    rc522_write(fd,0x0D,0x07); //BitFramingReg  0x0D
    //todo CARD_WRITE
    uint8_t data[1] = {0x26};
    int error; uint8_t back_data[100]; int back_len =0 ;
    card_write(fd,data,1,&error,&back_data,&back_len);

    return rc522_wait(fd,0x30);
}

// Anti-collision
int rc522_anticoll(int fd,uint8_t *uid){
    rc522_write(fd,BitFramingReg,0x00);
    rc522_write(fd,FIFOLevelReg,0x80);
    rc522_write(fd,FIFODataReg,0x93);
    rc522_write(fd,FIFODataReg,0x20);
    rc522_write(fd,CommandReg,MODE_TRANSREC);

    if(!rc522_wait(fd,0x30)) return 0;
    for(int i=0;i<5;i++) rc522_read(fd,FIFODataReg,&uid[i]);
    return 5;
}

// UID bytes -> dec
unsigned long uid_to_dec(uint8_t *uid)
{
    unsigned long val=0;
    for(int i=0;i<5;i++) val=(val<<8)|uid[i];
    return val;
}

int main()
{
    int fd=open(SPI_DEV,O_RDWR);
    if(fd<0){ perror("SPI"); return 1; }
    uint8_t mode=0; ioctl(fd,SPI_IOC_WR_MODE,&mode);
    uint32_t speed=SPI_SPEED; ioctl(fd,SPI_IOC_WR_MAX_SPEED_HZ,&speed);

    gpio_rst();
    rc522_init(fd);

    uint8_t ver=0;
    rc522_read(fd,VersionReg,&ver);
    printf("RC522 Version: 0x%02X\n",ver);
    if(ver==0x00||ver==0xFF){ printf("RC522 沒有回應，檢查 SPI / RST / CE0\n"); return 1; }

    printf("RC522 ready. Place card near reader.\n");

    uint8_t uid[5];
    unsigned long last_uid=0;
    time_t last_time=0;

    while(1){
        if(rc522_request(fd)){
            if(rc522_anticoll(fd,uid)){
                unsigned long uid_dec=uid_to_dec(uid);
                time_t now=time(NULL);
                if(uid_dec!=last_uid || (now-last_time)>=DEBOUNCE_SEC){
                    printf("UID: %lu\n",uid_dec);
                    last_uid=uid_dec;
                    last_time=now;
                }
            }
        }
        usleep(500000);
    }

    close(fd);
    return 0;
}
