#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x92997ed8, "_printk" },
	{ 0xe2964344, "__wake_up" },
	{ 0xc473eab3, "gpio_to_desc" },
	{ 0x9a51edda, "gpiod_unexport" },
	{ 0xfe990052, "gpio_free" },
	{ 0xc1514a3b, "free_irq" },
	{ 0x858288a5, "device_destroy" },
	{ 0x629fd72c, "class_destroy" },
	{ 0x2b3268f9, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xdcb764ad, "memset" },
	{ 0x656e4a6e, "snprintf" },
	{ 0xc8c7263c, "gpiod_get_raw_value" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x8da6585d, "__stack_chk_fail" },
	{ 0x7682ba4e, "__copy_overflow" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x8f571c63, "gpiod_set_raw_value" },
	{ 0x98cf60b3, "strlen" },
	{ 0xf9a482f9, "msleep" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x9c07566b, "cdev_init" },
	{ 0x27cabf04, "cdev_add" },
	{ 0xa3ccacbe, "__class_create" },
	{ 0x28d02607, "device_create" },
	{ 0x47229b5c, "gpio_request" },
	{ 0x88f3437d, "gpiod_direction_output_raw" },
	{ 0xb481fa3b, "gpiod_export" },
	{ 0xee656966, "gpiod_direction_input" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xb0a46249, "gpiod_to_irq" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0x1b7fde1c, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "D575CD7102F0E5843F44FEE");
