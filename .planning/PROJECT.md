# Bemanitools 5 — Bug Triage & Maintenance

## What This Is

Bemanitools is a Windows DLL injection toolkit that lets unmodified Konami arcade rhythm game binaries run on PC hardware by emulating arcade I/O boards (ezusb, BIO2, ACIO, P3IO, P4IO). It supports IIDX, DDR, jubeat, pop'n music, SOUND VOLTEX, and BeatStream across dozens of game versions.

This initiative is a maintenance cycle: triaging and prioritizing the open bug backlog, fixing regressions, landing near-ready contributions, and getting the project into a healthy state for a 5.50 release.

## Core Value

Existing supported games must boot and run correctly on current hardware — regressions are the highest priority.

## Requirements

### Validated

- ✓ IIDX versions 9-30 supported via iidxhook1-9 — existing
- ✓ DDR X and X2+ supported via ddrhook1-2 — existing
- ✓ jubeat 1 through festo supported via jbhook1-3 — existing
- ✓ pop'n music 15-18 supported via popnhook1 — existing
- ✓ SOUND VOLTEX 1-5+ supported via sdvxhook/sdvxhook2 — existing
- ✓ BeatStream supported via bsthook — existing
- ✓ Card reader emulation via eamio API — existing
- ✓ Multiple I/O backends per game (ezusb, BIO2, keyboard, ViGEm) — existing
- ✓ Configuration system with .conf files and config UI — existing

### Active

- [ ] Pop'n Music 15-18 regression fixed (v5.43→5.44 broke boot, #345/#341/#338)
- [ ] IIDX 11-15 timing issue resolved — song selection loads too quickly on modern hardware (#344)
- [ ] IIDX tricoro CN boot failure fixed (#351)
- [ ] IIDX 12 Happy Sky silent launch failure diagnosed and fixed (#356)
- [ ] IIDX 30 default logger configuration error fixed (#306)
- [ ] P2 keyboard card insertion working for DDR (#347)
- [ ] Near-ready PRs reviewed and merged: smartcard support (#355), MDXF (#350), minimaid+HID (#358)
- [ ] All open issues triaged with labels and priority
- [ ] Support/invalid issues closed with guidance

### Out of Scope

- BT6 refactoring (#305, #290-297) — separate effort with its own timeline
- New game series support — not priority for this cycle
- Metal Gear Arcade (#337) — different game engine, not Bemani
- Real-time chat/Discord support bot — not a software concern

## Context

The project has 30+ open GitHub issues spanning bugs, feature requests, support questions, and a major BT6 refactoring chain. No issues have labels or priority markers. The bug backlog clusters around:

1. **Pop'n Music regression** — 3 issues (#345, #341, #338) all reporting boot failures after v5.43. Likely a single root cause introduced in v5.44.
2. **IIDX boot/timing issues** — 4 issues (#344, #351, #356, #306) with varied symptoms across different IIDX versions.
3. **Input edge cases** — P2 card insertion (#347), touchscreen for jubeat (#308), Pop'n button inversion (#346).
4. **Community contributions** — 3 PRs waiting for review (#355, #350, #358).

The codebase has minimal test coverage (21 test files for ~150K LOC). Most validation happens through manual testing on real hardware or emulated environments.

## Constraints

- **Platform**: Windows only (XP through 11), built with MinGW cross-compilation
- **Testing**: No automated game-level testing; verification requires running actual game binaries
- **Hardware**: Some bugs may only reproduce with specific hardware configurations
- **BT5 only**: Fixes go on the current codebase; BT6 is a separate branch/effort

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Fix bugs on BT5, not BT6 | BT6 is WIP and not ready; users need fixes now | — Pending |
| Triage all issues before deep-diving | Understand the full picture before committing to fixes | — Pending |
| Pop'n regression is highest priority | 3 issues, likely single root cause, clear regression | — Pending |

---
*Last updated: 2026-02-28 after initialization*
