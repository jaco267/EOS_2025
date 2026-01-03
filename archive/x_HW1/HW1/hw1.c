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
//LED is connected to this GPIO
static char Seg_7_map[16] ={ 
  //abcdefg 
  0b1111110,//0 
  0b0110000,//1
  0b1101101,//2 
  0b1111001,//3 
  0b0110011,//4 
  0b1011011,//5 
  0b1011111,//6 
  0b1110000,//7 
  0b1111111,//8 
  0b1111011,//9 
  0b1110111,//A,a 
  0b0011111,//B,b 
  0b1001110,//C,c 
  0b0111101,//D,d 
  0b1001111,//E,e 
  0b1000111 //F,f 
}; 
/*                    
pin 腳                        RPi gpio pin  
           gnd                    1  2                                        
        g f | a b                 3  4                                                                  
        | | | | |                 5  6 ---gnd                                                            
         -------                  7  8 ---GPIO 14 (led6?)                                                             
        |   a   |                 9  10---GPIO 15 (led7?)                                                                    
        | f   b |    GPIO 17(a)---11 12---GPIO 18 (todo)                                                                    
        |   G   |    GPIO 27(b)---13 14                                                                
        | E   C | GPIO 22(led1)---15 16                                                               
        |   D . |                 17 18                                                                
        --------- GPIO 10(led2)---19 20                                                              
        | | | | |  GPIO 9(led3)---21 22                                                         
        e d | c Dp GPIO 11(led4)--23 24                                                          
           gnd                    25 26                                                             
                   GPIO 0 (led5)--27 28               
                     GPIO 5(f) ---29 30               
                     GPIO 6(g) ---31 32               
                                  33 34               
                                  35 36 ---GPIO 16 (d)               
                     GPIO 26(e)---37 38               
                           gnd    39 40 ---GPIO 21 (c)
*/
#define GPIO_17 (17) //a 
#define GPIO_27 (27) //b 
#define GPIO_21 (21) //C
#define GPIO_16 (16) //D 
#define GPIO_26 (26) //E 
#define GPIO_5 (5) //f 
#define GPIO_6 (6) //G 
/////////////////////////// 
#define GPIO_22 (22)
#define GPIO_10 (10)
#define GPIO_9 (9)
#define GPIO_11 (11)
#define GPIO_0 (0)
#define GPIO_14 (14)
#define GPIO_15 (15)
#define GPIO_18 (18)
//todo 26  
//* GPIO 21 to connect to LED   
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
/******************************************************/
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
// This function will be called when we read the Device file
static ssize_t etx_read(struct file *filp, char __user *buf, 
    size_t len, loff_t *off){
  uint8_t gpio_state = 0;
  //reading GPIO value
  gpio_state = gpio_get_value(GPIO_21); //* LED on: gpio_state=1, LED off: gpio_state=0
  //write to user
  len = 1;
  //??? send the gpio_state(0 or 1) to user  
  if( copy_to_user(buf, &gpio_state, len) > 0) {
    pr_err("ERROR: Not all the bytes have been copied to user\n");
  }
  pr_info("Read function : GPIO_21 = %d \n", gpio_state);
  return 0;
}
//! When we call echo (0 or 1) > /dev/ext_device
// This function will be called when we write the Device file
static ssize_t etx_write(struct file *filp,
  const char __user *buf, size_t len, loff_t *off){
  char rec_buf[32] = {0};
  char cmd[16] = {0};
  char arg[16] = {0};
  int i, j, value;
  unsigned char seg_pattern;
  int gpios[7] = {GPIO_17, GPIO_27, GPIO_21, GPIO_16, GPIO_26, GPIO_5, GPIO_6}; 
  // order: a,b,c,d,e,f,g
  // 從 user space 拷貝學號字串 //* buf is user space input, we copy buf into rec_buf
  if (copy_from_user(rec_buf, buf, len) > 0) {
    pr_err("ERROR: Not all bytes copied from user\n");
    return -EFAULT;
  }
  rec_buf[len] = '\0';  // 確保結尾有 '\0'
  pr_info("Received student ID: %s\n", rec_buf);


  // 解析指令，例如 "7seg A" 或 "led0 on"
  sscanf(rec_buf, "%15s %15s", cmd, arg);
  pr_info("Command: [%s], Arg: [%s]\n", cmd, arg);
  // ---- 控制 7-segment ----
  if (strcmp(cmd, "7seg") == 0) {
    if (strlen(arg) == 0) {
      pr_err("No 7seg argument provided\n");
      return len;
    }
    // 一個字一個字顯示
    for (i = 0; arg[i] != '\0'; i++) {
      // 將學號字元轉成七段顯示的值
      if (arg[i] >= '0' && arg[i] <= '9')
        value = arg[i] - '0';
      else if (arg[i] >= 'A' && arg[i] <= 'F')
        value = arg[i] - 'A' + 10;
      else if (arg[i] >= 'a' && arg[i] <= 'f')
        value = arg[i] - 'a' + 10;
      else { pr_err("Invalid character: %c (ignored)\n", arg[i]);
        continue;
      }
      seg_pattern = Seg_7_map[value]; // ex seg_7_map[0] = 0b1111110
      // 設定七段顯示器的 GPIO
      for (j = 0; j < 7; j++) {
        int bit = (seg_pattern >> (6 - j)) & 0x01;
        gpio_set_value(gpios[j], bit);
      }
      pr_info("Displaying [%c] on 7-segment\n", arg[i]);
      msleep(1000); // 延遲 1 秒再顯示下一個
    }
  }else if(strcmp(cmd, "led")==0){
    if (strlen(arg) == 0) {
      pr_err("No 7seg argument provided\n");
      return len;
    }
    if (arg[0]== '1')  {
      gpio_set_value(GPIO_22, 1); // 假設 LED 接在 GPIO22
      gpio_set_value(GPIO_10, 0);
      gpio_set_value(GPIO_9,  0);
      //
      gpio_set_value(GPIO_11 ,0);
      gpio_set_value(GPIO_0  ,0);
      gpio_set_value(GPIO_14 ,0);
      gpio_set_value(GPIO_15 ,0);
      gpio_set_value(GPIO_18 ,0);
      pr_info("LED0 turned ON\n");
    }else if (arg[0]=='2'){
      gpio_set_value(GPIO_22, 1); 
      gpio_set_value(GPIO_10, 1);
      gpio_set_value(GPIO_9,  0);
      gpio_set_value(GPIO_11 ,0);
      gpio_set_value(GPIO_0  ,0);
      gpio_set_value(GPIO_14 ,0);
      gpio_set_value(GPIO_15 ,0);
      gpio_set_value(GPIO_18 ,0);
    }else if (arg[0]=='3'){
      gpio_set_value(GPIO_22, 1); 
      gpio_set_value(GPIO_10, 1);
      gpio_set_value(GPIO_9,  1);
      gpio_set_value(GPIO_11 ,0);
      gpio_set_value(GPIO_0 ,0);
      gpio_set_value(GPIO_14 ,0);
      gpio_set_value(GPIO_15 ,0);
      gpio_set_value(GPIO_18 ,0);
    }else if (arg[0]=='4'){
      gpio_set_value(GPIO_22, 1); 
      gpio_set_value(GPIO_10, 1);
      gpio_set_value(GPIO_9,  1);
      gpio_set_value(GPIO_11 ,1);
      gpio_set_value(GPIO_0  ,0);
      gpio_set_value(GPIO_14 ,0);
      gpio_set_value(GPIO_15 ,0);
      gpio_set_value(GPIO_18 ,0);
    }else if (arg[0]=='5'){
      gpio_set_value(GPIO_22, 1); 
      gpio_set_value(GPIO_10, 1);
      gpio_set_value(GPIO_9,  1);
      gpio_set_value(GPIO_11 ,1);
      gpio_set_value(GPIO_0  ,1);
      gpio_set_value(GPIO_14 ,0);
      gpio_set_value(GPIO_15 ,0);
      gpio_set_value(GPIO_18 ,0);
    }else if (arg[0]=='6'){
      gpio_set_value(GPIO_22, 1); 
      gpio_set_value(GPIO_10, 1);
      gpio_set_value(GPIO_9,  1);
      gpio_set_value(GPIO_11 ,1);
      gpio_set_value(GPIO_0  ,1);
      gpio_set_value(GPIO_14 ,1);
      gpio_set_value(GPIO_15 ,0);
      gpio_set_value(GPIO_18 ,0);
    }else if (arg[0]=='7'){
      gpio_set_value(GPIO_22, 1); 
      gpio_set_value(GPIO_10, 1);
      gpio_set_value(GPIO_9 , 1);
      gpio_set_value(GPIO_11, 1);
      gpio_set_value(GPIO_0 , 1);
      gpio_set_value(GPIO_14 ,1);
      gpio_set_value(GPIO_15 ,1);
      gpio_set_value(GPIO_18 ,0);
    }else if (arg[0]=='8'){
      gpio_set_value(GPIO_22, 1); 
      gpio_set_value(GPIO_10, 1);
      gpio_set_value(GPIO_9,  1);
      gpio_set_value(GPIO_11 ,1);
      gpio_set_value(GPIO_0  ,1);
      gpio_set_value(GPIO_14 ,1);
      gpio_set_value(GPIO_15 ,1);
      gpio_set_value(GPIO_18 ,1);
    }else{
      gpio_set_value(GPIO_22, 0);
      gpio_set_value(GPIO_10, 0);
      gpio_set_value(GPIO_9, 0);
      gpio_set_value(GPIO_11 ,0);
      gpio_set_value(GPIO_0 ,0);
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
  if(!gpio_is_valid(GPIO_21) || 
     !gpio_is_valid(GPIO_16) ||
     !gpio_is_valid(GPIO_26) ||
     !gpio_is_valid(GPIO_22) ||
     !gpio_is_valid(GPIO_27) ||
     !gpio_is_valid(GPIO_17) ||
     !gpio_is_valid(GPIO_5)  ||
     !gpio_is_valid(GPIO_6)  ||//*------
     !gpio_is_valid(GPIO_10) ||
     !gpio_is_valid(GPIO_9)  ||
     //
     !gpio_is_valid(GPIO_11)||
     !gpio_is_valid(GPIO_0)||
     !gpio_is_valid(GPIO_14)||
     !gpio_is_valid(GPIO_15)||
     !gpio_is_valid(GPIO_18)
    ){//Checking the GPIO is valid or not
    pr_err("GPIO is not valid\n");
    goto r_device;
  }
  //Requesting the GPIO
  if(gpio_request(GPIO_21, "GPIO_21") < 0 ||
     gpio_request(GPIO_16, "GPIO_16") < 0 ||
     gpio_request(GPIO_26, "GPIO_26") < 0 ||
     gpio_request(GPIO_22, "GPIO_22") < 0 ||
     gpio_request(GPIO_27, "GPIO_27") < 0 ||
     gpio_request(GPIO_17, "GPIO_17") < 0 ||
     gpio_request(GPIO_5, "GPIO_5") < 0 ||
     gpio_request(GPIO_6, "GPIO_6") < 0  ||
     gpio_request(GPIO_10, "GPIO_10") < 0||
     gpio_request(GPIO_9, "GPIO_9") < 0 ||
     //
     gpio_request(GPIO_11,"GPIO_11") < 0 ||
     gpio_request(GPIO_0,"GPIO_0") < 0 ||
     gpio_request(GPIO_14,"GPIO_14") < 0 ||
     gpio_request(GPIO_15,"GPIO_15") < 0 ||
     gpio_request(GPIO_18,"GPIO_18") < 0 
    ){
    pr_err("ERROR: GPIO request failed\n");
    goto r_gpio;
  }
  //configure the GPIO as output
  gpio_direction_output(GPIO_21, 0);
  gpio_direction_output(GPIO_16, 0);

  gpio_direction_output(GPIO_26,0); 
  gpio_direction_output(GPIO_22,0); 
  gpio_direction_output(GPIO_27,0); 
  gpio_direction_output(GPIO_17,0); 
  gpio_direction_output(GPIO_5,0);  
  gpio_direction_output(GPIO_6,0);
  gpio_direction_output(GPIO_10,0);
  gpio_direction_output(GPIO_9,0);
  //
  gpio_direction_output(GPIO_11,0);
  gpio_direction_output(GPIO_0,0);
  gpio_direction_output(GPIO_14,0);
  gpio_direction_output(GPIO_15,0);
  gpio_direction_output(GPIO_18,0);
  /* Using this call the GPIO 21 will be visible in /sys/class/gpio/
  ** Now you can change gpio values by using below commands also.
  ** echo 1 > /sys/class/gpio/gpio21/value (turn ON the LED)
  ** echo 0 > /sys/class/gpio/gpio21/value (turn OFF the LED)
  ** cat /sys/class/gpio/gpio21/value (read the value LED)
  ** the 2nd argument prevents  direction from being changed.*/
  gpio_export(GPIO_21, false);
  gpio_export(GPIO_16, false);
  gpio_export(GPIO_26,false); 
  gpio_export(GPIO_22,false); 
  gpio_export(GPIO_27,false); 
  gpio_export(GPIO_17,false); 
  gpio_export(GPIO_5,false);  
  gpio_export(GPIO_6,false);
  gpio_export(GPIO_9,false);
  gpio_export(GPIO_10,false);
  //
  gpio_export(GPIO_11,false);
  gpio_export(GPIO_0,false);
  gpio_export(GPIO_14,false);
  gpio_export(GPIO_15,false);
  gpio_export(GPIO_18,false);
  pr_info("Device Driver Insert...Done!!!\n");
  return 0;
  r_gpio:
    gpio_free(GPIO_21);
    gpio_free(GPIO_16);
    gpio_free(GPIO_26); 
    gpio_free(GPIO_22); 
    gpio_free(GPIO_27); 
    gpio_free(GPIO_17); 
    gpio_free(GPIO_5);  
    gpio_free(GPIO_6);
    gpio_free(GPIO_9);
    gpio_free(GPIO_10);
    //
    gpio_free(GPIO_11);
    gpio_free(GPIO_0);
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
  gpio_unexport(GPIO_21);
  gpio_unexport(GPIO_16);
  gpio_unexport(GPIO_26); 
  gpio_unexport(GPIO_22); 
  gpio_unexport(GPIO_27); 
  gpio_unexport(GPIO_17); 
  gpio_unexport(GPIO_5);  
  gpio_unexport(GPIO_6);
  gpio_unexport(GPIO_9);
  gpio_unexport(GPIO_10);
//
  gpio_unexport(GPIO_11);
  gpio_unexport(GPIO_0);
  gpio_unexport(GPIO_14);
  gpio_unexport(GPIO_15);
  gpio_unexport(GPIO_18);
  gpio_free(GPIO_21);
  gpio_free(GPIO_16);
  gpio_free(GPIO_26); 
  gpio_free(GPIO_22); 
  gpio_free(GPIO_27); 
  gpio_free(GPIO_17); 
  gpio_free(GPIO_5);  
  gpio_free(GPIO_6);
  gpio_free(GPIO_9);
  gpio_free(GPIO_10);
//
  gpio_free(GPIO_11);
  gpio_free(GPIO_0);
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
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("A simple device driver - GPIO Driver");