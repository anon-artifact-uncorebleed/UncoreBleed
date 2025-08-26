#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <x86intrin.h>   /* _mm_clflush & _mm_mfence */

#define PAGE_SIZE 4096ULL
#define NACCESS 100000UL

/* Translate a virtual address to its physical address by reading
 * /proc/self/pagemap.  Requires CAP_SYS_ADMIN or root privileges. */
static uint64_t virt_to_phys(void *virt)
{
    static int pagemap_fd = -1;
    if (pagemap_fd == -1) {
        pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
        if (pagemap_fd < 0) {
            perror("open pagemap");
            exit(EXIT_FAILURE);
        }
    }

    uint64_t virt_addr = (uint64_t)virt;
    uint64_t page_idx  = virt_addr / PAGE_SIZE;
    uint64_t offset    = page_idx * sizeof(uint64_t);

    uint64_t entry;
    if (pread(pagemap_fd, &entry, sizeof(entry), offset) != sizeof(entry)) {
        perror("pread pagemap");
        exit(EXIT_FAILURE);
    }

    if (!(entry & (1ULL << 63))) {
        fprintf(stderr, "page not present in RAM\n");
        exit(EXIT_FAILURE);
    }

    uint64_t pfn = entry & ((1ULL << 55) - 1);
    return (pfn * PAGE_SIZE) | (virt_addr & (PAGE_SIZE - 1));
}

int main(void)
{
    /* 1. Allocate one page, page-aligned */
    void *buf = aligned_alloc(PAGE_SIZE, PAGE_SIZE);
    if (!buf) {
        perror("aligned_alloc");
        return EXIT_FAILURE;
    }

    /* Touch the page so it really exists */
    memset(buf, 0, PAGE_SIZE);
    if (mlock(buf, PAGE_SIZE)) {
        perror("mlock (non-fatal)");
    }

    /* 2. Get its physical base address */
    uint64_t phys_base = virt_to_phys(buf);
    printf("Physical base address: 0x%llx\n", (unsigned long long)phys_base);

    /* 3. Pre-compute 1-bit-flipped addresses INSIDE this 4 KiB page. */
    enum { NUM_ADDRS = 12 };
    void *addr_list[NUM_ADDRS];
    uint64_t phys_list[NUM_ADDRS];

    for (int i = 0; i < NUM_ADDRS; ++i) {
        uint64_t phys = phys_base ^ (1ULL << i);     /* flip bit i */
        uint64_t off  = phys & (PAGE_SIZE - 1);      /* offset inside page */
        addr_list[i]  = (char *)buf + off;
        phys_list[i]  = phys;                        /* record physical */
        printf("%2d: phys 0x%llx (offset 0x%03llx)\n",
               i, (unsigned long long)phys, (unsigned long long)off);
    }

    /* 4. Interactive hammer loop */
    int idx = 0;
    for (;;) {
        volatile uint8_t *p = (volatile uint8_t *)addr_list[idx];

        /* 100 000 次：每次 clflush + 读改写 ------------------------- */
        for (size_t i = 0; i < NACCESS; ++i) {
            _mm_clflush((void *)p);   /* 刷出所在 cache line */
            _mm_mfence();             /* 序列化，确保 flush 完成 */
            (*p)++;                   /* 实际访问：读–改–写 */
        }

        printf("Accessed index %d (0x%llx) %lu×.  [a]gain / [c]ontinue / [q]uit: ",
               idx, (unsigned long long)phys_list[idx], (unsigned long)NACCESS);
        fflush(stdout);

        int ch = getchar();
        if (ch == EOF) break;
        if (ch == '\n') continue;      /* empty line */

        if (ch == 'q') break;
        if (ch == 'c') idx = (idx + 1) % NUM_ADDRS;
        /* if 'a', keep idx unchanged */

        /* swallow the rest of the line */
        while (ch != '\n' && ch != EOF) ch = getchar();
    }

    munlock(buf, PAGE_SIZE);
    free(buf);
    return 0;
}

