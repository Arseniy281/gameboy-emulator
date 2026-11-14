# Quickstart Validation: gb-cpu-core

**Date**: 2026-10-22
**Spec**: specs/001-gb-cpu-core/spec.md

## Prerequisites

- Build the project via CMake on macOS (Apple Silicon).
- Ensure Blargg ROMs are available under `tests/roms/`.

## Steps

1. Configure and build:
   - `cmake -B build -S .`
   - `cmake --build build`
2. Run CPU tests via CTest:
   - `ctest --test-dir build`
3. Observe PASS for:
   - Blargg `cpu_instrs` (all subtests)
   - Blargg `instr_timing`

## Expected Outcomes

- All CPU instruction semantics and flags match expectations.
- Interrupt handling behaves per priority and timing rules.
- HALT/STOP states behave correctly, including HALT bug.
- stderr logging of PC and registers is available during early debugging; can be disabled.
