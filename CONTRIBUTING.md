# Contributing to nvml-unified-shim

Thank you for your interest in contributing! This project aims to enable NVML functionality on NVIDIA unified memory architectures.

---

## How to Contribute

### Reporting Issues

Found a bug or compatibility issue?

1. Check [existing issues](https://github.com/CINOAdam/nvml-unified-shim/issues)
2. If not found, [create a new issue](https://github.com/CINOAdam/nvml-unified-shim/issues/new)
3. Include:
   - Hardware details (DGX Spark, other unified memory system)
   - Software versions (CUDA, OS, application)
   - Steps to reproduce
   - Expected vs actual behavior
   - Debug logs (`NVML_SHIM_DEBUG=1`)

### Suggesting Features

Have an idea for improvement?

1. Open a [GitHub Discussion](https://github.com/CINOAdam/nvml-unified-shim/discussions)
2. Describe the use case
3. Explain why it would be valuable
4. Propose an implementation approach (optional)

### Contributing Code

#### Getting Started

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/nvml-unified-shim.git
   cd nvml-unified-shim
   ```

3. Create a branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

4. Make your changes

5. Test thoroughly:
   ```bash
   make -f Makefile.python clean
   make -f Makefile.python
   python3 tests/test_python.py
   ```

6. Commit with clear message:
   ```bash
   git commit -m "Add nvmlDeviceGetTemperature stub for compatibility"
   ```

7. Push and create pull request:
   ```bash
   git push origin feature/your-feature-name
   ```

#### Code Guidelines

**C Code Style**:
- Use 4-space indentation
- Follow existing naming conventions
- Add comments for non-obvious logic
- Include debug logging for new functions

**Example**:
```c
nvmlReturn_t nvmlDeviceGetTemperature(nvmlDevice_t device,
                                       nvmlTemperatureSensors_t sensorType,
                                       unsigned int *temp) {
    LOG_DEBUG("nvmlDeviceGetTemperature() called for sensor %d", sensorType);

    if (!temp) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    // GB10 thermal management is internal, return N/A
    *temp = 0;
    return NVML_ERROR_NOT_SUPPORTED;
}
```

**Testing Requirements**:
- Add test cases for new functions
- Ensure existing tests still pass
- Test on actual DGX Spark hardware if possible
- Include both success and error cases

**Documentation**:
- Update API_REFERENCE.md for new functions
- Add usage examples
- Document any limitations or special behavior

#### Areas for Contribution

**High Priority**:
1. **Process Tracking** - Implement `nvmlDeviceGetComputeRunningProcesses()`
2. **Additional NVML APIs** - Add commonly-used functions
3. **Thread Safety** - Add mutex protection for concurrent access
4. **Error Handling** - Improve error messages and validation

**Medium Priority**:
5. **Performance** - Optimize memory queries
6. **Testing** - Expand test coverage
7. **Multi-GPU** - Test and fix multi-GB10 scenarios
8. **Documentation** - Improve examples and guides

**Nice to Have**:
9. **Packaging** - Create deb/rpm packages
10. **CI/CD** - Set up automated testing
11. **Benchmarking** - Performance comparison tools

### Pull Request Process

1. **Before submitting**:
   - Run all tests
   - Update documentation
   - Add test cases for new features
   - Check for memory leaks (valgrind)

2. **PR Description**:
   - What does this change?
   - Why is it needed?
   - How was it tested?
   - Any breaking changes?

3. **Review Process**:
   - Maintainer will review within 48 hours
   - Address feedback
   - Once approved, PR will be merged

4. **After Merge**:
   - Your contribution will be in next release
   - You'll be added to CONTRIBUTORS list

---

## Development Setup

### Prerequisites

```bash
# On DGX Spark
sudo apt-get update
sudo apt-get install -y build-essential cuda-toolkit-12-8

# Verify CUDA
nvcc --version
```

### Building

```bash
# Clean build
make -f Makefile.python clean
make -f Makefile.python

# Install locally for testing
sudo make -f Makefile.python install

# Enable debug mode
export NVML_SHIM_DEBUG=1
```

### Testing

```bash
# Run test suite
python3 tests/test_python.py

# Test with MAX Engine
python3 -c "from max.driver import Accelerator; print(Accelerator())"

# Test with pynvml
python3 -c "import pynvml; pynvml.nvmlInit(); print(pynvml.nvmlDeviceGetCount()); pynvml.nvmlShutdown()"

# Test nvidia-smi wrapper
./nvidia-smi-wrapper
```

### Debugging

```bash
# Enable verbose logging
export NVML_SHIM_DEBUG=1

# Run with valgrind for memory leaks
valgrind --leak-check=full python3 tests/test_python.py

# Check symbols
nm -D libnvidia-ml.so.1 | grep nvml

# Trace library calls
LD_DEBUG=all python3 tests/test_python.py 2>&1 | grep nvml
```

---

## Code Organization

```
nvml-unified-shim/
├── src/
│   └── nvml_unified.c       # Main implementation
├── tests/
│   └── test_python.py       # Python test suite
├── nvidia-smi-wrapper        # nvidia-smi replacement
├── gpu-info                  # Alternative monitoring tool
├── Makefile.python           # Build system
├── README.md                 # Main documentation
├── API_REFERENCE.md          # API documentation
├── NVIDIA_COLLABORATION.md   # NVIDIA discussion topics
└── CONTRIBUTING.md           # This file
```

---

## Implementing New NVML Functions

### Template

```c
nvmlReturn_t nvmlYourFunction(nvmlDevice_t device, YourType *output) {
    LOG_DEBUG("nvmlYourFunction() called");

    // Validation
    if (!output) {
        return NVML_ERROR_INVALID_ARGUMENT;
    }

    if (!nvml_initialized) {
        return NVML_ERROR_UNINITIALIZED;
    }

    // Extract device index
    int device_index = (int)(uintptr_t)device - 1;
    if (device_index < 0 || device_index >= device_count) {
        return NVML_ERROR_NOT_FOUND;
    }

    // Implementation using CUDA runtime
    struct cudaDeviceProp prop;
    cudaError_t err = cudaGetDeviceProperties(&prop, device_index);
    if (err != cudaSuccess) {
        LOG_DEBUG("cudaGetDeviceProperties failed: %s",
                  cudaGetErrorString(err));
        return NVML_ERROR_NOT_FOUND;
    }

    // Fill output structure
    output->value = prop.something;

    LOG_DEBUG("Returned: %d", output->value);
    return NVML_SUCCESS;
}
```

### Forward Declaration

Add to header section:
```c
nvmlReturn_t nvmlYourFunction(nvmlDevice_t device, YourType *output);
```

### Test Case

Add to `tests/test_python.py`:
```python
def test_your_function():
    """Test nvmlYourFunction"""
    output = YourType()
    result = nvml.nvmlYourFunction(handle, ctypes.byref(output))
    assert result == 0, "nvmlYourFunction failed"
    assert output.value > 0, "Invalid output value"
    print("✅ nvmlYourFunction working")
```

### Documentation

Add to `API_REFERENCE.md`:
```markdown
### nvmlYourFunction()

Brief description.

\```c
nvmlReturn_t nvmlYourFunction(nvmlDevice_t device, YourType *output);
\```

**Parameters**:
- `device`: Device handle
- `output`: Output pointer

**Returns**: `NVML_SUCCESS` on success

**Implementation**: Describe how it's implemented

**Example**:
\```c
YourType output;
nvmlYourFunction(handle, &output);
printf("Value: %d\n", output.value);
\```
```

---

## Testing Checklist

Before submitting PR:

- [ ] Code compiles without warnings
- [ ] All existing tests pass
- [ ] New tests added for new features
- [ ] Tested on actual DGX Spark hardware
- [ ] Debug logging added for new functions
- [ ] API_REFERENCE.md updated
- [ ] README.md updated (if needed)
- [ ] No memory leaks (valgrind clean)
- [ ] Works with MAX Engine
- [ ] Works with pynvml
- [ ] Doesn't break nvidia-smi wrapper

---

## Release Process

For maintainers:

1. Update version in source
2. Update CHANGELOG.md
3. Tag release: `git tag v0.X.0`
4. Push tags: `git push --tags`
5. Create GitHub release
6. Build release binaries (if applicable)

---

## Getting Help

- **Questions**: [GitHub Discussions](https://github.com/CINOAdam/nvml-unified-shim/discussions)
- **Bugs**: [GitHub Issues](https://github.com/CINOAdam/nvml-unified-shim/issues)
- **Collaboration**: See NVIDIA_COLLABORATION.md

---

## Code of Conduct

Be respectful, constructive, and professional. We're all here to make unified memory GPUs more accessible.

---

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to making AI more accessible on unified memory architectures!
