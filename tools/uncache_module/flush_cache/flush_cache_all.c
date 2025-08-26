#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <asm/msr.h>
#include <asm/io.h>
#include <linux/version.h>

static int major;
static struct class *flush_cache_class;

// 让每个 CPU 核心执行 wbinvd 的函数
static void wbinvd_on_this_cpu(void *info)
{
    asm volatile("wbinvd" ::: "memory");
}

// 将 wbinvd_on_this_cpu 广播给所有在线 CPU
static void do_wbinvd_all_cores(void)
{
    // on_each_cpu() 会在所有在线 CPU 上执行同一个函数
    // 最后一个参数(1) 表示所有函数都执行完才返回
    on_each_cpu(wbinvd_on_this_cpu, NULL, 1);
}

// 设备文件的 open/release，简单实现即可
static int flush_cache_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int flush_cache_release(struct inode *inode, struct file *file)
{
    return 0;
}

// 当从设备文件 /dev/flush_cache_all 读数据时，执行 wbinvd_all
static ssize_t flush_cache_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
    // 在这里调用一次 "所有核心 wbinvd"
    do_wbinvd_all_cores();
    return 0;  // 实际不返回任何数据
}

static const struct file_operations flush_cache_fops = {
    .owner   = THIS_MODULE,
    .open    = flush_cache_open,
    .release = flush_cache_release,
    .read    = flush_cache_read,
};

/*
static int __init flush_cache_init(void)
{
    // 注册字符设备
    major = register_chrdev(0, "flush_cache_all", &flush_cache_fops);
    if (major < 0) {
        pr_err("flush_cache_all: register_chrdev failed\n");
        return major;
    }

    // 创建设备节点 /dev/flush_cache_all
    flush_cache_class = class_create(THIS_MODULE, "flush_cache_all_class");
    if (IS_ERR(flush_cache_class)) {
        unregister_chrdev(major, "flush_cache_all");
        return PTR_ERR(flush_cache_class);
    }
    device_create(flush_cache_class, NULL, MKDEV(major, 0), NULL, "flush_cache_all");

    pr_info("flush_cache_all module loaded, /dev/flush_cache_all created\n");
    return 0;
}
*/

static int __init flush_cache_init(void)
{
    /* 1. 注册字符设备 */
    major = register_chrdev(0, "flush_cache_all", &flush_cache_fops);
    if (major < 0) {
        pr_err("flush_cache_all: register_chrdev failed\n");
        return major;
    }

    /* 2. 创建设备类 —— 新 API 只要一个参数 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 9, 0)
    flush_cache_class = class_create("flush_cache_all_class");
#else
    flush_cache_class = class_create(THIS_MODULE, "flush_cache_all_class");
#endif
    if (IS_ERR(flush_cache_class)) {
        unregister_chrdev(major, "flush_cache_all");
        return PTR_ERR(flush_cache_class);
    }

    /* 3. 创建设备节点 */
    device_create(flush_cache_class, NULL, MKDEV(major, 0),
                  NULL, "flush_cache_all");

    pr_info("flush_cache_all module loaded, /dev/flush_cache_all created\n");
    return 0;
}

static void __exit flush_cache_exit(void)
{
    device_destroy(flush_cache_class, MKDEV(major, 0));
    class_destroy(flush_cache_class);
    unregister_chrdev(major, "flush_cache_all");
    pr_info("flush_cache_all module unloaded\n");
}

module_init(flush_cache_init);
module_exit(flush_cache_exit);
MODULE_LICENSE("GPL");
