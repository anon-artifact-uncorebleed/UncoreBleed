// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("CTR0_L reader + enable/reset control, full‑stop on disable");

/* ==== BDF 参数 ==== */
static u16 segment = 0;
static u8  bus     = 0xfe;
static u8  dev     = 0x0c;
static u8  fn      = 0;
module_param(segment, ushort, 0444);
module_param(bus,     byte,   0444);
module_param(dev,     byte,   0444);
module_param(fn,      byte,   0444);

/* ==== 寄存器偏移 & 位 ==== */
#define OFF_UNIT_CTL 0x438
#define OFF_CTR0_L   0x440
#define OFF_CTRL0    0x468

#define BIT_EN   (1u << 22)
#define BIT_RST  (1u << 17)
#define BIT_FRZ  (1u << 31)

#define BASE_EVENT 0x00000336   /* umask=3 | event=0x36 */

/* ==== 全局 ==== */
static void __iomem *cfg;
static struct dentry *dbg_dir;
static DEFINE_SPINLOCK(reg_lock);
static u32 current_ctrl;        /* 我们最后一次写入的 CTRL0 内容 */

/* ---- 读计数器低 32 位 ---- */
static inline u32 ctr_read(void)
{
	return readl(cfg + OFF_CTR0_L);
}

/* ---- ctr0low 文件 ---- */
static int ctr_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", ctr_read());
	return 0;
}
static int ctr_open(struct inode *i, struct file *f)
{
	return single_open(f, ctr_show, NULL);
}
static const struct file_operations ctr_fops = {
	.owner = THIS_MODULE, .open = ctr_open,
	.read  = seq_read,    .llseek = seq_lseek,
	.release = single_release,
};

/* ---- enable 文件：写 0 ⇒ CTRL0=0；写 1 ⇒ event|EN ---- */
static int enable_get(void *data, u64 *val)
{
	*val = !!(current_ctrl & BIT_EN);
	return 0;
}
static int enable_set(void *data, u64 v)
{
	unsigned long flags;
	spin_lock_irqsave(&reg_lock, flags);

	if (v) {                           /* 开启计数 */
		current_ctrl = BASE_EVENT | BIT_EN;
	} else {                           /* 彻底停计数 */
		current_ctrl = 0;
	}
	writel(current_ctrl, cfg + OFF_CTRL0);

	spin_unlock_irqrestore(&reg_lock, flags);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(enable_fops, enable_get, enable_set, "%llu\n");

/* ---- reset 文件：Freeze → RST=1 → 恢复 ---- */
static ssize_t reset_write(struct file *f,
                           const char __user *ubuf, size_t len, loff_t *off)
{
	unsigned long flags;
	u32 uctl;

	spin_lock_irqsave(&reg_lock, flags);

	/* Freeze 计数器域 */
	uctl = readl(cfg + OFF_UNIT_CTL);
	writel(uctl | BIT_FRZ, cfg + OFF_UNIT_CTL);

	/* 拉高 RST 一拍 */
	writel(current_ctrl | BIT_RST, cfg + OFF_CTRL0);
	writel(current_ctrl,           cfg + OFF_CTRL0);

	/* Un‑freeze */
	writel(uctl & ~BIT_FRZ, cfg + OFF_UNIT_CTL);

	spin_unlock_irqrestore(&reg_lock, flags);
	return len;
}
static const struct file_operations reset_fops = {
	.owner = THIS_MODULE, .write = reset_write,
};

/* ---- ECAM 转物理地址 ---- */
static int ecam_phys(resource_size_t *out)
{
	struct acpi_table_header *tbl;
	if (acpi_get_table("MCFG", 0, &tbl))
		return -ENODEV;

	struct acpi_table_mcfg *m = (void *)tbl;
	struct acpi_mcfg_allocation *a = (void *)(m + 1);
	u32 n = (m->header.length - sizeof(*m)) / sizeof(*a);

	for (u32 i = 0; i < n; i++, a++) {
		if (a->pci_segment == segment &&
		    bus >= a->start_bus_number &&
		    bus <= a->end_bus_number) {
			*out = a->address
			     + ((resource_size_t)bus << 20)
			     + ((resource_size_t)dev << 15)
			     + ((resource_size_t)fn  << 12);
			acpi_put_table(tbl);
			return 0;
		}
	}
	acpi_put_table(tbl);
	return -ENODEV;
}

/* ---- init ---- */
static int __init m2m_ctrl_init(void)
{
	resource_size_t phys;
	int err = ecam_phys(&phys);
	if (err) {
		pr_err("m2m_ctrl32: ECAM entry not found\n");
		return err;
	}

	cfg = ioremap(phys, PAGE_SIZE);
	if (!cfg) return -EIO;

	/* 初始化为 event+EN=1 */
	current_ctrl = BASE_EVENT | BIT_EN;
	writel(current_ctrl, cfg + OFF_CTRL0);

	dbg_dir = debugfs_create_dir("m2m_ctrl32", NULL);
	if (!dbg_dir) { err = -ENOMEM; goto err_unmap; }

	if (!debugfs_create_file("ctr0low", 0444, dbg_dir, NULL, &ctr_fops) ||
	    !debugfs_create_file("enable", 0644, dbg_dir, NULL, &enable_fops) ||
	    !debugfs_create_file("reset",  0200, dbg_dir, NULL, &reset_fops))
	{
		err = -ENOMEM; goto err_dbg;
	}

	pr_info("m2m_ctrl32: loaded (seg=%u bus=%02x dev=%02x fn=%u)\n",
	        segment, bus, dev, fn);
	return 0;

err_dbg:
	debugfs_remove_recursive(dbg_dir);
err_unmap:
	iounmap(cfg);
	return err;
}

/* ---- exit ---- */
static void __exit m2m_ctrl_exit(void)
{
	/* 停计数器再退出 */
	writel(0, cfg + OFF_CTRL0);
	debugfs_remove_recursive(dbg_dir);
	if (cfg) iounmap(cfg);
	pr_info("m2m_ctrl32 unloaded\n");
}

module_init(m2m_ctrl_init);
module_exit(m2m_ctrl_exit);
