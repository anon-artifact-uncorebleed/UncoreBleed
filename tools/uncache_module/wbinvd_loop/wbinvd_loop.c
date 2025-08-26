// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/smp.h>

static struct task_struct *flusher_thread;
static unsigned int interval_us = 1000;   // 默认刷新间隔：1 ms，可通过模块参数调整
module_param(interval_us, uint, 0444);
MODULE_PARM_DESC(interval_us, "Delay between consecutive WBINVD in microseconds");

static int flusher_fn(void *unused)
{
        allow_signal(SIGKILL);
        pr_info("wbinvd_flusher: started, interval=%u µs\n", interval_us);
        while (!kthread_should_stop()) {
                /* 调用 wbinvd 指令：写回并失效所有缓存层级 */
                asm volatile("wbinvd" ::: "memory");

                /* 微秒级延时（忙等待+调度点） */
                //if (interval_us)
                //        usleep_range(interval_us, interval_us + 50);

                cond_resched();  // 让出 CPU，避免长时间占用
        }
        pr_info("wbinvd_flusher: stopping\n");
        return 0;
}

static int __init wbinvd_init(void)
{
        pr_info("wbinvd_flusher: loading module\n");
        /* 将线程固定到所有 CPU 上轮询可选，这里只在当前 CPU 上运行 */
        flusher_thread = kthread_run(flusher_fn, NULL, "wbinvd_flusher");
        if (IS_ERR(flusher_thread))
                return PTR_ERR(flusher_thread);
        return 0;
}

static void __exit wbinvd_exit(void)
{
        if (flusher_thread)
                kthread_stop(flusher_thread);
        pr_info("wbinvd_flusher: module unloaded\n");
}

MODULE_AUTHOR("CDC");
MODULE_DESCRIPTION("High-frequency WBINVD flusher");
MODULE_LICENSE("GPL");

module_init(wbinvd_init);
module_exit(wbinvd_exit);

