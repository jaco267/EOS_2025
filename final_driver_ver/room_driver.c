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
    // 1. 在kernel space 定義要回傳的字串
    const char *message = "hello world\n"; // 包含換行符
    size_t message_len = strlen(message);
    ssize_t ret;
    // 2. 處理檔案偏移量 (確保只讀取一次)
    if (*off > 0) {
        // 如果 offset 大於 0，表示已經讀取過了，回傳 0 代表檔案結尾 (EOF)
        return 0;
    }
    // 3. 確保使用者提供的緩衝區夠大
    if (message_len > len) {
        // if 要傳送的msg_len 大於使用者要求的長度，只傳送使用者要求的長度
        // 在這裡，為簡單起見，我們假設緩衝區夠大，或者只傳送部分
        // ret = len; 
        // 為了確保完整字串傳輸，通常 Client 應提供足夠空間，但此處我們取兩者最小值。
        ret = (message_len < len) ? message_len : len;
    } else {
        ret = message_len;
    }
    // 4. 將字串從核心空間複製到使用者空間
    // copy_to_user(目標使用者緩衝區, 來源核心變數, 位元組數)
    // 成功時回傳 0，失敗時回傳未複製的位元組數 (> 0)
    if( copy_to_user(buf, message, ret) > 0) {
        pr_err("ERROR: Not all the bytes have been copied to user\n");
        return -EFAULT; // 回傳標準 I/O 錯誤碼
    }
    *off += ret; // 5. 更新檔案偏移量 (重要)
    pr_info("Read function: Sent '%s', length %zd.\n", message, ret);
    // 6. 回傳實際複製的位元組數
    return ret; // <--- 必須回傳實際複製的位元組數 (在這裡是 message_len)
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
  pr_info("Received room ID: %s\n", rec_buf);

  // 解析指令，例如 "7seg A" 或 "led0 on"
  sscanf(rec_buf, "%15s %15s", cmd, arg);
  pr_info("Command: [%s], Arg: [%s]\n", cmd, arg);
  //todo -- 控制 7-segment ---
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
  pr_info("Device Driver Insert...Done!!!\n");
  return 0;
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