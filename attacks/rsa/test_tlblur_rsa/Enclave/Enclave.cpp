#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>
#include <time.h>

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */

static inline void enclave_delay_1s(void)
{
    const uint64_t TARGET = 1ULL * 10000ULL * 8000ULL;   // 2 M 次循环 ≈ 1 s
    for (volatile uint64_t i = 0; i < TARGET; ++i)
        __asm__ volatile("pause");
}

int printf(const char* fmt, ...)
{
    char buf[BUFSIZ] = { '\0' };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return (int)strnlen(buf, BUFSIZ - 1) + 1;
}

/* ---------- 公共原型 ---------- */
long long multiply(long long x, long long y, long long mod);
long long square  (long long x,                 long long mod);

/* ---------- 工具宏：把函数锁进独立 4 KiB 页 ---------- */
#define PAGE_ALIGN        __attribute__((aligned(256)))
#define PAGE_SECTION(sec) __attribute__((section(sec)))

/* ---------- square：独占另一页 ---------- */

__attribute__((noinline, used
#if __GNUC__ >= 11
  , noipa
#endif
))

//long long PAGE_ALIGN PAGE_SECTION(".text.square_page")
long long
square(long long x, long long mod)
{
    //printf("ENTER square()\n");
    __uint128_t z = (__uint128_t)x * x % mod;
    return (long long)z;
}


/* ---------- multiply：独占一页 ---------- */
__attribute__((noinline, used
#if __GNUC__ >= 11
  , noipa
#endif
))

//long long PAGE_ALIGN PAGE_SECTION(".text.multiply_page")
long long PAGE_ALIGN PAGE_SECTION(".text.one_page")
multiply(long long x, long long y, long long mod)
{
    //128-bit 中间结果，防止 (x*y) 溢出；GCC/Clang 都支持
    //printf("ENTER multiply()\n");
    __uint128_t z = (__uint128_t)x * y % mod;
    return (long long)z;
}

long int rsa_n = 3501765933723641651L;   // p=2059688429, q=1700143519
long int rsa_e = 65537L;                 // 常用公开指数
//long int rsa_d = 3436365293708324969L;   // 62-bit
long int rsa_d = 20771;
static int ITER = 16;
//int rsa_d = 1;

int modpow(long long a, long long b, long long n)
{
    //ocall_print_string("ENTER modpow()\n");
    // volatile int zero = atoi("0");
    // malloc(0);
    char text[50];
    strncpy(text, "hello", 50);
    long long res = 1;
    uint16_t mask = 0x8000;

    uint64_t t0_ns = 0, t1_ns = 0;
    uint64_t dt_ns = 0;

    ocall_get_time_ns(&t0_ns);

    for (int i = ITER; i >= 0; i--)
    {
        //stop 1s
        //enclave_delay_1s();

        res = square(res, n);

        if (b & mask) {
            //__asm__ __volatile__("clflush (%0)" :: "r"(multiply));
            //__asm__ __volatile__("mfence" ::: "memory");
            //multiply(res, a, n);
            //__asm__ __volatile__("clflush (%0)" :: "r"(multiply));
            //__asm__ __volatile__("mfence" ::: "memory");
            res = multiply(res, a, n); 
        }
        mask = mask >> 1;
    }

    ocall_get_time_ns(&t1_ns);
    
    dt_ns = t1_ns - t0_ns;

    printf("Loop elapsed: %.3f ms, ITER=%d, per-iter=%.3f ns\n",
        dt_ns / 1e6, (int)ITER, (double)dt_ns / (double)ITER);
    
    return res;
}

int modpow_fast(long long a, long long b, long long n)
{
    char text[50];
    strncpy(text, "hello", 50);
    long long res = 1;
    uint16_t mask = 0x8000;

    for (int i=15; i >= 0; i--)
    {

        res = square(res, n);
        if (b & mask) {

            res = multiply(res, a, n); 
        }
        mask = mask >> 1;
    }
    return res;
}

void ecall_warmup() {
    
    int res = 90;

    modpow_fast(res ,rsa_d, rsa_n);

    printf("mutiply's VA = %p, square's VA = %p\n", multiply, square);
    printf("STOP for find m2m...\n");
    ocall_wait_for_user_input();
    
    //modpow(res ,rsa_d, rsa_n);

    return;
}

void ecall_allocate_and_loop_memory() {
    
    int res = 90;

    printf("mutiply's VA = %p, square's VA = %p\n", multiply, square);
    printf("STOP for VA->PA, Press 'c' to continue...\n");
    ocall_wait_for_user_input();

    modpow(res ,rsa_d, rsa_n);

    printf("ECALL: RSA completed.\n");
    
    return;
}
