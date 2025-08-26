/*
 * mem_stride_clflush_repeat.c
 *
 * 功能一览
 * --------------------------------------------------------------------------
 * • 运行时首先打印进程 PID。
 * • 申请并零化整页 (4 KiB) 内存，输出其虚拟 / 物理首地址。
 * • 以 64 B 步长遍历该页：
 *       ┌─ 每次读写循环前先 clflush 目标地址所在 cache‐line，
 *       │  随后对该字节执行自增，完整循环 100 000 次。
 *       └─ 每完成一轮循环即等待用户指令：
 *              a / A  → 在 **同一地址** 再执行 100 000 次访问
 *              c / C  → 跳到 **下一地址**（+64 B）
 *              其它   → 立即结束程序
 *
 * 物理地址通过 /proc/self/pagemap 获取；若无 CAP_SYS_ADMIN 权限则打印
 * "<unavailable>"。
 *
 * 编译（x86-64 / Linux）：
 *     gcc -O0 -std=c11 -Wall -o mem_stride_clflush_repeat \
 *         mem_stride_clflush_repeat.c
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <immintrin.h>      /* _mm_clflush / _mm_mfence */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*----------------- 虚拟地址 → 物理地址 ------------------------------------*/
static uint64_t virt_to_phys(void *va)
{
    const uint64_t page_size = (uint64_t)sysconf(_SC_PAGESIZE);
    const uint64_t virt      = (uint64_t)va;
    const uint64_t page_off  = virt & (page_size - 1);
    const uint64_t index     = virt / page_size;

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) return (uint64_t)-1;

    uint64_t entry;
    if (lseek(fd, (off_t)(index * sizeof(entry)), SEEK_SET) == -1 ||
        read(fd, &entry, sizeof(entry)) != (ssize_t)sizeof(entry)) {
        close(fd);
        return (uint64_t)-1;
    }
    close(fd);

    if (!(entry & (1ULL << 63)))    /* bit 63 == 1 → page present */
        return (uint64_t)-1;

    uint64_t pfn = entry & ((1ULL << 55) - 1);   /* bits 0-54 */
    return (pfn * page_size) + page_off;
}

/*---------------------------------------------------------------------------*/
int main(void)
{
    /* ---------- 进程信息 -------------------------------------------------- */
    printf("PID: %d\n", getpid());

    /* ---------- 常量定义 -------------------------------------------------- */
    const size_t PAGE    = 16384;    /* 4 KiB  */
    const size_t STRIDE  = 1024;      /* 64 B   */
    const size_t NACCESS = 100000;  /* 每轮访问次数 */

    /* ---------- 申请并初始化内存 ---------------------------------------- */
    void *buf = NULL;
    if (posix_memalign(&buf, PAGE, PAGE) != 0) {
        perror("posix_memalign");
        return EXIT_FAILURE;
    }
    memset(buf, 0, PAGE);           /* 显式置零并触发缺页 */

    /* ---------- 打印首地址 ------------------------------------------------ */
    printf("Allocated 4 KiB @ virtual %p\n", buf);
    uint64_t phys0 = virt_to_phys(buf);
    if (phys0 != (uint64_t)-1)
        printf("                physical 0x%016" PRIx64 "\n", phys0);
    else
        puts("                physical <unavailable>");

    puts("\n开始访问…（'a'=重复当前地址，'c'=下一地址，其它键退出）");

    /* ---------- 主循环 ---------------------------------------------------- */
    int exit_flag = 0;

    for (size_t off = 0; off < PAGE && !exit_flag; off += STRIDE) {

        volatile unsigned char *p = (unsigned char *)buf + off;

        /* 打印当前目标地址信息 */
        uint64_t phys = virt_to_phys((void *)p);
        printf("\n>>> 轮次 %-2zu  地址 virt %p",
               off / STRIDE + 1, (void *)p);
        if (phys != (uint64_t)-1)
            printf(", phys 0x%016" PRIx64, phys);
        putchar('\n');

        /* --------- 针对当前地址的重复访问循环 --------------------------- */
        while (!exit_flag) {

            /* 100 000 次：每次 clflush + 读改写 ------------------------- */
            for (size_t i = 0; i < NACCESS; ++i) {
                _mm_clflush((void *)p);   /* 刷出所在 cache line */
                _mm_mfence();             /* 序列化，确保 flush 完成 */
                (*p)++;                   /* 实际访问 */
            }

            puts("完成 100 000 次访问。输入 "
                 "'a' 重复当前地址，'c' 下一地址，其它键退出。");

            int ch = getchar();

            /* 丢弃行尾残留字符，确保下次 getchar() 等待真正输入 */
            if (ch != '\n')
                while ((getchar()) != '\n' && !feof(stdin)) ;

            if (ch == 'a' || ch == 'A') {
                /* 再来一次：继续 while，同一地址再跑 100 000 次 */
                continue;
            } else if (ch == 'c' || ch == 'C') {
                /* 跳出 while，外层 for 循环进入下一地址 */
                break;
            } else {
                /* 结束程序 */
                exit_flag = 1;
                break;
            }
        }
    }

    free(buf);
    puts("\n结束。");
    return 0;
}

