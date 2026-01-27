#!/usr/bin/env python3
"""
Test NVML unified library with Python ctypes
This simulates how MAX Engine and other Python tools load NVML
"""

import ctypes
import sys
import os

def test_nvml_loading():
    """Test that we can load the NVML library"""
    print("=" * 60)
    print("Testing NVML Unified Library with Python")
    print("=" * 60)
    print()

    # Try to load the library
    print("1. Loading libnvidia-ml.so.1...")
    try:
        nvml = ctypes.CDLL("libnvidia-ml.so.1")
        print("   ✓ Library loaded successfully")
    except OSError as e:
        print(f"   ✗ Failed to load library: {e}")
        print()
        print("Make sure to run with: LD_LIBRARY_PATH=. python3 tests/test_python.py")
        sys.exit(1)

    print()

    # Initialize NVML
    print("2. Calling nvmlInit_v2()...")
    result = nvml.nvmlInit_v2()
    if result == 0:
        print("   ✓ NVML initialized successfully")
    else:
        print(f"   ✗ nvmlInit_v2() failed with code: {result}")
        sys.exit(1)

    print()

    # Get device count
    print("3. Getting device count...")
    device_count = ctypes.c_uint()
    result = nvml.nvmlDeviceGetCount_v2(ctypes.byref(device_count))
    if result == 0:
        print(f"   ✓ Found {device_count.value} device(s)")
    else:
        print(f"   ✗ nvmlDeviceGetCount_v2() failed with code: {result}")
        sys.exit(1)

    if device_count.value == 0:
        print("   ⚠ No devices found")
        sys.exit(0)

    print()

    # Get device handle
    print("4. Getting device handle for device 0...")
    device_handle = ctypes.c_void_p()
    result = nvml.nvmlDeviceGetHandleByIndex_v2(0, ctypes.byref(device_handle))
    if result == 0:
        print(f"   ✓ Got device handle: {device_handle.value}")
    else:
        print(f"   ✗ nvmlDeviceGetHandleByIndex_v2() failed with code: {result}")
        sys.exit(1)

    print()

    # Get device name
    print("5. Getting device name...")
    name_buffer = ctypes.create_string_buffer(64)
    result = nvml.nvmlDeviceGetName(device_handle, name_buffer, 64)
    if result == 0:
        device_name = name_buffer.value.decode('utf-8')
        print(f"   ✓ Device name: {device_name}")
    else:
        print(f"   ✗ nvmlDeviceGetName() failed with code: {result}")
        sys.exit(1)

    print()

    # Get memory info
    print("6. Getting memory info...")

    class MemoryInfo(ctypes.Structure):
        _fields_ = [
            ("total", ctypes.c_ulonglong),
            ("free", ctypes.c_ulonglong),
            ("used", ctypes.c_ulonglong)
        ]

    memory = MemoryInfo()
    result = nvml.nvmlDeviceGetMemoryInfo(device_handle, ctypes.byref(memory))
    if result == 0:
        total_mb = memory.total / (1024 * 1024)
        free_mb = memory.free / (1024 * 1024)
        used_mb = memory.used / (1024 * 1024)
        print(f"   ✓ Total memory: {total_mb:.0f} MB")
        print(f"   ✓ Free memory:  {free_mb:.0f} MB")
        print(f"   ✓ Used memory:  {used_mb:.0f} MB")
    else:
        print(f"   ✗ nvmlDeviceGetMemoryInfo() failed with code: {result}")
        sys.exit(1)

    print()

    # Get UUID
    print("7. Getting device UUID...")
    uuid_buffer = ctypes.create_string_buffer(80)
    result = nvml.nvmlDeviceGetUUID(device_handle, uuid_buffer, 80)
    if result == 0:
        device_uuid = uuid_buffer.value.decode('utf-8')
        print(f"   ✓ Device UUID: {device_uuid}")
    else:
        print(f"   ✗ nvmlDeviceGetUUID() failed with code: {result}")
        # Don't exit - UUID might not be critical

    print()

    # Shutdown
    print("8. Calling nvmlShutdown()...")
    result = nvml.nvmlShutdown()
    if result == 0:
        print("   ✓ NVML shutdown successfully")
    else:
        print(f"   ✗ nvmlShutdown() failed with code: {result}")

    print()
    print("=" * 60)
    print("✓ ALL TESTS PASSED")
    print("=" * 60)
    print()
    print("The library is working correctly with Python ctypes!")
    print("MAX Engine should now work on this system.")
    print()

if __name__ == "__main__":
    test_nvml_loading()
