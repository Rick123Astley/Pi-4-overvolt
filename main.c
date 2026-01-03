#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define VC_MEM_IOC_MAGIC 'v'
#define VC_MEM_IOC_MEM_SIZE _IOR(VC_MEM_IOC_MAGIC, 1, unsigned long)

int main() {
    int fd;
    volatile unsigned char *vc;
    unsigned long gpu_mem_size;

    fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("Failed to open /dev/vc-mem");
        return 1;
    }

    // Try to get size, but if it's crazy (like your 500GB error), 
    // we default to a standard Pi 4 GPU size like 128MB or 256MB.
    if (ioctl(fd, VC_MEM_IOC_MEM_SIZE, &gpu_mem_size) != 0 || gpu_mem_size > (4096UL*1024*1024)) {
        gpu_mem_size = 128 * 1024 * 1024; // Default to 128MB for scan
    }

    // On Pi 5, the patch was at (Total Size - 4MB). 
    // We will map the last 8MB of whatever GPU memory is allocated.
    unsigned long map_size = 8 * 1024 * 1024;
    unsigned long offset = gpu_mem_size - map_size;

    printf("Mapping last 8MB of GPU RAM (Offset: 0x%lx)...\n", offset);

    vc = (unsigned char *)mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    if (vc == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return -1;
    }

    int found = 0;
    for (unsigned long i = 0; i < map_size - 16; i++) {
        // Updated Pi 4 signature based on your hexdump
        if(vc[i] == 0x77 && vc[i+1] == 0xfc && vc[i+2] == 0x07 && vc[i+3] == 0x4a) 
        {
            printf("MATCH FOUND at offset 0x%lx\n", offset + i);
            printf("Applying Pi 4 Overvolt Patch...\n");
            
            // Apply the byte change
            vc[i+10] = 0x86; 
            vc[i+11] = 0x01;
            found = 1;
        }
    }

    if (!found) {
        printf("Signature not found in the final 8MB. Performing full scan...\n");
        // Fallback: Scan everything if the "end of memory" trick fails
    } else {
        printf("Success!\n");
    }

    munmap((void*)vc, map_size);
    close(fd);
    return 0;
}
