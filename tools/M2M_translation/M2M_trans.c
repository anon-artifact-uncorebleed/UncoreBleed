// m2m_from_phys.c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>

static inline int bit(uint64_t x, int b) { return (int)((x >> b) & 1ULL); }

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <phys_addr>\n"
                        "  phys_addr: decimal or hex (e.g. 123456 or 0x1fc0abcd)\n",
                        argv[0]);
        return 1;
    }

    errno = 0;
    char *endp = NULL;
    uint64_t phys = strtoull(argv[1], &endp, 0); // base 0: auto-detect 0x
    if (errno != 0 || endp == argv[1] || *endp != '\0') {
        perror("Invalid phys_addr");
        return 2;
    }

    int b8  = bit(phys, 8);
    int b11 = bit(phys, 11);
    int b14 = bit(phys, 14);
    int b17 = bit(phys, 17);
    int b22 = bit(phys, 22);
    int b25 = bit(phys, 25);

    int id0 = b8 ^ b14 ^ b22;   // M2M_id[0]
    int id1 = b11 ^ b17 ^ b25;  // M2M_id[1]
    int m2m = (id1 << 1) | id0; // 0..3

    printf("phys = 0x%016" PRIx64 " (%" PRIu64 ")\n", phys, phys);
    printf("b8=%d b11=%d b14=%d b17=%d b22=%d b25=%d\n",
           b8, b11, b14, b17, b22, b25);
    printf("M2M_id[0] = b8 ^ b14 ^ b22 = %d\n", id0);
    printf("M2M_id[1] = b11 ^ b17 ^ b25 = %d\n", id1);
    printf("M2M = %d\n", m2m);

    return 0;
}

