/*
 * NVML Unified Memory Shim for Grace Blackwell GB10
 *
 * Provides CUDA runtime fallback when NVML fails on unified memory architectures.
 * Use with LD_PRELOAD to make NVML-dependent applications work on GB10.
 *
 * Author: TISMJedi Homelab
 * License: MIT
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cuda_runtime.h>

/* NVML Types and Constants */
#define NVML_SUCCESS 0
#define NVML_ERROR_NOT_SUPPORTED 3
#define NVML_ERROR_UNINITIALIZED 1
#define NVML_DEVICE_NAME_BUFFER_SIZE 64

typedef void* nvmlDevice_t;
typedef int nvmlReturn_t;

typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_v1_t;

typedef struct {
    unsigned int version;
    unsigned long long total;
    unsigned long long reserved;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_v2_t;

/* Function pointers to real NVML */
static nvmlReturn_t (*real_nvmlInit)(void) = NULL;
static nvmlReturn_t (*real_nvmlShutdown)(void) = NULL;
static nvmlReturn_t (*real_nvmlDeviceGetCount)(unsigned int *deviceCount) = NULL;
static nvmlReturn_t (*real_nvmlDeviceGetHandleByIndex)(unsigned int index, nvmlDevice_t *device) = NULL;
static nvmlReturn_t (*real_nvmlDeviceGetMemoryInfo)(nvmlDevice_t device, nvmlMemory_v1_t *memory) = NULL;
static nvmlReturn_t (*real_nvmlDeviceGetMemoryInfo_v2)(nvmlDevice_t device, nvmlMemory_v2_t *memory) = NULL;
static nvmlReturn_t (*real_nvmlDeviceGetName)(nvmlDevice_t device, char *name, unsigned int length) = NULL;
static const char* (*real_nvmlErrorString)(nvmlReturn_t result) = NULL;

/* State tracking */
static int shim_initialized = 0;
static int using_cuda_fallback = 0;

#define LOG_DEBUG(fmt, ...) \
    if (getenv("NVML_SHIM_DEBUG")) { \
        fprintf(stderr, "[NVML-SHIM] " fmt "\n", ##__VA_ARGS__); \
    }

/* Initialize function pointers to real NVML */
static void init_real_nvml() {
    if (shim_initialized) return;

    LOG_DEBUG("Initializing NVML shim...");

    real_nvmlInit = dlsym(RTLD_NEXT, "nvmlInit");
    real_nvmlShutdown = dlsym(RTLD_NEXT, "nvmlShutdown");
    real_nvmlDeviceGetCount = dlsym(RTLD_NEXT, "nvmlDeviceGetCount");
    real_nvmlDeviceGetHandleByIndex = dlsym(RTLD_NEXT, "nvmlDeviceGetHandleByIndex");
    real_nvmlDeviceGetMemoryInfo = dlsym(RTLD_NEXT, "nvmlDeviceGetMemoryInfo");
    real_nvmlDeviceGetMemoryInfo_v2 = dlsym(RTLD_NEXT, "nvmlDeviceGetMemoryInfo_v2");
    real_nvmlDeviceGetName = dlsym(RTLD_NEXT, "nvmlDeviceGetName");
    real_nvmlErrorString = dlsym(RTLD_NEXT, "nvmlErrorString");

    shim_initialized = 1;
    LOG_DEBUG("NVML shim initialized");
}

/* Read system memory from /proc/meminfo */
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

/* Get GPU memory usage via CUDA runtime */
static unsigned long long get_cuda_memory_used(int device_index) {
    size_t free_mem, total_mem;
    cudaError_t err;

    // Validate device index first
    int device_count;
    err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess || device_index >= device_count) {
        LOG_DEBUG("Invalid device index %d (count=%d)", device_index, device_count);
        return 0;
    }

    // Set device
    err = cudaSetDevice(device_index);
    if (err != cudaSuccess) {
        LOG_DEBUG("cudaSetDevice failed: %s", cudaGetErrorString(err));
        return 0;
    }

    // Query memory
    err = cudaMemGetInfo(&free_mem, &total_mem);
    if (err != cudaSuccess) {
        LOG_DEBUG("cudaMemGetInfo failed: %s", cudaGetErrorString(err));
        return 0;
    }

    return total_mem - free_mem;
}

/*
 * NVML API Implementations
 */

nvmlReturn_t nvmlInit(void) {
    init_real_nvml();

    if (real_nvmlInit) {
        nvmlReturn_t ret = real_nvmlInit();
        LOG_DEBUG("nvmlInit() -> %d", ret);
        return ret;
    }

    LOG_DEBUG("nvmlInit() -> using CUDA fallback");
    using_cuda_fallback = 1;
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlShutdown(void) {
    if (real_nvmlShutdown) {
        return real_nvmlShutdown();
    }
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetCount(unsigned int *deviceCount) {
    init_real_nvml();

    if (real_nvmlDeviceGetCount) {
        nvmlReturn_t ret = real_nvmlDeviceGetCount(deviceCount);

        if (ret == NVML_SUCCESS) {
            LOG_DEBUG("nvmlDeviceGetCount() -> %d devices (via NVML)", *deviceCount);
            return ret;
        }

        LOG_DEBUG("NVML device count failed (%d), trying CUDA fallback", ret);
    }

    /* Fallback to CUDA runtime */
    int cuda_device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&cuda_device_count);

    if (err == cudaSuccess) {
        *deviceCount = (unsigned int)cuda_device_count;
        using_cuda_fallback = 1;
        LOG_DEBUG("nvmlDeviceGetCount() -> %d devices (via CUDA fallback)", *deviceCount);
        return NVML_SUCCESS;
    }

    LOG_DEBUG("CUDA fallback also failed: %s", cudaGetErrorString(err));
    return NVML_ERROR_UNINITIALIZED;
}

nvmlReturn_t nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t *device) {
    init_real_nvml();

    if (real_nvmlDeviceGetHandleByIndex && !using_cuda_fallback) {
        nvmlReturn_t ret = real_nvmlDeviceGetHandleByIndex(index, device);
        if (ret == NVML_SUCCESS) {
            LOG_DEBUG("nvmlDeviceGetHandleByIndex(%u) -> success (via NVML)", index);
            return ret;
        }
    }

    /* Fallback: Create fake handle (just use index as pointer) */
    *device = (nvmlDevice_t)(unsigned long)(index + 1);
    using_cuda_fallback = 1;
    LOG_DEBUG("nvmlDeviceGetHandleByIndex(%u) -> fake handle (CUDA fallback)", index);
    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_v1_t *memory) {
    init_real_nvml();

    if (real_nvmlDeviceGetMemoryInfo && !using_cuda_fallback) {
        nvmlReturn_t ret = real_nvmlDeviceGetMemoryInfo(device, memory);
        if (ret == NVML_SUCCESS) {
            LOG_DEBUG("nvmlDeviceGetMemoryInfo() -> success (via NVML)");
            return ret;
        }

        if (ret != NVML_ERROR_NOT_SUPPORTED) {
            return ret; // Some other error, don't fallback
        }

        LOG_DEBUG("NVML memory query not supported, using CUDA fallback");
    }

    /* Fallback for unified memory architecture */
    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    unsigned long long total = get_system_memory_total();
    unsigned long long available = get_system_memory_available();
    unsigned long long used = get_cuda_memory_used(device_index);

    memory->total = total;
    memory->used = used;
    memory->free = available;

    using_cuda_fallback = 1;
    LOG_DEBUG("nvmlDeviceGetMemoryInfo() -> total=%llu MB, used=%llu MB, free=%llu MB (CUDA fallback)",
              total / (1024*1024), used / (1024*1024), available / (1024*1024));

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(nvmlDevice_t device, nvmlMemory_v2_t *memory) {
    init_real_nvml();

    if (real_nvmlDeviceGetMemoryInfo_v2 && !using_cuda_fallback) {
        nvmlReturn_t ret = real_nvmlDeviceGetMemoryInfo_v2(device, memory);
        if (ret == NVML_SUCCESS) {
            LOG_DEBUG("nvmlDeviceGetMemoryInfo_v2() -> success (via NVML)");
            return ret;
        }

        if (ret != NVML_ERROR_NOT_SUPPORTED) {
            return ret;
        }

        LOG_DEBUG("NVML memory query not supported, using CUDA fallback");
    }

    /* Fallback for unified memory architecture */
    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    unsigned long long total = get_system_memory_total();
    unsigned long long available = get_system_memory_available();
    unsigned long long used = get_cuda_memory_used(device_index);

    memory->version = 2;
    memory->total = total;
    memory->used = used;
    memory->free = available;
    memory->reserved = 0;

    using_cuda_fallback = 1;
    LOG_DEBUG("nvmlDeviceGetMemoryInfo_v2() -> total=%llu MB, used=%llu MB, free=%llu MB (CUDA fallback)",
              total / (1024*1024), used / (1024*1024), available / (1024*1024));

    return NVML_SUCCESS;
}

nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char *name, unsigned int length) {
    init_real_nvml();

    if (real_nvmlDeviceGetName && !using_cuda_fallback) {
        nvmlReturn_t ret = real_nvmlDeviceGetName(device, name, length);
        if (ret == NVML_SUCCESS) {
            LOG_DEBUG("nvmlDeviceGetName() -> '%s' (via NVML)", name);
            return ret;
        }
    }

    /* Fallback: Get name via CUDA */
    unsigned int device_index = (unsigned int)((unsigned long)device - 1);

    struct cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_index);

    if (err == cudaSuccess) {
        snprintf(name, length, "%s", prop.name);
        using_cuda_fallback = 1;
        LOG_DEBUG("nvmlDeviceGetName() -> '%s' (CUDA fallback)", name);
        return NVML_SUCCESS;
    }

    LOG_DEBUG("nvmlDeviceGetName() failed: %s", cudaGetErrorString(err));
    return NVML_ERROR_UNINITIALIZED;
}

const char* nvmlErrorString(nvmlReturn_t result) {
    if (real_nvmlErrorString) {
        return real_nvmlErrorString(result);
    }

    switch (result) {
        case NVML_SUCCESS: return "Success";
        case NVML_ERROR_UNINITIALIZED: return "Uninitialized";
        case NVML_ERROR_NOT_SUPPORTED: return "Not Supported";
        default: return "Unknown Error";
    }
}

/* Constructor - runs when library is loaded */
__attribute__((constructor))
static void nvml_shim_init(void) {
    LOG_DEBUG("═══════════════════════════════════════════");
    LOG_DEBUG("  NVML Unified Memory Shim - PoC v0.1");
    LOG_DEBUG("  Grace Blackwell GB10 Support");
    LOG_DEBUG("═══════════════════════════════════════════");
    LOG_DEBUG("Set NVML_SHIM_DEBUG=1 for verbose logging");
    init_real_nvml();
}
