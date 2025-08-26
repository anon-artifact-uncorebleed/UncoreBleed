// build: gcc -O2 -Wall -march=native -o dram_act_demo dram_act_demo.c
// run  : sudo ./dram_act_demo [iters] [stride_bytes] [buf_bytes]

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sched.h>

static inline void fence_all(void) {
    _mm_mfence(); // 序列化，稳妥起见
}

int main(int argc, char **argv) {
    size_t iters  = (argc > 1) ? strtoull(argv[1], NULL, 0) : (size_t)10ULL * 1000 * 1000;
    size_t stride = (argc > 2) ? strtoull(argv[2], NULL, 0) : (size_t)256 * 1024; // 先试 256 KiB
    size_t bufsz  = (argc > 3) ? strtoull(argv[3], NULL, 0) : (size_t)4 * 1024 * 1024; // 4 MiB

    // 绑核（可选）：减少调度噪声
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
    sched_setaffinity(0, sizeof(set), &set);

    // mmap 匿名内存，不初始化；建议页对齐，方便物理映射更整齐些
    uint8_t *buf = mmap(NULL, bufsz, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if (buf == MAP_FAILED) { perror("mmap"); return 1; }

    // 鎖常驻，避免换出
    if (mlock(buf, bufsz) != 0) { perror("mlock"); /* 失败也不致命 */ }

    // 两个地址：buf[0] 与 buf[stride]
    if (stride >= bufsz) {
        fprintf(stderr, "stride must be < buffer size\n");
        return 2;
    }
    volatile uint8_t *a = buf;
    volatile uint8_t *b = buf + stride;

    // 预热：把页建好（避免首次缺页影响统计）
    for (size_t i = 0; i < bufsz; i += 4096) buf[i] = (uint8_t)i;

    // 主循环：A/B 往返读取；每次都 clflushopt 强制走 DRAM
    for (size_t i = 0; i < iters; i++) {
        _mm_clflushopt((void*)a);
        fence_all();
        (void)*a;

        _mm_clflushopt((void*)b);
        fence_all();
        (void)*b;
    }

    // 保持进程存活片刻，便于观测外部计数器（可改/去掉）
    usleep(1000);

    munlock(buf, bufsz);
    munmap(buf, bufsz);
    return 0;
}
