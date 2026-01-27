/*
 * NVML Unified Memory Library Replacement for Grace Blackwell GB10
 *
 * Complete standalone NVML implementation using CUDA runtime.
 * Works with Python ctypes and all applications expecting libnvidia-ml.so.1
 *
 * Author: TISMJedi Homelab
 * License: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cuda_runtime.h>

/* NVML Types and Constants */
#define NVML_SUCCESS 0
#define NVML_ERROR_NOT_SUPPORTED 3
#define NVML_ERROR_UNINITIALIZED 1
#define NVML_ERROR_INVALID_ARGUMENT 2
#define NVML_ERROR_NOT_FOUND 6
#define NVML_DEVICE_NAME_BUFFER_SIZE 64
#define NVML_DEVICE_UUID_BUFFER_SIZE 80
#define NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE 16

typedef void* nvmlDevice_t;
typedef int nvmlReturn_t;

typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

typedef struct {
    unsigned int version;
    unsigned long long total;
    unsigned long long reserved;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_v2_t;

typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

typedef struct {
    char busId[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE];
    unsigned int domain;
    unsigned int bus;
    unsigned int device;
    unsigned int pciDeviceId;
    unsigned int pciSubSystemId;
} nvmlPciInfo_t;

typedef struct {
    unsigned int rate;
    unsigned int gen;
    unsigned int width;
} nvmlPcieLinkState_t;

/* State tracking */
static int nvml_initialized = 0;
static int device_count = 0;

/* Forward declarations */
nvmlReturn_t nvmlInit_v2(void);
nvmlReturn_t nvmlInitWithFlags(unsigned int flags);
nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int *deviceCount);
nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index, nvmlDevice_t *device);
nvmlReturn_t nvmlDeviceGetPciInfo_v3(nvmlDevice_t device, nvmlPciInfo_t *pci);
nvmlReturn_t nvmlSystemGetNVMLVersion(char *version, unsigned int length);

#define LOG_DEBUG(fmt, ...) \
    if (getenv("NVML_SHIM_DEBUG")) { \
        fprintf(stderr, "[NVML-UNIFIED] " fmt "\n", ##__VA_ARGS__); \
    }

/* Helper: Read system memory from /proc/meminfo */
static unsigned long long get_system_memory_total() {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;

    char line[256];
    unsigned long long mem_total = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %llu kB", &mem_total) == 1) {
            break;
        }
    }
    fclose(f);

    return mem_total * 1024; // Convert to bytes
}

static unsigned long long get_system_memory_available() {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;

    char line[256];
    unsigned long long mem_available = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemAvailable: %llu kB", &mem_available) == 1) {
            break;
        }
    }
    fclose(f);

    return mem_available * 1024; // Convert to bytes
}

/* Helper: Get GPU memory usage via CUDA runtime */
static unsigned long long get_cuda_memory_used(int device_index) {
    size_t free_mem, total_mem;
    cudaError_t err;

    int cuda_device_count;
    err = cudaGetDeviceCount(&cuda_device_count);
    if (err != cudaSuccess || device_index >= cuda_device_count) {
        return 0;
    }

    int current_device;
    cudaGetDevice(&current_device);

    err = cudaSetDevice(device_index);
    if (err != cudaSuccess) {
        return 0;
    }

    err = cudaMemGetInfo(&free_mem, &total_mem);
    if (err != cudaSuccess) {
        cudaSetDevice(current_device); // Restore
        return 0;
    }

    cudaSetDevice(current_device); // Restore
    return total_mem - free_mem;
}

/*
 * Core NVML API Implementation
 */

nvmlReturn_t nvmlInit(void) {
    return nvmlInit_v2();
}

nvmlReturn_t nvmlInit_v2(void) {
    LOG_DEBUG("nvmlInit_v2() called");

    if (nvml_initialized) {
        LOG_DEBUG("Already initialized");
        return NVML_SUCCESS;
    }

    // Initialize CUDA runtime
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess) {
        LOG_DEBUG("CUDA initialization failed: %s", cudaGetErrorString(err));
        return NVML_ERROR_UNINITIALIZED;
    }

    nvml_initialized = 1;
    LOG_DEBUG("Initialized with %d CUDA device(s)", device_count);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlShutdown(void) {
    LOG_DEBUG("nvmlShutdown() called");

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    // CUDA cleanup happens automatically
    nvml_initialized = 0;
    device_count = 0;

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlSystemGetDriverVersion(char *version, unsigned int length) {
    LOG_DEBUG("nvmlSystemGetDriverVersion() called");

    if (!version || length == 0) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    // Return driver version matching nvidia-smi version
    // This MUST match nvmlSystemGetNVMLVersion for nvidia-smi compatibility
    snprintf(version, length, "580.126.09");

    LOG_DEBUG("Driver version: %s", version);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlSystemGetCudaDriverVersion(int *cudaDriverVersion) {
    LOG_DEBUG("nvmlSystemGetCudaDriverVersion() called");

    if (!cudaDriverVersion) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    cudaError_t err = cudaDriverGetVersion(cudaDriverVersion);

    if (err != cudaSuccess) {
        LOG_DEBUG("cudaDriverGetVersion failed: %s", cudaGetErrorString(err));
        return NVML_ERROR_UNINITIALIZED;
    }

    LOG_DEBUG("CUDA driver version: %d", *cudaDriverVersion);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlSystemGetCudaDriverVersion_v2(int *cudaDriverVersion) {
    return nvmlSystemGetCudaDriverVersion(cudaDriverVersion);
}

nvmlReturn_t nvmlDeviceGetCount(unsigned int *deviceCount) {
    return nvmlDeviceGetCount_v2(deviceCount);
}

nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int *deviceCount) {
    LOG_DEBUG("nvmlDeviceGetCount_v2() called");

    if (!deviceCount) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        // Auto-initialize if not done yet
        nvmlReturn_t ret = nvmlInit_v2();
        if (ret != NVML_SUCCESS) {
            return ret;
        }
    }

    *deviceCount = (unsigned int)device_count;
    LOG_DEBUG("Returning %d device(s)", device_count);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t *device) {
    return nvmlDeviceGetHandleByIndex_v2(index, device);
}

nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index, nvmlDevice_t *device) {
    LOG_DEBUG("nvmlDeviceGetHandleByIndex_v2(%u) called", index);

    if (!device) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    if (index >= (unsigned int)device_count) {
        LOG_DEBUG("Invalid index %u (max %d)", index, device_count - 1);
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    // Use index+1 as handle (avoid NULL pointer)
    *device = (nvmlDevice_t)(unsigned long)(index + 1);
    LOG_DEBUG("Created handle for device %u", index);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t *memory) {
    LOG_DEBUG("nvmlDeviceGetMemoryInfo() called");

    if (!device || !memory) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    // Get unified memory stats
    unsigned long long total = get_system_memory_total();
    unsigned long long available = get_system_memory_available();
    unsigned long long used = get_cuda_memory_used(device_index);

    memory->total = total;
    memory->used = used;
    memory->free = available;

    LOG_DEBUG("Memory: total=%llu MB, used=%llu MB, free=%llu MB",
              total / (1024*1024), used / (1024*1024), available / (1024*1024));

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(nvmlDevice_t device, nvmlMemory_v2_t *memory) {
    LOG_DEBUG("nvmlDeviceGetMemoryInfo_v2() called");

    if (!device || !memory) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    // Version field is input: caller specifies struct version
    // MAX uses: sizeof(struct) | (2 << 24)
    // We just need to accept version field as-is, don't validate it strictly
    unsigned int input_version = memory->version;
    LOG_DEBUG("Input version: 0x%x", input_version);

    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    unsigned long long total = get_system_memory_total();
    unsigned long long available = get_system_memory_available();
    unsigned long long used = get_cuda_memory_used(device_index);

    // Preserve version field (it's input, not output)
    memory->version = input_version;
    memory->total = total;
    memory->used = used;
    memory->free = available;
    memory->reserved = 0;

    LOG_DEBUG("Memory v2: version=0x%x, total=%llu MB, used=%llu MB, free=%llu MB",
              memory->version, total / (1024*1024), used / (1024*1024), available / (1024*1024));

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char *name, unsigned int length) {
    LOG_DEBUG("nvmlDeviceGetName() called");

    if (!device || !name || length == 0) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    struct cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_index);

    if (err != cudaSuccess) {
        LOG_DEBUG("cudaGetDeviceProperties failed: %s", cudaGetErrorString(err));
        return NVML_ERROR_NOT_FOUND;
    }

    snprintf(name, length, "%s", prop.name);
    LOG_DEBUG("Device name: %s", name);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetUUID(nvmlDevice_t device, char *uuid, unsigned int length) {
    LOG_DEBUG("nvmlDeviceGetUUID() called");

    if (!device || !uuid || length < NVML_DEVICE_UUID_BUFFER_SIZE) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    // Get CUDA UUID
    struct cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_index);

    if (err != cudaSuccess) {
        return NVML_ERROR_NOT_FOUND;
    }

    // Format UUID from CUDA device UUID
    cudaUUID_t cuda_uuid = prop.uuid;
    snprintf(uuid, length, "GPU-%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (unsigned char)cuda_uuid.bytes[0], (unsigned char)cuda_uuid.bytes[1],
        (unsigned char)cuda_uuid.bytes[2], (unsigned char)cuda_uuid.bytes[3],
        (unsigned char)cuda_uuid.bytes[4], (unsigned char)cuda_uuid.bytes[5],
        (unsigned char)cuda_uuid.bytes[6], (unsigned char)cuda_uuid.bytes[7],
        (unsigned char)cuda_uuid.bytes[8], (unsigned char)cuda_uuid.bytes[9],
        (unsigned char)cuda_uuid.bytes[10], (unsigned char)cuda_uuid.bytes[11],
        (unsigned char)cuda_uuid.bytes[12], (unsigned char)cuda_uuid.bytes[13],
        (unsigned char)cuda_uuid.bytes[14], (unsigned char)cuda_uuid.bytes[15]);

    LOG_DEBUG("UUID: %s", uuid);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetUtilizationRates(nvmlDevice_t device, nvmlUtilization_t *utilization) {
    LOG_DEBUG("nvmlDeviceGetUtilizationRates() called");

    if (!device || !utilization) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    // For unified memory architecture, we don't have separate GPU utilization
    // Return 0 for now (would need to query from CUDA or other source)
    utilization->gpu = 0;
    utilization->memory = 0;

    LOG_DEBUG("Utilization: gpu=0%%, memory=0%% (not available on unified memory)");

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetCudaComputeCapability(nvmlDevice_t device, int *major, int *minor) {
    LOG_DEBUG("nvmlDeviceGetCudaComputeCapability() called");

    if (!device || !major || !minor) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    struct cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_index);

    if (err != cudaSuccess) {
        LOG_DEBUG("cudaGetDeviceProperties failed: %s", cudaGetErrorString(err));
        return NVML_ERROR_NOT_FOUND;
    }

    *major = prop.major;
    *minor = prop.minor;

    LOG_DEBUG("Compute capability: %d.%d", *major, *minor);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetPciInfo(nvmlDevice_t device, nvmlPciInfo_t *pci) {
    return nvmlDeviceGetPciInfo_v3(device, pci);
}

nvmlReturn_t nvmlDeviceGetPciInfo_v3(nvmlDevice_t device, nvmlPciInfo_t *pci) {
    LOG_DEBUG("nvmlDeviceGetPciInfo_v3() called");

    if (!device || !pci) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    struct cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_index);

    if (err != cudaSuccess) {
        return NVML_ERROR_NOT_FOUND;
    }

    // Fill PCI info from CUDA properties
    snprintf(pci->busId, NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE, "%04x:%02x:%02x.0",
             prop.pciDomainID, prop.pciBusID, prop.pciDeviceID);
    pci->domain = prop.pciDomainID;
    pci->bus = prop.pciBusID;
    pci->device = prop.pciDeviceID;
    pci->pciDeviceId = 0; // Not available from CUDA
    pci->pciSubSystemId = 0;

    LOG_DEBUG("PCI: %s (domain=%u, bus=%u, device=%u)",
              pci->busId, pci->domain, pci->bus, pci->device);

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlInitWithFlags(unsigned int flags) {
    LOG_DEBUG("nvmlInitWithFlags(flags=%u) called", flags);
    // Flags are typically for debugging/profiling, ignore for now
    return nvmlInit_v2();
}

nvmlReturn_t nvmlSystemGetNVMLVersion(char *version, unsigned int length) {
    LOG_DEBUG("nvmlSystemGetNVMLVersion() called");

    if (!version || length == 0) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    // Report NVML version matching the system driver version
    // nvidia-smi expects this to match exactly with the driver
    snprintf(version, length, "580.126.09");

    LOG_DEBUG("NVML version: %s", version);
    return NVML_SUCCESS;
}

const char* nvmlErrorString(nvmlReturn_t result) {
    switch (result) {
        case NVML_SUCCESS: return "Success";
        case NVML_ERROR_UNINITIALIZED: return "Uninitialized";
        case NVML_ERROR_INVALID_ARGUMENT: return "Invalid Argument";
        case NVML_ERROR_NOT_SUPPORTED: return "Not Supported";
        case NVML_ERROR_NOT_FOUND: return "Not Found";
        default: return "Unknown Error";
    }
}

/* Library constructor - runs when loaded */
__attribute__((constructor))
static void nvml_unified_init(void) {
    const char *debug = getenv("NVML_SHIM_DEBUG");
    if (debug && atoi(debug)) {
        fprintf(stderr, "═══════════════════════════════════════════\n");
        fprintf(stderr, "  NVML Unified Memory Library v0.2\n");
        fprintf(stderr, "  Grace Blackwell GB10 Support\n");
        fprintf(stderr, "  Python Compatible\n");
        fprintf(stderr, "═══════════════════════════════════════════\n");
    }
}
