# Codebase Structure

**Analysis Date:** 2026-02-28

## Directory Layout

```
bemanitools/
├── src/
│   ├── main/               # All Bemanitools code: hooks, APIs, emulators, utilities
│   │   ├── bemanitools/    # Bemanitools public API headers (eamio.h, iidxio.h, etc.)
│   │   ├── hook/           # Core hooking infrastructure (symbol patching, D3D9, I/O)
│   │   ├── hooklib/        # Hooking utilities (RS232, SetupAPI, adapters, memfile)
│   │   ├── inject/         # DLL injector executable
│   │   ├── launcher/       # Konami AVS launcher
│   │   ├── cconfig/        # Configuration file parsing library
│   │   ├── config/         # Configuration UI executable
│   │   ├── util/           # Logging, threading, memory, and utility functions
│   │   ├── security/       # Konami security emulation
│   │   ├── mm/             # Memory management
│   │   ├── thread/         # Thread utilities
│   │   ├── nv/             # NVIDIA driver utilities
│   │   │
│   │   ├── iidxhook*/      # Beatmania IIDX hooks (iidxhook1-9, iidxhook5-cn)
│   │   ├── iidxhook-util/  # Shared utilities for IIDX hooks (config parsing, graphics, clocking)
│   │   ├── iidxio/         # IIDX I/O dispatcher DLL
│   │   ├── iidxio-ezusb/   # IIDX I/O via ezusb emulation
│   │   ├── iidxio-ezusb2/  # IIDX I/O via ezusb2 emulation
│   │   ├── iidxio-bio2/    # IIDX I/O via BIO2 emulation
│   │   ├── iidxio-async/   # IIDX I/O with async threading
│   │   ├── iidx-ezusb-exit-hook/    # Exit game with button combo on ezusb
│   │   ├── iidx-ezusb2-exit-hook/   # Exit game with button combo on ezusb2
│   │   ├── iidx-bio2-exit-hook/     # Exit game with button combo on BIO2
│   │   ├── iidx-irbeat-patch/       # Patch IR beat timing
│   │   ├── iidxhook-d3d9/           # D3D9 graphics utilities for IIDX
│   │   ├── ezusb/          # Cypress ezusb chip communication
│   │   ├── ezusb-emu/      # Generic ezusb emulation framework
│   │   ├── ezusb-iidx/     # IIDX-specific ezusb hardware protocol
│   │   ├── ezusb-iidx-emu/ # IIDX ezusb message nodes
│   │   ├── ezusb-iidx-16seg-emu/   # IIDX 16-segment display nodes
│   │   ├── ezusb-iidx-fpga-flash/  # FPGA firmware flashing tool
│   │   ├── ezusb-iidx-sram-flash/  # SRAM content flashing tool
│   │   ├── ezusb2/         # Cypress ezusb FX2 chip communication
│   │   ├── ezusb2-emu/     # Generic ezusb2 emulation framework
│   │   ├── ezusb2-iidx/    # IIDX-specific ezusb2 protocol
│   │   ├── ezusb2-iidx-emu/        # IIDX ezusb2 message nodes
│   │   ├── ezusb-tool/     # Generic ezusb diagnostic tool
│   │   ├── ezusb2-tool/    # Generic ezusb2 diagnostic tool
│   │   ├── ezusb2-dbg-hook/        # Debug hook for ezusb2 messages
│   │   │
│   │   ├── ddrhook1/       # DDR X hook
│   │   ├── ddrhook2/       # DDR X2+ and later hooks
│   │   ├── ddrhook-util/   # Shared utilities for DDR hooks
│   │   ├── ddrio/          # DDR I/O dispatcher DLL
│   │   ├── ddrio-p3io/     # DDR I/O via P3IO emulation
│   │   ├── ddrio-mm/       # DDR I/O via motion board (MM)
│   │   ├── ddrio-smx/      # DDR I/O via StepMania eXtreme (SMX) pad
│   │   ├── ddrio-async/    # DDR I/O with async threading
│   │   ├── ddriotest/      # DDR I/O testing tool
│   │   ├── p3io/           # P3IO board communication
│   │   ├── p3ioemu/        # P3IO board emulation
│   │   ├── p3iodrv/        # P3IO driver
│   │   ├── p3io-ddr-tool/  # DDR cabinet hardware testing tool
│   │   ├── p4io/           # P4IO board communication
│   │   ├── p4iodrv/        # P4IO driver
│   │   ├── p4ioemu/        # P4IO board emulation
│   │   │
│   │   ├── jbhook1/        # jubeat 1-2 hook
│   │   ├── jbhook2/        # jubeat knit-copious hook
│   │   ├── jbhook3/        # jubeat saucer+ and clan hook
│   │   ├── jbhook-util/    # Shared utilities for jubeat hooks
│   │   ├── jbhook-util-p3io/       # jubeat hook utilities for P3IO
│   │   ├── jbio/           # jubeat I/O dispatcher DLL
│   │   ├── jbio-p4io/      # jubeat I/O via P4IO emulation
│   │   ├── jbio-magicbox/  # jubeat I/O via MagicBox controller
│   │   ├── jbiotest/       # jubeat I/O testing tool
│   │   │
│   │   ├── popnhook1/      # pop'n music hook
│   │   ├── popnhook-util/  # Shared utilities for pop'n music hooks
│   │   ├── popnio/         # pop'n music I/O dispatcher DLL
│   │   ├── ezusb2-popn/    # pop'n music specific ezusb2
│   │   ├── ezusb2-popn-emu/        # pop'n music ezusb2 emulation
│   │   ├── ezusb2-popn-shim/       # pop'n music ezusb2 shim layer
│   │   │
│   │   ├── sdvxhook/       # SOUND VOLTEX (1-4) hook
│   │   ├── sdvxhook2/      # SOUND VOLTEX (5+) hook
│   │   ├── sdvxhook2-cn/   # SOUND VOLTEX Chinese region hook
│   │   ├── sdvxio/         # SOUND VOLTEX I/O dispatcher DLL
│   │   ├── sdvxio-bio2/    # SOUND VOLTEX I/O via BIO2 emulation
│   │   ├── sdvxio-kfca/    # SOUND VOLTEX I/O via KFCA reader
│   │   │
│   │   ├── bsthook/        # BeatStream hook
│   │   ├── bstio/          # BeatStream I/O dispatcher DLL
│   │   │
│   │   ├── bio2/           # BIO2 board communication
│   │   ├── bio2drv/        # BIO2 driver
│   │   ├── bio2emu/        # BIO2 board emulation
│   │   ├── bio2emu-iidx/   # BIO2 IIDX-specific emulation
│   │   │
│   │   ├── acio/           # ACIO (Konami serial) board communication
│   │   ├── aciodrv/        # ACIO driver
│   │   ├── aciodrv-proc/   # ACIO processor
│   │   ├── acioemu/        # ACIO board emulation
│   │   ├── aciomgr/        # ACIO manager
│   │   ├── aciotest/       # ACIO testing tool
│   │   │
│   │   ├── eamio/          # Card reader I/O dispatcher DLL
│   │   ├── eamio-icca/     # Card reader via ICCA interface
│   │   ├── eamiotest/      # Card reader testing tool
│   │   │
│   │   ├── extio/          # Extended I/O interface
│   │   ├── extiodrv/       # Extended I/O driver
│   │   ├── extiotest/      # Extended I/O testing tool
│   │   │
│   │   ├── geninput/       # Generic keyboard/mouse input abstraction
│   │   ├── dinput/         # DirectInput wrapper
│   │   ├── vigem-ddrio/    # DDR I/O via Xbox controller (ViGEm)
│   │   ├── vigem-iidxio/   # IIDX I/O via Xbox controller (ViGEm)
│   │   ├── vigem-sdvxio/   # SOUND VOLTEX I/O via Xbox controller (ViGEm)
│   │   ├── vigemstub/      # ViGEm stub driver
│   │   │
│   │   ├── d3d9-monitor-check/     # Detect monitor refresh rate
│   │   ├── d3d9-util/      # D3D9 utilities
│   │   ├── d3d9-frame-graph-hook/  # D3D9 frame graph debugging
│   │   ├── d3d9exhook/     # Direct3D 9Ex hooking
│   │   │
│   │   ├── camhook/        # Camera input hook (for arcade cameras)
│   │   ├── mempatch-hook/  # Memory patching hook for game binary
│   │   ├── imgui/          # ImGui GUI framework
│   │   ├── imgui-bt/       # ImGui Bemanitools theme
│   │   ├── imgui-debug/    # ImGui debug overlay for games
│   │   │
│   │   ├── vefxio/         # Video effects I/O dispatcher
│   │   ├── pcbidgen/       # Konami PCBID generator tool
│   │   ├── nvgpu/          # NVIDIA GPU configuration tool
│   │   ├── asio/           # Audio Stream Input/Output
│   │   ├── avs-util/       # Konami AVS utilities
│   │   ├── unicorntail/    # Bemanitools bootstrap utility
│   │   ├── imports/        # External DLL import definitions (AVS, NVAPI, ViGEm kernel)
│   │   └── bemanitools/    # (see above)
│   │
│   └── test/               # Unit and integration tests
│       ├── test/           # Main test runner
│       ├── cconfig/        # Configuration parser tests
│       ├── d3d9hook/       # D3D9 hook tests
│       ├── iidxhook-util/  # IIDX hook utility tests
│       ├── iidxhook8/      # IIDX 8 specific tests
│       ├── security/       # Security emulation tests
│       ├── util/           # Utility function tests
│       └── iidxhook/       # Legacy IIDX hook tests
│
├── doc/                    # Documentation
│   ├── iidxhook/          # IIDX hook setup and configuration guides
│   ├── ddrhook/           # DDR hook setup and configuration guides
│   ├── jbhook/            # jubeat hook setup and configuration guides
│   ├── popnhook/          # pop'n music hook setup and configuration guides
│   ├── vigem/             # ViGEm (Xbox controller) setup guides
│   ├── tools/             # Standalone tool documentation
│   ├── api.md             # Bemanitools public APIs (eamio, iidxio, ddrio, etc.)
│   ├── architecture.md    # High-level architecture overview
│   ├── development.md     # Developer setup and build guide
│   └── release-process.md # Release procedures
│
├── dist/                   # Distribution files and templates
│   ├── iidx/              # IIDX batch files, config templates, documentation
│   └── ...
│
├── build/                  # Build output directory (generated at build time)
│   ├── bin/               # Compiled .exe and .dll files
│   ├── obj/               # Object files (.o)
│   ├── dep/               # Dependency files (.d)
│   ├── zip/               # Distributable .zip packages
│   └── docker/            # Docker build artifacts
│
├── scripts/               # Build and utility scripts
├── GNUmakefile           # Main build configuration
├── Module.mk             # Module definitions and build rules
├── Dockerfile.build      # Docker image for CI/CD builds
├── Dockerfile.dev        # Docker image for development
├── .clang-format         # Code formatting rules
└── README.md             # Project overview
```

## Directory Purposes

**`src/main/`:**
- Purpose: All production code (hooks, APIs, emulators, tools)
- Contains: DLL and .exe implementations organized by game series or feature area
- Key files: `*.c`, `*.cpp`, `*.h` (headers), `*.def` (DLL exports), `Module.mk` (build config)

**`src/test/`:**
- Purpose: Unit and integration tests
- Contains: Test runners, mock implementations, test suites
- Key files: Test `.c` files with `TEST_*` functions, test configurations

**`doc/`:**
- Purpose: User-facing and developer documentation
- Contains: Game-specific setup guides, API docs, architecture notes, release procedures
- Key files: `.md` markdown documentation

**`dist/`:**
- Purpose: Distribution templates and batch scripts
- Contains: Sample config files, game startup batch files, documentation for end users
- Key files: `.bat` batch scripts, `.conf` config templates, `.txt` docs

**`build/` (generated):**
- Purpose: Compilation output
- Contains: Compiled binaries, object files, build artifacts, distributable packages
- Key files: `.exe`, `.dll`, `.zip` distributions

## Key File Locations

**Entry Points:**

- `src/main/inject/` - Command-line DLL injection utility
- `src/main/launcher/` - GUI AVS launcher for running games with hooks
- `src/main/config/config.c` - Configuration UI for bemanitools APIs
- Game-specific hooks (e.g., `src/main/iidxhook1/dllmain.c`) - Injected into game process

**Configuration:**

- `src/main/cconfig/` - Configuration file parsing library; used by all hooks
- `dist/iidx/iidxhook-*.conf` - Example IIDX hook config files
- `dist/ddrhook/ddrhook-*.conf` - Example DDR hook config files
- Hook-specific config in each hook directory (e.g., `src/main/iidxhook1/config-iidxhook1.c`)

**Core Infrastructure:**

- `src/main/hook/table.c` - Symbol patching and API hooking
- `src/main/hook/d3d9.c` - Direct3D 9 hooking for graphics
- `src/main/hook/iohook.c` - Windows I/O and file system hooking
- `src/main/hooklib/rs232.c` - Serial port communication
- `src/main/hooklib/setupapi.c` - Device enumeration via Windows SetupAPI
- `src/main/util/log.c` - Logging system (output to game's log file)
- `src/main/util/thread.c` - Thread creation and synchronization

**Bemanitools Public APIs:**

- `src/main/bemanitools/eamio.h` - Card reader interface
- `src/main/bemanitools/iidxio.h` - IIDX I/O interface
- `src/main/bemanitools/ddrio.h` - DDR I/O interface
- `src/main/bemanitools/jbio.h` - jubeat I/O interface
- `src/main/bemanitools/popnio.h` - pop'n music I/O interface
- `src/main/bemanitools/sdvxio.h` - SOUND VOLTEX I/O interface
- `src/main/bemanitools/glue.h` - Logging and utility macros

**I/O Implementations:**

- `src/main/iidxio/iidxio.c` - IIDX I/O dispatcher; loads hardware-specific DLL at runtime
- `src/main/iidxio-ezusb/` - IIDX I/O using ezusb emulation
- `src/main/ddrio/ddrio.c` - DDR I/O dispatcher
- `src/main/eamio/eamio.c` - Card reader dispatcher

**Hardware Emulation (IIDX ezusb example):**

- `src/main/ezusb/` - Low-level Cypress ezusb chip communication
- `src/main/ezusb-emu/` - Generic ezusb emulation state machine
- `src/main/ezusb-iidx/ezusb-iidx.h` - IIDX-specific message formats
- `src/main/ezusb-iidx-emu/` - IIDX message node implementations (serial, FPGA, security, lighting, coin)

**Game-Specific Hooks (IIDX example):**

- `src/main/iidxhook1/dllmain.c` - Hook initialization and setup
- `src/main/iidxhook-util/d3d9.c` - Shared graphics utilities
- `src/main/iidxhook-util/config-*.c` - Shared config parsing
- `src/main/iidxhook-util/eamuse.c` - Shared eamuse (network) utilities

**Testing:**

- `src/test/test/` - Main test runner
- `src/test/cconfig/` - Config parser tests
- Tests are compiled as `.exe` and run by `run-tests-wine.sh` on Windows/Wine

## Naming Conventions

**Files:**

- C source: `.c` (e.g., `dllmain.c`, `config-gfx.c`)
- C headers: `.h` (e.g., `iidxhook.h`, `config.h`)
- DLL definitions: `.def` (e.g., `iidxhook1.def`) - lists exported functions
- Resource scripts: `.rc` (e.g., `config.rc`) - for UI resources and version info
- Configuration: `.conf` (e.g., `iidxhook-09.conf`) - runtime config files
- Module build rules: `Module.mk` (e.g., `src/main/iidxio/Module.mk`)

**Directories:**

- Hook directories: `{game}hook` or `{game}hook{version}` (e.g., `iidxhook1`, `ddrhook2`)
- I/O implementations: `{game}io` (e.g., `iidxio`, `ddrio`)
- Hardware emulators: `{hardware}-emu` or `{hardware}` (e.g., `ezusb-iidx-emu`, `bio2`)
- Utilities: `{name}-util` (e.g., `iidxhook-util`, `ddrhook-util`)
- Tools: Named by purpose (e.g., `ddriotest`, `pcbidgen`, `nvgpu`)

**Functions:**

- Public API functions (exported): `{module}_{function}` (e.g., `eam_io_init`, `iidx_io_poll`)
- Internal static functions: `{purpose}_{detail}` (e.g., `iidxhook1_setup_d3d9_hooks`)
- Windows hooks: `my_{original_function}` (e.g., `my_OpenProcess`)
- Config callbacks: `{config}_handle_{key}` (e.g., `config_gfx_handle_window_width`)

## Where to Add New Code

**New Game Hook:**
- Create `src/main/{game}hook{version}/` directory
- Implement `dllmain.c` with `DLL_PROCESS_ATTACH` handler
- Create `config-{game}hook{version}.c/h` for config parsing
- Create `Module.mk` with build rules pointing to `src/main/{game}hook{version}`
- Add `include src/main/{game}hook{version}/Module.mk` to root `Module.mk`
- Add config template to `dist/{game}/{game}hook-*.conf`
- Add documentation to `doc/{game}hook/`

**New I/O Implementation:**
- Create `src/main/{game}io-{backend}/` directory (e.g., `iidxio-bio2`)
- Implement functions from `src/main/bemanitools/{game}io.h`
- Load into bemanitools API dispatcher (e.g., `src/main/iidxio/iidxio.c`)
- Create `Module.mk` and test if applicable
- Document in `doc/api.md`

**New Hardware Emulator:**
- Create `src/main/{hardware}-emu/` directory
- Implement message processing state machine
- Register in parent DLL's initialization (typically in a hook's `dllmain.c`)
- Create `Module.mk` and link into appropriate hook or I/O implementation
- Add test if complex logic

**Utility or Tool:**
- Create `src/main/{tool-name}/` directory
- Implement `main.c` with Windows `int main()` entry point
- Create `Module.mk` declaring as `.exe`
- Document in `doc/tools/{tool-name}.md`

**Configuration:**
- Game-specific config: Add fields to hook's `config-*.c/h` files
- Shared config: Add to `src/main/{game}hook-util/config-*.c/h` and share among version variants
- Config UI: Edit `src/main/config/` if modifying the generic config tool
- Distribution templates: Add example to `dist/{game}/{game}hook-*.conf`

## Special Directories

**`src/main/bemanitools/`:**
- Purpose: Public API headers defining contracts for I/O implementations
- Generated: No (manually maintained)
- Committed: Yes
- Files: Header-only; no implementation

**`src/main/imports/`:**
- Purpose: Import definitions (.def files) for external DLLs (AVS, NVAPI, ViGEm)
- Generated: No (manually maintained)
- Committed: Yes
- Files: `.def` files consumed by `dlltool` to create import libraries

**`build/` (entire directory):**
- Purpose: Compilation output
- Generated: Yes (from `make` or `make clean`)
- Committed: No (in `.gitignore`)
- Subdirs: `bin/`, `obj/`, `dep/`, `zip/`

**`dist/`:**
- Purpose: Distribution templates and batch scripts for end users
- Generated: No (manually maintained)
- Committed: Yes
- Used by: Packaged in distributable `.zip` files

**`doc/`:**
- Purpose: User and developer documentation
- Generated: Partially (some generated from comments via doc tools, not currently in use)
- Committed: Yes
- Subdirs: Organized by game series or topic

---

*Structure analysis: 2026-02-28*
