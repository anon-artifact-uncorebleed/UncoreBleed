// wbinvd_test.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/preempt.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/processor.h>
#include <asm/unistd.h>
#include <asm/msr.h>

static int pin_cpu = -1;        // 绑核；-1 表示不绑
module_param(pin_cpu, int, 0644);
MODULE_PARM_DESC(pin_cpu, "CPU to pin (-1 = no pin)");

static int dirty_mb = 64;       // 脏化的数据量（MB）
module_param(dirty_mb, int, 0644);
MODULE_PARM_DESC(dirty_mb, "Megabytes to dirty before WBINVD");

static int iters = 5;           // 重复次数
module_param(iters, int, 0644);
MODULE_PARM_DESC(iters, "Iterations");

static bool use_cd = false;     // 是否在测量前设置 CR0.CD 关缓存
module_param(use_cd, bool, 0644);
MODULE_PARM_DESC(use_cd, "Set CR0.CD before WBINVD");

static void dirty_buffer(volatile char *buf, size_t sz)
{
    // 每 64B 改一次，制造脏行
    size_t i;
    for (i = 0; i < sz; i += 64)
        buf[i] = (char)i;
}

static inline u64 rdtsc_begin(void)
{
    unsigned a, d;
    asm volatile("cpuid\n\t"
                 "rdtsc\n\t"
                 : "=a"(a), "=d"(d)
                 : "a"(0)
                 : "rbx", "rcx", "memory");
    return ((u64)d << 32) | a;
}

static inline u64 rdtsc_end(void)
{
    unsigned a, d, c;
    asm volatile("rdtscp\n\t"
                 : "=a"(a), "=d"(d), "=c"(c)
                 :
                 : "memory");
    asm volatile("cpuid\n\t" : : "a"(0) : "rbx", "rcx", "rdx", "memory");
    return ((u64)d << 32) | a;
}

static u64 measure_once(bool do_wbinvd)
{
    unsigned long flags;
    u64 t0, t1;

    preempt_disable();
    local_irq_save(flags);
    t0 = rdtsc_begin();
    if (do_wbinvd)
        asm volatile("wbinvd" ::: "memory");
    t1 = rdtsc_end();
    local_irq_restore(flags);
    preempt_enable();

    return t1 - t0;
}

static int __init wbinvd_test_init(void)
{
    int i;
    u64 best = ~0ULL;
    u64 sum = 0;
    void *buf = NULL;
    size_t sz = (size_t)dirty_mb * 1024ull * 1024ull;
    unsigned long old_cr0 = 0;

    if (pin_cpu >= 0 && pin_cpu < nr_cpu_ids) {
        int ret = set_cpus_allowed_ptr(current, cpumask_of(pin_cpu));
        if (ret)
            pr_warn("pin to CPU %d failed: %d\n", pin_cpu, ret);
        else
            pr_info("Pinned to CPU %d\n", raw_smp_processor_id());
    }

    if (dirty_mb > 0) {
        buf = vmalloc(sz);
        if (!buf) {
            pr_err("vmalloc %zu bytes failed\n", sz);
            return -ENOMEM;
        }
    }

    if (use_cd) {
        old_cr0 = read_cr0();
        write_cr0(old_cr0 | X86_CR0_CD); // 关缓存
        asm volatile("wbinvd" ::: "memory"); // 关之后先清一次
        pr_info("CR0.CD set, caches disabled for the test window\n");
    }

    // 预热一次，避免第一次 CPUID/TSCP 不稳定
    (void)measure_once(false);

    for (i = 0; i < iters; i++) {
        // 每次测量前重新脏化
        if (buf)
            dirty_buffer((volatile char *)buf, sz);

        u64 overhead = measure_once(false);   // 基线（无 WBINVD）
        u64 with_wb  = measure_once(true);    // 含 WBINVD
        u64 delta = (with_wb > overhead) ? (with_wb - overhead) : 0;

        best = (delta < best) ? delta : best;
        sum += delta;

        pr_info("[iter %d] raw=%llu cyc, overhead=%llu cyc, wbinvd=%llu cyc, delta=%llu cyc\n",
                i, with_wb, overhead, with_wb, delta);
    }

    if (use_cd) {
        write_cr0(old_cr0);               // 恢复 CR0
        asm volatile("wbinvd" ::: "memory"); // 打开缓存后再清一次更干净
    }

    if (buf)
        vfree(buf);

    pr_info("WBINVD delta cycles: avg=%llu, best=%llu (iters=%d, dirty_mb=%d)\n",
            sum / iters, best, iters, dirty_mb);

    pr_info("Note: result is cycles. To get ns ≈ cycles / (CPU_GHz)\n");
    return 0;
}

static void __exit wbinvd_test_exit(void)
{
    pr_info("wbinvd_test unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("you");
MODULE_DESCRIPTION("Measure WBINVD latency in cycles");
module_init(wbinvd_test_init);
module_exit(wbinvd_test_exit);

