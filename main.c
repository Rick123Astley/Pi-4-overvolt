#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

// We will scan the first 512MB of physical RAM. 
// This is where the Pi 4 firmware and hardware control structures reside.
#define SCAN_SIZE (512 * 1024 * 1024) 
#define BLOCK_SIZE (1 * 1024 * 1024) // 1MB chunks to be safe

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("Error opening /dev/mem (Are you root? Is iomem=relaxed set?)");
        return 1;
    }

    printf("Starting Global System RAM scan for CPU voltage lock...\n");

    for (uint64_t offset = 0; offset < SCAN_SIZE; offset += BLOCK_SIZE) {
        unsigned char *map_base = mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
        
        if (map_base == MAP_FAILED) {
            continue; // Skip areas the kernel won't let us touch
        }

        for (uint32_t i = 0; i < BLOCK_SIZE - 12; i++) {
            // Searching for the logic: cmp rX, 6 | mov.cs rY, rX | mov.cc rY, 6
            // Based on your previous dump, we use the VC6 instruction anchors:
            if (map_base[i] == 0x07 && map_base[i+1] == 0x4a && 
                map_base[i+4] == 0x07 && map_base[i+5] == 0xc0) {
                
                printf("MATCH FOUND at Physical Address: 0x%llx\n", (long long)(offset + i));
                
                // Check if this is the specific 'over_voltage' clamp
                // We overwrite the limit to allow the CPU to pull more voltage
                printf("Patching CPU Voltage limit...\n");
                map_base[i+10] = 0x86; 
                map_base[i+11] = 0x01;
            }
        }
        munmap(map_base, BLOCK_SIZE);
    }

    printf("Scan complete.\n");
    close(fd);
    return 0;
}
