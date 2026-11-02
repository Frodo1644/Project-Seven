# Build Guide - Project Seven

## Prerequisites

### Required Software
- Windows 10/11 (x64)
- Visual Studio 2022 with C++ tools
- MSVC 14.44.35207 or compatible
- Windows SDK 10.0.26100.0
- CMake 4.4.4 or later
- JUCE 9.0.4 installed at `C:/SDK/JUCE/install`

### Optional Tools
- Cubase 15 (for VST3 host testing)
- Screenshot tools for visual verification

## Environment Setup

### 1. Install JUCE
JUCE should be installed at `C:/SDK/JUCE/install`. This is the recommended path; if you install elsewhere, update `JUCE_ROOT` in CMakeLists.txt.

### 2. Configure CMake
Create a build directory and configure:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DPROJECT_SEVEN_BUILD_VST3_SMOKE=ON ^
  -DPROJECT_SEVEN_BUILD_EDITOR_SNAPSHOT=ON
```

If JUCE is at a different location, override `JUCE_ROOT`:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DJUCE_ROOT="D:/path/to/JUCE" ^
  -DPROJECT_SEVEN_BUILD_VST3_SMOKE=ON
```

### 3. Build
Build the VST3 plugin:

```bash
cmake --build build --config Release --target ProjectSeven_VST3
```

The output is at `build/ProjectSeven_artefacts/Release/VST3/Project Seven.vst3`.

### 4. Build Tests
Tests use the same core library as the plugin:

```bash
cmake --build build --config Release --target ProjectSevenTests
```

Run:

```bash
build/ProjectSevenTests_artefacts/Release/ProjectSevenTests.exe
```

Expected output: 111 checks, 0 failures.

### 5. Build VST3 Smoke Test
Smoke test uses the hosted VST3 bundle:

```bash
cmake --build build --config Release --target ProjectSevenVst3Smoke
```

Run:

```bash
build/ProjectSevenVst3Smoke_artefacts/Release/ProjectSevenVst3Smoke.exe
```

Expected output: PASS (0 failure(s)), hosted peak: ~0.1959.

### 6. Build Editor Snapshot (optional)
Renders the editor to PNG for visual verification:

```bash
cmake --build build --config Release --target ProjectSevenEditorSnapshot
```

Usage:

```bash
ProjectSevenEditorSnapshot.exe <output_path> [width] [height] [preset_index]
```

Presets: 0=Standard B, 1=Drop A, 2=Drop G, 3=Drop F.

Example:

```bash
ProjectSevenEditorSnapshot.exe editor_preview.png 1000 650 3
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PROJECT_SEVEN_BUILD_VST3_SMOKE` | OFF | Build VST3 host smoke test |
| `PROJECT_SEVEN_BUILD_EDITOR_SNAPSHOT` | OFF | Build editor screenshot tool |
| `JUCE_ROOT` | C:/SDK/JUCE | JUCE installation path |
| `CMAKE_MSVC_RUNTIME_LIBRARY` | MultiThreaded | Static runtime linking |

## Static Runtime

The plugin is built with the static MSVC runtime. Verify with:

```bash
dumpbin /dependents build/ProjectSeven_artefacts/Release/VST3/Project Seven.vst3
```

You should see no `VCRUNTIME140.dll`, `MSVCP140.dll`, or `UCRT.dll` dependencies.

## Dependencies

The VST3 bundle should only depend on Windows system DLLs:
- KERNEL32.dll
- MSIMG32.dll
- USER32.dll
- AuthenticAMD.dll (if AMD CPU)
- uxtheme.dll (if Windows version supports it)

## Troubleshooting

### JUCE not found
If CMake cannot find JUCE, verify `JUCE_ROOT` points to the correct path. The build expects:
- `JUCE_ROOT/install/lib/cmake/JUCE-9.0.4/JUCEConfig.cmake`
- `JUCE_ROOT/install/include/JUCE-9.0.4/`

### Linker errors
Ensure all JUCE modules are available and the platform toolset matches.

### VST3 smoking fails
Verify the VST3 bundle path matches the smoke test configuration. The smoke test uses:
`$<TARGET_FILE_DIR:ProjectSeven>/VST3/Project Seven.vst3`

### Editor snapshot not found
The editor snapshot target requires `PROJECT_SEVEN_BUILD_EDITOR_SNAPSHOT=ON`.

## Release Build

For a clean release build:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DPROJECT_SEVEN_BUILD_VST3_SMOKE=ON
cmake --build build --config Release --target ProjectSeven_VST3
cmake --build build --config Release --target ProjectSevenTests
cmake --build build --config Release --target ProjectSevenVst3Smoke
```
