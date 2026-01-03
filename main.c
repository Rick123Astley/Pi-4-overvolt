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

#define VC_MEM_IOC_MAGIC 'v'
#define VC_MEM_IOC_MEM_SIZE _IOR(VC_MEM_IOC_MAGIC, 1, unsigned long)

int main() {
    int fd;
    volatile unsigned char *vc;
    unsigned long total_gpu_mem;
    long page_size = sysconf(_SC_PAGESIZE);
    
    fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("unable to open /dev/vc-mem");
        return 1;
    }

    // Get the size from the driver
    if (ioctl(fd, VC_MEM_IOC_MEM_SIZE, &total_gpu_mem) != 0) {
        perror("ioctl failed");
        return 1;
    }

    // MAP LOGIC: 
    // We want a 4MB window. 
    // We must ensure 'offset' is a multiple of page_size (usually 4096).
    unsigned long map_size = (1024 * 1024 * 4); 
    
    // Calculate offset and round it DOWN to the nearest page boundary
    unsigned long raw_offset = total_gpu_mem - map_size;
    unsigned long aligned_offset = raw_offset & ~(page_size - 1);

    printf("Page Size: %ld\n", page_size);
    printf("Total GPU Mem: 0x%lx\n", total_gpu_mem);
    printf("Aligned Offset: 0x%lx\n", aligned_offset);

    vc = (unsigned char *)mmap(0, map_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, aligned_offset);
    
    if (vc == MAP_FAILED) {
        printf("mmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    // 3. Search and Patch (VC6 instructions found in your hexdump)
    int found = 0;
    for (int i = 0; i < map_size - 12; i++) {
        // Your specific start4.elf pattern: 77 fc 07 4a
        if(vc[i] == 0x77 && vc[i+1] == 0xfc && vc[i+2] == 0x07 && vc[i+3] == 0x4a) 
        {
            printf("Found Overvolt Limit Signature at index 0x%x! Replacing...\n", i);
            
            // Mirroring the Pi 5 logic (8A 51 -> 86 01 for Pi 4 registers)
            vc[i+10] = 0x86; 
            vc[i+11] = 0x01;
            found = 1;
        }
    }

    if(!found) printf("Signature not found in the mapped window.\n");

    munmap((void*)vc, map_size);
    close(fd);
    return 0;
}
