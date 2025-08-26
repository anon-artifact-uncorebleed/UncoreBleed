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
	"\x18\x00\x00\x00\x7b\xf7\x19\x1d"
	"physical_mask\0\0\0"
	"\x14\x00\x00\x00\x32\xb4\x35\x8a"
	"sme_me_mask\0"
	"\x18\x00\x00\x00\x44\x35\xd1\xda"
	"ptrs_per_p4d\0\0\0\0"
	"\x10\x00\x00\x00\xfb\x94\x30\xfb"
	"pv_ops\0\0"
	"\x14\x00\x00\x00\xe6\x10\xec\xd4"
	"BUG_func\0\0\0\0"
	"\x1c\x00\x00\x00\x5e\xd7\xd8\x7c"
	"page_offset_base\0\0\0\0"
	"\x18\x00\x00\x00\x6c\x1e\x65\x97"
	"vmemmap_base\0\0\0\0"
	"\x18\x00\x00\x00\x64\xbd\x8f\xba"
	"_raw_spin_lock\0\0"
	"\x2c\x00\x00\x00\x61\xe5\x48\xa6"
	"__ubsan_handle_shift_out_of_bounds\0\0"
	"\x30\x00\x00\x00\x94\x97\xc8\x2a"
	"__tracepoint_mmap_lock_start_locking\0\0\0\0"
	"\x14\x00\x00\x00\xd2\x19\xbc\x57"
	"down_write\0\0"
	"\x30\x00\x00\x00\x90\x3e\xf2\x57"
	"__tracepoint_mmap_lock_acquire_returned\0"
	"\x1c\x00\x00\x00\x34\x4b\xb5\xb5"
	"_raw_spin_unlock\0\0\0\0"
	"\x1c\x00\x00\x00\x0f\x81\x69\x24"
	"__rcu_read_unlock\0\0\0"
	"\x1c\x00\x00\x00\x71\x22\x5a\x5a"
	"__cpu_online_mask\0\0\0"
	"\x20\x00\x00\x00\xba\x35\xf8\x63"
	"on_each_cpu_cond_mask\0\0\0"
	"\x28\x00\x00\x00\xf6\x18\x86\x61"
	"__tracepoint_mmap_lock_released\0"
	"\x14\x00\x00\x00\x25\x7a\x80\xce"
	"up_write\0\0\0\0"
	"\x30\x00\x00\x00\x70\xb1\x10\x53"
	"__mmap_lock_do_trace_acquire_returned\0\0\0"
	"\x2c\x00\x00\x00\x24\x44\x59\xcc"
	"__mmap_lock_do_trace_start_locking\0\0"
	"\x28\x00\x00\x00\xa8\x97\xb8\x99"
	"__mmap_lock_do_trace_released\0\0\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x18\x00\x00\x00\x0f\xed\xe4\x71"
	"find_get_pid\0\0\0\0"
	"\x18\x00\x00\x00\x2b\x2c\x4a\x2d"
	"get_pid_task\0\0\0\0"
	"\x14\x00\x00\x00\x8b\x33\xb8\x97"
	"get_task_mm\0"
	"\x14\x00\x00\x00\xa1\x19\x8b\x66"
	"down_read\0\0\0"
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x10\x00\x00\x00\xa2\x54\xb9\x53"
	"up_read\0"
	"\x1c\x00\x00\x00\x9a\x05\xa5\xe3"
	"__put_task_struct\0\0\0"
	"\x10\x00\x00\x00\x6a\x0b\xd9\xde"
	"mmput\0\0\0"
	"\x20\x00\x00\x00\x5f\x69\x96\x02"
	"refcount_warn_saturate\0\0"
	"\x2c\x00\x00\x00\xc6\xfa\xb1\x54"
	"__ubsan_handle_load_invalid_value\0\0\0"
	"\x18\x00\x00\x00\x96\xd8\x55\x89"
	"param_ops_ulong\0"
	"\x18\x00\x00\x00\x9c\x94\x5e\x64"
	"param_ops_int\0\0\0"
	"\x14\x00\x00\x00\xbb\x6d\xfb\xbd"
	"__fentry__\0\0"
	"\x18\x00\x00\x00\xfc\xaa\xa0\x40"
	"__flush_tlb_all\0"
	"\x1c\x00\x00\x00\xca\x39\x82\x5b"
	"__x86_return_thunk\0\0"
	"\x14\x00\x00\x00\x83\x9d\xd7\x72"
	"pgdir_shift\0"
	"\x18\x00\x00\x00\x54\xac\x26\x34"
	"boot_cpu_data\0\0\0"
	"\x18\x00\x00\x00\x34\x61\x23\x68"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "1FB556310F9910D8F136F61");
