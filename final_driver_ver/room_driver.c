#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h> //copy_to/from_user()
#include <linux/gpio.h> //GPIO

/*
pin 腳                          RPi gpio pin  
           gnd                      1  2                                
        g f | a b                   3  4                           
        | | | | |                   5  6 ---gnd                       
         -------                    7  8 ---GPIO 14 (led6?)         
        |   a   |                   9  10---GPIO 15 (led7?)         
        | f   b |    GPIO 17  (a)---11 12---GPIO 18 (todo)       
        |   G   |    GPIO 27  (b)---13 14                     
        | E   C |    GPIO 22  (c)---15 16                    
        |   D . |                   17 18                         
        ---------    GPIO 10  (d)---19 20                     
        | | | | |    GPIO  9  (e)---21 22                   
        e d | c Dp   GPIO 11  (f)---23 24             
           gnd                      25 26             
                     GPIO  0  (g)---27 28               
                     GPIO  5(led)---29 30               
                     GPIO  6(led)---31 32               
                                    33 34               
                                    35 36 ---GPIO 16 (led)               
                     GPIO 26(led)---37 38               
                             gnd    39 40 ---GPIO 21 (led)
*/
#define GPIO_14 (14) //led 
#define GPIO_15 (15) //led 
#define GPIO_18 (18) //led

dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;
static int __init etx_driver_init(void);
static void __exit etx_driver_exit(void);
/*************** Driver functions **********************/
static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp,
      char __user *buf, size_t len,loff_t * off);
static ssize_t etx_write(struct file *filp,
      const char *buf, size_t len, loff_t * off);

//File operation structure
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = etx_read,
    .write = etx_write,
    .open = etx_open,
    .release = etx_release,
};
// This function will be called when we open the Device file
static int etx_open(struct inode *inode, struct file *file){
  pr_info("Device File Opened...!!!\n"); return 0;
}
// This function will be called when we close the Device file
static int etx_release(struct inode *inode, struct file *file){
  pr_info("Device File Closed...!!!\n"); return 0;
}

//! when we $ cat /dev/etx_device
// etx_read is called when we read the Device file
static ssize_t etx_read(struct file *filp, char __user *buf, 
    size_t len, loff_t *off){
    
    // 核心緩衝區：用於儲存格式化後的 GPIO 狀態字串
    // 假設我們需要足夠的空間儲存 "GPIO_14:1, GPIO_15:0, GPIO_18:1\n"
    char kernel_buffer[128]; 
    size_t required_len;
    ssize_t ret;
    
    // 1. 讀取 GPIO 狀態
    uint8_t state_14 = gpio_get_value(GPIO_14);
    uint8_t state_15 = gpio_get_value(GPIO_15);
    uint8_t state_18 = gpio_get_value(GPIO_18);
    
    // 2. 格式化結果字串
    // 將多個狀態值格式化到 kernel_buffer 中
    required_len = snprintf(kernel_buffer, sizeof(kernel_buffer), 
                            "leds:  GPIO_14:%d, GPIO_15:%d, GPIO_18:%d\n", 
                            state_14, state_15, state_18);
    
    // 處理檔案偏移量 (確保只讀取一次)
    if (*off > 0) {
        return 0; // 如果 offset 大於 0，表示已經讀取過了，回傳 0 代表檔案結尾 (EOF)
    }

    // 3. 確定實際要傳送的位元組數
    if (required_len > len) {
        // 如果格式化後的字串長度超過使用者提供的緩衝區大小 (len)，則截斷
        ret = len; 
    } else {
        ret = required_len;
    }
    
    // 4. 將字串從核心空間複製到使用者空間
    // copy_to_user(目標使用者緩衝區, 來源核心變數, 位元組數)
    if( copy_to_user(buf, kernel_buffer, ret) > 0) {
        pr_err("ERROR: Not all the bytes have been copied to user\n");
        return -EFAULT; // 回傳標準 I/O 錯誤碼
    }
    
    // 5. 更新檔案偏移量 (重要)
    *off += ret;
    
    pr_info("Read function: Sent GPIO states: '%s', length %zd.\n", kernel_buffer, ret);
    
    // 6. 回傳實際複製的位元組數
    return ret; 
}

//! When we call echo (0 or 1) > /dev/ext_device
// This function will be called when we write the Device file
static ssize_t etx_write(struct file *filp,
    const char __user *buf, size_t len, loff_t *off){
  char rec_buf[32] = {0};
  char cmd[16] = {0};
  char arg[16] = {0};
  // 從 user space 拷貝字串 //* buf is user space input, we copy buf into rec_buf
  if (copy_from_user(rec_buf, buf, len) > 0) {
    pr_err("ERROR: Not all bytes copied from user\n");
    return -EFAULT;
  }
  rec_buf[len] = '\0';  // 確保結尾有 '\0'
  pr_info("Received cmd: %s\n", rec_buf);

  // 解析指令，例如 "7seg A" 或 "led0 on"
  sscanf(rec_buf, "%15s %15s", cmd, arg);
  pr_info("Command: [%s], Arg: [%s]\n", cmd, arg);
  //todo -- 控制 7-segment ---
  if(strcmp(cmd, "led")==0){ //* 控制 led  
    if (strlen(arg) == 0) {
        pr_err("No 7seg argument provided\n");
        return len;
    }
    if (arg[0]== '1')  {
        gpio_set_value(GPIO_14, 1); // 假設 LED 接在 GPIO22
        gpio_set_value(GPIO_15, 0);
        gpio_set_value(GPIO_18, 0);
    }else if (arg[0]=='2'){
        gpio_set_value(GPIO_14 ,0);
        gpio_set_value(GPIO_15 ,1);
        gpio_set_value(GPIO_18 ,0);
    }else if (arg[0]=='3'){
        gpio_set_value(GPIO_14 ,0);
        gpio_set_value(GPIO_15 ,0);
        gpio_set_value(GPIO_18 ,1);
    }else{
        gpio_set_value(GPIO_14 ,0);
        gpio_set_value(GPIO_15 ,0);
        gpio_set_value(GPIO_18 ,0);
        pr_info("LED0 turned OFF\n");
    }
  }
  return len;
}

//** Module Init function
static int __init etx_driver_init(void){
  /*Allocating Major number*/
  if((alloc_chrdev_region(&dev, 0, 1, "etx_Dev")) <0){
    pr_err("Cannot allocate major number\n");
    goto r_unreg;
  }
  pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
  /*Creating cdev structure*/
  cdev_init(&etx_cdev,&fops);
  /*Adding character device to the system*/
  if((cdev_add(&etx_cdev,dev,1)) < 0){
    pr_err("Cannot add the device to the system\n");
    goto r_del;
  }
  /*Creating struct class*/
  if((dev_class = class_create(THIS_MODULE,"etx_class")) == NULL){
    pr_err("Cannot create the struct class\n");
    goto r_class;
  }
  /*Creating device*/
  if((device_create(dev_class,NULL,dev,NULL,"etx_device")) == NULL){
    pr_err( "Cannot create the Device \n");
    goto r_device;
  }
  if(!gpio_is_valid(GPIO_14)||
     !gpio_is_valid(GPIO_15)||
     !gpio_is_valid(GPIO_18)
    ){//Checking the GPIO is valid or not
    pr_err("GPIO is not valid\n");
    goto r_device;
  }
  //Requesting the GPIO
  if(gpio_request(GPIO_14,"GPIO_14") < 0 ||
     gpio_request(GPIO_15,"GPIO_15") < 0 ||
     gpio_request(GPIO_18,"GPIO_18") < 0 
    ){
    pr_err("ERROR: GPIO request failed\n");
    goto r_gpio;
  }
  gpio_direction_output(GPIO_14,0);
  gpio_direction_output(GPIO_15,0);
  gpio_direction_output(GPIO_18,0);

  gpio_export(GPIO_14,false);
  gpio_export(GPIO_15,false);
  gpio_export(GPIO_18,false);
  pr_info("Device Driver Insert...Done!!!\n");
  return 0;
  r_gpio:
    gpio_free(GPIO_14);
    gpio_free(GPIO_15);
    gpio_free(GPIO_18);
  r_device:
    device_destroy(dev_class,dev);
  r_class:
    class_destroy(dev_class);
  r_del:
    cdev_del(&etx_cdev);
  r_unreg:
    unregister_chrdev_region(dev,1);
  return -1;
}

// Module exit function
static void __exit etx_driver_exit(void){
  gpio_unexport(GPIO_14);
  gpio_unexport(GPIO_15);
  gpio_unexport(GPIO_18);
  gpio_free(GPIO_14);
  gpio_free(GPIO_15);
  gpio_free(GPIO_18);

  device_destroy(dev_class,dev);
  class_destroy(dev_class);
  cdev_del(&etx_cdev);
  unregister_chrdev_region(dev, 1);
  pr_info("Device Driver Remove...Done!!\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("EltonHong <timmyhorng@gmail.com>");
MODULE_DESCRIPTION("room management device driver - GPIO Driver");