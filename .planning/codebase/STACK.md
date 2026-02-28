# Technology Stack

**Analysis Date:** 2026-02-28

## Languages

**Primary:**
- C99 - Core implementation of all hooks, drivers, and emulators
- C++ - Used selectively in game-specific modules and newer components (e.g., imgui bindings)

**Secondary:**
- Shell (Bash) - Build scripts and test runners
- Python - Build tooling and development utilities (mdformat for documentation)

## Runtime

**Environment:**
- Windows XP SP3 and later (Windows 7, 8, 10 primary targets)
- Cross-compilation from Linux via MinGW-w64 toolchain
- Target architecture: x86 (32-bit) and x86-64 (64-bit)

**Package Manager:**
- None - All dependencies are statically linked or provided via import definitions

## Frameworks

**Core:**
- AVS Framework - Konami's proprietary middleware framework for arcade games
  - Multiple versions supported: 0, 803, 1002, 1101, 1304, 1306, 1403, 1508, 1601, 1603, 1700
  - Imported via `avs.h` and `avs-ea3.h` from `src/imports/`
  - Version encoding: minor * 100 + patch (e.g., 2.16.7 = 1607)

**Graphics:**
- Direct3D 9 - Game rendering pipeline interception and enhancement
- NVIDIA API (NVAPI) - GPU configuration and profiling tools

**Input/Output:**
- Win32 API - Native Windows system calls for hardware interaction
- USB via libusb - EZUSB board communication
- Serial communication via RS-232 - Legacy hardware protocols
- ACIO protocol - Konami arcade cabinet IO standard

**Testing:**
- Wine (optional) - Windows application emulation for testing on Linux

**Build/Dev:**
- GNU Make - Primary build system with complex module-based compilation
- GCC/G++ via MinGW-w64 - Cross-compilation toolchain
- clang-format - Code formatting enforcement
- mdformat - Markdown documentation formatting

## Key Dependencies

**Critical (Third-party):**
- mingw-w64 - Cross-compilation toolchain for producing Windows binaries
- libusb - USB device communication for hardware controllers
- ViGEm (ViGEm Client) - Virtual Xbox controller driver for cabinet simulation
  - Linked via `src/imports/import_*_indep_ViGEmClient.def`
  - Used in `vigem-*` modules for exposing bemanitools APIs as XBOX controllers
- SMX (StepMania X) - StepMania cabinet IO protocol support
  - Header in `src/imports/SMX.h`
  - Used in `ddrio-smx` module for DDR cabinet support
- CH341 - USB serial converter driver support
  - Header in `src/imports/ch341.h`

**Build Infrastructure:**
- Docker - Containerized build environments (Debian 11.6-slim)
- containerd/runc - Container runtime management

## Configuration

**Build Configuration:**
- GNUmakefile - Main build orchestration
- Module.mk - Per-module build definitions
- `.clang-format` - Code formatting rules
- BUILDDIR variable - Configurable build output directory (default: `build/`)

**Version Information:**
- Git revision embedded in binaries at build time via `-DGITREV=$(gitrev)`
- Version file generated during build: `version` (contains git commit hash)
- Project version in `README.md` (currently: 5.50)

**Compiler Flags:**
- Optimization: `-O2` with function/data section separation
- Standards: `-std=c99` for C files
- Warnings: `-Wall` enforced, `-Werror` in release builds
- Static linking: `-static -static-libgcc -static-libstdc++`
- Platform targeting: Windows XP via `-DWINVER=0x0601 -D_WIN32_WINNT=0x0601`

## Platform Requirements

**Development:**
- Host OS: Linux, macOS, or Windows
- Git - Version control and commit hash extraction
- make - Build orchestration
- mingw-w64 toolchain (includes gcc, g++, windres, dlltool, ar, ranlib, strip)
- clang-format - Code formatter
- Python 3 with pip - mdformat installation
- Docker daemon - For containerized builds
- Wine (optional) - For running tests without Windows VM

**Production/Runtime:**
- Windows XP SP3+ or Windows 7/8/10
- DirectX 9 End-User Runtime (June 2010)
- Microsoft Visual C++ 2010 SP1 Redistributable (32-bit)
- Microsoft Visual C++ 2013 Redistributable (both 32-bit and 64-bit)
- Supported Bemani arcade game installations

**Hardware Targets:**
- P3IO - DDR cabinet IO board
- P4IO - jubeat IO board
- BIO2 - IIDX/SDVX IO board
- EZUSB/EZUSB2 - Older IIDX IO boards
- ACIO devices - Generic arcade cabinet peripherals
- Coin readers (ICCA) - E-Amusement card readers

## Build Artifacts

**Output Structure:**
- `build/bin/indep-32/` - 32-bit independent (no specific AVS version) binaries
- `build/bin/indep-64/` - 64-bit independent binaries
- `build/bin/avs2_*-32/` - 32-bit AVS-version-specific binaries
- `build/bin/avs2_*-64/` - 64-bit AVS-version-specific binaries
- `build/bemanitools.zip` - Distribution package

**Artifact Types:**
- `.dll` - Game hook libraries and emulation shims
- `.exe` - Standalone tools and utilities
- `.a` - Static libraries for linking

---

*Stack analysis: 2026-02-28*
