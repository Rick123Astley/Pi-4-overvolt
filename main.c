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
    
    // We will manually define the scan size to 128MB 
    // This avoids the "500GB" error you saw.
    unsigned long mem_size = 128 * 1024 * 1024; 
    
    fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("Error opening /dev/vc-mem");
        return 1;
    }

    printf("Attempting to map 128MB of VC memory...\n");

    // Map 128MB starting from the beginning of the VC region
    vc = (unsigned char *)mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (vc == MAP_FAILED) {
        printf("mmap failed: %s (Try reducing mem_size to 64MB)\n", strerror(errno));
        close(fd);
        return -1;
    }

    printf("Scanning... please wait.\n");

    int found = 0;
    for (unsigned long i = 0; i < mem_size - 12; i++) {
        // Still searching for the same voltage limit signature
        if(vc[i] == 0xA6 && vc[i+1] == 0x4a && vc[i+2] == 0x07 && vc[i+3] == 0xc0 && 
           vc[i+4] == 0x06 && vc[i+5] == 0x31 && vc[i+6] == 0x07) 
        {
            printf("MATCH FOUND at offset: 0x%lx\n", i);
            printf("Patching bytes at 0x%lx and 0x%lx\n", i+8, i+9);
            vc[i+8] = 0x86; 
            vc[i+9] = 0x01;
            found = 1;
        }
    }

    if (!found) {
        printf("Signature NOT found in the first 128MB.\n");
    } else {
        printf("Patch applied successfully!\n");
    }

    munmap((void*)vc, mem_size);
    close(fd);
    return 0;
}
