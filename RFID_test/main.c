#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <string.h>
#include <errno.h>

#define SPI_DEV        "/dev/spidev0.0"
#define SPI_SPEED      1000000

#define RC522_RST_GPIO 25

// MFRC522 Registers
#define CommandReg     0x01
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define BitFramingReg  0x0D
#define ModeReg        0x11
#define TxASKReg       0x15
#define TModeReg       0x2A
#define TPrescalerReg  0x2B
#define TReloadRegH    0x2C
#define TReloadRegL    0x2D
#define TxControlReg 0x14
// Commands
#define PCD_Idle       0x00
#define PCD_Mem        0x01
#define PCD_Transceive 0x0C
#define PCD_SoftReset  0x0F

static int spi_fd = -1;
static struct gpiod_chip *chip;
static struct gpiod_line *rst_line;

// SPI write
int rc522_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2];
    buf[0] = (reg << 1) & 0x7E;
    buf[1] = val;
    return write(spi_fd, buf, 2);
}

// SPI read
int rc522_read(uint8_t reg, uint8_t *val)
{
    uint8_t tx[2], rx[2];
    tx[0] = ((reg << 1) & 0x7E) | 0x80;
    tx[1] = 0;
    if (write(spi_fd, tx, 2) != 2) return -1;
    if (read(spi_fd, rx, 2) != 2) return -1;
    *val = rx[1];
    return 0;
}

// Reset RC522
void rc522_reset()
{
    gpiod_line_set_value(rst_line, 0);
    usleep(100000);
    gpiod_line_set_value(rst_line, 1);
    usleep(100000);
    rc522_write(CommandReg, PCD_SoftReset);
    usleep(100000);
}

// Init RC522
void rc522_init()
{
    rc522_reset();

    rc522_write(TModeReg, 0x8D);
    rc522_write(TPrescalerReg, 0x3E);
    rc522_write(TReloadRegL, 30);
    rc522_write(TReloadRegH, 0);
    rc522_write(TxASKReg, 0x40);
    rc522_write(ModeReg, 0x3D);

    rc522_write(TxControlReg, 0x03);
}

// Request card (REQA)
void rc522_request()
{
    rc522_write(BitFramingReg, 0x07);
    rc522_write(FIFOLevelReg, 0x80); // clear FIFO
    rc522_write(FIFODataReg, 0x26);  // REQA
    rc522_write(CommandReg, PCD_Transceive);
    rc522_write(BitFramingReg, 0x87);
    usleep(100000);
}

// Anti-collision, read UID
int rc522_anticoll(uint8_t *uid)
{
    int i;
    rc522_write(BitFramingReg, 0x00);
    rc522_write(FIFOLevelReg, 0x80); // clear FIFO
    rc522_write(FIFODataReg, 0x93); // ANTICOLL CL1
    rc522_write(FIFODataReg, 0x20);
    rc522_write(CommandReg, PCD_Transceive);
    rc522_write(BitFramingReg, 0x80);
    usleep(100000);
    for (i = 0; i < 5; i++)
        rc522_read(FIFODataReg, &uid[i]);
    return 5;
}

int main()
{
    uint8_t uid[5];
    int ret;

    // open SPI
    spi_fd = open(SPI_DEV, O_RDWR);
    if (spi_fd < 0) { perror("SPI open"); return 1; }

    uint8_t mode = 0;
    uint32_t speed = SPI_SPEED;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    // setup RST GPIO
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) { perror("gpiod_chip_open"); return 1; }
    rst_line = gpiod_chip_get_line(chip, RC522_RST_GPIO);
    gpiod_line_request_output(rst_line, "rc522_rst", 1);

    rc522_init();

    printf("RC522 ready. Place card near reader.\n");

    while (1)
    {
        rc522_request();
        ret = rc522_anticoll(uid);
        if (ret > 0)
        {
            printf("UID: ");
            for (int i = 0; i < 4; i++) printf("%02X ", uid[i]);
            printf("\n");
            sleep(1);
        }
    }

    gpiod_line_release(rst_line);
    gpiod_chip_close(chip);
    close(spi_fd);
    return 0;
}
