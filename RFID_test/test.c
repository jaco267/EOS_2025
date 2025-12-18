#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define SPI_DEV "/dev/spidev0.0"
#define SPI_SPEED 250000  // 250 kHz
#define RST_GPIO 25

// 將 GPIO25 當作 RST 控制
void gpio_rst()
{
    FILE *f;
    f = fopen("/sys/class/gpio/export","w");
    if(f){ fprintf(f,"%d",RST_GPIO); fclose(f); }
    f = fopen("/sys/class/gpio/gpio25/direction","w");
    if(f){ fprintf(f,"out"); fclose(f); }
    f = fopen("/sys/class/gpio/gpio25/value","w");
    if(f){ fprintf(f,"0"); fclose(f); }
    usleep(100000); // 100ms
    f = fopen("/sys/class/gpio/gpio25/value","w");
    if(f){ fprintf(f,"1"); fclose(f); }
    usleep(150000); // 150ms
}

int spi_xfer(int fd, uint8_t *tx, uint8_t *rx, size_t len)
{
    struct spi_ioc_transfer tr;
    memset(&tr,0,sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = len;
    tr.speed_hz = SPI_SPEED;
    tr.delay_usecs = 0;
    tr.bits_per_word = 8;
    tr.cs_change = 0;

    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if(ret < 1) perror("SPI_IOC_MESSAGE");
    return ret;
}

// RC522 讀寄存器
int rc522_read(int fd, uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = { (reg<<1)|0x80, 0x00 };
    uint8_t rx[2] = {0,0};
    if(spi_xfer(fd, tx, rx, 2) < 1) return -1;
    *val = rx[1];
    return 0;
}

// RC522 SoftReset
int rc522_reset(int fd)
{
    uint8_t tx[2] = { 0x01<<1, 0x0F }; // CommandReg = SoftReset
    uint8_t rx[2] = {0,0};
    return spi_xfer(fd, tx, rx, 2);
}

int main()
{
    int fd = open(SPI_DEV,O_RDWR);
    if(fd<0){ perror("SPI open"); return 1; }

    uint8_t mode=0;
    ioctl(fd,SPI_IOC_WR_MODE,&mode);

    uint32_t speed=SPI_SPEED;
    ioctl(fd,SPI_IOC_WR_MAX_SPEED_HZ,&speed);

    gpio_rst();

    rc522_reset(fd);
    usleep(200000); // 200ms 等待 SoftReset 完成

    uint8_t ver=0;
    if(rc522_read(fd, 0x37, &ver)<0){ perror("rc522_read"); return 1; }

    printf("VersionReg: 0x%02X\n",ver);
    if(ver==0x00||ver==0xFF){
        printf("RC522 沒有回應，檢查 SPI / RST / CE0\n");
        return 1;
    } else {
        printf("RC522 SPI 通訊正常\n");
    }

    close(fd);
    return 0;
}
