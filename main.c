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

// Mirroring the Pi 5's use of VC_MEM IOCTLs
#define VC_MEM_IOC_MAGIC 'v'
#define VC_MEM_IOC_MEM_SIZE _IOR(VC_MEM_IOC_MAGIC, 1, unsigned long)

int main() {
    int fd;
    volatile unsigned char *vc;
    unsigned long size;
    
    // 1. Open the same device as the Pi 5 code
    fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("unable to open /dev/vc-mem");
        return 1;
    }

    // 2. Get the GPU memory size to calculate the offset
    ioctl(fd, VC_MEM_IOC_MEM_SIZE, &size);

    // On Pi 5, the offset was roughly at the end of RAM.
    // On Pi 4, the firmware usually sits in the top few MBs of the GPU cutout.
    // We map a 4MB window at the very top of the reported VC memory.
    unsigned long my_size = (1024 * 1024 * 4); 
    unsigned long offset = size - my_size;

    printf("Mapping 4MB window at top of VC-Mem (Offset: 0x%lx)\n", offset);

    vc = (unsigned char *)mmap(0, my_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, offset);
    
    if (vc == (unsigned char *)-1) {
        printf("mmap failed: %s\n", strerror(errno));
        return -1;
    }

    // 3. Search and Patch using the Pi 4 (VC6) instruction set
    // We are looking for the clamp logic: cmp r6, 6 | mov.cs r7, r6 | mov.cc r7, 6
    for (int i = 0; i < my_size - 12; i++) {
        // This is the VC6 equivalent of your Pi 5's "A6 4A..." signature
        // It matches the '77 fc 07 4a' we found in your start4.elf dump
        if(vc[i] == 0x77 && vc[i+1] == 0xfc && vc[i+2] == 0x07 && vc[i+3] == 0x4a) 
        {
            printf("Found Overvolt Limit Signature! Replacing...\n");
            
            // The Patch:
            // We jump to the 'mov.cc r7, 6' instruction and change it to 'mov.cc r7, r6'
            // This mirrors your Pi 5 patch: 8A 51 -> 86 01 (for VC6 registers)
            vc[i+10] = 0x86; 
            vc[i+11] = 0x01;
            printf("Patch applied at address %p\n", &vc[i+10]);
        }
    }

    close(fd);
    printf("Done.\n");
    return 0;
}
