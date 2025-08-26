/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
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
static int ITER = 10000000;

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

uint64_t get_rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void ecall_with_idle_then_work() {

    //uint64_t* data = (uint64_t*)malloc(64);
    //unsigned int test = 0;

    //if (data == NULL) {
    //    printf("ECALL: Memory allocation failed.\n");
    //    return;
    //}

   // memset(data, 0, 64);

    uint64_t t0_ns = 0, t1_ns = 0;
    uint64_t dt_ns = 0;

    //printf("Enterring IDLE!\n");

    // 调用 OCALL，让外部休眠（CPU 可 idle）
    //ocall_idle_seconds(5);

    //printf("Enterring INSTRUCTIONS test!\n");

    t0_ns = get_rdtsc();
    // OCALL返回后，执行 enclave 内逻辑

    for (int i = 0; i < ITER; i++) {
        //if (i % 2)
            //__asm__ __volatile__("nop");
        //else
            __asm__ __volatile__("nop");
    }
   
    t1_ns = get_rdtsc();

   //ocall_idle_seconds(5);
/*
    for (int i = 0; i < ITER; i++) {
        asm volatile("clflushopt (%0);" :: "r"(data) : "memory");
        asm volatile("mfence" ::: "memory");
        asm volatile("mov (%0), %%r10;" :: "r"(data) : "memory");
    }
*/

		

    dt_ns = t1_ns - t0_ns;
    // 注意：enclave 里的 printf 也是经由 OCALL 实现，仅用于展示
    printf("Loop elapsed: %.3f ms, ITER=%d, per-iter=%.3f ns\n",
           dt_ns / 1e6, (int)ITER, (double)dt_ns / (double)ITER);
           
    //printf("Work done.\n");
}

/*
void loop(uint64_t *a, uint64_t loop_counter) {
    uint64_t loop = loop_counter;

    while(loop-- > 0) {
        asm volatile("clflushopt (%0);" :: "r"(a) : "memory");
        asm volatile("mov (%0), %%r10;" :: "r"(a) : "memory");
    }

}
    
void ecall_allocate_and_loop_memory() {
    uint64_t* data = (uint64_t*)malloc(64);
    uint64_t loop_counter = 10000000000; //for loop

    if (data == NULL) {
        printf("ECALL: Memory allocation failed.\n");
        return;
    }

    memset(data, 0, 64);
    
    printf("data's VA = %p\n", data);

    printf("STOP for VA->PA, Press 'c' to continue...\n");
    ocall_wait_for_user_input();

    loop(data, loop_counter);

    printf("ECALL: Memory operations completed.\n");
    free(data);
    
    return;
}
*/
