// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt) "uc_page: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/sched/mm.h>
#include <linux/version.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <linux/smp.h>          /* 确保包含 */
#include <linux/pgtable.h>

static void flush_tlb_ipi(void *unused)
{
        __flush_tlb_all();      /* 调本地 inline helper */
}

static inline pte_t *pte_map_lock(struct mm_struct *mm, pmd_t *pmd,
                                  unsigned long addr, spinlock_t **ptl)
{
        spinlock_t *lock = pte_lockptr(mm, pmd);
        *ptl = lock;
        spin_lock(lock);

#ifdef CONFIG_HIGHPTE           /* 高端内存需要临时映射 */
        return (pte_t *)kmap_local_pfn(pmd_pfn(*pmd)) +
               pte_index(addr);
#else                           /* 低端内存：直接虚拟地址 */
        return (pte_t *)pmd_page_vaddr(*pmd) +
               pte_index(addr);
#endif
}

/* ────────────── 模块参数 ────────────── */
static int pid = -1;
module_param(pid, int, 0444);
MODULE_PARM_DESC(pid, "PID of target process");

static unsigned long virt_addr;
module_param(virt_addr, ulong, 0444);
MODULE_PARM_DESC(virt_addr, "User virtual address");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("demo");
MODULE_DESCRIPTION("Set one user page uncacheable, restore on rmmod");

static pte_t saved_pte;
static bool  modified;
static struct mm_struct *mm_keep;

/* 用 vaddr 找到 pte，返回加锁后的指针和 ptl；失败返回 NULL */
static pte_t *walk_get_locked_pte(struct mm_struct *mm,
                                  unsigned long addr,
                                  spinlock_t **ptl_out)
{
        pgd_t *pgd = pgd_offset(mm, addr);
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;

        if (pgd_none(*pgd) || pgd_bad(*pgd))
                return NULL;

        p4d = p4d_offset(pgd, addr);
        if (p4d_none(*p4d) || p4d_bad(*p4d))
                return NULL;

        pud = pud_offset(p4d, addr);
        if (pud_none(*pud) || pud_bad(*pud))
                return NULL;

        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd) || pmd_bad(*pmd))
                return NULL;                    /* 仅处理 4 KiB 普通页 */

        //return pte_offset_map_lock(mm, pmd, addr, ptl_out);
        return pte_map_lock(mm, pmd, addr, ptl_out);
}

static int set_uc(struct mm_struct *mm, unsigned long addr, bool make_uc)
{
        pte_t *ptep;
        spinlock_t *ptl;
        pte_t old, new;

        mmap_write_lock(mm);

        ptep = walk_get_locked_pte(mm, addr, &ptl);
        if (!ptep) {
                mmap_write_unlock(mm);
                return -EFAULT;
        }

        old = *ptep;
        new = old;

        if (make_uc) {
                /* PCD=1 | PWT=1 | PAT=0  → 索引 3 (UC) */
                new = __pte(pte_val(new) | _PAGE_PCD | _PAGE_PWT);
                new = __pte(pte_val(new) & ~_PAGE_PAT);
        } else {
                new = saved_pte;       /* 恢复 */
        }

        if (!pte_same(old, new))
                set_pte_at(mm, addr, ptep, new);

        pte_unmap_unlock(ptep, ptl);

        on_each_cpu(flush_tlb_ipi, NULL, 1);               /* 让所有 CPU 丢弃该进程全部 TLB */

        mmap_write_unlock(mm);
        return 0;
}

/* ────────── insmod ────────── */
static int __init uc_init(void)
{
        struct task_struct *task;
        int ret;

        if (pid < 0 || virt_addr == 0) {
                pr_err("usage: insmod uc_page.ko pid=<PID> virt_addr=<ADDR>\n");
                return -EINVAL;
        }

        task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
        if (!task)
                return -ESRCH;

        mm_keep = get_task_mm(task);
        put_task_struct(task);

        if (!mm_keep) {
                pr_err("task has no mm (kernel thread?)\n");
                return -EINVAL;
        }

        /* 备份原 PTE（读锁足够） */
        {
                pte_t *ptep;
                spinlock_t *ptl;

                mmap_read_lock(mm_keep);
                ptep = walk_get_locked_pte(mm_keep, virt_addr, &ptl);
                if (!ptep) {
                        mmap_read_unlock(mm_keep);
                        mmput(mm_keep);
                        return -EFAULT;
                }
                saved_pte = *ptep;

                /* ====== 在这里插入打印 ====== */
                {
                        phys_addr_t pa = (phys_addr_t)pte_pfn(saved_pte) << PAGE_SHIFT;
                        pr_info("virt 0x%lx → phys 0x%llx (PFN %llu)\n",
                                virt_addr,
                                (unsigned long long)pa,
                                (unsigned long long)pte_pfn(saved_pte));
                }
                /* ====== 打印结束 ====== */

                pte_unmap_unlock(ptep, ptl);
                mmap_read_unlock(mm_keep);
        }

        ret = set_uc(mm_keep, virt_addr, true);
        if (ret) {
                pr_err("fail to set UC (%d)\n", ret);
                mmput(mm_keep);
                return ret;
        }
        modified = true;

        pr_info("pid %d  page 0x%lx => UC\n", pid, virt_addr & PAGE_MASK);
        return 0;
}

/* ────────── rmmod ────────── */
static void __exit uc_exit(void)
{
        if (modified && mm_keep) {
                set_uc(mm_keep, virt_addr, false);
                pr_info("page restored\n");
                mmput(mm_keep);
        }
}
module_init(uc_init);
module_exit(uc_exit);
