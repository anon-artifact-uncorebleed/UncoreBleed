/*
 * pcie_reader_pci_fast.c
 *   读取 PCI 配置空间寄存器，极短间隔采样并批量写入 record.txt
 *
 * build: gcc -O2 -Wall -lpci -pthread -o pcie_reader_pci_fast pcie_reader_pci_fast.c
 * run  : sudo ./pcie_reader_pci_fast 0000:fe:0c.0 0x440 [cpu]
 */

#define _GNU_SOURCE
#include <pci/pci.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_LINES  8192          /* flush 批量行数 */
#define LINE_BYTES 32            /* 每行预留字节 */

static volatile int keep_running = 1;
static void sigint_handler(int s){ (void)s; keep_running = 0; }

static struct pci_dev *open_pci_dev(struct pci_access *pacc,
                                    const char *bdf_str)
{
    unsigned dom, bus, dev, fun;
    if (sscanf(bdf_str, "%x:%x:%x.%x", &dom, &bus, &dev, &fun) != 4) {
        fprintf(stderr, "BDF 格式应为 0000:bb:dd.f\n");
        return NULL;
    }
    struct pci_dev *d = pci_get_dev(pacc, dom, bus, dev, fun);
    if (!d) {
        fprintf(stderr, "找不到设备 %s\n", bdf_str);
        return NULL;
    }
    pci_fill_info(d, PCI_FILL_IDENT | PCI_FILL_BASES);
    return d;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
            "用法: %s <0000:bb:dd.f> <offset hex> [cpu_affinity]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *bdf    = argv[1];
    uint32_t    offset = strtoul(argv[2], NULL, 0) & ~3u;   /* DWORD 对齐 */

    /* 可选：绑到指定 CPU */
    if (argc >= 4) {
        int cpu = atoi(argv[3]);
        cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
        sched_setaffinity(0, sizeof(set), &set);
    }

    /* 初始化 libpci */
    struct pci_access *pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);

    struct pci_dev *dev = open_pci_dev(pacc, bdf);
    if (!dev) return EXIT_FAILURE;

    /* 自管缓冲 */
    char *buf = aligned_alloc(64, BUF_LINES * LINE_BYTES);
    if (!buf){ perror("aligned_alloc"); return EXIT_FAILURE; }
    size_t idx = 0;

    int fd_out = open("record.txt",
                      O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, 0644);
    if (fd_out < 0){ perror("record.txt"); return EXIT_FAILURE; }

    signal(SIGINT, sigint_handler);
    struct timespec ts;

    while (keep_running) {
        uint32_t val = pci_read_long(dev, offset);
        clock_gettime(CLOCK_REALTIME, &ts);

        idx += snprintf(buf + idx, LINE_BYTES,
                        "%ld.%09ld 0x%08" PRIX32 "\n",
                        ts.tv_sec, ts.tv_nsec, val);

        if (idx >= BUF_LINES * (LINE_BYTES - 1)) {
            if (write(fd_out, buf, idx) < 0){ perror("write"); break; }
            idx = 0;
        }
    }

    if (idx) write(fd_out, buf, idx);

    close(fd_out);
    free(buf);
    pci_cleanup(pacc);          /* ← 原来写错成 pci_free() */

    return 0;
}

