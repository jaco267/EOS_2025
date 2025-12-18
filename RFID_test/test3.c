#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <string.h>

#define RST_PIN 25    // BCM GPIO 25
#define SPI_DEV "/dev/spidev0.0"
#define SPI_SPEED 1000000 // 1 MHz

// MFRC522 registers
#define CommandReg          0x01
#define ComIEnReg           0x02
#define DivIEnReg           0x03
#define ComIrqReg           0x04
#define DivIrqReg           0x05
#define FIFODataReg         0x09
#define FIFOLevelReg        0x0A
#define ControlReg          0x0C
#define BitFramingReg       0x0D
#define ModeReg             0x11
#define TxControlReg        0x14
#define TxAutoReg           0x15
#define TModeReg            0x2A
#define TPrescalerReg       0x2B
#define TReloadRegH         0x2C
#define TReloadRegL         0x2D
#define VersionReg          0x37

// Commands
#define PCD_IDLE            0x00
#define PCD_AUTHENT         0x0E
#define PCD_TRANSCEIVE      0x0C
#define PCD_RESETPHASE      0x0F
#define PCD_CALCCRC         0x03

static int spi_fd = -1;
static struct gpiod_chip *chip;
static struct gpiod_line *rst_line;

// SPI write register
void MFRC522_WriteReg(uint8_t addr, uint8_t val){
    uint8_t buf[2];
    buf[0] = ((addr<<1)&0x7E);
    buf[1] = val;
    write(spi_fd, buf, 2);
}

// SPI read register
uint8_t MFRC522_ReadReg(uint8_t addr){
    uint8_t buf[2];
    buf[0] = ((addr<<1)&0x7E)|0x80;
    buf[1] = 0;
    write(spi_fd, buf, 2);
    read(spi_fd, buf, 2);
    return buf[1];
}

// Reset MFRC522
void MFRC522_Reset(){
    MFRC522_WriteReg(CommandReg, PCD_RESETPHASE);
}

// Init MFRC522
void MFRC522_Init(){
    // Initialize GPIO using libgpiod
    chip = gpiod_chip_open("/dev/gpiochip0");
    if(!chip){ perror("gpiod_chip_open"); return; }

    rst_line = gpiod_chip_get_line(chip, RST_PIN);
    if(!rst_line){ perror("gpiod_chip_get_line"); return; }

    if(gpiod_line_request_output(rst_line, "mfrc522", 1) < 0){
        perror("gpiod_line_request_output");
        return;
    }

    // Initialize SPI
    spi_fd = open(SPI_DEV, O_RDWR);
    if(spi_fd < 0){ perror("SPI open"); return; }

    uint8_t mode = 0;
    uint32_t speed = SPI_SPEED;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    MFRC522_Reset();

    MFRC522_WriteReg(TModeReg, 0x8D);
    MFRC522_WriteReg(TPrescalerReg, 0x3E);
    MFRC522_WriteReg(TReloadRegL, 30);
    MFRC522_WriteReg(TReloadRegH, 0);
    MFRC522_WriteReg(TxAutoReg, 0x40);
    MFRC522_WriteReg(ModeReg, 0x3D);
    MFRC522_WriteReg(TxControlReg, 0x83);
}

// Simple Request command
int MFRC522_Request(uint8_t *TagType){
    MFRC522_WriteReg(BitFramingReg, 0x07);
    MFRC522_WriteReg(FIFOLevelReg, 0x80);
    MFRC522_WriteReg(FIFODataReg, 0x26); // REQA
    MFRC522_WriteReg(CommandReg, PCD_TRANSCEIVE);

    // Wait for completion
    int i;
    for(i=0; i<2000; i++){
        uint8_t n = MFRC522_ReadReg(ComIrqReg);
        if(n & 0x30) break;
    }

    uint8_t n = MFRC522_ReadReg(FIFOLevelReg);
    if(n != 2) return 1; // error

    TagType[0] = MFRC522_ReadReg(FIFODataReg);
    TagType[1] = MFRC522_ReadReg(FIFODataReg);
    return 0;
}

int main(){
    MFRC522_Init();
    printf("MFRC522 Initialized.\n");

    uint8_t tagType[2];
    while(1){
        if(MFRC522_Request(tagType) == 0){
            printf("Card detected, Type: 0x%02X 0x%02X\n", tagType[0], tagType[1]);
        }
        usleep(500000);
    }

    gpiod_line_release(rst_line);
    gpiod_chip_close(chip);
    close(spi_fd);
    return 0;
}
