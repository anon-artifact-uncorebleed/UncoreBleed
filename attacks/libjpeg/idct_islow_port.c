// idct_islow_port.c
// Minimal, standalone port to build & run the provided _jpeg_idct_islow().
// Keeps the function body, variable names, structure, and order *unchanged*.
// Added: address-of-label prints for the AC-fastpath block in Pass 1.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>   // for ptrdiff_t

#include <unistd.h>   // getpid, sysconf
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>

/* ---- Minimal JPEG-ish type system & macros (compat layer) ---- */

#define DCTSIZE   8
#define DCTSIZE2  64

typedef int JDIMENSION;
typedef long JLONG;

/* Sample types */
typedef unsigned char _JSAMPLE;
typedef _JSAMPLE* _JSAMPROW;
typedef _JSAMPLE** _JSAMPARRAY;

/* Coefficient storage (like JCOEF in libjpeg-turbo) */
typedef short JCOEF;
typedef JCOEF* JCOEFPTR;

/* Decompress struct (only what we need) */
typedef struct {
    _JSAMPLE *range_limit_table;
} jpeg_decompress_struct;
typedef jpeg_decompress_struct* j_decompress_ptr;

/* Component info (only what we need) */
typedef struct {
    void* dct_table;  /* points to quantization table (int[64]) */
} jpeg_component_info;

/* ISLOW multiplier type (int in the reference implementation) */
typedef int ISLOW_MULT_TYPE;

/* libjpeg-style GLOBAL macro */
#define GLOBAL(type) type

/* Constants: scaling */
#define CONST_BITS   13
#define PASS1_BITS   2

/* Fixed-point helpers */
#define ONE         (1L << CONST_BITS)
#define FIX(x)      ((int) ((x) * (double)ONE + 0.5))

/* Fixed constants used by the AAN 8x8 IDCT (same names as libjpeg) */
#define FIX_0_298631336  FIX(0.298631336)
#define FIX_0_390180644  FIX(0.390180644)
#define FIX_0_541196100  FIX(0.541196100)
#define FIX_0_765366865  FIX(0.765366865)
#define FIX_0_899976223  FIX(0.899976223)
#define FIX_1_175875602  FIX(1.175875602)
#define FIX_1_501321110  FIX(1.501321110)
#define FIX_1_847759065  FIX(1.847759065)
#define FIX_1_961570560  FIX(1.961570560)
#define FIX_2_053119869  FIX(2.053119869)
#define FIX_2_562915447  FIX(2.562915447)
#define FIX_3_072711026  FIX(3.072711026)

/* Shifts with rounding like libjpeg */
#define RIGHT_SHIFT(x,shft)  ((x) >> (shft))
#define DESCALE(x,n)         RIGHT_SHIFT((x) + (1L << ((n)-1)), n)
#define LEFT_SHIFT(x,shft)   ((x) << (shft))

/* Dequantize & multiply helpers */
#define DEQUANTIZE(coef,quant)  ((JLONG) (coef) * (quant))
#define MULTIPLY(var,const_)    ((JLONG) (var) * (const_))

/* Range limit logic: emulate libjpeg's table-based clamp. */
#define RANGE_MASK  (1023)  /* 2 bits guard + 8 bits data -> 10 bits */
static _JSAMPLE range_limit_table[1024];

static int virt_to_phys(void *vaddr, uint64_t *paddr_out) {
    static int pagemap_fd = -1;
    if (pagemap_fd == -1) {
        pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
        if (pagemap_fd < 0) return -1;  // open failed (likely permission)
    }

    long pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize <= 0) return -2;

    uintptr_t va = (uintptr_t)vaddr;
    uint64_t vpn = va / (uint64_t)pagesize;
    off_t offset = (off_t)(vpn * 8ULL);

    uint64_t entry = 0;
    ssize_t n = pread(pagemap_fd, &entry, sizeof(entry), offset);
    if (n != (ssize_t)sizeof(entry)) return -3;   // read failed

    // pagemap format: bit63 present; bits 0..54 PFN (see Linux Documentation/vm/pagemap.rst)
    if ((entry & (1ULL << 63)) == 0) return -4;   // not present
    uint64_t pfn = entry & ((1ULL << 55) - 1);

    uint64_t page_off = va % (uint64_t)pagesize;
    *paddr_out = (pfn * (uint64_t)pagesize) + page_off;
    return 0;
}

static void print_va_pa(const char* tag, void* va) {
    uint64_t pa = 0;
    int rc = virt_to_phys(va, &pa);
    if (rc == 0) {
        printf("%s: VA=%p  PA=0x%llx\n", tag, va, (unsigned long long)pa);
    } else {
        printf("%s: VA=%p  PA=<n/a rc=%d errno=%d>\n", tag, va, rc, errno);
    }
}

static void build_range_limit_table(void) {
    /* The table maps values in [-256..511] (after &RANGE_MASK) to [0..255]. */
    for (int i = 0; i < 1024; i++) {
        int val = i - 256; /* center around 0 */
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        range_limit_table[i] = (_JSAMPLE) val;
    }
}

static _JSAMPLE *IDCT_range_limit(j_decompress_ptr cinfo) {
    (void)cinfo;
    return range_limit_table;
}

/* observe complexity decisions */
#define PRINT_BLOCK_FLAG(b)  do { printf("BLOCK_COMPLEX_FLAG=0x%02X\n", (unsigned)(b)); } while(0)

/* Placeholder for SHIFT_TEMPS (unused in islow) */
#define SHIFT_TEMPS  /* nothing */

/* ---- The function body from the user (kept *exactly* as given, plus 2 labels + printf) ---- */

GLOBAL(void)
_jpeg_idct_islow(j_decompress_ptr cinfo, jpeg_component_info *compptr,
                 JCOEFPTR coef_block, _JSAMPARRAY output_buf,
                 JDIMENSION output_col)
{
  JLONG tmp0, tmp1, tmp2, tmp3;
  JLONG tmp10, tmp11, tmp12, tmp13;
  JLONG z1, z2, z3, z4, z5;
  JCOEFPTR inptr;
  ISLOW_MULT_TYPE *quantptr;
  int *wsptr;
  _JSAMPROW outptr;
  _JSAMPLE *range_limit = IDCT_range_limit(cinfo);
  int ctr;
  int workspace[DCTSIZE2];      /* buffers data between passes */
  SHIFT_TEMPS

  //struct timespec ts_start, ts_end;

  int block_complex = 0;

  /* Pass 1: process columns from input, store into work array. */
  /* Note results are scaled up by sqrt(8) compared to a true IDCT; */
  /* furthermore, we scale the results by 2**PASS1_BITS. */

  inptr = coef_block;
  quantptr = (ISLOW_MULT_TYPE *)compptr->dct_table;
  wsptr = workspace;

    /* --- 追加：暂停等待 + 打印 workspace 地址、PID、物理地址 --- */
    printf("[pause] PID=%d  col=%d  workspace base=%p  size=%zu bytes",
       getpid(), DCTSIZE - ctr, (void*)workspace, sizeof(workspace));

    uint64_t pa = 0;
    int rc = virt_to_phys((void*)workspace, &pa);
    if (rc == 0) {
        printf("  phys=0x%llx", (unsigned long long)pa);
    } else {
        printf("  phys=<n/a rc=%d errno=%d>", rc, errno);
    }
    printf("\n");

    print_va_pa("FOR_BEGIN", &&FOR_LOOP_BEGIN);
    print_va_pa("FOR_END  ", &&FOR_LOOP_END);

    printf("Press 'c' then Enter to continue...\n");
    fflush(stdout);

    int ch;
    do { ch = getchar(); } while (ch != 'c' && ch != 'C' && ch != EOF);
    int discard;
    while ((discard = getchar()) != '\n' && discard != EOF) {}

/*
    printf("Press 'c' then Enter to continue...\n");
    fflush(stdout);

    int ch;
    do { ch = getchar(); } while (ch != 'c' && ch != 'C' && ch != EOF);
    int discard;
    while ((discard = getchar()) != '\n' && discard != EOF) {}
*/
  /* --- 追加结束 --- */

  for (ctr = DCTSIZE; ctr > 0; ctr--) {
    /* Due to quantization, we will usually find that many of the input
     * coefficients are zero, especially the AC terms.  We can exploit this
     * by short-circuiting the IDCT calculation for any column in which all
     * the AC terms are zero.  In that case each output is equal to the
     * DC coefficient (with scale factor as needed).
     * With typical images and quantization tables, half or more of the
     * column DCT calculations can be simplified this way.
     */
FOR_LOOP_BEGIN: ;    
    //clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start);
    if (inptr[DCTSIZE * 1] == 0 && inptr[DCTSIZE * 2] == 0 &&
        inptr[DCTSIZE * 3] == 0 && inptr[DCTSIZE * 4] == 0 &&
        inptr[DCTSIZE * 5] == 0 && inptr[DCTSIZE * 6] == 0 &&
        inptr[DCTSIZE * 7] == 0) {
      /* AC terms all zero */
      int dcval = LEFT_SHIFT(DEQUANTIZE(inptr[DCTSIZE * 0],
                             quantptr[DCTSIZE * 0]), PASS1_BITS);

      //printf("after wsptr base address: %p\n", (void*)wsptr);

      wsptr[DCTSIZE * 0] = dcval;
      wsptr[DCTSIZE * 1] = dcval;
      wsptr[DCTSIZE * 2] = dcval;
      wsptr[DCTSIZE * 3] = dcval;
      wsptr[DCTSIZE * 4] = dcval;
      wsptr[DCTSIZE * 5] = dcval;
      wsptr[DCTSIZE * 6] = dcval;
      wsptr[DCTSIZE * 7] = dcval;

      inptr++;                  /* advance pointers to next column */
      quantptr++;
      wsptr++;

      //clock_gettime(CLOCK_MONOTONIC_RAW, &ts_end);

      //uint64_t start_ns = (uint64_t)ts_start.tv_sec * 1000000000ULL + ts_start.tv_nsec;
      //uint64_t end_ns   = (uint64_t)ts_end.tv_sec   * 1000000000ULL + ts_end.tv_nsec;
      //printf("for 循环耗时: %llu ns (%.3f ms)\n",(unsigned long long)(end_ns - start_ns),(end_ns - start_ns) / 1e6);

      continue;
    }
FOR_LOOP_END: ;

    block_complex = 1;

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */

    z2 = DEQUANTIZE(inptr[DCTSIZE * 2], quantptr[DCTSIZE * 2]);
    z3 = DEQUANTIZE(inptr[DCTSIZE * 6], quantptr[DCTSIZE * 6]);

    z1 = MULTIPLY(z2 + z3, FIX_0_541196100);
    tmp2 = z1 + MULTIPLY(z3, -FIX_1_847759065);
    tmp3 = z1 + MULTIPLY(z2, FIX_0_765366865);
    
    z2 = DEQUANTIZE(inptr[DCTSIZE * 0], quantptr[DCTSIZE * 0]);
    z3 = DEQUANTIZE(inptr[DCTSIZE * 4], quantptr[DCTSIZE * 4]);

    tmp0 = LEFT_SHIFT(z2 + z3, CONST_BITS);
    tmp1 = LEFT_SHIFT(z2 - z3, CONST_BITS);

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

    tmp0 = DEQUANTIZE(inptr[DCTSIZE * 7], quantptr[DCTSIZE * 7]);
    tmp1 = DEQUANTIZE(inptr[DCTSIZE * 5], quantptr[DCTSIZE * 5]);
    tmp2 = DEQUANTIZE(inptr[DCTSIZE * 3], quantptr[DCTSIZE * 3]);
    tmp3 = DEQUANTIZE(inptr[DCTSIZE * 1], quantptr[DCTSIZE * 1]);

    z1 = tmp0 + tmp3;
    z2 = tmp1 + tmp2;
    z3 = tmp0 + tmp2;
    z4 = tmp1 + tmp3;
    z5 = MULTIPLY(z3 + z4, FIX_1_175875602); /* sqrt(2) * c3 */

    tmp0 = MULTIPLY(tmp0, FIX_0_298631336); /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp1 = MULTIPLY(tmp1, FIX_2_053119869); /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp2 = MULTIPLY(tmp2, FIX_3_072711026); /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp3 = MULTIPLY(tmp3, FIX_1_501321110); /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 = MULTIPLY(z1, -FIX_0_899976223); /* sqrt(2) * ( c7-c3) */
    z2 = MULTIPLY(z2, -FIX_2_562915447); /* sqrt(2) * (-c1-c3) */
    z3 = MULTIPLY(z3, -FIX_1_961570560); /* sqrt(2) * (-c3-c5) */
    z4 = MULTIPLY(z4, -FIX_0_390180644); /* sqrt(2) * ( c5-c3) */

    z3 += z5;
    z4 += z5;

    tmp0 += z1 + z3;
    tmp1 += z2 + z4;
    tmp2 += z2 + z3;
    tmp3 += z1 + z4;

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

    wsptr[DCTSIZE * 0] = (int)DESCALE(tmp10 + tmp3, CONST_BITS - PASS1_BITS);
    wsptr[DCTSIZE * 7] = (int)DESCALE(tmp10 - tmp3, CONST_BITS - PASS1_BITS);
    wsptr[DCTSIZE * 1] = (int)DESCALE(tmp11 + tmp2, CONST_BITS - PASS1_BITS);
    wsptr[DCTSIZE * 6] = (int)DESCALE(tmp11 - tmp2, CONST_BITS - PASS1_BITS);
    wsptr[DCTSIZE * 2] = (int)DESCALE(tmp12 + tmp1, CONST_BITS - PASS1_BITS);
    wsptr[DCTSIZE * 5] = (int)DESCALE(tmp12 - tmp1, CONST_BITS - PASS1_BITS);
    wsptr[DCTSIZE * 3] = (int)DESCALE(tmp13 + tmp0, CONST_BITS - PASS1_BITS);
    wsptr[DCTSIZE * 4] = (int)DESCALE(tmp13 - tmp0, CONST_BITS - PASS1_BITS);

    inptr++;                    /* advance pointers to next column */
    quantptr++;
    wsptr++;

    //clock_gettime(CLOCK_MONOTONIC_RAW, &ts_end);
  }

  //uint64_t start_ns = (uint64_t)ts_start.tv_sec * 1000000000ULL + ts_start.tv_nsec;
  //uint64_t end_ns   = (uint64_t)ts_end.tv_sec   * 1000000000ULL + ts_end.tv_nsec;
  //printf("for 循环耗时: %llu ns (%.3f ms)\n",(unsigned long long)(end_ns - start_ns),(end_ns - start_ns) / 1e6);

  /* Pass 2: process rows from work array, store into output array. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

  wsptr = workspace;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    outptr = output_buf[ctr] + output_col;
    /* Rows of zeroes can be exploited in the same way as we did with columns.
     * However, the column calculation has created many nonzero AC terms, so
     * the simplification applies less often (typically 5% to 10% of the time).
     * On machines with very fast multiplication, it's possible that the
     * test takes more time than it's worth.  In that case this section
     * may be commented out.
     */

#ifndef NO_ZERO_ROW_TEST
    if (wsptr[1] == 0 && wsptr[2] == 0 && wsptr[3] == 0 && wsptr[4] == 0 &&
        wsptr[5] == 0 && wsptr[6] == 0 && wsptr[7] == 0) {
      /* AC terms all zero */
      _JSAMPLE dcval = range_limit[(int)DESCALE((JLONG)wsptr[0],
                                                PASS1_BITS + 3) & RANGE_MASK];

      outptr[0] = dcval;
      outptr[1] = dcval;
      outptr[2] = dcval;
      outptr[3] = dcval;
      outptr[4] = dcval;
      outptr[5] = dcval;
      outptr[6] = dcval;
      outptr[7] = dcval;

      wsptr += DCTSIZE;         /* advance pointer to next row */
      continue;
    }
#endif

    block_complex = 1;

    /* Even part: reverse the even part of the forward DCT. */
    /* The rotator is sqrt(2)*c(-6). */

    z2 = (JLONG)wsptr[2];
    z3 = (JLONG)wsptr[6];

    z1 = MULTIPLY(z2 + z3, FIX_0_541196100);
    tmp2 = z1 + MULTIPLY(z3, -FIX_1_847759065);
    tmp3 = z1 + MULTIPLY(z2, FIX_0_765366865);

    tmp0 = LEFT_SHIFT((JLONG)wsptr[0] + (JLONG)wsptr[4], CONST_BITS);
    tmp1 = LEFT_SHIFT((JLONG)wsptr[0] - (JLONG)wsptr[4], CONST_BITS);

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    /* Odd part per figure 8; the matrix is unitary and hence its
     * transpose is its inverse.  i0..i3 are y7,y5,y3,y1 respectively.
     */

    tmp0 = (JLONG)wsptr[7];
    tmp1 = (JLONG)wsptr[5];
    tmp2 = (JLONG)wsptr[3];
    tmp3 = (JLONG)wsptr[1];

    z1 = tmp0 + tmp3;
    z2 = tmp1 + tmp2;
    z3 = tmp0 + tmp2;
    z4 = tmp1 + tmp3;
    z5 = MULTIPLY(z3 + z4, FIX_1_175875602); /* sqrt(2) * c3 */

    tmp0 = MULTIPLY(tmp0, FIX_0_298631336); /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp1 = MULTIPLY(tmp1, FIX_2_053119869); /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp2 = MULTIPLY(tmp2, FIX_3_072711026); /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp3 = MULTIPLY(tmp3, FIX_1_501321110); /* sqrt(2) * ( c1+c3-c5-c7) */
    z1 = MULTIPLY(z1, -FIX_0_899976223); /* sqrt(2) * ( c7-c3) */
    z2 = MULTIPLY(z2, -FIX_2_562915447); /* sqrt(2) * (-c1-c3) */
    z3 = MULTIPLY(z3, -FIX_1_961570560); /* sqrt(2) * (-c3-c5) */
    z4 = MULTIPLY(z4, -FIX_0_390180644); /* sqrt(2) * ( c5-c3) */

    z3 += z5;
    z4 += z5;

    tmp0 += z1 + z3;
    tmp1 += z2 + z4;
    tmp2 += z2 + z3;
    tmp3 += z1 + z4;

    /* Final output stage: inputs are tmp10..tmp13, tmp0..tmp3 */

    outptr[0] = range_limit[(int)DESCALE(tmp10 + tmp3,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];
    outptr[7] = range_limit[(int)DESCALE(tmp10 - tmp3,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];
    outptr[1] = range_limit[(int)DESCALE(tmp11 + tmp2,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];
    outptr[6] = range_limit[(int)DESCALE(tmp11 - tmp2,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];
    outptr[2] = range_limit[(int)DESCALE(tmp12 + tmp1,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];
    outptr[5] = range_limit[(int)DESCALE(tmp12 - tmp1,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];
    outptr[3] = range_limit[(int)DESCALE(tmp13 + tmp0,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];
    outptr[4] = range_limit[(int)DESCALE(tmp13 - tmp0,
                                         CONST_BITS + PASS1_BITS + 3) &
                            RANGE_MASK];

    wsptr += DCTSIZE;           /* advance pointer to next row */
  }

  //PRINT_BLOCK_FLAG(block_complex ? 0xFF : 0x00);
}

/* ---- Tiny demo / self-test ---- */
static void dump8x8(const char* title, _JSAMPLE rows[8][8]) {
    printf("%s\n", title);
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) printf("%3u ", (unsigned)rows[r][c]);
        printf("\n");
    }
    printf("\n");
}
static inline int idx(int r, int c){ return r*DCTSIZE + c; }

int main(void) {
    build_range_limit_table();

    jpeg_decompress_struct cinfo = { .range_limit_table = range_limit_table };
    jpeg_component_info comp;

    /* Unit quant table (all 1's) */
    int qtbl[DCTSIZE2];
    for (int i = 0; i < DCTSIZE2; i++) qtbl[i] = 1;
    comp.dct_table = qtbl;

    /* Output buffer: 8 rows of 8 samples */
    _JSAMPLE rows[8][8];
    _JSAMPARRAY rowptrs = (_JSAMPARRAY)malloc(sizeof(_JSAMPROW)*8);
    for (int r = 0; r < 8; r++) rowptrs[r] = rows[r];

    //Case 1: DC-only block (fast path, no AC)
    {
        JCOEF block[DCTSIZE2] = {0};

        /* 每列都给一个较大的 DC，保证输出显著非零 */
        for (int c = 0; c < 8; c++) {
            block[idx(0,c)] = 1000;        /* DC */
        }

        /* 奇数列放一些 AC（触发慢路径）；偶数列 AC 全 0（触发快路径） */
        for (int c = 0; c < 8; c++) {
            if (c & 1) {                   /* c=1,3,5,7 → 慢路径 */
                block[idx(1,c)] = 300;     /* 竖向一阶频率 */
                block[idx(2,c)] = -200;    /* 再放一点别的频率，确保不是全 0 */
                /* 你也可以只留一个 AC，比如只设 idx(1,c)=300，其他保持 0 */
            }
            /* c=0,2,4,6 → 快路径（保持 AC 全 0） */
        }
        _jpeg_idct_islow(&cinfo, &comp, block, rowptrs, /*output_col=*/0);
        dump8x8("IDCT output (8x8):", rows);
    }


    /* Case 2: Mixed AC block (slow path, AC present; coefficients large to avoid all-zero after scaling) */
{
    JCOEF block[DCTSIZE2] = {0};

    /* 设定系数幅度（可按需调大/调小，避免 DESCALE 后变 0） */
    const int DCv  = 1000;   /* DC 幅度 */
    const int ACv1 = 300;    /* AC1 幅度 */
    const int ACv2 = -200;   /* AC2 幅度 */

    /* 运行长度（分段）：比如 {2,3,1,2} 表示
       DC×2, AC×3, DC×1, AC×2 依次铺满 8 列 */
    int runs[] = {2, 3, 1, 2};
    int nRuns = (int)(sizeof(runs)/sizeof(runs[0]));

    int c = 0;           /* 当前列 */
    int ac_on = 0;       /* 0 表示当前段是 DC 段，1 表示 AC 段 */
    for (int r = 0; r < nRuns && c < 8; ++r) {
        for (int k = 0; k < runs[r] && c < 8; ++k, ++c) {
            /* 每列先放 DC，保证图样不至于太小 */
            block[idx(0, c)] = DCv;
            if (ac_on) {
                /* 该列加入少量 AC（触发慢路径） */
                block[idx(1, c)] = ACv1;
                block[idx(2, c)] = ACv2;
            }
        }
        ac_on ^= 1;  /* 段与段之间在 DC/AC 间切换 */
    }
    /* 若 runs 之和 < 8，则剩余列补 DC 段 */
    while (c < 8) {
        block[idx(0, c++)] = DCv;
    }

    /* （可选）打印一下预测的快/慢列分布，便于核对 */
    int fast_cols = 0, slow_cols = 0;
    for (int col = 0; col < 8; ++col) {
        int all0 = 1;
        for (int row = 1; row < 8; ++row) {
            if (block[idx(row, col)] != 0) { all0 = 0; break; }
        }
        if (all0) fast_cols++; else slow_cols++;
    }
    printf("=== Case 2: segmented columns: fast=%d, slow=%d ===\n", fast_cols, slow_cols);

    _jpeg_idct_islow(&cinfo, &comp, block, rowptrs, /*output_col=*/0);
    dump8x8("IDCT output (8x8):", rows);
}

    free(rowptrs);
    return 0;
}
