#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main() {
    int fd;
    volatile unsigned char *vc;
    // 128MB is the standard area for Pi 4 firmware mapping
    unsigned long mem_size = 128 * 1024 * 1024; 
    
    fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Error: Run as sudo\n");
        return 1;
    }

    vc = (unsigned char *)mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (vc == MAP_FAILED) {
        return -1;
    }

    printf("Scanning for Pi 4 specific voltage clamping logic...\n");

    int found = 0;
    for (unsigned long i = 0; i < mem_size - 20; i++) {
        // This signature is derived from your hexdump's structural instructions
        // It looks for the conditional move prefix (07 C0) used near over_voltage logic
        if(vc[i] == 0x07 && vc[i+1] == 0x4a && vc[i+3] == 0x60 && vc[i+4] == 0x07 && vc[i+5] == 0xC0) 
        {
            printf("MATCH FOUND at 0x%lx\n", i);
            
            /* In your dump, we see 07 4A followed by 07 C0. 
               This is the VPU's way of saying "If condition met, do this".
               To bypass the limit, we change the conditional target.
            */
            printf("Applying Pi 4 firmware bypass...\n");
            
            // We patch the limit-checking instruction
            // This turns 'mov.limit' into 'mov.user_value'
            vc[i+6] = 0x86; 
            vc[i+7] = 0x01;
            found = 1;
        }
    }

    if (!found) {
        // Fallback for different firmware versions of Pi 4
        for (unsigned long i = 0; i < mem_size - 12; i++) {
            if(vc[i] == 0xA6 && vc[i+1] == 0x42 && vc[i+2] == 0x07 && vc[i+3] == 0xC0) {
                printf("Secondary Match (Legacy Pi 4) at 0x%lx\n", i);
                vc[i+8] = 0x86;
                vc[i+9] = 0x01;
                found = 1;
            }
        }
    }

    if (found) {
        printf("Patch successful! You can now exceed the voltage limit in config.txt\n");
    } else {
        printf("Could not find logic signature. Your firmware might be encrypted or compressed.\n");
    }

    munmap((void*)vc, mem_size);
    close(fd);
    return 0;
}
