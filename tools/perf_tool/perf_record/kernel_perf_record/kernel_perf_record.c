// SPDX-License-Identifier: GPL-2.0
/*
 * pcie_reader_kmod.c
 *   读取 PCI 配置空间寄存器（DWORD），在内核态极短间隔采样 N 次，
 *   计算并输出平均采样间隔。
 *
 * build:
 *   make
 *
 * run:
 *   sudo insmod pcie_reader_kmod.ko bdf=0000:fe:0c.0 offset=0x440 cpu=7 iter=100
 *   dmesg | tail -n 50
 *
 * note:
 *   - 与用户态版本一致：每次先读寄存器，再取时间戳
 *   - 使用 ktime_get_ns()，时间基准为 MONOTONIC
 *   - 可选将当前加载线程绑到指定 CPU，并在采样区间禁迁移
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/cpumask.h>
#include <linux/preempt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/math64.h>

static char *bdf = "0000:00:00.0";
module_param(bdf, charp, 0644);
MODULE_PARM_DESC(bdf, "PCI BDF 格式 0000:bb:dd.f");

static unsigned int offset = 0;
module_param(offset, uint, 0644);
MODULE_PARM_DESC(offset, "PCI 配置空间偏移（十六进制/十进制均可），自动按 DWORD 对齐");

static int cpu = -1;
module_param(cpu, int, 0644);
MODULE_PARM_DESC(cpu, "可选，采样时绑到的 CPU 编号；<0 表示不绑");

static unsigned int iter = 100;
module_param(iter, uint, 0644);
MODULE_PARM_DESC(iter, "采样次数（默认 100）");

static int parse_bdf(const char *s, unsigned int *dom,
                     unsigned int *bus, unsigned int *dev, unsigned int *fun)
{
    /* 期望形如 0000:fe:0c.0 */
    if (sscanf(s, "%x:%x:%x.%x", dom, bus, dev, fun) != 4)
        return -EINVAL;
    return 0;
}

static int __init pcie_reader_init(void)
{
    unsigned int dom, bus, dev, fun;
    struct pci_dev *pdev = NULL;
    u64 *ts = NULL;
    unsigned int i, n;
    u64 sum = 0, avg_ns = 0, freq_khz = 0;
    int ret = 0;

    if (parse_bdf(bdf, &dom, &bus, &dev, &fun)) {
        pr_err("pcie_reader: BDF 格式错误，应为 0000:bb:dd.f\n");
        return -EINVAL;
    }

    if (iter < 2) {
        pr_err("pcie_reader: iter 至少为 2，用于计算间隔\n");
        return -EINVAL;
    }

    /* DWORD 对齐 */
    offset &= ~0x3u;

    pdev = pci_get_domain_bus_and_slot(dom, bus, PCI_DEVFN(dev, fun));
    if (!pdev) {
        pr_err("pcie_reader: 找不到设备 %s\n", bdf);
        return -ENODEV;
    }

    /* 可选 CPU 亲和性：尽量把当前线程放到目标 CPU 上 */
    if (cpu >= 0) {
        int err = set_cpus_allowed_ptr(current, cpumask_of(cpu));
        if (err) {
            pr_warn("pcie_reader: 设定 CPU 亲和性失败，err=%d，继续\n", err);
        }
    }

    ts = kcalloc(iter, sizeof(*ts), GFP_KERNEL);
    if (!ts) {
        ret = -ENOMEM;
        goto out_put;
    }

    /*
     * 采样区间内禁迁移，尽量减少抖动。
     * （不禁抢占以避免长时间阻塞内核；需要更极限可配合 preempt_disable()，但风险更高）
     */
    migrate_disable();

    for (i = 0; i < iter; i++) {
        u32 val;
        /* 与用户态一致：先读，再记时间 */
        pci_read_config_dword(pdev, offset, &val);
        ts[i] = ktime_get_ns();
        /* 防止编译器过度优化（虽然函数调用本身已有副作用） */
        barrier();
    }

    migrate_enable();

    /* 计算平均间隔（ns） */
    n = iter - 1;
    for (i = 1; i < iter; i++) {
        u64 d = ts[i] - ts[i - 1];
        sum += d;
    }
    avg_ns = div64_u64(sum, n);
    if (avg_ns)
        freq_khz = div64_u64(1000000ULL, avg_ns); /* kHz = 1e6 / avg_ns */

    pr_info("pcie_reader: %s offset=0x%x iter=%u\n", bdf, offset, iter);
    pr_info("average sample interval = %llu ns，≈ %llu kHz\n",
            (unsigned long long)avg_ns, (unsigned long long)freq_khz);

    kfree(ts);

out_put:
    pci_dev_put(pdev);
    return ret;
}

static void __exit pcie_reader_exit(void)
{
    pr_info("pcie_reader: 卸载完成\n");
}

module_init(pcie_reader_init);
module_exit(pcie_reader_exit);

MODULE_AUTHOR("you");
MODULE_DESCRIPTION("PCI 配置空间快速采样（内核模块版）");
MODULE_LICENSE("GPL v2");
