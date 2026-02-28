# External Integrations

**Analysis Date:** 2026-02-28

## APIs & External Services

**Konami AVS Framework:**
- AVS (Arcade Versioning System) - Proprietary middleware for all Bemani arcade games
  - SDK/Import: `src/imports/avs.h`, `src/imports/avs-ea3.h`
  - Auth: None (internal framework)
  - Configuration: Per-module via compilation flags specifying AVS version
  - Versions: 0, 803, 1002, 1101, 1304, 1306, 1403, 1508, 1601, 1603, 1700
  - Used by: All game hooks (iidxhook*, ddrhook*, jbhook*, popnhook, sdvxhook, bsthook, vefxio)

**Graphics Acceleration:**
- Direct3D 9 - Game rendering API interception
  - Client: Windows built-in (d3d9.dll)
  - Used by: `d3d9exhook`, `d3d9-util`, `d3d9-monitor-check`, `d3d9-frame-graph-hook`
  - Hooks: Monitor refresh rate detection, frame timing analysis, graphics pipeline interception

**GPU Management:**
- NVIDIA NVAPI - GPU profiling and configuration
  - SDK/Headers: `src/imports/nvapi/`
  - Used by: `nvgpu` tool for NVIDIA GPU configuration and custom timing profiles

## Hardware Interfaces

**Cabinet IO Protocols:**
- P3IO (DDR IO) - Dance Dance Revolution IO board protocol
  - Emulation: `p3ioemu`, `p3iodrv`
  - Drivers: `p3io-ddr-tool` for testing/debugging
  - Implementation: `src/main/p3io/`, `src/main/p3iodrv/`, `src/main/p3ioemu/`
  - Testing tool: `p3io-ddr-tool.exe`

- P4IO (jubeat IO) - jubeat cabinet IO board
  - Emulation: `p4ioemu`, `p4iodrv`
  - Implementation: `src/main/p4io/`, `src/main/p4iodrv/`, `src/main/p4ioemu/`

- BIO2 - IIDX/SDVX IO board
  - Drivers: `bio2drv`, `bio2emu`, `bio2emu-iidx`
  - Implementation: `src/main/bio2/`, `src/main/bio2drv/`, `src/main/bio2emu/`, `src/main/bio2emu-iidx/`
  - Used by: `iidxio-bio2.dll`, `sdvxio-bio2.dll`, `jbio-p4io.dll`

- ACIO - Arcade Cabinet IO (generic peripheral interface)
  - Driver: `aciodrv`, `acioemu`, `aciomgr`
  - Testing: `aciotest.exe`, `aciodrv-proc`
  - Implementation: `src/main/acio/`, `src/main/aciodrv/`, `src/main/acioemu/`, `src/main/aciomgr/`

- EZUSB/EZUSB2 - Cypress EZ-USB FPGA boards (older IIDX)
  - Implementation: `src/main/ezusb/`, `src/main/ezusb2/`
  - Emulation: `ezusb-emu`, `ezusb-iidx-emu`, `ezusb2-emu`, `ezusb2-iidx-emu`, `ezusb2-popn-emu`
  - Tools: `ezusb-tool.exe`, `ezusb2-tool.exe`, `ezusb-iidx-fpga-flash.exe`, `ezusb-iidx-sram-flash.exe`
  - Variants: `ezusb2-popn-shim` for pop'n music EZUSB2

**Serial Communication:**
- RS-232 Serial Protocol - Legacy hardware communication
  - Implementation: `src/main/hooklib/rs232.c`
  - Used by: ACIO devices and legacy cabinet hardware

**USB:**
- libusb - USB device communication (CH341 serial converters, SMX controllers)
  - Used by: EZUSB drivers, cabinet IO communication
  - Headers: `src/imports/ch341.h` for USB serial converter support

## Input Device Integration

**Game Controllers:**
- ViGEm (Virtual Gamepad Emulation) - Virtual XBOX controller exposure
  - SDK/Client: `src/imports/import_*_indep_ViGEmClient.def`
  - Implementation: `src/main/vigem-ddrio/`, `src/main/vigem-iidxio/`, `src/main/vigem-sdvxio/`, `src/main/vigemstub/`
  - Purpose: Expose bemanitools IO APIs as XBOX controllers for cabinet simulation

- DirectInput - Windows native input API
  - Implementation: `src/main/dinput/`
  - Used by: Generic input handling, controller support

- StepMania X (SMX) - StepMania cabinet controller protocol
  - SDK/Header: `src/imports/SMX.h`
  - Implementation: `src/main/ddrio-smx/`
  - Purpose: DDR cabinet IO support via StepMania controllers

- CH341 - USB serial converter for cabinet hardware
  - SDK/Header: `src/imports/ch341.h`
  - Used by: Legacy serial cabinet communication

**Input Drivers:**
- ASIO - Audio Stream Input/Output
  - Implementation: `src/main/asio/`
  - Purpose: Audio/music game timing synchronization

## Data Storage & Configuration

**Local File System:**
- Configuration files stored in user AppData directory: `C:\Users\<username>\AppData\Roaming\DJHACKERS`
- NVRAM/game data - File-based game state persistence
  - Implementation: `src/main/hooklib/memfile.c` for memory-mapped files
  - Used by: All game hooks for persistent game state

**Game Configuration:**
- Text-based INI-like configuration files (.conf format)
- Configuration management: `src/main/cconfig/` for configuration parsing
- Dynamic configuration tool: `config.exe` for input/output setup

## Card Reader Integration

**EAMUSE Card Readers:**
- ICCA Protocol - Konami card reader communication
  - Driver: `eamio`, `eamio-icca`
  - Implementation: `src/main/eamio/`, `src/main/eamio-icca/`, `src/main/aciodrv/icca.c`
  - Testing: `eamiotest.exe`
  - Configuration: `src/main/eamio-icca/config-icc.c` for reader configuration

## Monitoring & Observability

**Logging:**
- Console/file-based logging
  - Implementation: `src/main/util/log.c`
  - Used by: All modules for debug/error reporting

**Performance Analysis:**
- Frame time graphing: `d3d9-frame-graph-hook.dll`
- GPU profiling: `nvgpu.exe` for NVIDIA GPU configuration
- Monitor refresh rate detection: `d3d9-monitor-check.exe`

**Game-Specific Debugging:**
- Reverse engineering tools: OllyDbg support
- Render call tracing: apitrace integration (external tool)
- IO protocol analyzers: Module-specific test executables

## CI/CD & Deployment

**Build Automation:**
- GitHub Actions - Automated build pipeline
  - Workflows: `.github/workflows/build-master.yaml`, `.github/workflows/build-tag.yaml`
  - Trigger: master branch commits and version tags (format: `*.*`)
  - Build environment: Ubuntu 22.04

**Containerized Builds:**
- Docker - Cross-compilation in standardized environment
  - Build image: `Dockerfile.build` (Debian 11.6-slim + mingw-w64)
  - Dev image: `Dockerfile.dev` (adds clang-format, mdformat)
  - Base: `debian:11.6-slim@sha256:f7d141c1ec6af549958a7a2543365a7829c2cdc4476308ec2e182f8a7c59b519`

**Artifact Distribution:**
- GitHub Releases - Binary distribution
  - Release package: ZIP archive with all game-specific and tool binaries
  - Artifact retention: 90 days (on master branch), indefinite (on releases)
  - Publish action: marvinpinto/action-automatic-releases

**Version Control:**
- Git - Source control with embedded commit hash in binaries
  - Versioning: Semantic versioning (e.g., 5.50, 5.49)
  - Changelog: `CHANGELOG.md` maintained per Keep a Changelog format

## Platform Integration

**Windows System:**
- Win32 API - System calls for process, file, registry, device interaction
  - Setup API: `src/main/hooklib/setupapi.c` for device detection
  - Registry: Game configuration persistence
  - Process injection: `inject.exe` for DLL injection into game processes
  - Launcher: `launcher.exe` bootstraps AVS environment

**Game Process Hooking:**
- Process injection mechanism - DLL loading into target game process
  - Implementation: `src/main/inject/`, `src/main/hook/`, `src/main/hooklib/`
  - Methods: SetWindowsHookEx, DLL injection for game API interception

## Memory & Security

**Cryptography:**
- Crypto utilities for game data handling
  - Implementation: `src/main/util/crypto.c`
  - Used by: Game state encryption, PCBID generation

**Security Utilities:**
- PCBID generation: `pcbidgen.exe` for arcade cabinet ID spoofing
- Memory patching: `mempatch-hook.dll` for runtime memory modification

**Testing & Validation:**
- Test frameworks: Custom C-based test suites in `src/test/`
- Wine test runner: `run-tests-wine.sh` for automated testing on Linux

---

*Integration audit: 2026-02-28*
