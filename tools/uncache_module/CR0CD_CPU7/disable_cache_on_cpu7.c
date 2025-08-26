// SPDX-License-Identifier: GPL-2.0
/*
 * Disable caches on a specific logical CPU (default: 7 & its SMT sibling),
 * and re‑enable them on module exit.
 *
 * Tested on Linux 5.4 – 6.9, x86‑64.
 */
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/percpu.h>
#include <asm/cacheflush.h>   /* wbinvd() */
#include <asm/processor.h>    /* read_cr0()/write_cr0() */

static int cpu_primary = 7;   /* insmod cpu_primary=… to override */
module_param(cpu_primary, int, 0444);
MODULE_PARM_DESC(cpu_primary, "Primary logical CPU to disable cache on");

//static int cpu_sibling = 15;  /* set −1 若无 SMT */
//module_param(cpu_sibling, int, 0444);
//MODULE_PARM_DESC(cpu_sibling, "SMT sibling logical CPU (or −1)");

/* 每 CPU 保存原始 CR0，用于恢复 */
static DEFINE_PER_CPU(unsigned long, saved_cr0);

static void __cache_ctl_this_cpu(bool disable)
{
	unsigned long flags, cr0;

	local_irq_save(flags);

	/* 先写回+失效，以免丢数据或读到脏行 */
	wbinvd();

	cr0 = disable ? read_cr0() | X86_CR0_CD  /* 置 CD */
	              : this_cpu_read(saved_cr0) & ~X86_CR0_CD; /* 清 CD */

	/*  NW 永远清 0，防止 write‑through/UC 组合异常 */
	cr0 &= ~X86_CR0_NW;
	write_cr0(cr0);

	/* 再失效一次，确保状态一致 */
	wbinvd();

	pr_info("CPU %d: cache %sabled (CR0=%lx)\n",
	        smp_processor_id(), disable ? "dis" : "en", read_cr0());

	local_irq_restore(flags);
}

static void disable_cache_cpu(void *info)  { this_cpu_write(saved_cr0, read_cr0()); __cache_ctl_this_cpu(true); }
static void enable_cache_cpu(void *info)   { __cache_ctl_this_cpu(false); }

static int __init nocache_init(void)
{
	int ret = smp_call_function_single(cpu_primary, disable_cache_cpu, NULL, true);
	if (ret) return ret;

	//if (cpu_sibling >= 0 && cpu_online(cpu_sibling))
	//	ret = smp_call_function_single(cpu_sibling, disable_cache_cpu, NULL, true);

	return ret;
}

static void __exit nocache_exit(void)
{
	smp_call_function_single(cpu_primary, enable_cache_cpu, NULL, true);
	//if (cpu_sibling >= 0 && cpu_online(cpu_sibling))
	//	smp_call_function_single(cpu_sibling, enable_cache_cpu, NULL, true);
}

module_init(nocache_init);
module_exit(nocache_exit);
MODULE_AUTHOR("CDC");
MODULE_LICENSE("GPL");