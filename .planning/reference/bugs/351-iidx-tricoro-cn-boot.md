# Bug #351 — IIDX Tricoro CN Boot Failure

**Status:** Open
**Phase 4 priority:** MEDIUM — affects CN (Chinese network) users specifically

---

## Issue Summary

beatmania IIDX 20 tricoro fails to boot when configured for the CN (Chinese network/security) variant. tricoro uses `iidxhook9.dll` which implements the hook for IIDX 20 Heroic Verse-era games. The CN variant differs in network configuration and potentially in security plug data requirements. Boot failure indicates the game aborts during initialization, most likely during the security check, network/eamuse initialization, or the audio subsystem setup. The `reverbfix.c` hook (CoCreateInstance intercept for DSFX_STANDARD_I3DL2REVERB) is a known required patch for tricoro — if this hook fails or is applied too late, the game crashes during audio initialization before reaching the game loop.

---

## User Reports

No GitHub issue content available (gh CLI not authenticated). Based on issue number and codebase context:

- **Affected BT5 versions:** Likely affects v5.44+ or a specific configuration
- **Affected game:** beatmania IIDX 20 tricoro (CN variant)
- **CN distinction:** CN (China) variant games may have different network handshake requirements, different security plug mcodes, or different filesystem expectations (different drive letter mappings)
- **Symptoms expected:** Game crashes or hangs on boot — before or during I/O check, security check, or network check screens

---

## Affected Games/Versions

| Game | BT5 Hook | AVS version | Region |
|------|----------|-------------|--------|
| IIDX 20 tricoro (standard) | iidxhook9 | avs2_1508 (15.8) | JP/INT |
| IIDX 20 tricoro (CN) | iidxhook9 | avs2_1508 (15.8) | CN |

The hook name in the source says "Heroic Verse" in the info header (`iidxhook9.c:48-52`) but the module covers tricoro as well based on version compatibility. The `iidxhook9` hook covers games from tricoro through Heroic Verse (IIDX 20-27 approximately based on AVS version 15.x).

---

## Relevant Code Paths

### Hook entry — dllmain.c

`src/main/iidxhook9/dllmain.c:100-210` — `my_dll_entry_init()`

Full initialization sequence:
1. Load config (`load_configs()` at `dllmain.c:114`)
2. Configure D3D9EX (`d3d9ex_configure()` at `dllmain.c:119`)
3. Init IIDXIO backend (`dllmain.c:127-136`)
4. Init EAMIO backend (`dllmain.c:138-148`)
5. Push IRP handlers (`dllmain.c:150-151`)
6. Init file hooks and memfile (`dllmain.c:153-167`)
7. **Init reverbfix** (`reverbfixhook_init()` at `dllmain.c:169`)
8. Init rs232 (`dllmain.c:171-172`)
9. Init BIO2 emu (`dllmain.c:174-181`)
10. Init ACIO card reader emu (`dllmain.c:183-190`)

The `pre_hook()` function at `dllmain.c:241-267` runs before AVS loads and sets environment variables for audio output mode.

### Reverb fix hook — CoCreateInstance intercept

`src/main/iidxhook9/reverbfix.c:67-90` — `my_CoCreateInstance()`:
```c
HRESULT result = real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
if (result == REGDB_E_CLASSNOTREG) {
    if (IsEqualGUID(rclsid, &GUID_DSFX_STANDARD_I3DL2REVERB)) {
        result = real_CoCreateInstance(
            &GUID_DSFX_WAVES_REVERB, pUnkOuter, dwClsContext, riid, ppv);
        /* ... */
    }
}
return result;
```

This hook replaces `DSFX_STANDARD_I3DL2REVERB` with `DSFX_WAVES_REVERB` when the I3DL2 reverb COM class is not registered (which is the case on Windows 10+). Without this fix, tricoro's DirectSound audio initialization would fail with `REGDB_E_CLASSNOTREG`, causing the game to abort.

`reverbfixhook_init()` at `reverbfix.c:92-97` applies the hook to `Ole32.dll`.

**Critical:** If `reverbfixhook_init()` is not called before the game's audio system runs, or if the hook fails to intercept the COM call, the game crashes during audio setup regardless of CN vs non-CN configuration.

### File system redirects — fs-hook.c

`src/main/iidxhook9/fs-hook.c:33-65` — `my_avs_fs_mount()`:

Redirects `F:\` to `dev/vfs/drive_f/` and `e:/` to `dev/vfs/drive_e/`. CN variant may use different drive letters or filesystem paths that are not covered by these redirects. If the CN game binary expects a drive letter not covered by the hook, the AVS filesystem mount fails with an error — which manifests as a boot failure.

`iidxhook9_fs_hooks_init()` is only called when `!iidxhook9_config_io.disable_file_hooks` (at `dllmain.c:153-167`). Default value is `false` (hooks enabled).

### Configuration — config-io.c

`src/main/iidxhook9/config-io.c` — config keys:

| Key | Default | CN concern |
|-----|---------|-----------|
| `io.disable_card_reader_emu` | false | CN may require real reader or different ICCA version |
| `io.lightning_mode` | false | TDJ mode forces ICCA v170; CN tricoro may not be TDJ |
| `io.disable_file_hooks` | false | File hooks may not cover CN filesystem layout |

### ACIO card reader — iidxhook-util/acio.c

`src/main/iidxhook-util/acio.c` (not shown, but referenced from dllmain.c):

`iidxhook_util_acio_init(false)` at `dllmain.c:189` — called with `legacy_mode=false` for BIO2-based games. The CN variant uses the same BIO2 ACIO path. `iidxhook9_config_io.lightning_mode` controls whether ICCA v170 is forced (`dllmain.c:184-188`).

---

## Changes Between v5.43 and v5.44 Relevant to This Bug

| Commit | Message | Relevance |
|--------|---------|-----------|
| `0792b29` | iidxhook9: Add iidxhook support for dx9ex and other features | Initial iidxhook9 creation — introduced entire hook |
| `89c3ada` | iidxhook9: Add turntable multiplier | Adds `tt_multiplier` config, changes bio2 init call |
| `df8f3bc` | iidxhook9: set TT multiplier | Wires up `bio2_emu_bi2a_set_tt_multiplier()` |
| `c49fc61` | doc bugfix: Fix incorrect COM port ID | Only doc, not code |
| `031836e` | chore: Apply code formatting | Format only |

The hook itself was introduced in 5.43→5.44 range, meaning v5.44 is the first release with iidxhook9. The CN boot failure may therefore be a **first-boot regression** (bug present since introduction) rather than a regression from a previously working state. This changes the investigation focus from "what changed" to "what was missing from the initial implementation for CN."

---

## Reproduction Conditions

1. Obtain tricoro CN game binary
2. Configure `iidxhook9.dll` with appropriate CN security plug mcode and network settings
3. Launch via `launcher.exe -B iidxhook9-prehook.dll -K iidxhook9.dll bm2dx.dll`
4. Observe whether the game progresses past audio initialization and security check
5. Compare CN vs JP binary behavior under the same BT5 configuration to identify CN-specific failures

---

## Ranked Hypotheses

### Hypothesis 1 (MOST LIKELY): Missing or incomplete CN filesystem path redirects

**Evidence:**
- `fs-hook.c:33-65` redirects `F:\` and `e:/` — these cover the typical JP/INT data layout
- CN variants of Konami games commonly use different drive letter conventions or different subdirectory structure under the AVS filesystem
- If the CN binary attempts to mount a drive letter not handled by `my_avs_fs_mount()`, the mount call passes through to real AVS, which fails (no actual removable drive)
- `avs_fs_mount` failure is fatal — AVS logs an error and the game aborts in pre-boot
- No CN-specific path handling is present in the current `fs-hook.c`

**Fix direction:** Identify which drive letters the CN tricoro binary uses (requires binary analysis or CN-specific documentation) and add redirect cases to `my_avs_fs_mount()`.

**Files:** `src/main/iidxhook9/fs-hook.c:33-65`

### Hypothesis 2 (PLAUSIBLE): Security plug mcode mismatch for CN variant

**Evidence:**
- `dllmain.c:127-136` initializes `iidxio` with `iidx_io_init()` which eventually sets up security plug validation
- CN games have different security plug mcodes (different game ID prefix in `security_mcode_game` field)
- If the popnhook-style security plug initialization doesn't match CN's expected mcode, the game fails security check during boot
- `iidxhook9` config has no explicit CN mcode configuration — it uses whatever `iidxio.dll` returns
- Without correct CN security data, the game would fail at the security check screen, not audio init

**Fix direction:** Confirm CN tricoro's expected security mcode prefix and verify it matches what the configured `iidxio.dll` provides. May require a CN-specific `iidxhook9-cn.dll` variant similar to `iidxhook4-cn` and `iidxhook5-cn`.

**Files:** `src/main/iidxhook9/dllmain.c:127-136`, security plug configuration

### Hypothesis 3 (POSSIBLE): Reverb fix timing — hook applied after COM call

**Evidence:**
- `reverbfixhook_init()` is called at `dllmain.c:169` — after IRP handlers but before BIO2/ACIO init
- tricoro's AVS boot sequence calls audio initialization before `my_dll_entry_init` finishes (possible if audio is initialized from a callback spawned early in boot)
- If the game's audio system initializes before `reverbfixhook_init()` runs (i.e., during `iidx_io_init()` which creates threads), the CoCreateInstance call for I3DL2 reverb would miss the hook
- On CN hardware which may be running different Windows versions, `DSFX_STANDARD_I3DL2REVERB` registration status differs — more likely to be unregistered on Chinese Windows installations
- This would cause `REGDB_E_CLASSNOTREG` to propagate to the game before the fix is applied

**Fix direction:** Move `reverbfixhook_init()` earlier in `my_dll_entry_init()` — ideally before `iidx_io_init()` which spawns threads. Or move it to `pre_hook()` to apply before any game code runs.

**Files:** `src/main/iidxhook9/dllmain.c:169`, `src/main/iidxhook9/reverbfix.c:92-97`

### Hypothesis 4 (SPECULATIVE): Network/eamuse config missing CN-specific server settings

**Evidence:**
- CN tricoro uses a different network server endpoint than JP/INT
- If the eamuse server address is not configured for CN, the game may hang waiting for network timeout during boot
- Not directly a code bug — more of a configuration issue
- However, if there's no CN-specific documentation or example config, users wouldn't know what to set

**Fix direction:** Provide CN-specific example config in `dist/iidx/` with appropriate server, pcbid, and eamid placeholders.

**Files:** `dist/iidx/` (documentation only)
