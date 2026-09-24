# Building Velocity-Next

## Prerequisites

- **CMake 3.21+** (needed for the `$<TARGET_RUNTIME_DLLS:...>` generator expression that copies Qt's runtime DLLs)
- **C++20 compiler**:
  - **Windows**: MinGW 13.1.0+ or MSVC 2022 (Visual Studio 17.0+)
  - **macOS**: Clang (Xcode Command Line Tools)
  - **Linux**: GCC 11+ or Clang 14+
- **Git**, **Python 3**, and a working internet connection (for vcpkg building Botan, and for sideloading Qt - see below; both happen automatically on first configure)
- **Qt is not a manual prerequisite** - see "Qt Configuration" below

## Quick Start

```bash
# Clone the repository (--recurse-submodules pulls in the vcpkg submodule)
git clone --recurse-submodules https://github.com/Pandoriaantje/Velocity-Next
cd Velocity-Next

# If you already cloned without --recurse-submodules:
# git submodule update --init --recursive

# Configure (choose one preset)
cmake --preset windows-mingw-release    # Windows MinGW Release
cmake --preset windows-mingw-debug      # Windows MinGW Debug
cmake --preset windows-msvc-release     # Windows MSVC Release
cmake --preset windows-msvc-debug       # Windows MSVC Debug
cmake --preset macos-release            # macOS Release  
cmake --preset macos-debug              # macOS Debug
cmake --preset linux-release            # Linux Release
cmake --preset linux-debug              # Linux Debug

# Build
cmake --build --preset [chosen-preset]
```

## Automated Builds (GitHub Actions)

**Don't want to build locally?** Fork this repository and GitHub Actions will automatically build for all platforms!

### Available Workflows

- **Windows MinGW** - Produces portable .zip with Qt dependencies bundled
- **Windows MSVC** - MSVC 2022 build for maximum compatibility
- **Linux** - Ubuntu build with AppImage support (planned)
- **macOS** - .app bundle and DMG installer

### Triggering Builds

1. **Fork the repository** on GitHub
2. **Enable Actions** in your fork (Settings → Actions → General)
3. **Manual builds**: Go to Actions tab → Select workflow → "Run workflow"
4. **Automated releases**: Push a version tag (e.g., `v0.2.0`) to trigger all platforms

### Downloading Pre-built Releases

Visit the [Releases page](https://github.com/Pandoriaantje/Velocity-Next/releases) for official builds.

## Botan Dependency

The cryptography library is managed by [vcpkg](https://vcpkg.io), pinned as a Git submodule at `externals/vcpkg`:

- **Manifest-driven**: Declared in `vcpkg.json`; CMake resolves and builds it automatically on first configure
- **Static linkage**: Built as a static library and embedded into XboxInternals (see the `VCPKG_TARGET_TRIPLET` set per-preset in `CMakePresets.json`)
- **Submodule required**: Run `git submodule update --init --recursive` if you cloned without `--recurse-submodules`
- **First configure is slow**: Botan is compiled from source the first time; subsequent configures reuse the vcpkg build cache

## Qt Configuration

- **Components Required**: Core, Xml, Widgets, Network, Concurrent
- **Version**: pinned to 6.8.3 (see `QT_SIDELOAD_VERSION` in `cmake/SideloadQt.cmake`)

### Sideloaded by default

Qt is **not** a manual prerequisite. On first configure, `cmake/SideloadQt.cmake` downloads a pinned copy of Qt straight from Qt's official archives (via [aqtinstall](https://github.com/miurahr/aqtinstall), the same tool `jurplel/install-qt-action` uses in CI) into `externals/qt/` inside the repository - nothing is installed system-wide, and nothing is written outside the project directory. This makes builds reproducible across machines without depending on whatever Qt happens to already be on the system.

- **Requires**: Python 3 and internet access (only on the first configure per Qt version/platform; `pip install aqtinstall` runs automatically if needed)
- **First configure is slow**: downloads roughly 1-2 GB; subsequent configures reuse `externals/qt/` and are instant
- **MinGW note**: the MinGW *compiler* itself is still expected on PATH and must match the sideloaded Qt's MinGW kit (13.1.0+) - only the Qt libraries/headers are sideloaded, not the compiler

### Using a system Qt install instead

To opt out of sideloading and use your own Qt install:

```bash
cmake --preset windows-msvc-release -DVELOCITY_SIDELOAD_QT=OFF -DCMAKE_PREFIX_PATH="/path/to/your/qt"
```

## Build Options

### Release Builds (Recommended for end users)

```bash
# Windows MinGW Release
cmake --preset windows-mingw-release
cmake --build --preset windows-mingw-release

# Windows MSVC Release
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release

# macOS Release
cmake --preset macos-release
cmake --build --preset macos-release

# Linux Release
cmake --preset linux-release
cmake --build --preset linux-release
```

### Debug Builds (For development)

```bash
# Windows MinGW Debug
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug

# Windows MSVC Debug
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug

# macOS Debug
cmake --preset macos-debug
cmake --build --preset macos-debug

# Linux Debug
cmake --preset linux-debug
cmake --build --preset linux-debug
```

### Library Configuration

#### Shared Library (Default)
```bash
# Build XboxInternals as shared library (default - no flags needed)
cmake --preset windows-mingw-release
```
- **Windows**: Creates `libXboxInternals.dll` (automatically copied to Velocity output directory)
- **macOS**: Creates `libXboxInternals.dylib` (handled by macOS bundle system)
- **Linux**: Creates `libXboxInternals.so` (system finds via rpath)
- **Note**: Shared library is automatically placed alongside the executable - no manual copying needed

#### Static Library
```bash
# Build only static library
cmake --preset windows-mingw-release -DBUILD_XBOXINTERNALS_SHARED=OFF
```
- Creates `libXboxInternals.a` (all platforms)
- Library code is compiled directly into VelocityNext executable
- No separate DLL/shared library needed at runtime
- Results in larger executable but simpler distribution (single file)
- **Note**: When shared is OFF, static is automatically enabled

#### Both Shared and Static
```bash
# Build both shared and static versions (for library distribution)
cmake --preset windows-mingw-release -DBUILD_XBOXINTERNALS_STATIC=ON
```
- Creates both `libXboxInternals.dll` and `libXboxInternals.a`
- Velocity executable uses the shared version
- Useful for developers who want both options available

### Botan Version

The Botan version is pinned by the `externals/vcpkg` submodule commit (see its `ports/botan/vcpkg.json`). To pick up a newer Botan release, update the submodule to a newer vcpkg commit:

```bash
cd externals/vcpkg
git fetch
git checkout <newer-commit>
cd ../..
git add externals/vcpkg
```

## Platform-Specific Notes

### Windows

#### MinGW
- **Presets**: `windows-mingw-release` or `windows-mingw-debug`
- **Compiler**: MinGW 13.1.0+ on PATH (must match the sideloaded Qt's MinGW kit - Qt itself is sideloaded automatically, see "Qt Configuration")
- **Output**: Executable with Windows resource data

#### MSVC (Visual Studio 2022)
- **Presets**: `windows-msvc-release` or `windows-msvc-debug`
- **Requirements**: Visual Studio 2022 (17.0+) with C++ workload
- **Setup**: Run from **Developer Command Prompt for VS 2022** or **Developer PowerShell for VS 2022**
- **Output**: Executable with Windows resource data

**MSVC Build Example:**
```bash
# Open Developer Command Prompt for VS 2022 or Developer PowerShell for VS 2022
# Then run:
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

**Note**: MSVC builds require:
- Visual Studio 2022 or Build Tools for Visual Studio 2022
- C++ CMake tools for Windows component
- Ninja build system (included with Visual Studio)
- Qt is sideloaded automatically on first configure (see "Qt Configuration")

### macOS
- **Bundle**: Creates .app bundle with proper metadata
- **Icons**: Uses velocity.icns for application icon
- **Output**: Velocity.app bundle

#### macOS Distribution

**Prerequisites:**
- Xcode command line tools installed
- Build the project first using the presets above (this sideloads Qt into `externals/qt/`)
- `app.entitlements` file exists in project root

**Note**: The deployment script (`deploy_mac.sh`) automatically finds `macdeployqt6` in this order:
1. The sideloaded Qt at `externals/qt/*/macos/bin`
2. Your PATH
3. QT6_PREFIX_PATH environment variable
4. Common installation locations (~/Qt, Homebrew, etc.)

If you built with `-DVELOCITY_SIDELOAD_QT=OFF` and need to point at your own Qt:

```bash
export PATH="$HOME/Qt/[version]/macos/bin:$PATH"
# or
export QT6_PREFIX_PATH="/path/to/your/qt"

# Make the deployment script executable (first time only)
chmod +x deploy_mac.sh

# Deploy and sign (from project root)
./deploy_mac.sh

# For debug builds
./deploy_mac.sh out/build/macos-debug
```

**Note**: Uses ad-hoc signing by default. For distribution, replace `-` with your Developer ID in the script.

### Linux
- **Standard**: Creates standard Linux executable
- **Output**: Velocity binary

## Troubleshooting

### Qt Sideload Issues

```bash
# Ensure Python 3 and aqtinstall work
python -m aqt version

# Force a clean re-download (delete the sideloaded copy)
rm -rf externals/qt
cmake --preset your-preset

# Check internet connection (aqtinstall needs it to fetch Qt on first configure)
```

To bypass sideloading entirely and use your own Qt install:
```bash
cmake --preset your-preset -DVELOCITY_SIDELOAD_QT=OFF -DCMAKE_PREFIX_PATH="/path/to/qt"
```

### Botan / vcpkg Build Issues
```bash
# Ensure the vcpkg submodule is initialized
git submodule update --init --recursive

# Clean and rebuild (also clears the vcpkg install tree for this preset)
rm -rf out/build
cmake --preset your-preset

# Check internet connection (vcpkg needs it to fetch Botan sources on first build)
```
### Compiler Issues
- Ensure your compiler supports C++20
- Match Qt kit with your compiler (MinGW Qt with MingGW compiler)

## Output Locations

Build outputs are organized in:
- `out/build/windows-mingw-release/` - Windows MinGW Release
- `out/build/windows-mingw-debug/` - Windows MinGW Debug
- `out/build/macos-release/` - macOS Release
- `out/build/macos-debug/` - macOS Debug
- `out/build/linux-release/` - Linux Release
- `out/build/linux-debug/` - Linux Debug

## Running Velocity

### Windows - Administrator Privileges Required

**Important**: On Windows, Velocity requires **Administrator privileges** to access physical drives (Xbox 360 hard drives and USB devices).

**To run Velocity with the necessary permissions:**

1. Navigate to the VelocityNext executable:
   - Release: `out/build/windows-mingw-release/VelocityNext/VelocityNext.exe`
   - Debug: `out/build/windows-mingw-debug/VelocityNext/VelocityNext.exe`

2. **Right-click** on `VelocityNext.exe` → Select **"Run as administrator"**

**Why Administrator access is needed:**
- Windows requires elevated privileges to open raw physical disk devices (`\\.\PHYSICALDRIVE#`)
- This is necessary for the Device Viewer to detect and access Xbox 360 drives connected via USB (using SATA-to-USB adapters, etc.)
- Without Administrator rights, Xbox 360 drives will **not be detected** in the Device Viewer

**Alternative - Create Administrator Shortcut:**
1. Right-click `VelocityNext.exe` → **"Create shortcut"**
2. Right-click the shortcut → **"Properties"**
3. Click **"Advanced..."** button
4. Check **"Run as administrator"**
5. Click **OK** → **OK**

Now you can use this shortcut to always launch Velocity with proper permissions.

### Linux

On Linux, access to raw block devices requires root privileges or membership in the `disk` group.

**Option 1: Run with sudo** (recommended for testing)
```bash
sudo ./out/build/linux-release/VelocityNext/VelocityNext
```

**Option 2: Add user to disk group** (for regular use)
```bash
# Add your user to the disk group
sudo usermod -a -G disk $USER

# Log out and log back in for changes to take effect
```

**Note**: Xbox 360 drives connected via USB should appear as `/dev/sd*` devices (e.g., `/dev/sdb`).

### macOS

On macOS, access to raw disk devices typically requires administrator privileges.

**Run with sudo:**
```bash
sudo ./out/build/macos-release/VelocityNext.app/Contents/MacOS/VelocityNext
```

**Note**: Xbox 360 drives should appear as `/dev/disk*` or `/dev/rdisk*` devices.

---

For more details, see the project documentation or check the CMakeLists.txt files for advanced configuration options.