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
	"\x14\x00\x00\x00\xbb\x6d\xfb\xbd"
	"__fentry__\0\0"
	"\x10\x00\x00\x00\xe6\x6e\xab\xbc"
	"sscanf\0\0"
	"\x24\x00\x00\x00\x8a\x6c\x91\xe6"
	"pci_get_domain_bus_and_slot\0"
	"\x1c\x00\x00\x00\x91\xc9\xc5\x52"
	"__kmalloc_noprof\0\0\0\0"
	"\x18\x00\x00\x00\x59\x89\x60\x49"
	"migrate_disable\0"
	"\x20\x00\x00\x00\x54\x95\xac\x79"
	"pci_read_config_dword\0\0\0"
	"\x14\x00\x00\x00\x65\x93\x3f\xb4"
	"ktime_get\0\0\0"
	"\x18\x00\x00\x00\xe4\x72\x72\x4d"
	"migrate_enable\0\0"
	"\x10\x00\x00\x00\xba\x0c\x7a\x03"
	"kfree\0\0\0"
	"\x14\x00\x00\x00\x4f\xcf\x76\x75"
	"pci_dev_put\0"
	"\x18\x00\x00\x00\x56\xd7\xaa\x31"
	"const_pcpu_hot\0\0"
	"\x18\x00\x00\x00\x32\x2c\xc1\x01"
	"cpu_bit_bitmap\0\0"
	"\x20\x00\x00\x00\x8e\x88\x53\xe3"
	"set_cpus_allowed_ptr\0\0\0\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x28\x00\x00\x00\xb3\x1c\xa2\x87"
	"__ubsan_handle_out_of_bounds\0\0\0\0"
	"\x18\x00\x00\x00\x72\xb2\x10\xe7"
	"param_ops_uint\0\0"
	"\x18\x00\x00\x00\x9c\x94\x5e\x64"
	"param_ops_int\0\0\0"
	"\x18\x00\x00\x00\x61\x36\x3a\x59"
	"param_ops_charp\0"
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x1c\x00\x00\x00\xca\x39\x82\x5b"
	"__x86_return_thunk\0\0"
	"\x18\x00\x00\x00\x34\x61\x23\x68"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "D0CBABAF3487709C190D469");
