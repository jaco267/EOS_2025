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
#include <linux/interrupt.h>   // request_irq, free_irq, IRQF_*
#include <linux/irq.h>         // IRQ trigger flags（保險）
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
                                    35 36 ---GPIO 16 (led or buttom)               
                     GPIO 26(led)---37 38               
                             gnd    39 40 ---GPIO 21 (led)
*/
//* led 
#define GPIO_14 (14) //led 
#define GPIO_15 (15) //led 
#define GPIO_18 (18) //led
// 7seg  
#define GPIO_17 (17) //a 
#define GPIO_27 (27) //b 
#define GPIO_22 (22) //C
// #define GPIO_10 (10) //D

// #define GPIO_9  (9) //E 
// #define GPIO_11 (11) 
#define GPIO_0   (0) //D
//----
#define GPIO_5   (5) //E
#define GPIO_6   (6) //F
#define GPIO_13 (13) //G 

//* buttom 
#define GPIO_16 (16) //buttom

#define NUM_GPIOS 10 
static const int All_gpios[NUM_GPIOS] = {GPIO_14, GPIO_15, GPIO_18,
    GPIO_17, GPIO_27, GPIO_22,GPIO_0,GPIO_5,GPIO_6,GPIO_13};
//*---------LED----------------------------
#define NUM_LEDS 3   
static const int led_gpios[NUM_LEDS] = {GPIO_14, GPIO_15, GPIO_18};
static char ledmap[4] = {0b000, 0b001, 0b010, 0b100};
//*---------7seg-----------------------------
#define NUM_7seg 7 
static const int seg7_gpios[NUM_7seg] = {GPIO_17, GPIO_27, GPIO_22,GPIO_0,GPIO_5,GPIO_6,GPIO_13};
static char Seg_7_map[17] ={ 
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
    0b1000111, //F,f 
    0b0000000  //ggggggg GGGG turn offf
}; 

//*----button interrupt----
static int btn_irq; 
static atomic_t btn_pressed = ATOMIC_INIT(0); 
static wait_queue_head_t btn_wq;
//* debounce 
#define DEBOUNCE_MS 500

static unsigned long last_irq_time = 0;
static irqreturn_t button_isr(int irq, void *dev_id){
    unsigned long now = jiffies;
    if (time_before(now, last_irq_time + msecs_to_jiffies(DEBOUNCE_MS))) {
        return IRQ_HANDLED;  // 彈跳 → 忽略
    }
  //應為是接上 vdd : 沒按 gpio_16= 0 , 按下  gpio_16=1
  if (gpio_get_value(GPIO_16) == 1) {
    last_irq_time = now;
    atomic_set(&btn_pressed, 1);
    wake_up_interruptible(&btn_wq);
  }
  return IRQ_HANDLED;
}



//*---------------------------
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
  pr_info("Device File Opened...\n"); return 0;
}
// This function will be called when we close the Device file
static int etx_release(struct inode *inode, struct file *file){
  pr_info("Device File Closed...\n"); return 0;
}
//! when we $ cat /dev/etx_device
// etx_read is called when we read the Device file
static ssize_t etx_read(struct file *filp, char __user *buf, 
    size_t len, loff_t *off){
   char kbuf[32];
   int ret;

   if (*off > 0)
       return 0;
   
   // 等待按鈕事件 「只要 btn_pressed == 0 就睡覺；變成 != 0 就醒來」
   wait_event_interruptible(btn_wq,
                            atomic_read(&btn_pressed));

   atomic_set(&btn_pressed, 0);

   snprintf(kbuf, sizeof(kbuf), "BTN:1\n");

   ret = copy_to_user(buf, kbuf, strlen(kbuf));
   if (ret)
       return -EFAULT;

   *off += strlen(kbuf);
   return strlen(kbuf);
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
  //seg7_gpios[NUM_7seg] Seg_7_map 
  unsigned char seg_pattern;
  int value; 
  if (strcmp(cmd, "7seg") == 0) {
    if (strlen(arg) == 0) {pr_err("No 7seg argument provided\n"); return len;}
    // 一個字一個字顯示
    for (int i = 0; arg[i] != '\0'; i++) {
      // 將學號字元轉成七段顯示的值
      if (arg[i] >= '0' && arg[i] <= '9')
        value = arg[i] - '0';
      else if (arg[i] >= 'A' && arg[i] <= 'G')
        value = arg[i] - 'A' + 10;
      else if (arg[i] >= 'a' && arg[i] <= 'g')
        value = arg[i] - 'a' + 10;
      else { pr_err("Invalid char: %c (ignored)\n", arg[i]);
        continue;
      }
      seg_pattern = Seg_7_map[value]; // ex seg_7_map[0] = 0b1111110
      // 設定七段顯示器的 GPIO
      for (int j = 0; j < 7; j++) {
        int bit = (seg_pattern >> (6 - j)) & 0x01;
        gpio_set_value(seg7_gpios[j], bit);
      }
      pr_info("Displaying [%c] on 7-segment\n", arg[i]);
      if (i < (strlen(arg)-1))  msleep(1000); // 延遲 1 秒再顯示下一個
    }
  }else if(strcmp(cmd, "led")==0){ //* 控制 led  
    if (strlen(arg) == 0) {
        pr_err("No 7seg argument provided\n");
        return len;
    }
    int led_map_idx = 0; 
    if (arg[0]== '1')  {     led_map_idx = 1; 
    }else if (arg[0]=='2'){  led_map_idx = 2;
    }else if (arg[0]=='3'){  led_map_idx = 3;
    }else{ pr_info("LED0 turned OFF\n");}
    for(int j=0; j<NUM_LEDS ; j++){
        int bit = (ledmap[led_map_idx]>>j) & 0x01;  
        gpio_set_value(led_gpios[j], bit);
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
  //*------check gpio is valid------
  for (int i = 0; i < NUM_GPIOS; i++){
    if(!gpio_is_valid(All_gpios[i])){pr_err("GPIO is not valid\n");
        goto r_device;
    }
  }
  if (!gpio_is_valid(GPIO_16)){pr_err("Button GPIO is not valid\n");
    goto r_device; 
  }
  //*--------Requesting the GPIO-------
  char gpio_label[10]; // 用來存放 "GPIO_XX" 的標籤
  for (int i = 0; i < NUM_GPIOS; i++){
    snprintf(gpio_label, sizeof(gpio_label), "GPIO_%d", All_gpios[i]);
    if (gpio_request(All_gpios[i], gpio_label) < 0){pr_err("ERROR: GPIO request failed\n");
        goto r_gpio;
    }
  }
  if (gpio_request(GPIO_16, "GPIO_16")<0){pr_err("Failed to request GPIO_BTN\n");
    goto r_gpio;
  }
  //*--------set GPIO inout----------
  for (int i = 0; i < NUM_GPIOS; i++){
    gpio_direction_output(All_gpios[i],0);  
    gpio_export(All_gpios[i],false);
  }
  gpio_direction_input(GPIO_16);  
  gpio_export(GPIO_16, false); 
  //* ----btn irq---------
  init_waitqueue_head(&btn_wq);
  btn_irq = gpio_to_irq(GPIO_16);
  if (btn_irq < 0) {pr_err("Failed to get IRQ for GPIO_16\n");
    goto r_gpio;
  }
  if (request_irq(btn_irq, button_isr,
      // IRQF_TRIGGER_FALLING, 
      IRQF_TRIGGER_RISING,
      "gpio_button_irq",  NULL)) {
    pr_err("Failed to request IRQ\n");
    goto r_gpio;
  }
  //*---------done-------------------------
  pr_info("Device Driver Insert...Done!!!\n");
  return 0;
  //? do we need gpio unexport here?
  r_gpio:
    for (int i = 0; i < NUM_GPIOS; i++){
        gpio_free(All_gpios[i]);
    }
    gpio_free(GPIO_16);
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
  for (int i = 0; i < NUM_GPIOS; i++){
    gpio_unexport(All_gpios[i]);
    gpio_free(All_gpios[i]);
  }    
  gpio_unexport(GPIO_16);
  free_irq(btn_irq, NULL); 
  gpio_free(GPIO_16);
  //-------------------------------------------
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