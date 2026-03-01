# Requirements: Bemanitools 5 — Bug Triage & Maintenance

**Defined:** 2026-03-01
**Core Value:** Existing supported games must boot and run correctly on current hardware — regressions are the highest priority.

## v1 Requirements

Requirements for this milestone. AIDE infrastructure is the top priority — it enables everything else and serves as research into reusable agentic tooling for reverse engineering.

### AI-Assisted Development

- [ ] **AIDE-01**: Build system works reliably for agentic iteration (build, verify, repeat)
- [ ] **AIDE-02**: Testing infrastructure established where feasible (unit tests for pure logic, integration stubs)
- [ ] **AIDE-03**: Reference material for hooked targets available (decompiled headers, API contracts, protocol docs)
- [ ] **AIDE-04**: Development workflow documented for agentic use (how to build, test, verify changes)
- [ ] **AIDE-05**: Decompilation tooling integrated for agent access to original binaries (e.g., Ghidra MCP or similar) — agents can query decompiled functions, understand hook targets, and cross-reference with hook implementations

### Regressions

- [ ] **REGR-01**: Pop'n Music 15-18 boots and runs correctly (v5.43→5.44 regression, #345/#341/#338)
- [ ] **REGR-02**: IIDX 11-15 song selection timing works on modern hardware (#344)
- [ ] **REGR-03**: IIDX tricoro boots with CN network config (#351)

### Bug Fixes

- [ ] **BUGF-01**: IIDX 12 Happy Sky launches without silent failure (#356)
- [ ] **BUGF-02**: IIDX 30 default logger configuration works correctly (#306)
- [ ] **BUGF-03**: DDR P2 keyboard card insertion works (#347)

### Contributions

- [ ] **CONT-01**: Smartcard support PR reviewed and merged (#355)
- [ ] **CONT-02**: MDXF PR reviewed and merged (#350)
- [ ] **CONT-03**: Minimaid+HID PR reviewed and merged (#358)

### Triage

- [ ] **TRIA-01**: All open issues labeled and prioritized
- [ ] **TRIA-02**: Support/invalid issues closed with guidance

## v2 Requirements

### Extended Testing

- **TEST-01**: Integration tests that exercise hook DLLs against stubbed game binaries
- **TEST-02**: CI pipeline runs tests on every PR

### Additional Bugs

- **BUGF-04**: jubeat touchscreen input issues (#308)
- **BUGF-05**: Pop'n button inversion (#346)

## Out of Scope

| Feature | Reason |
|---------|--------|
| BT6 refactoring (#305, #290-297) | Separate effort with its own timeline |
| New game series support | Not priority for this cycle |
| Metal Gear Arcade (#337) | Different game engine, not Bemani |
| Full decompilation of game binaries | Legal/scope concerns; only API contracts and protocol docs |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| AIDE-01 | — | Pending |
| AIDE-02 | — | Pending |
| AIDE-03 | — | Pending |
| AIDE-04 | — | Pending |
| AIDE-05 | — | Pending |
| REGR-01 | — | Pending |
| REGR-02 | — | Pending |
| REGR-03 | — | Pending |
| BUGF-01 | — | Pending |
| BUGF-02 | — | Pending |
| BUGF-03 | — | Pending |
| CONT-01 | — | Pending |
| CONT-02 | — | Pending |
| CONT-03 | — | Pending |
| TRIA-01 | — | Pending |
| TRIA-02 | — | Pending |

**Coverage:**
- v1 requirements: 16 total
- Mapped to phases: 0
- Unmapped: 16 ⚠️

---
*Requirements defined: 2026-03-01*
*Last updated: 2026-03-01 after initial definition*
