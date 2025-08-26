/*  m2m_bit32_bsearch_offonly.c  –  全部打印为 off
 *  编译： gcc -O2 -Wall -std=c11 -o m2m_bit32_bsearch m2m_bit32_bsearch_offonly.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <inttypes.h>
#include <x86intrin.h>

#define PAGE_SZ   4096UL
#define CHUNK_PG  4096UL          /* 16 MiB */
#define NACCESS   100000UL
#define PFN_MASK  ((1ULL << 55) - 1)

/* ----------- (pfn, idx) 对 ----------- */
struct Pair { uint64_t pfn; uint32_t idx; };
static int cmp_pair(const void *a, const void *b)
{
    uint64_t pa = ((const struct Pair*)a)->pfn;
    uint64_t pb = ((const struct Pair*)b)->pfn;
    return (pa > pb) - (pa < pb);
}

/* ----------- hammer ----------- */
static void hammer(void *va)
{
    volatile uint64_t *p = (volatile uint64_t*)va;
    for (size_t i = 0; i < NACCESS; ++i) {
        _mm_clflush((void*)p);
        _mm_mfence();
        (*p)++;
    }
}

/* ----------- 记录项 ----------- */
/* 所有记录都用 "off" 打印，bit 表示翻转的位 */
struct Rec { int bit; void *va; uint64_t phys; };

#ifndef MAX_PFN_BITS
#define MAX_PFN_BITS 40
#endif

static int regenerate(struct Rec *res, size_t res_cap, uint64_t *base_phys_out,
                      uint8_t *pool, size_t npages,
                      const uint64_t *pfnlist, const struct Pair *pairs)
{
    size_t base_idx = (size_t)rand() % npages;
    uint64_t base_pfn = pfnlist[base_idx];
    uint64_t page_off = (uint64_t)rand() & 0xFFF;          /* 0–4095 B */
    uint64_t base_phys = (base_pfn << 12) | page_off;
    uint8_t *base_va0  = pool + base_idx * PAGE_SZ;        /* 页首 VA */
    void    *base_va   = base_va0 + page_off;

    *base_phys_out = base_phys;

    size_t nres = 0;

    /* 0) 自身 */
    if (nres < res_cap) {
        res[nres].bit   = -1;
        res[nres].va    = base_va;
        res[nres].phys  = base_phys;
        ++nres;
    }

    /* 1) 页内偏移（低 12 位） */
    for (int b = 0; b <= 11; ++b) {
        if (nres >= res_cap) break;
        uint64_t new_off = page_off ^ (1ULL << b);
        res[nres].bit    = b;
        res[nres].va     = base_va0 + new_off;
        res[nres].phys   = (base_pfn << 12) | new_off;
        ++nres;
    }

    /* 2) PFN 位（第 12 位及以上） */
    for (int b = 0; b < MAX_PFN_BITS; ++b) {
        if (nres >= res_cap) break;
        uint64_t tgt_pfn = base_pfn ^ (1ULL << b);

        struct Pair key = { .pfn = tgt_pfn, .idx = 0 };
        struct Pair *hit = bsearch(&key, pairs, npages, sizeof *pairs, cmp_pair);
        if (!hit) continue;

        uint64_t tgt_phys = (tgt_pfn << 12) | page_off;
        void *va = pool + (size_t)hit->idx * PAGE_SZ + page_off;

        res[nres].bit   = 12 + b;   /* 把 PFN 翻转的位打印成 off 12+bit */
        res[nres].va    = va;
        res[nres].phys  = tgt_phys;
        ++nres;
    }

    return (int)nres;
}

/* ================= 主程序 ================= */
int main(int argc, char **argv)
{
    double pct = (argc == 2) ? atof(argv[1]) : 50.0;
    if (pct <= 0.0 || pct > 100.0) {
        fprintf(stderr, "percent 0–100\n"); return 1;
    }

    long total_pages = sysconf(_SC_PHYS_PAGES);
    size_t npages    = (size_t)(total_pages * (pct / 100.0));
    size_t nbytes    = npages * PAGE_SZ;

    printf("mmap %.1f%% -> %.2f GiB …\n",
           pct, nbytes / 1024.0 / 1024 / 1024);

    uint8_t *pool = mmap(NULL, nbytes,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                         -1, 0);
    if (pool == MAP_FAILED) { perror("mmap"); return 1; }

    /* 触发缺页，保证物理页分配 */
    size_t step = npages / 100 ? npages / 100 : 1;
    for (size_t pg = 0; pg < npages; ++pg) {
        pool[pg * PAGE_SZ] = 0;
        if (!(pg % step) || pg + 1 == npages)
            fprintf(stderr, "\r[1/3] Touch: %3zu%%",
                    (pg + 1) * 100 / npages);
    }
    fprintf(stderr, "\n");

    /* pagemap 读 PFN */
    int pm_fd = open("/proc/self/pagemap", O_RDONLY);
    if (pm_fd < 0) { perror("open pagemap"); return 1; }

    uint64_t     *pfnlist = malloc(npages * sizeof(uint64_t));
    struct Pair  *pairs   = malloc(npages * sizeof(struct Pair));
    if (!pfnlist || !pairs) { perror("malloc"); return 1; }

    uint64_t *buf = malloc(CHUNK_PG * 8);
    if (!buf) { perror("malloc buf"); return 1; }

    size_t pg = 0;
    while (pg < npages) {
        size_t left = npages - pg;
        size_t now  = left < CHUNK_PG ? left : CHUNK_PG;

        off_t off = ((uintptr_t)(pool + pg * PAGE_SZ)) >> 9;
        if (pread(pm_fd, buf, now * 8, off) != (ssize_t)(now * 8)) {
            perror("pread pagemap"); return 1;
        }
        for (size_t i = 0; i < now; ++i, ++pg) {
            uint64_t pfn   = buf[i] & PFN_MASK;
            pfnlist[pg]    = pfn;
            pairs[pg].pfn  = pfn;
            pairs[pg].idx  = (uint32_t)pg;
        }
        if (!(pg % step) || pg == npages)
            fprintf(stderr, "\r[2/3] Read pagemap: %3zu%%",
                    pg * 100 / npages);
    }
    fprintf(stderr, "\n");
    close(pm_fd);
    free(buf);

    qsort(pairs, npages, sizeof *pairs, cmp_pair);

    const size_t RES_MAX = 1 + 12 + MAX_PFN_BITS;
    struct Rec  *res = malloc(sizeof(struct Rec) * RES_MAX);
    if (!res) { perror("malloc res"); return 1; }

    uint64_t   base_phys;
    int nres;
    srand((unsigned)time(NULL));

    do {
        nres = regenerate(res, RES_MAX, &base_phys, pool, npages,
                          pfnlist, pairs);
    } while (nres <= 0);

    printf("\nBase  phys 0x%llx\n", (unsigned long long)base_phys);
    for (int i = 0; i < nres; ++i) {
        if (res[i].bit == -1)
            printf("  [self]        phys 0x%llx  virt %p\n",
                   (unsigned long long)res[i].phys, res[i].va);
        else
            printf("  [off b%2d]    phys 0x%llx  virt %p\n",
                   res[i].bit, (unsigned long long)res[i].phys, res[i].va);
    }

    int cur = 0;
    while (1) {
        printf("\n");
        if (res[cur].bit == -1)
            printf("[self] – phys 0x%llx  ", (unsigned long long)res[cur].phys);
        else
            printf("[off b%2d] – phys 0x%llx  ",
                   res[cur].bit, (unsigned long long)res[cur].phys);
        printf("[a]gain [c]next [t]rand [q]uit: ");
        fflush(stdout);

        int ch = getchar();
        if (ch == 'a') {
            hammer(res[cur].va);
        } else if (ch == 'c') {
            cur = (cur + 1) % nres;
        } else if (ch == 't') {
            do {
                nres = regenerate(res, RES_MAX, &base_phys, pool, npages,
                                  pfnlist, pairs);
            } while (nres <= 0);
            printf("\nNew base phys 0x%llx; %d items:\n",
                   (unsigned long long)base_phys, nres);
            for (int i = 0; i < nres; ++i) {
                if (res[i].bit == -1)
                    printf("  [self]        phys 0x%llx  virt %p\n",
                           (unsigned long long)res[i].phys, res[i].va);
                else
                    printf("  [off b%2d]    phys 0x%llx  virt %p\n",
                           res[i].bit, (unsigned long long)res[i].phys, res[i].va);
            }
            cur = 0;
        } else if (ch == 'q' || ch == EOF) {
            break;
        }
        if (ch != '\n')
            while (getchar() != '\n' && !feof(stdin));
    }

    munmap(pool, nbytes);
    free(pfnlist);
    free(pairs);
    free(res);
    return 0;
}

