// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/highmem.h>
#include <linux/pid.h>

/* ---------- 模块参数 ---------- */
static unsigned long virt_addr;
module_param(virt_addr, ulong, 0444);
MODULE_PARM_DESC(virt_addr,
        "User virtual address to translate (e.g. virt_addr=0x12345678)");

static int pid = -1;
module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "Target PID (default: current)");

/* ---------- 辅助打印 ---------- */
static void parse_pte(pte_t pte)
{
    //unsigned long pfn = pte_pfn(pte);  // 获取页框号（物理页框号）
    //printk(KERN_INFO "PFN = 0x%lx\n", pfn);
	unsigned long pte_value = pte.pte;

    if (pte_value & (1UL << 1)) {
        printk(KERN_INFO "Write permission: Yes\n");
    } else {
        printk(KERN_INFO "Write permission: No\n");
    }

    if (pte_value & (1UL << 2)) {
        printk(KERN_INFO "User/Supervisor: User\n");
    } else {
        printk(KERN_INFO "User/Supervisor: Supervisor\n");
    }

    if (pte_value & (1UL << 3)) {
        printk(KERN_INFO "Write-through: Yes\n");
    } else {
        printk(KERN_INFO "Write-through: No\n");
    }

    if (pte_value & (1UL << 4)) {
        printk(KERN_INFO "Page uncacheable: Yes\n");
    } else {
        printk(KERN_INFO "Page uncacheable: No\n");
    }

    if (pte_value & (1UL << 5)) {
        printk(KERN_INFO "Page accessed: Yes\n");
    } else {
        printk(KERN_INFO "Page accessed: No\n");
    }

    if (pte_value & (1UL << 6)) {
        printk(KERN_INFO "Page dirty: Yes\n");
    } else {
        printk(KERN_INFO "Page dirty: No\n");
    }

    if (pte_value & (1UL << 7)) {
        printk(KERN_INFO "PAT: Yes\n");
    } else {
        printk(KERN_INFO "PAT: No\n");
    }

    if (pte_value & (1UL << 8)) {
        printk(KERN_INFO "Global page: Yes\n");
    } else {
        printk(KERN_INFO "Global page: No\n");
    }
	
    if (pte_value & 1) {
        printk(KERN_INFO "Page present: Yes\n");
    } else {
        printk(KERN_INFO "Page present: No\n");
    }
}

/* ---------- VA → PA ---------- */
static int virt_to_phys_user(struct task_struct *task,
                             unsigned long vaddr,
                             unsigned long *paddr_out)
{
        struct mm_struct       *mm;
        struct vm_area_struct  *vma;
        spinlock_t             *ptl = NULL;
        pte_t                  *pte = NULL;
        unsigned long           pfn;
        int                     ret = -EFAULT;

        if (!task)
                return -ESRCH;

        mm = get_task_mm(task);
        if (!mm)
                return -EINVAL;

        mmap_read_lock(mm);
        vma = find_vma(mm, vaddr);
        if (!vma || vaddr < vma->vm_start) {
                mmap_read_unlock(mm);
                ret = -EFAULT;
                goto out_mmput;
        }

        ret = follow_pte(vma, vaddr, &pte, &ptl);
        mmap_read_unlock(mm);
        if (ret)
                goto out_mmput;

        if (!pte_present(*pte)) {
                ret = -ENXIO;
                goto out_unlock;
        }

        pfn         = pte_pfn(*pte);
        *paddr_out  = (pfn << PAGE_SHIFT) | (vaddr & (PAGE_SIZE - 1));

        parse_pte(*pte);
        ret = 0;

out_unlock:
        pte_unmap_unlock(pte, ptl);
out_mmput:
        mmput(mm);
        return ret;
}

/* ---------- 模块入口/退出 ---------- */
static int __init ptw_init(void)
{
        struct task_struct *task = NULL;
        unsigned long phys = 0;
        int err;

        if (!virt_addr) {
                pr_err("[ptw] need virt_addr (e.g. insmod ptw.ko virt_addr=0x1234 [...])\n");
                return -EINVAL;
        }

        if (pid > 0) {
                struct pid *kpid = find_get_pid(pid);
                if (!kpid) {
                        pr_err("[ptw] PID %d not found\n", pid);
                        return -ESRCH;
                }
                task = get_pid_task(kpid, PIDTYPE_PID);
                put_pid(kpid);
                if (!task) {
                        pr_err("[ptw] task for PID %d vanished\n", pid);
                        return -ESRCH;
                }
                pr_info("[ptw] Target PID %d\n", pid);
        } else {
                task = current;
                pr_info("[ptw] Target = current process (PID %d)\n", task_pid_nr(task));
        }

        err = virt_to_phys_user(task, virt_addr, &phys);

        if (task != current)
                put_task_struct(task);        /* 解除引用 */

        if (err)
                pr_err("[ptw] translate 0x%lx failed: %d\n", virt_addr, err);
        else
                pr_info("[ptw] VA 0x%lx ➜ PA 0x%lx\n", virt_addr, phys);

        return err;
}

static void __exit ptw_exit(void)
{
        pr_info("[ptw] module unloaded\n");
}

module_init(ptw_init);
module_exit(ptw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("dianoFord (adapted)");
MODULE_DESCRIPTION("Translate user VA ➜ PA (supports pid & virt_addr) on Linux ≥6.11");

