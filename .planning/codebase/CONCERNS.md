# Codebase Concerns

**Analysis Date:** 2026-02-28

## Tech Debt

**USB Drive Hotplugging Emulation:**
- Issue: DDR USB memory emulation (`src/main/ddrhook-util/usbmem.c:246`) uses a crude approach without proper hotplug handling. Once a device errors, it stays disabled until game restart—not authentic cabinet behavior.
- Files: `src/main/ddrhook-util/usbmem.c` (lines 246-268)
- Impact: Players cannot simulate ejecting/reinserting USB drives; limits testing of USB failure scenarios
- Fix approach: Implement keybind-driven hotplug simulation to toggle device state without restart

**Gray Arrow Rendering on NVIDIA Cards:**
- Issue: Known graphics issue on NVIDIA GPUs with gray arrows in DDR (applies to X3 as well)
- Files: `src/main/ddrhook-util/gfx.c` (line 1)
- Impact: Visual artifacts on NVIDIA systems; affects player experience
- Fix approach: Investigate D3D9 rendering state management specific to NVIDIA drivers; may require driver-specific workarounds

**FPGA Command Naming Inconsistency:**
- Issue: FPGA command enums in IIDX ezusb have misleading names pending refactoring
- Files: `src/main/ezusb-iidx/fpga-cmd.h` (lines 6, 8)
- Impact: Code maintainability; confusing naming makes modifications error-prone
- Fix approach: Rename commands to reflect actual firmware behavior (e.g., `reset`, `check`)

**Incomplete D3D9 Hook Test:**
- Issue: Test vertex buffer creation and rendering not implemented
- Files: `src/test/d3d9hook/main.c` (line 133)
- Impact: D3D9 rendering path untested; potential gaps in graphics hook coverage
- Fix approach: Implement actual vertex buffer test cases with rendering validation

## Known Bugs

**P4IO Device Path Mismatch:**
- Symptoms: Game receives `\\p4io\\p4io` instead of expected `\\p4io` from setupapi calls
- Files: `src/main/p4ioemu/device.c` (line 268)
- Trigger: Any game requesting P4IO device via setupapi when P4IO emulation is active
- Workaround: Currently hardcoded to accept the doubled path; not ideal but functional
- Fix approach: Investigate setupapi path construction in iohook layer to prevent duplication at source

**Render Target 0 NULL Setting:**
- Symptoms: Setting render target 0 to NULL may be illegal in D3D9 under certain conditions
- Files: `src/main/popnhook1/d3d9.c` (line 77)
- Trigger: Specific pop'n music rendering scenarios
- Workaround: Unknown; needs investigation
- Fix approach: Test against D3D9 documentation and NVIDIA/AMD driver behavior; may need conditional logic

## Security Considerations

**Hardcoded ICCA Reader Key:**
- Risk: Security reader key in ICCA card reader emulation is not randomized (always `0x14243444`)
- Files: `src/main/acioemu/icca.c` (line 509)
- Current mitigation: Private server only; not exposed to network
- Recommendations: Use PRNG to generate reader key per session/boot; add environment variable to override for testing

**Memory Patching Capabilities:**
- Risk: `mem_nop()` allows arbitrary memory patching via page protection manipulation; `mempatch-hook` can patch raw memory addresses via config files
- Files: `src/main/util/mem.c`, mempatch-hook implementation
- Current mitigation: Local tool only; no remote execution
- Recommendations: Document security implications clearly; consider adding checksum validation of patched regions to detect corruption

**Legacy NVAPI Usage:**
- Risk: Code uses deprecated NVAPI functions (deprecated since 290+); future NVIDIA driver updates could break functionality
- Files: `src/imports/nvapi/nvapi.h`, `src/main/nvgpu/main.c`
- Current mitigation: None; falls back to older driver behavior
- Recommendations: Migrate to modern NVAPI equivalents; provide fallback for systems with latest drivers

**Unvalidated Buffer Operations:**
- Risk: Multiple uses of `strncpy()` without null-termination guarantee; reliance on hardcoded buffer sizes
- Files: `src/main/bsthook/kfca.c`, `src/main/ddrhook-util/spike.c`, various others (160+ malloc/calloc, 302 memcpy/strcpy uses)
- Current mitigation: Static buffer sizes; xmalloc/xcalloc abort on failure
- Recommendations: Audit all string operations for off-by-one errors; consider safe_strcpy wrapper

## Performance Bottlenecks

**No Async IO for Network Emulation:**
- Problem: EZUSB serial communication processing and card reader operations are synchronous
- Files: `src/main/ezusb-iidx-emu/node-serial.c` (1374 lines), ICCA/card emulation code
- Cause: Blocking I/O during frame rendering; can cause frame hitches on slower hardware
- Improvement path: Implement async queue processing for emulated device commands; may already be partially addressed by iidxio-async wrapper

**D3D9 Monitor Check Tool Overhead:**
- Problem: Full rendering test on every check
- Files: `src/main/d3d9-monitor-check/`
- Cause: Runs complete D3D9 setup even for simple refresh rate queries
- Improvement path: Lightweight detection mode for basic queries; full render test optional

**Configuration File Parsing:**
- Problem: Entire config tree parsed and validated upfront; large configs with many sections cause startup delay
- Files: `src/main/config/`, cconfig library
- Cause: Linear parsing without lazy loading
- Improvement path: Lazy-load config sections on demand; cache parsed sections

## Fragile Areas

**IO Hooking Infrastructure:**
- Files: `src/main/hook/iohook.c` (958 lines), `src/main/hook/d3d9.c` (934 lines), related hook implementations
- Why fragile: Windows kernel API dependency; overlapped I/O state machine; minimal test coverage (only 21 test files for 150K LOC)
- Safe modification: Coordinate changes across hook/table.h; all IRP state transitions must be validated with actual Windows versions (XP, 7, 10+)
- Test coverage: Critical path (CreateFileW, ReadFile, WriteFile) tested; IOCTL edge cases untested

**EZUSB Firmware Emulation:**
- Files: `src/main/ezusb-iidx-emu/`, `src/main/ezusb-emu/`, FPGA command handling
- Why fragile: Protocol reverse-engineered from real hardware; minor bit-flip or timing difference breaks sync
- Safe modification: Protocol changes must be validated against real board dumps; serial frame format is critical—any change requires hardware validation
- Test coverage: Basic command/response pairs tested; full state machine scenarios untested

**Game-Specific DLL Hooks:**
- Files: `src/main/iidxhook-util/`, `src/main/ddrhook-util/`, `src/main/jbhook/`, etc. (126 subdirectories in src/main)
- Why fragile: Each hook version targets a specific game binary; changes to one hook can affect shared libraries
- Safe modification: Isolate changes to single hook version; validate against all supported game versions for that hook series
- Test coverage: Manual arcade hardware testing only; no automated game-specific test suite

**Chart Patching System:**
- Files: `src/main/iidxhook-util/chart-patch.c` (741 lines)
- Why fragile: Binary pattern matching and patching against game memory; version-specific offsets hardcoded
- Safe modification: Pattern changes must be validated against chart dumps for each game version; any offset change requires re-testing all supported charts
- Test coverage: No automated coverage; relies on manual player reports

## Scaling Limits

**Resource Accumulation in Emulated Devices:**
- Current capacity: Each active emulated IO device maintains state (card slots, buffers, sequence numbers); no cleanup on long-running sessions
- Limit: Potential slow creep of memory on 24/7 arcade cabinets; no maximum buffer size enforcement
- Scaling path: Implement device state cleanup/reset on idle periods; add memory accounting to dashboard; set max buffer thresholds

**Configuration Size:**
- Current capacity: Typical config files ~50-200 lines; nested sections supported
- Limit: No enforced upper bound; deeply nested configs not tested (performance scales linearly)
- Scaling path: Lazy-load config sections on demand; add depth limit validation

## Dependencies at Risk

**NVIDIA GPU Driver Compatibility:**
- Risk: Code relies on deprecated NVAPI (deprecated since ~2015); NVIDIA continues removing legacy API support
- Impact: Future driver updates may break NVIDIA GPU configuration features (`nvgpu` tool); falls back silently
- Migration plan: Audit NVAPI usage in `src/main/nvgpu/main.c`; replace with modern NVAPI equivalents or DXGI; prioritize NV12 display format support

**DirectX 9 EOL:**
- Risk: DirectX 9 no longer supported by Microsoft; driver vendors dropping support
- Impact: Games may fail to initialize on future Windows versions; visual corruption on bleeding-edge drivers
- Migration plan: Not feasible for arcade games (source code unavailable); bemanitools already provides compatibility layer via hooks

**Windows XP/7 Compatibility:**
- Risk: Primary targets (XP, Win7) are end-of-life; modern toolchains dropping support
- Impact: Build tools (MinGW) may stop supporting XP ABI; newer VC++ runtimes incompatible
- Migration plan: Maintain frozen toolchain (mingw with -Werror); consider backport to universal Windows binary if possible

## Missing Critical Features

**Hotplug USB Device Simulation:**
- Problem: No way to simulate USB drive removal/reinsertion during gameplay
- Blocks: Proper testing of USB error recovery paths
- Workaround: Restart game

**ICCA Session Randomization:**
- Problem: Card reader key always hardcoded; no true randomness
- Blocks: Proper security testing of card reader protocol
- Workaround: None; acceptable for private server

**Driver-Agnostic Monitor Timing:**
- Problem: No unified way to query actual monitor refresh rate and timing across NVIDIA/AMD/Intel drivers
- Blocks: Automated sync validation; sync configuration stays manual
- Workaround: Manual driver control panel configuration

**Automated Emulated Device Testing:**
- Problem: No test harness for emulated IO (ICCA, EZUSB, ACIO, etc.) beyond manual gameplay
- Blocks: CI/CD validation of device protocol changes
- Workaround: Manual testing on real hardware or cabinet emulator

## Test Coverage Gaps

**Windows API Integration:**
- What's not tested: CreateFileW/A variants with different dwCreationDisposition flags; OVERLAPPED I/O state transitions under contention; GetLastError/SetLastError sequences across exception boundaries
- Files: `src/main/hook/iohook.c`, `src/main/hook/table.c`
- Risk: Subtle concurrency bugs; platform-specific behavior differences between XP/7/10
- Priority: High - affects core stability

**D3D9 Render State Machine:**
- What's not tested: Multiple render targets in flight; state validation across BeginScene/EndScene; device lost/reset recovery paths
- Files: `src/main/hook/d3d9.c`, game-specific D3D9 hooks
- Risk: Rendering corruption under edge conditions; device reset during gameplay
- Priority: High - affects visual output

**Emulated Device Protocol Edge Cases:**
- What's not tested: Out-of-order commands; malformed packets; timeout recovery; bulk transfer interruption
- Files: `src/main/ezusb-iidx-emu/`, `src/main/acioemu/`, `src/main/bio2emu/`
- Risk: Games hang or crash on protocol errors; emulation doesn't match real hardware resilience
- Priority: Medium - affects stability on edge cases

**Configuration File Validation:**
- What's not tested: Circular references in config includes; missing required fields; invalid enum values; type coercion edge cases
- Files: `src/main/cconfig/`, `src/main/config/`
- Risk: Cryptic errors or silent failures on malformed configs
- Priority: Medium - affects user experience

**Memory Safety:**
- What's not tested: Integer overflow in size calculations (xmalloc/malloc); buffer overruns with untrusted input (config parsing, serial data)
- Files: `src/main/util/mem.c`, string utilities
- Risk: Information disclosure or code execution via malformed input
- Priority: Low-Medium - private server only, but good defensive practice

---

*Concerns audit: 2026-02-28*
