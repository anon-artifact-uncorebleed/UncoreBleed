/*  m2m_bit32_fast.c  –  with progress display
 *  gcc -O2 -Wall -std=c11 -o m2m_bit32_fast m2m_bit32_fast.c
 *  sudo ./m2m_bit32_fast [percent]   # e.g. 50 75 90
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
#include <x86intrin.h>

#define PAGE_SZ   4096UL
#define NACCESS   100000UL
#define PFN_MASK  ((1ULL << 55) - 1)

/* ---------- PFN -> idx hash ---------- */
#define H_BITS 24
#define H_SZ   (1UL << H_BITS)
static int32_t *h_tbl;
static inline void hash_init(void){
    h_tbl = malloc(H_SZ * sizeof(int32_t));
    if (!h_tbl){ perror("hash malloc"); exit(1); }
    memset(h_tbl, 0xFF, H_SZ * sizeof(int32_t));
}
static inline void hash_insert(uint64_t pfn, int32_t idx){
    uint64_t k = pfn & (H_SZ - 1);
    while (h_tbl[k] != -1) k = (k + 1) & (H_SZ - 1);
    h_tbl[k] = idx;
}
static inline int32_t hash_find(uint64_t pfn, const uint64_t *pfnlist){
    uint64_t k = pfn & (H_SZ - 1);
    while (h_tbl[k] != -1){
        if (pfnlist[h_tbl[k]] == pfn) return h_tbl[k];
        k = (k + 1) & (H_SZ - 1);
    }
    return -1;
}

/* ---------- hammer ---------- */
static void hammer(void *va){
    volatile uint64_t *p = (volatile uint64_t*)va;
    for (size_t i = 0; i < NACCESS; ++i){
        _mm_clflush((void*)p);
        _mm_mfence();
        (*p)++;
    }
}

struct Rec { int bit; int32_t idx; };

int main(int argc, char **argv)
{
    /* -------- 参数与 mmap -------- */
    double pct = (argc == 2) ? atof(argv[1]) : 90.0;
    if (pct <= 0.0 || pct > 100.0){
        fprintf(stderr, "percent must be in (0,100]\n");
        return 1;
    }

    long total_pages = sysconf(_SC_PHYS_PAGES);
    size_t npages    = (size_t)(total_pages * (pct / 100.0));
    size_t nbytes    = npages * PAGE_SZ;

    printf("mmap %.1f%% -> %.2f GiB ...\n",
           pct, nbytes / 1024.0 / 1024 / 1024);

    uint8_t *pool = mmap(NULL, nbytes,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                         -1, 0);
    if (pool == MAP_FAILED){ perror("mmap"); return 1; }

    /* -------- (1/3) Fault-in pages -------- */
    size_t progress_step = npages / 100 ? npages / 100 : 1;
    for (size_t pg = 0; pg < npages; ++pg){
        pool[pg * PAGE_SZ] = 0;
        if (!(pg % progress_step)){
            fprintf(stderr, "\r[1/3] Touch pages: %3zu%%",
                    pg * 100 / npages);
        }
    }
    //printf("000\n");
    fprintf(stderr, "\r[1/3] Touch pages: 100%%\n");

    //printf("111\n");

    /* -------- (2/3) 批量读取 pagemap -------- */
    int pm_fd = open("/proc/self/pagemap", O_RDONLY);
    if (pm_fd < 0){ 
	    perror("open pagemap"); 
	    return 1; 
    }

    //printf("222\n");
    uint64_t *pfnlist = malloc(npages * sizeof(uint64_t));
    if (!pfnlist){ perror("malloc pfnlist"); return 1; }

    //printf("333\n");

    hash_init();

    const size_t STEP_PG = (256 * 1024 * 1024UL) / PAGE_SZ;  /* 256 MiB */
    const size_t CHUNK_PG = 256;        /* 1 MiB */
    uint64_t *buf = malloc(CHUNK_PG * 8);
    if (!buf){ perror("malloc buf"); return 1; }

    //printf("444\n");

    size_t pg = 0;
    while (pg < npages){
        size_t left = npages - pg;
        size_t now  = left < CHUNK_PG ? left : CHUNK_PG;

        off_t off = ((uintptr_t)(pool + pg * PAGE_SZ)) >> 9;
        if (pread(pm_fd, buf, now * 8, off) != (ssize_t)(now * 8)){
            perror("pread pagemap"); return 1;
        }
        for (size_t i = 0; i < now; ++i, ++pg){
            uint64_t entry = buf[i];
            uint64_t pfn   = entry & PFN_MASK;
            pfnlist[pg]    = pfn;
            hash_insert(pfn, (int32_t)pg);
        }
        if (!(pg % STEP_PG) || pg == npages) {
        	fprintf(stderr, "\r[2/3] Read pagemap: %3zu%%", pg * 100 / npages);
        	fflush(stderr);
    	}
    }
    fprintf(stderr, "\r[2/3] Read pagemap: 100%%\n");
    free(buf);
    close(pm_fd);

    printf("Committed %zu pages (%.2f GiB)\n",
           npages, npages * PAGE_SZ / 1024.0 / 1024 / 1024);

    /* -------- (3/3) 随机 PFN + 搜互补位 -------- */
    srand((unsigned)time(NULL));
    size_t   base_idx = (size_t)rand() % npages;
    uint64_t base_pfn = pfnlist[base_idx];
    printf("Base PFN 0x%lx  (virt %p)\n",
           base_pfn, pool + base_idx * PAGE_SZ);

    struct Rec rec[32]; int nrec = 0;
    for (int bit = 12; bit <= 31; ++bit){
        uint64_t tgt  = base_pfn ^ (1ULL << bit);
        int32_t  idx2 = hash_find(tgt, pfnlist);
        if (idx2 >= 0){
            rec[nrec].bit = bit;
            rec[nrec].idx = idx2;
            ++nrec;
        }
    }
    if (!nrec){ puts("No complements found – exiting."); return 0; }

    printf("Found %d complement bits:", nrec);
    for (int i = 0; i < nrec; ++i) printf(" %d", rec[i].bit);
    puts("");

    /* -------- 交互 hammer -------- */
    int cur = 0;
    while (1){
        int     bit  = rec[cur].bit;
        int32_t idx2 = rec[cur].idx;
        uint64_t phys = (uint64_t)pfnlist[idx2] << 12;
        void   *va   = pool + (size_t)idx2 * PAGE_SZ;

        printf("bit %d – phys 0x%llx  [a]gain [c]next [q]quit: ",
               bit, (unsigned long long)phys);
        fflush(stdout);

        int ch = getchar();
        if (ch == 'a')      hammer(va);
        else if (ch == 'c') cur = (cur + 1) % nrec;
        else if (ch == 'q' || ch == EOF) break;

        if (ch != '\n')
            while (getchar() != '\n' && !feof(stdin));
    }

    munmap(pool, nbytes);
    free(pfnlist);
    free(h_tbl);
    return 0;
}

