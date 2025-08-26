// build: make -C /lib/modules/$(uname -r)/build M=$(PWD) modules
// usage: sudo insmod cr0peek.ko cpu=7   # 在CPU7上读取
//        dmesg | tail                   # 查看输出

#include <linux/module.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <asm/special_insns.h>   // read_cr0()

static int cpu = 0;
module_param(cpu, int, 0444);
MODULE_PARM_DESC(cpu, "Target logical CPU to read CR0 on");

static unsigned long cr0_val;

static void read_cr0_on_cpu(void *info)
{
    (void)info;
    cr0_val = read_cr0();
}

static int __init cr0peek_init(void)
{
    int ncpus = num_online_cpus();

    if (cpu < 0 || cpu >= nr_cpu_ids || !cpu_online(cpu)) {
        pr_err("cr0peek: CPU %d is invalid or offline (online=%d)\n",
               cpu, ncpus);
        return -EINVAL;
    }

    // 在目标CPU上执行 read_cr0()
    int ret = smp_call_function_single(cpu, read_cr0_on_cpu, NULL, 1);
    if (ret) {
        pr_err("cr0peek: smp_call_function_single failed: %d\n", ret);
        return ret;
    }

    pr_info("cr0peek: CPU %d CR0 = 0x%lx (NW=%lu, CD=%lu)\n",
            cpu, cr0_val,
            (cr0_val >> 29) & 1UL,   // NW bit 29
            (cr0_val >> 30) & 1UL);  // CD bit 30
    return 0;
}

static void __exit cr0peek_exit(void) { }

MODULE_LICENSE("GPL");
MODULE_AUTHOR("you");
MODULE_DESCRIPTION("Read CR0 (NW/CD) on a specific CPU");
module_init(cr0peek_init);
module_exit(cr0peek_exit);

