#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

int main() {
    int fd;
    volatile unsigned char *vc;
    // 128MB is a safe window for the start4.elf firmware area
    unsigned long mem_size = 128 * 1024 * 1024; 
    
    fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("Error: Run as sudo");
        return 1;
    }

    vc = (unsigned char *)mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (vc == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return -1;
    }

    printf("Scanning Pi 4 memory for overvolt limit...\n");

    int found = 0;
    for (unsigned long i = 0; i < mem_size - 16; i++) {
        /* This is a common Pi 4 (BCM2711) pattern for:
           cmp r6, 6      (A6 42)
           mov.cs r7, r6  (07 C0 06 31)
           mov.cc r7, 6   (07 C0 86 51)
        */
        if(vc[i] == 0xA6 && vc[i+1] == 0x42 && vc[i+2] == 0x07 && vc[i+3] == 0xC0 &&
           vc[i+4] == 0x06 && vc[i+5] == 0x31) 
        {
            printf("MATCH FOUND at offset: 0x%lx\n", i);
            
            // The Patch: 
            // We change the 'limit' value (6) to something much higher, like 16 (0x10)
            // Or we change the logic to always take the user's value.
            // Replacing 86 51 (limit to 6) with 86 01 (take the input register)
            printf("Applying Pi 4 Bypass...\n");
            vc[i+8] = 0x86; 
            vc[i+9] = 0x01; 
            found = 1;
        }
    }

    if (!found) {
        printf("Standard signature not found. Trying alternative scan...\n");
        // Alternative: Search for the hex bytes specifically associated with 'over_voltage' limit checks
        for (unsigned long i = 0; i < mem_size - 8; i++) {
            if (vc[i] == 0x07 && vc[i+1] == 0xC0 && vc[i+2] == 0x8A && vc[i+3] == 0x51) {
                 printf("Found potential limit check at 0x%lx. Byte signature: %02x %02x\n", i, vc[i+2], vc[i+3]);
            }
        }
    } else {
        printf("Done! Overvolt limit should be lifted.\n");
    }

    munmap((void*)vc, mem_size);
    close(fd);
    return 0;
}
