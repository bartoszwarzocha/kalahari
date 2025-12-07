# GitHub Actions - Kalahari CI/CD

## Project CI Structure

```
.github/workflows/
├── ci-windows.yml    # Windows: MSVC + vcpkg + Ninja
├── ci-linux.yml      # Linux: GCC + vcpkg + Ninja
└── ci-macos.yml      # macOS: Clang + vcpkg + Ninja
```

## Build Matrix

| Platform | Compiler | Generator | Triplet |
|----------|----------|-----------|---------|
| Windows | MSVC (cl) | Ninja | x64-windows |
| Linux | GCC | Ninja | x64-linux |
| macOS | Clang | Ninja | x64-osx |

Build types: `Debug`, `Release`

## Dependencies (vcpkg.json)

```json
{
  "dependencies": [
    "qt6-base",
    "qt6-svg",
    "spdlog",
    "nlohmann-json",
    "catch2"
  ]
}
```

## Common Failure Patterns

### 1. vcpkg Cache Issues
**Symptom:** "vcpkg hash mismatch" or unexpected rebuild
**Solution:** Increment cache key version
```yaml
key: windows-msvc-vcpkg-v5-${{ hashFiles('vcpkg.json') }}  # v4 → v5
```

### 2. Qt6 Not Found
**Symptom:** "Could not find Qt6" during CMake configure
**Causes:**
- vcpkg install failed silently
- Wrong triplet
- Cache corrupted

**Solution:**
```yaml
# Ensure triplet matches platform
-DVCPKG_TARGET_TRIPLET=x64-windows  # or x64-linux, x64-osx
```

### 3. MSVC Not Found (Windows)
**Symptom:** "cl is not recognized" or MSVC errors
**Solution:** Ensure msvc-dev-cmd runs before CMake
```yaml
- uses: ilammy/msvc-dev-cmd@v1
  with:
    arch: x64
```

### 4. Timeout (90 min exceeded)
**Symptom:** Job cancelled after 90 minutes
**Causes:**
- vcpkg building from source (cache miss)
- Slow network

**Solution:**
- Check cache hit in logs
- Increase timeout if needed (max 360 min)

### 5. Permission Denied
**Symptom:** Cannot write to directory
**Solution:** Check working directory, use `${{ github.workspace }}`

### 6. Submodule Issues
**Symptom:** Missing files, CMake can't find sources
**Solution:**
```yaml
- uses: actions/checkout@v4
  with:
    submodules: recursive
```

## Best Practices

### Cache Configuration
```yaml
- uses: actions/cache@v4
  with:
    path: |
      vcpkg
      build/vcpkg_installed
    key: ${{ runner.os }}-vcpkg-v4-${{ hashFiles('vcpkg.json') }}-${{ matrix.build_type }}
    restore-keys: |
      ${{ runner.os }}-vcpkg-v4-${{ hashFiles('vcpkg.json') }}-
      ${{ runner.os }}-vcpkg-v4-
```

### Timeout
```yaml
timeout-minutes: 90  # Prevent runaway builds
```

### Matrix Strategy
```yaml
strategy:
  matrix:
    build_type: [Debug, Release]
  fail-fast: false  # Don't cancel other jobs if one fails
```

### Artifacts (Release only)
```yaml
- uses: actions/upload-artifact@v4
  if: matrix.build_type == 'Release'
  with:
    name: kalahari-${{ runner.os }}-${{ matrix.build_type }}
    path: build/bin/kalahari*
    retention-days: 7
```

## Workflow Template

```yaml
name: PLATFORM

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]
  workflow_dispatch:

jobs:
  build:
    name: Platform Build & Test
    runs-on: platform-latest
    timeout-minutes: 90

    strategy:
      matrix:
        build_type: [Debug, Release]
      fail-fast: false

    steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive

    # Platform-specific setup...

    - name: Cache vcpkg
      uses: actions/cache@v4
      with:
        path: |
          vcpkg
          build/vcpkg_installed
        key: ${{ runner.os }}-vcpkg-v4-${{ hashFiles('vcpkg.json') }}-${{ matrix.build_type }}

    - name: Configure CMake
      run: |
        cmake -B build \
          -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
          -DCMAKE_TOOLCHAIN_FILE=${{ github.workspace }}/vcpkg/scripts/buildsystems/vcpkg.cmake \
          -G Ninja

    - name: Build
      run: cmake --build build --config ${{ matrix.build_type }} --parallel

    - name: Test
      run: ctest --test-dir build --output-on-failure
```

## Debugging Tips

1. **Check cache hit:** Look for "Cache hit" vs "Cache miss" in logs
2. **vcpkg logs:** Check `build/vcpkg_installed/vcpkg/issue_body.md`
3. **CMake logs:** Check `build/CMakeCache.txt`
4. **Enable debug:** Add `ACTIONS_STEP_DEBUG: true` to secrets

## Links

- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [vcpkg Documentation](https://vcpkg.io/en/docs/README.html)
- [ilammy/msvc-dev-cmd](https://github.com/ilammy/msvc-dev-cmd)
