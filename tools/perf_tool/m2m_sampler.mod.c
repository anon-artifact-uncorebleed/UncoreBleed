#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

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

#ifdef CONFIG_MITIGATION_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const char ____versions[]
__used __section("__versions") =
	"\x1c\x00\x00\x00\xd5\x8a\x25\x4e"
	"simple_attr_open\0\0\0\0"
	"\x14\x00\x00\x00\xa3\x75\xe5\x88"
	"single_open\0"
	"\x14\x00\x00\x00\xcb\xc0\x25\x23"
	"seq_printf\0\0"
	"\x18\x00\x00\x00\x40\xc3\xcd\x16"
	"acpi_get_table\0\0"
	"\x18\x00\x00\x00\x33\x9a\xb9\x92"
	"acpi_put_table\0\0"
	"\x10\x00\x00\x00\x09\xcd\x80\xde"
	"ioremap\0"
	"\x1c\x00\x00\x00\x9b\xc0\x62\xea"
	"debugfs_create_dir\0\0"
	"\x1c\x00\x00\x00\x41\x16\xbf\x37"
	"debugfs_create_file\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x1c\x00\x00\x00\xd3\x05\x6a\x5a"
	"debugfs_attr_read\0\0\0"
	"\x1c\x00\x00\x00\x2a\x51\x65\xc1"
	"debugfs_attr_write\0\0"
	"\x1c\x00\x00\x00\x85\x31\xb8\x0e"
	"simple_attr_release\0"
	"\x14\x00\x00\x00\xa2\xe6\x82\x14"
	"seq_lseek\0\0\0"
	"\x14\x00\x00\x00\xb8\x4d\xa6\xfc"
	"seq_read\0\0\0\0"
	"\x18\x00\x00\x00\x30\xf2\x77\x80"
	"single_release\0\0"
	"\x18\x00\x00\x00\xc9\xab\xa1\x27"
	"param_ops_byte\0\0"
	"\x1c\x00\x00\x00\x1c\x5e\xaf\xcc"
	"param_ops_ushort\0\0\0\0"
	"\x14\x00\x00\x00\xbb\x6d\xfb\xbd"
	"__fentry__\0\0"
	"\x1c\x00\x00\x00\xca\x39\x82\x5b"
	"__x86_return_thunk\0\0"
	"\x20\x00\x00\x00\x0b\x05\xdb\x34"
	"_raw_spin_lock_irqsave\0\0"
	"\x24\x00\x00\x00\x70\xce\x5c\xd3"
	"_raw_spin_unlock_irqrestore\0"
	"\x18\x00\x00\x00\xe2\x0f\x12\x6e"
	"debugfs_remove\0\0"
	"\x10\x00\x00\x00\x53\x39\xc0\xed"
	"iounmap\0"
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x18\x00\x00\x00\x34\x61\x23\x68"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "62C820843B5176537A8B7A1");
