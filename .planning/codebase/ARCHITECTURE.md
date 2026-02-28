# Architecture

**Analysis Date:** 2026-02-28

## Pattern Overview

**Overall:** Bemanitools 5 uses a **Protocol Emulation Architecture**. Instead of replacing game code (as BT4 did), BT5 emulates the I/O protocols spoken by real arcade hardware (ezusb boards, BIO2 boards, ACIO serial devices, etc.), allowing unmodified game binaries to run with custom controllers and hardware.

**Key Characteristics:**
- Games are patched via injected DLLs that hook Windows APIs and emulate hardware protocols
- Each game requires a version-specific hook DLL (`iidxhook1.dll`, `ddrhook2.dll`, etc.)
- I/O is abstracted through standardized Bemanitools APIs (eamio, iidxio, ddrio, jbio, etc.)
- AVS version-specific builds allow single hook DLL to support multiple arcade revisions
- Hardware is emulated (ezusb, BIO2, ACIO) or replaced with custom controllers (dinput, vigem)

## Layers

**Game Entry Point:**
- Purpose: The running arcade game binary (bm2dx.exe, ddr.exe, etc.)
- Location: Not in repository; target of hooks via `inject.exe`
- Contains: Konami's binary arcade game with unmodified logic
- Depends on: Hooked DLLs (iidxhook, ddrhook, etc.) to provide hardware simulation
- Used by: Bemanitools via DLL injection

**Hook DLLs (Game-Specific Patches):**
- Purpose: Intercept game API calls and provide hardware emulation
- Location: `src/main/iidxhook*/`, `src/main/ddrhook*/`, `src/main/jbhook*/`, `src/main/popnhook*/`, `src/main/sdvxhook*/`, `src/main/bsthook/`
- Contains: Per-game DLL that hooks d3d8/d3d9, file I/O, hardware comms, and provides config UI
- Depends on: Hook infrastructure (`src/main/hook/`), game-specific utils (e.g., `src/main/iidxhook-util/`), API implementations
- Used by: Injected into game process at startup

**Hook Infrastructure:**
- Purpose: Core functionality for API hooking, DLL injection, and process manipulation
- Location: `src/main/hook/`, `src/main/hooklib/`, `src/main/cconfig/`, `src/main/inject/`, `src/main/launcher/`
- Contains: Symbol table patching (`table.c`), I/O hooking (`iohook.c`), D3D9 hooks (`d3d9.c`), config system
- Depends on: Windows API (PEB manipulation, PE headers, API interception)
- Used by: All hook DLLs, launcher, injector

**Bemanitools API Layer:**
- Purpose: Abstract hardware interfaces so multiple implementations can swap without changing game
- Location: `src/main/bemanitools/` (headers only), implementations throughout
- Contains: Standardized APIs: `eamio.h` (card readers), `iidxio.h` (IIDX I/O), `ddrio.h` (DDR I/O), `jbio.h` (jubeat), `popnio.h` (pop'n music), `sdvxio.h` (SOUND VOLTEX)
- Depends on: Nothing; defines contracts only
- Used by: All I/O implementations and hooks

**I/O Implementations:**
- Purpose: Concrete implementations of Bemanitools APIs for various hardware targets
- Location: `src/main/eamio/`, `src/main/iidxio/`, `src/main/ddrio/`, `src/main/jbio/`, `src/main/popnio/`, `src/main/sdvxio/`
- Contains: Dispatcher DLLs that load hardware-specific implementations at runtime
- Depends on: Bemanitools API headers, hardware abstraction layers
- Used by: Games via injected hook DLLs

**Hardware Emulation Layer:**
- Purpose: Simulate real arcade I/O boards (ezusb, BIO2, ACIO, P3IO, P4IO)
- Location: `src/main/ezusb/`, `src/main/ezusb-emu/`, `src/main/ezusb-iidx/`, `src/main/ezusb2/`, `src/main/bio2/`, `src/main/bio2emu/`, `src/main/acio/`, `src/main/acioemu/`, `src/main/p3io/`, `src/main/p3ioemu/`, `src/main/p4io/`
- Contains: Wire protocol implementations (serial/USB), message parsing, state machines
- Depends on: Windows API (comms), hooklib (adapter, rs232)
- Used by: Emulation DLLs loaded by I/O implementations

**Hardware Adapter Layer:**
- Purpose: Bridge emulated protocols with actual input devices or native controller interfaces
- Location: `src/main/hooklib/` (setupapi, adapter, rs232), `src/main/dinput/`, `src/main/vigem-*/`
- Contains: SetupAPI device enumeration, RS232 serial comms, DirectInput wrappers, ViGEm (Xbox controller) emulation
- Depends on: Windows API, underlying input system
- Used by: Hardware emulation layer to interact with system

**Input Abstraction:**
- Purpose: Accept input from various sources (arcade controls, PC gamepad, keyboard, mouse)
- Location: `src/main/geninput/`, `src/main/dinput/`, `src/main/vigem-*/`
- Contains: Keyboard/mouse input (`geninput`), DirectInput wrappers, Xbox controller emulation
- Depends on: Windows API (DirectInput, ViGEm kernel driver)
- Used by: Hardware emulators and I/O implementations

**Configuration System:**
- Purpose: Provide UI and file persistence for runtime settings
- Location: `src/main/config/`, `src/main/cconfig/`, game-specific config in each hook (config-*.c, config-*.h)
- Contains: Config file parsers, UI forms (e.g., `config.exe` for input/output setup), command-line option handling
- Depends on: Bemanitools APIs, Windows API
- Used by: Hook DLLs during initialization

**Utility Libraries:**
- Purpose: Shared functionality across all modules
- Location: `src/main/util/`, `src/main/mm/`, `src/main/thread/`, `src/main/avs-util/`, `src/main/d3d9-util/`, `src/main/security/`
- Contains: Logging, threading, memory management, D3D9 helpers, security patches
- Depends on: Windows API and C stdlib
- Used by: Every other layer

**Testing & Auxiliaries:**
- Purpose: Emulation of hardware for testing, hardware diagnostics, and non-game tools
- Location: `src/main/*emu/`, `src/main/*test`, `src/main/launcher/`, `src/main/pcbidgen/`, `src/main/nvgpu/`
- Contains: Standalone emulators, device testing tools, Konami AVS launcher, Nvidia GPU config, PCB ID generator
- Depends on: Respective layers above
- Used by: Developers and arcade operators

## Data Flow

**Game Launch & Hook Injection:**

1. User runs `inject.exe` with game binary and hook DLL name
2. `inject.exe` spawns the game process suspended
3. Hook infrastructure patches game's import table to load hook DLL
4. Game resumes; game loader imports injected hook DLL
5. Hook DLL's `DllMain()` executes, initializing all subystems

**Game Initialization (from iidxhook1 example):**

1. Hook DLL intercepts `OpenProcess()` to detect game startup
2. Hook loads config file (iidxhook-*.conf) via `cconfig` subsystem
3. Hook sets up graphics hooks for D3D9 (or d3d8-to-d3d9 wrapper)
4. Hook initializes ezusb emulator with message nodes (serial, FPGA, security, coin)
5. Hook registers with `iidxio` API implementation; game can now call standard I/O functions
6. Config UI (cconfig) becomes available if enabled
7. Game runs; all I/O calls route through bemanitools APIs to emulators

**I/O Request Path:**

1. Game calls `iidxio_poll()` (or equivalent API function)
2. `iidxio` dispatcher DLL routes to loaded implementation (e.g., `iidxio-ezusb`)
3. Emulator implementation translates to protocol messages (e.g., ezusb serial packets)
4. Message routed to hardware emulator (e.g., `ezusb-iidx-emu`)
5. Emulator parses message, updates internal state or calls input layer
6. Input layer reads from controller source (geninput, dinput, or actual device)
7. Response returned back up the stack to game

**State Management:**

- **Game State:** Managed by game binary; bemanitools does not store game state
- **Hardware State:** Stored in emulator nodes (e.g., ezusb SRAM, BIO2 state machines)
- **Configuration:** Loaded from `.conf` files at hook initialization; read-only during gameplay
- **Input State:** Polled from input layer on every I/O call; no buffering of previous frames

## Key Abstractions

**Hardware Protocol Emulator:**
- Purpose: Translate I/O API calls to/from hardware wire protocols
- Examples: `src/main/ezusb-iidx/`, `src/main/bio2/`, `src/main/acio/`
- Pattern: Stateful message handlers in DLL; emulate UART/USB comms
- Used by: I/O implementations and testing tools

**Message Node Architecture:**
- Purpose: Decompose complex board emulation into pluggable modules
- Examples: `ezusb-iidx` has separate nodes for serial, FPGA, security plug, coin mech, lighting
- Pattern: Nodes registered in a dispatch table; `DllMain` instantiates and links them
- Used by: `ezusb-iidx-emu`, `ezusb2-iidx-emu` (allows code reuse, easier testing)

**Hook Dispatcher DLL:**
- Purpose: Load hardware-specific implementation at runtime based on config
- Examples: `iidxio.dll`, `ddrio.dll`, `eamio.dll`
- Pattern: Exports standard API functions; internally loads `.dll` specified in config at first call
- Used by: Games via Bemanitools API

**AVS Version Abstraction:**
- Purpose: Support multiple Konami AVS arcade system versions with single hook
- Pattern: GNUmakefile builds hook with `-DAVS_VERSION=X` for each version (0, 803, 1002, 1101, etc.)
- Used by: Compiled object files to select correct AVS import definitions and APIs

**Config Subsystem (cconfig):**
- Purpose: Unified configuration framework for all hooks and implementations
- Pattern: `.conf` files with `[section]` headers and `key=value` pairs; parsed at runtime
- Used by: Hook DLLs to load game-specific, graphics, I/O, security, and eamuse settings

## Entry Points

**Main Entry:**
- Location: Game executable (e.g., `bm2dx.exe`)
- Triggers: User runs game with `inject.exe`
- Responsibilities: Runs arcade game; calls bemanitools APIs for I/O

**Hook DLL Entry:**
- Location: `src/main/iidxhook1/dllmain.c` (and similar for other games)
- Triggers: Game loader imports DLL after injection
- Responsibilities: Parse config, setup graphics hooks, initialize hardware emulators, register with API implementations

**Launcher Entry:**
- Location: `src/main/launcher/` (Windows GUI and command-line launcher)
- Triggers: User runs `launcher.exe` instead of `inject.exe` manually
- Responsibilities: Set up AVS environment, load AVS security dongle config, inject hook DLL, launch game

**Inject Utility Entry:**
- Location: `src/main/inject/` (command-line DLL injector)
- Triggers: User runs `inject.exe game.exe hook.dll [options]`
- Responsibilities: Suspend game, patch import table, resume game with DLL loaded

**Testing/Tool Entries:**
- Location: Various `*test`, `*emu`, tool directories
- Triggers: User runs standalone `.exe` from tools.zip
- Responsibilities: Emulate hardware for testing, run diagnostics, manage devices

## Error Handling

**Strategy:** Errors are logged but rarely fatal; games attempt to continue with degraded I/O.

**Patterns:**

- **I/O Failures:** If a hardware call fails, emulator returns "not ready" or "no card" state; game typically shows error on display or waits
- **Config Errors:** Missing or malformed `.conf` file uses hardcoded defaults; warning logged
- **Hook Initialization:** If hook fails to initialize, game may run unhooked (no I/O); logged as critical
- **Protocol Errors:** Unexpected messages in protocol ignored or logged; emulator continues with last known state

## Cross-Cutting Concerns

**Logging:** Implemented via `src/main/util/log.h`; each module calls `log_info()`, `log_warning()`, `log_fatal()`. Game's log file (typically `log.txt`) aggregates all output.

**Validation:** Input data from hardware/network validated before use. Config file values range-checked. Game command validation left to game itself.

**Authentication:** Konami AVS security system handled by `security/` module; emulated by launcher and hooks. PCBID generation (`pcbidgen`) used to create mock IDs for unlicensed hardware.

**Thread Safety:** Each hook DLL runs in game process (single thread context for game, potentially multiple for emulators). Emulators use Windows synchronization primitives (mutexes, events) where needed. I/O calls are synchronous (blocking) unless marked otherwise.
