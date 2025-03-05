# LibUV Custom Build for Node.js

This guide provides detailed instructions on building a custom version of LibUV for Node.js on Windows 11.

## Testing Environment

- **OS**: Windows 11 Home
- **Processor**: 12th Gen Intel(R) Core(TM) i7-12000H
- **RAM**: 16GB
- **Storage**: NVMe WD_BLACK SN7100 1TB SSD
- **GPU**: NVIDIA GeForce 3050 Ti Laptop GPU with 4GB VRAM

## Prerequisites

### Step 0: Install Visual Studio Code & Windows SDK
1. Download and install the latest version of Visual Studio Code from [code.visualstudio.com/download](https://code.visualstudio.com/download)
2. Download and install Windows SDK v10.0.231000.2454 from [developer.microsoft.com/windows/downloads/windows-sdk](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)

### Step 1: Install Chrome
1. Download Chrome Setup v134.0.6947.0 from [google.com/chrome](https://www.google.com/chrome/dr/download/?brand=CHBD&ds_kid=43700075934933066&gad_source=1&gclid=Cj0KCQiAwOe8BhCCARIsAGKeD54MqdtvsN3LpEYaROVSCoMk0WDSPgf6RRV1NEf9Gvy-ZiFxnE-oSj4aAqnQEALw_wcB&gclsrc=aw.ds)
2. Complete the installation process

### Step 2: Install Node.js
1. Download Node.js v22.12.1 (LTS) from [nodejs.org/en/download](https://nodejs.org/en/download)
2. Install node-v22.13.1-x64.msi to `C:\Program Files\nodejs`

### Step 3: Download LibUV
1. Go to the LibUV repository: [github.com/libuv/libuv/tree/v1.x](https://github.com/libuv/libuv/tree/v1.x) (Dated 1/29/2025)
2. Download the libuv-1.x.zip into a folder and extract the files
3. Open the libuv-1.x folder in Visual Studio Code

### Step 4: Install CMake
1. Download cmake-3.31.5-windows-x86_64.msi from [cmake.org/download](https://cmake.org/download/)
2. Install CMake to `C:\Program Files\CMake`

### Step 5: Restart Computer
Restart your computer once you've completed Steps 1-4.

## Build Instructions

### Step 6: Build and Install LibUV

#### Generate Build Files
```bash
mkdir build
cd build
cmake ..
```

#### Build the Project
```bash
cmake --build . --config Release
```

#### Install the Library
```bash
cmake --install . --config Release
```
This will install libuv-custom to `C:\usr\local\libuv-custom`

### Step 7: Setup Environment Variables
1. Go to Environment Variables > System Variables
2. Add `C:\usr\local\libuv-custom\bin` to the Path variable
3. Click OK on all windows to save changes

#### Compile Test Program
```bash
cl test.c /I"C:\usr\local\libuv-custom\include" /link "C:\usr\local\libuv-custom\lib\uv.lib"
```

## Integration with Node.js

### Step 8: Using the libuv-custom Library in Node.js

#### Backup Existing Node.js Library
```bash
mkdir C:\NodeJS_Backup
xcopy "C:\Program Files\nodejs\*.*" "C:\NodeJS_Backup\" /E /H /C /I
```

#### Check Current Versions
```bash
node -v
node -p "process.versions.uv"
```

#### Replace LibUV Files
```bash
copy "C:\usr\local\libuv-custom\bin\uv.dll" "C:\Program Files\nodejs\" /Y
```

#### Revert to Original Version (if needed)
```bash
xcopy "C:\NodeJS_Backup\*.*" "C:\Program Files\nodejs\" /E /H /C /I /Y
```

## Notes
- This guide specifically targets Windows 11 with the specified hardware configuration
- Always create a backup before replacing system files
- The LibUV version used is from the v1.x branch dated 1/29/2025
- The compilation process may take a while due to the complexity of the project