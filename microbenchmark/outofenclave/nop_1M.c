#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef void(*fn_t)(void);

int main(void) {
    const size_t N = 10000000;             // NOP 次数
    const size_t SZ = N + 1;              // NOP + ret

    // 申请一块可执行内存（RWX，简便起见；更严格可以先 RW 再 mprotect 成 RX）
    uint8_t* p = mmap(NULL, SZ, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // 填充 1,000,000 个 NOP (0x90)
    for (size_t i = 0; i < N; i++) p[i] = 0x90;

    // 在末尾加一个 ret (0xC3)，否则 CPU 会继续跑到未定义区域
    p[N] = 0xC3;

    // 清理 I-cache，确保 CPU 能执行最新指令
    __builtin___clear_cache((char*)p, (char*)p + SZ);

    // 转换为函数指针并调用
    ((fn_t)p)();

    return 0;
}

