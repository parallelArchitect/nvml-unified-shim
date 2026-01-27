/*
 * Basic NVML shim test
 * Compile: gcc -o test_basic test_basic.c -lnvidia-ml
 * Run with shim: LD_PRELOAD=../libnvml-unified.so ./test_basic
 */

#include <stdio.h>
#include <nvml.h>

int main() {
    nvmlReturn_t result;
    unsigned int device_count = 0;

    printf("═══════════════════════════════════════\n");
    printf("  NVML Shim Test\n");
    printf("═══════════════════════════════════════\n\n");

    // Initialize NVML
    printf("1. Initializing NVML...\n");
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        printf("   ✗ Failed to initialize NVML: %s\n", nvmlErrorString(result));
        return 1;
    }
    printf("   ✓ NVML initialized\n\n");

    // Get device count
    printf("2. Getting device count...\n");
    result = nvmlDeviceGetCount(&device_count);
    if (result != NVML_SUCCESS) {
        printf("   ✗ Failed to get device count: %s\n", nvmlErrorString(result));
        return 1;
    }
    printf("   ✓ Found %u device(s)\n\n", device_count);

    // Iterate through devices
    for (unsigned int i = 0; i < device_count; i++) {
        nvmlDevice_t device;
        char name[NVML_DEVICE_NAME_BUFFER_SIZE];
        nvmlMemory_t memory;

        printf("3. Device %u:\n", i);

        // Get device handle
        result = nvmlDeviceGetHandleByIndex(i, &device);
        if (result != NVML_SUCCESS) {
            printf("   ✗ Failed to get device handle: %s\n", nvmlErrorString(result));
            continue;
        }
        printf("   ✓ Got device handle\n");

        // Get device name
        result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
        if (result == NVML_SUCCESS) {
            printf("   ✓ Name: %s\n", name);
        } else {
            printf("   ✗ Failed to get name: %s\n", nvmlErrorString(result));
        }

        // Get memory info
        result = nvmlDeviceGetMemoryInfo(device, &memory);
        if (result == NVML_SUCCESS) {
            printf("   ✓ Memory Info:\n");
            printf("     Total:     %llu MB\n", memory.total / (1024*1024));
            printf("     Free:      %llu MB\n", memory.free / (1024*1024));
            printf("     Used:      %llu MB\n", memory.used / (1024*1024));
        } else {
            printf("   ✗ Failed to get memory info: %s\n", nvmlErrorString(result));
        }

        printf("\n");
    }

    // Shutdown
    printf("4. Shutting down NVML...\n");
    result = nvmlShutdown();
    if (result == NVML_SUCCESS) {
        printf("   ✓ NVML shutdown\n");
    }

    printf("\n═══════════════════════════════════════\n");
    printf("  ✓ All tests passed!\n");
    printf("═══════════════════════════════════════\n");

    return 0;
}
