#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static int ITER = 5000000;

static inline uint64_t now_ns(void) {
    struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

int main(void) {
    //uint64_t* data = (uint64_t*)malloc(64);

    //if (data == NULL) {
    //    printf("ECALL: Memory allocation failed.\n");
    //    return -1;
    //}

    //memset(data, 0, 64);


    //printf("[User] Sleeping for 1 seconds...\n");
    //fflush(stdout);
    sleep(1);  // 期间可让 CPU idle

    //printf("[User] Starting work loop (NOPs)...\n");
    //fflush(stdout);

    //uint64_t t0 = now_ns();
    for (int i = 0; i < ITER; i++) {
        if (i % 2);
            //__asm__ __volatile__("nop");
        else;
            //__asm__ __volatile__("nop");
    }

    /*
    printf("[User] Starting work loop (memory access)...\n");
    fflush(stdout);

    for (int i = 0; i < ITER; i++) {
        asm volatile("clflushopt (%0);" :: "r"(data) : "memory");
        asm volatile("mfence" ::: "memory");
        asm volatile("mov (%0), %%r10;" :: "r"(data) : "memory");
    }
    */
    //uint64_t t1 = now_ns();

    //uint64_t dt = t1 - t0;
    //printf("[User] Loop elapsed: %.3f ms (ITER=%d, %.3f ns/iter)\n",
    //       dt / 1e6, ITER, (double)dt / (double)ITER);
    //printf("[User] Work done.\n");
    return 0;
}
