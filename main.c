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
#define VC_MEM_IOC_MEM_PHYS_ADDR _IOR(VC_MEM_IOC_MAGIC, 0, unsigned long)
#define VC_MEM_IOC_MEM_SIZE      _IOR(VC_MEM_IOC_MAGIC, 1, unsigned long)

int main() {
    int fd;
    volatile unsigned char *vc;
    unsigned long phys_addr, mem_size;
    
    fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("Error: Ensure you are root and vc-mem is enabled");
        return 1;
    }

    // Get the actual boundaries of the GPU memory on THIS Pi
    ioctl(fd, VC_MEM_IOC_MEM_PHYS_ADDR, &phys_addr);
    ioctl(fd, VC_MEM_IOC_MEM_SIZE, &mem_size);

    printf("VC Phys Addr: 0x%lx\n", phys_addr);
    printf("VC Total Size: %ld MB\n", mem_size / (1024 * 1024));

    // Map the ENTIRE VC memory range to scan it
    vc = (unsigned char *)mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (vc == MAP_FAILED) {
        printf("mmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    printf("Scanning memory for overvolt lock signature...\n");

    int found = 0;
    // We scan the whole range (mem_size) minus signature length
    for (unsigned long i = 0; i < mem_size - 12; i++) {
        // This is your Pi 5 signature - we search for it on the Pi 4
        if(vc[i] == 0xA6 && vc[i+1] == 0x4a && vc[i+2] == 0x07 && vc[i+3] == 0xc0 && 
           vc[i+4] == 0x06 && vc[i+5] == 0x31 && vc[i+6] == 0x07) 
        {
            printf("MATCH FOUND at offset: 0x%lx (Physical: 0x%lx)\n", i, phys_addr + i);
            
            // Apply the patch
            printf("Patching bytes at 0x%lx and 0x%lx\n", i+8, i+9);
            vc[i+8] = 0x86; 
            vc[i+9] = 0x01;
            found = 1;
            // Depending on firmware, there might be multiple checks. 
            // Continue scanning or 'break' if you only expect one.
        }
    }

    if (!found) {
        printf("Signature NOT found. The Pi 4 firmware uses different instructions for voltage limits.\n");
    } else {
        printf("Patch applied successfully. Reboot may be required if the firmware reloads on boot.\n");
    }

    munmap((void*)vc, mem_size);
    close(fd);
    return 0;
}
