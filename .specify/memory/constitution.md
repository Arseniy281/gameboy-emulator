<!--
Sync Impact Report

- Version change: none → 1.0.0
- Modified principles: (n/a, initial document)
- Added sections: Core Principles; Additional Constraints; Development Workflow; Governance
- Removed sections: none
- Follow-up TODOs: none
-->

# GameBoyEmulator Constitution

## Core Principles

### I. Spec-Accurate CPU Behavior
All SM83 CPU instructions MUST implement documented semantics and cycle counts.

- Z, N, H, C flags MUST be updated exactly per Pan Docs for every instruction.
- Interrupt dispatch, IF/IE bits, HALT/STOP behavior, and EI/DI timing MUST
  follow the specification.
- Memory and I/O side effects (including OAM DMA and HRAM access) MUST match
  documented hardware behavior. Any hardware-undefined behavior encountered
  MUST be explicitly documented in-code with a Pan Docs citation.
- Passing authoritative test suites (see Principle III) is the acceptance
  criterion for correctness.

Rationale: Accuracy-first ensures testability, predictable behavior for users,
and a stable base for future optimization.

### II. Cycle-Synchronized PPU and Timing
The PPU MUST be synchronized with the CPU at the cycle level.

- PPU advancement MUST be driven by CPU cycles so that LY/STAT, mode timings,
  and VBlank events occur at correct boundaries.
- OAM DMA timing MUST block the CPU for the documented duration and update
  OAM exactly once per DMA invocation.
- Timer/divider behavior, serial timing, and STAT interrupts MUST align with
  documented timing rules.
- Any scheduler/stepping logic MUST preserve determinism given the same inputs.

Rationale: Many games and tests rely on precise timing; cycle-aligned
synchronization is required to pass timing-sensitive suites.

### III. Tests as Quality Gate
Blargg and Mooneye test suites are mandatory gates for correctness.

- The emulator MUST pass Blargg cpu_instrs and instr_timing on the target
  platform before any release or milestone is considered complete.
- Relevant Mooneye-GB tests (CPU/PPU/interrupt/timing focused) MUST be used to
  verify behavior. Newly implemented behavior MUST not introduce regressions in
  previously passing tests.
- Continuous integration (when present) MUST execute these suites or an
  equivalent subset on every change to core CPU/PPU/timing code.

Rationale: Objective, automated tests prevent regressions and validate fidelity
to the hardware specification.

### IV. Simplicity First; No Premature Optimization
Readable, minimal, and maintainable code takes precedence over micro-optimizations.

- Implement the simplest correct design that satisfies Principles I–III.
- Do NOT optimize performance until correctness is verified and a profile
  identifies a hotspot with a measurable goal.
- Any optimization MUST include a note: scenario profiled, baseline metrics,
  change made, and measured improvement.

Rationale: Premature optimization increases complexity and risk without proven
benefit; correctness and clarity reduce long-term cost.

### V. Technology and Dependency Policy
Constrained stack and dependencies ensure portability and control.

- Language: C++20. Non-standard extensions MUST NOT be required.
- Build: CMake as the sole build system.
- Target: macOS on Apple Silicon is the primary target; other platforms MAY be
  supported opportunistically without compromising the primary target.
- Dependencies: Third-party Game Boy emulation libraries are prohibited. Other
  third-party libraries MAY be used if they are not emulation-specific,
  materially reduce risk, and are justified in documentation. The C++ standard
  library is preferred where feasible.

Rationale: A focused toolchain reduces friction, and avoiding emulation
libraries guarantees we understand and control all behavior.

## Additional Constraints

- Source MUST compile with the supported Apple Clang toolchain on Apple Silicon
  using CMake presets or documented configuration.
- Behavior MUST be deterministic given the same ROM and configuration (no
  hidden random seeds or time dependencies in core emulation).
- Undefined behavior in C++ MUST be avoided; code interacting with bit-level
  CPU/PPU state MUST use well-defined integer operations and fixed-width types.
- Logging/tracing MAY be added for debugging but MUST be disableable at build
  time and MUST NOT alter timing-sensitive code paths when disabled.
- External assets or ROMs used for testing MUST NOT be distributed unless their
  licenses permit it; test harnesses SHOULD fetch or mount them locally.

## Development Workflow

- Every change impacting CPU, PPU, timers, or memory-mapped I/O MUST include a
  reference to the relevant Pan Docs section in code comments or the PR.
- Add or update tests when implementing new instructions or timing behavior;
  failing tests block merges.
- Code review focuses on Principles I–V compliance and regression risk.
- Interfaces and modules SHOULD remain small and composable; prefer small,
  verifiable steps over broad refactors unless a clear payoff is documented.
- Public documentation (README or docs) SHOULD enumerate supported features,
  known limitations, and the set of passing test suites.

## Governance

- This constitution supersedes ad-hoc practices where they conflict. Exceptions
  require explicit approval and MUST be time-bound and documented.
- Amendments are proposed via PRs that include: the change, rationale, expected
  impact, and migration guidance (if any). Maintainers approve amendments.
- Versioning of this document uses semantic versioning:
  MAJOR for incompatible removals or redefinitions; MINOR for new principles or
  materially expanded guidance; PATCH for clarifications and non-semantic edits.
- Compliance reviews SHOULD occur at significant milestones or monthly, verifying
  Principles and gates remain satisfied.
- Pan Docs (gbdev.io/pandocs) is the authoritative hardware reference for
  behavior and citations.

**Version**: 1.0.0 | **Ratified**: 2026-10-22 | **Last Amended**: 2026-10-22
