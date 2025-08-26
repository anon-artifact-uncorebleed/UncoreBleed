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

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include <stdarg.h>
#include <stdio.h> /* vsnprintf */
#include <string.h>
#include <time.h>

/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
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


void load(uint64_t *a, uint64_t reads) {

    int cur_reads = reads;

    while(cur_reads-- > 0) {
        asm volatile("mov (%0), %%r10;" :: "r"(a) : "memory");
	asm volatile("clflushopt (%0);" :: "r"(a) : "memory");
    }

}

/*
void ecall_allocate_and_loop_memory() {
    
    printf("ECALL: Starting memory allocation.\n");
    uint64_t* buffer = (uint64_t*)malloc(64);
    if (buffer == NULL) {
        printf("ECALL: Memory allocation failed.\n");
        return;
    }

    printf("ECALL: Memory allocated.\n");
    memset(buffer, 0, 64);
    printf("ECALL: Memory initialized.\n");

    for (int i = 0; i < 1000; i++) {
        //buffer[i % 512] ^= 0xDEADBEEF;
    }
    printf("ECALL: Memory operations completed.\n");

    free(buffer);
}
*/
uint64_t buffer1[256];
uint64_t allocate_and_loop_memory() {
    
    printf("ECALL: Starting memory allocation.\n");
    
    uint64_t* buffer2 = (uint64_t*)malloc(4096);
    
    if (buffer2 == NULL) {
        printf("ECALL: Memory allocation failed.\n");
        return -1;
    }

    memset(buffer1, 0, 4096);
    memset(buffer2, 0, 4096);
    
    printf("ECALL: Memory initialized.\n");
    
    int cur_reads = 1000000;

    printf("Entering Sleep().\n");

    for (int i = 0; i < 1500000000; i++)
	asm volatile("nop");

    printf("Leaving Sleep() and Entering load(buffer1)\n");
    for (int i = 0; i < 100; i++){
	load(buffer1,1000000);
    }
    
    printf("Entering load(buffer2)\n");
    for (int i = 0; i < 100; i++){
	load(buffer2,1000000);
    }

    printf("Entering Sleep().\n");
    for (int i = 0; i < 500000000; i++)
        asm volatile("nop");


    printf("ECALL: Memory operations completed.\n");

    free(buffer1);
    free(buffer2);

    return 0;
}
