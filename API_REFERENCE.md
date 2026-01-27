# API Reference

Complete reference for all implemented NVML functions in nvml-unified-shim.

---

## Initialization & Cleanup

### nvmlInit_v2()

Initialize NVML library.

```c
nvmlReturn_t nvmlInit_v2(void);
```

**Returns**: `NVML_SUCCESS` on success, `NVML_ERROR_UNINITIALIZED` on failure

**Implementation**: Calls `cudaGetDeviceCount()` to initialize CUDA runtime.

**Example**:
```c
nvmlReturn_t result = nvmlInit_v2();
if (result != NVML_SUCCESS) {
    fprintf(stderr, "Failed to initialize: %s\n", nvmlErrorString(result));
}
```

---

### nvmlInit()

Legacy initialization function. Calls `nvmlInit_v2()`.

```c
nvmlReturn_t nvmlInit(void);
```

---

### nvmlInitWithFlags()

Initialize NVML with optional flags.

```c
nvmlReturn_t nvmlInitWithFlags(unsigned int flags);
```

**Parameters**:
- `flags`: Initialization flags (currently ignored, reserved for future use)

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Calls `nvmlInit_v2()`, flags parameter reserved for compatibility.

---

### nvmlShutdown()

Shut down NVML library.

```c
nvmlReturn_t nvmlShutdown(void);
```

**Returns**: `NVML_SUCCESS` on success, `NVML_ERROR_UNINITIALIZED` if not initialized

**Implementation**: Clears internal state. CUDA cleanup happens automatically.

---

## Device Queries

### nvmlDeviceGetCount_v2()

Get number of GPU devices.

```c
nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int *deviceCount);
```

**Parameters**:
- `deviceCount`: Output pointer for device count

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns value from `cudaGetDeviceCount()`.

**Example**:
```c
unsigned int count;
nvmlDeviceGetCount_v2(&count);
printf("Found %u GPU(s)\n", count);
```

---

### nvmlDeviceGetHandleByIndex_v2()

Get device handle by index.

```c
nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(
    unsigned int index,
    nvmlDevice_t *device
);
```

**Parameters**:
- `index`: Device index (0-based)
- `device`: Output pointer for device handle

**Returns**:
- `NVML_SUCCESS` on success
- `NVML_ERROR_INVALID_ARGUMENT` if device is NULL
- `NVML_ERROR_NOT_FOUND` if index out of range

**Implementation**: Creates fake handle from index (index + 1).

---

### nvmlDeviceGetName()

Get device name.

```c
nvmlReturn_t nvmlDeviceGetName(
    nvmlDevice_t device,
    char *name,
    unsigned int length
);
```

**Parameters**:
- `device`: Device handle
- `name`: Output buffer for device name
- `length`: Buffer length

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns name from `cudaGetDeviceProperties()`.

**Example Output**: `"NVIDIA GB10"`

---

### nvmlDeviceGetMemoryInfo_v2()

Get device memory information (version 2 with reserved field).

```c
typedef struct {
    unsigned int version;
    unsigned long long total;
    unsigned long long reserved;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_v2_t;

nvmlReturn_t nvmlDeviceGetMemoryInfo_v2(
    nvmlDevice_t device,
    nvmlMemory_v2_t *memory
);
```

**Parameters**:
- `device`: Device handle
- `memory`: Pointer to memory info structure

**Returns**: `NVML_SUCCESS` on success

**Implementation**:
- `total`: From `/proc/meminfo` MemTotal (unified memory)
- `free`: From `/proc/meminfo` MemAvailable
- `used`: From `cudaMemGetInfo()` on device
- `reserved`: Always 0
- `version`: Preserved from input (important for MAX Engine compatibility)

**Example**:
```c
nvmlMemory_v2_t mem;
mem.version = 0x2000028;  // MAX Engine version encoding
nvmlDeviceGetMemoryInfo_v2(handle, &mem);
printf("Total: %llu MB\n", mem.total / (1024*1024));
```

---

### nvmlDeviceGetMemoryInfo()

Get device memory information (version 1).

```c
typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

nvmlReturn_t nvmlDeviceGetMemoryInfo(
    nvmlDevice_t device,
    nvmlMemory_t *memory
);
```

**Implementation**: Same as v2 but simpler structure.

---

### nvmlDeviceGetUUID()

Get device UUID.

```c
nvmlReturn_t nvmlDeviceGetUUID(
    nvmlDevice_t device,
    char *uuid,
    unsigned int length
);
```

**Parameters**:
- `device`: Device handle
- `uuid`: Output buffer for UUID string
- `length`: Buffer length

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns CUDA device UUID from `cudaGetDeviceProperties()`.

**Example Output**: `"GPU-9e31e9c4-7cfa-100f-dd07-43875a3ccd72"`

---

### nvmlDeviceGetPciInfo_v3()

Get PCI information for device.

```c
typedef struct {
    char busId[16];
    unsigned int domain;
    unsigned int bus;
    unsigned int device;
    unsigned int pciDeviceId;
    unsigned int pciSubSystemId;
} nvmlPciInfo_t;

nvmlReturn_t nvmlDeviceGetPciInfo_v3(
    nvmlDevice_t device,
    nvmlPciInfo_t *pci
);
```

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns PCI info from `cudaGetDeviceProperties()`.

**Example Output**:
- `busId`: `"000f:01:00.0"`
- `domain`: `15`
- `bus`: `1`
- `device`: `0`

---

### nvmlDeviceGetUtilizationRates()

Get GPU utilization rates.

```c
typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

nvmlReturn_t nvmlDeviceGetUtilizationRates(
    nvmlDevice_t device,
    nvmlUtilization_t *utilization
);
```

**Returns**: `NVML_SUCCESS`

**Implementation**: Returns 0% for both GPU and memory utilization.

**Rationale**: Unified memory architecture doesn't have traditional GPU utilization metrics. Memory is shared between CPU and GPU, so separate GPU utilization doesn't apply.

---

### nvmlDeviceGetCudaComputeCapability()

Get CUDA compute capability.

```c
nvmlReturn_t nvmlDeviceGetCudaComputeCapability(
    nvmlDevice_t device,
    int *major,
    int *minor
);
```

**Parameters**:
- `device`: Device handle
- `major`: Output pointer for major version
- `minor`: Output pointer for minor version

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns compute capability from `cudaGetDeviceProperties()`.

**Example Output**: `major=12, minor=1` (SM 12.1 for GB10)

---

## System Queries

### nvmlSystemGetDriverVersion()

Get driver version string.

```c
nvmlReturn_t nvmlSystemGetDriverVersion(
    char *version,
    unsigned int length
);
```

**Parameters**:
- `version`: Output buffer for version string
- `length`: Buffer length

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns driver version matching nvidia-smi (580.126.09).

**Note**: For nvidia-smi compatibility, this must match `nvmlSystemGetNVMLVersion()`.

---

### nvmlSystemGetCudaDriverVersion()

Get CUDA driver version as integer.

```c
nvmlReturn_t nvmlSystemGetCudaDriverVersion(int *cudaDriverVersion);
```

**Parameters**:
- `cudaDriverVersion`: Output pointer for CUDA version

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns value from `cudaDriverGetVersion()`.

**Format**: Integer encoded as major*1000 + minor*10 (e.g., 13000 for CUDA 13.0)

---

### nvmlSystemGetCudaDriverVersion_v2()

Get CUDA driver version (v2). Identical to v1.

```c
nvmlReturn_t nvmlSystemGetCudaDriverVersion_v2(int *cudaDriverVersion);
```

---

### nvmlSystemGetNVMLVersion()

Get NVML version string.

```c
nvmlReturn_t nvmlSystemGetNVMLVersion(
    char *version,
    unsigned int length
);
```

**Parameters**:
- `version`: Output buffer for version string
- `length`: Buffer length

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Returns version matching driver (580.126.09).

**Important**: Must match `nvmlSystemGetDriverVersion()` for nvidia-smi compatibility.

---

## Error Handling

### nvmlErrorString()

Get error string for return code.

```c
const char* nvmlErrorString(nvmlReturn_t result);
```

**Parameters**:
- `result`: NVML return code

**Returns**: Human-readable error string

**Supported Codes**:
- `NVML_SUCCESS`: "Success"
- `NVML_ERROR_UNINITIALIZED`: "Uninitialized"
- `NVML_ERROR_INVALID_ARGUMENT`: "Invalid Argument"
- `NVML_ERROR_NOT_SUPPORTED`: "Not Supported"
- `NVML_ERROR_NOT_FOUND`: "Not Found"

---

## Return Codes

```c
#define NVML_SUCCESS                    0
#define NVML_ERROR_UNINITIALIZED        1
#define NVML_ERROR_INVALID_ARGUMENT     2
#define NVML_ERROR_NOT_SUPPORTED        3
#define NVML_ERROR_NOT_FOUND            6
```

---

## Constants

```c
#define NVML_DEVICE_NAME_BUFFER_SIZE        64
#define NVML_DEVICE_UUID_BUFFER_SIZE        80
#define NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE  16
```

---

## Debug Logging

Enable debug output:
```bash
export NVML_SHIM_DEBUG=1
```

All functions log their calls and return values when debug mode is enabled:
```
[NVML-UNIFIED] nvmlInit_v2() called
[NVML-UNIFIED] Initialized with 1 CUDA device(s)
[NVML-UNIFIED] nvmlDeviceGetMemoryInfo_v2() called
[NVML-UNIFIED] Memory: total=128GB, free=110GB, used=18GB
```

---

## Implementation Notes

### Memory Queries
- Use `/proc/meminfo` for total/free unified memory
- Use `cudaMemGetInfo()` for per-device used memory
- This provides accurate accounting for unified memory architecture

### Version Fields
- MAX Engine encodes version as: `sizeof(struct) | (2 << 24)`
- We preserve the input version field to maintain compatibility

### Fake Handles
- Device handles are created as: `(void*)(uintptr_t)(index + 1)`
- This allows index extraction: `(int)(uintptr_t)handle - 1`
- Simple but effective for single-GPU systems

### Thread Safety
- Current implementation is not thread-safe
- Add mutex protection if needed for multi-threaded applications

---

## Not Yet Implemented

Functions that may be needed for broader compatibility:

- `nvmlDeviceGetComputeRunningProcesses()` - Process tracking
- `nvmlDeviceGetTemperature()` - Temperature monitoring (N/A on GB10)
- `nvmlDeviceGetPowerUsage()` - Power monitoring (N/A on GB10)
- `nvmlDeviceGetEnforcedPowerLimit()` - Power limits
- `nvmlDeviceGetMaxClockInfo()` - Clock information
- `nvmlDeviceGetApplicationsClock()` - Application clocks

**Contribution welcome** for these functions if needed by your application!

---

## Migration from Official NVML

If migrating from official NVML:

### Works Identically
- All initialization/shutdown functions
- Device enumeration
- Memory queries (with semantic differences noted)
- Device properties (name, UUID, PCI, compute capability)
- Version queries

### Different Semantics
- **Utilization**: Returns 0% (unified memory has no separate GPU util)
- **Memory accounting**: Total = full system memory (unified architecture)

### Not Available
- Temperature monitoring (handled internally by GB10)
- Power monitoring (integrated superchip)
- Process tracking (not yet implemented)

### Example Migration
```c
// Official NVML code - works unchanged
nvmlInit_v2();
nvmlDeviceGetCount_v2(&count);
nvmlDeviceGetHandleByIndex_v2(0, &handle);
nvmlDeviceGetName(handle, name, sizeof(name));
nvmlDeviceGetMemoryInfo(handle, &mem);
nvmlShutdown();

// No changes needed! Drop-in replacement.
```
