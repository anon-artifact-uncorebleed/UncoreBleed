// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/percpu.h>
#include <asm/cacheflush.h>      /* wbinvd() */
#include <asm/paravirt.h>        /* read_cr0() / write_cr0() */

DEFINE_PER_CPU(unsigned long, saved_cr0);

static void toggle_cache_this_cpu(void *disable)
{
        unsigned long cr0 = read_cr0();
        bool to_disable = (bool)(uintptr_t)disable;

        if (to_disable) {
                this_cpu_write(saved_cr0, cr0);

                /* Step 1: 写回并失效所有缓存行 */
                wbinvd();

                /* Step 2: CD=1, NW=0 */
                cr0 |= X86_CR0_CD;
                cr0 &= ~X86_CR0_NW;
                write_cr0(cr0);

                pr_info("cpu %u: cache disabled\n", smp_processor_id());
        } else {
                /* Step 1: 先 WBINVD，写回脏行 */
                wbinvd();

                /* Step 2: 恢复原 CR0（清 CD/NW 恢复） */
                cr0 = this_cpu_read(saved_cr0);
                write_cr0(cr0);

                /* Step 3: 再 WBINVD，防止旧行以 UC 语义残留 */
                wbinvd();

                pr_info("cpu %u: cache re-enabled\n", smp_processor_id());
        }
}

/* ---------------- 模块入口 / 退出 ---------------- */

static int __init cd_global_init(void)
{
        pr_info("disabling cache on all online CPUs …\n");
        on_each_cpu(toggle_cache_this_cpu, (void *)true, 1 /* wait */);
        return 0;
}

static void __exit cd_global_exit(void)
{
        pr_info("restoring cache on all CPUs …\n");
        on_each_cpu(toggle_cache_this_cpu, (void *)false, 1);
}

module_init(cd_global_init);
module_exit(cd_global_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("CDC");
MODULE_DESCRIPTION("Globally disable/enable cache by setting CR0.CD on all CPUs");
