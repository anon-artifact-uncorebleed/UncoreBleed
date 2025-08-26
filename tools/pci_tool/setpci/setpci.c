#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void set_pci_register(const char *device, uint64_t physical_address) {
    uint32_t hi_addr = (uint32_t)(physical_address >> 32); // 高 14 位地址
    uint32_t lo_addr = (uint32_t)(physical_address & 0xFFFFFFC0); // 低 26 位地址，保证是64字节对齐

    char cmd[256];

    snprintf(cmd, sizeof(cmd), "setpci -s %s 0x494.L=%x", device, hi_addr);
    printf("Running command: %s\n", cmd);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "setpci -s %s 0x490.L=%x", device, lo_addr);
    printf("Running command: %s\n", cmd);
    system(cmd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device> <physical_address>\n", argv[0]);
        return 1;
    }

    char *device = argv[1];
    uint64_t physical_address;

    // 将命令行输入的物理地址字符串转换为数字
    if (sscanf(argv[2], "%lx", &physical_address) != 1) {
        fprintf(stderr, "Invalid physical address format.\n");
        return 1;
    }

    // 调用设置函数
    set_pci_register(device, physical_address);

    return 0;
}

