# Implementation Plan: gb-ppu

**Branch**: `002-gb-ppu` | **Date**: 2026-10-24 | **Spec**: specs/002-gb-ppu/spec.md

**Input**: Feature specification from `/specs/002-gb-ppu/spec.md`

**Note**: This plan guides research and design deliverables for implementing the DMG PPU. Initial milestone targets background-only rendering to reach first picture quickly, with sprites and window added subsequently.

## Summary

Implement the Picture Processing Unit (PPU) for the original handheld model with cycle-accurate scanline timing and interrupts. The PPU is modeled as a `PPU` class that advances via `tick(cycles)` driven by the CPU. Video output is presented via a display layer using a simple nearest-neighbor presentation. Milestone 1 focuses on background tile rendering only; sprites and window are deferred to a subsequent milestone to accelerate first-frame validation.

## Technical Context

- Language/Version: C++20
- Primary Dependencies: SDL2 (presentation)
- Storage: N/A
- Testing: Run Tetris retail ROM for visual validation; optional targeted timing ROMs later
- Target Platform: macOS (Apple Silicon)
- Project Type: Desktop application with core as modules (`src/ppu`, `src/video`)
- Performance Goals: Correctness-first; maintain 60 FPS-equivalent timing under normal CPU drive; defer optimization
- Constraints: Deterministic timing (456 cycles per visible line), PPU modes (2→3→0 lines 0–143; 1 lines 144–153), VBlank/STAT interrupts, palette-based grayscale output; initial milestone excludes sprites and window

Unknowns (to resolve in Phase 0):

- Exact LCDC bit behaviors needed in Milestone 1 (tile data/sign select, tile map selection) — NEEDS CLARIFICATION
- STAT interrupt enable bits default state, and coincidence flag update timing — NEEDS CLARIFICATION
- Presentation pixel format mapping (which grayscale levels to use) — NEEDS CLARIFICATION

## Constitution Check

GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.

- Principle I (Spec Accuracy): Mode timing, LY/LYC behavior, and interrupts are prioritized; initial scope constrained to background ensuring correctness.
- Principle II (Deterministic Sync): CPU drives PPU via cycles; PPU maintains internal timing state; presentation decoupled.
- Principle III (Progressive Milestones): Background-first milestone to achieve earliest visual feedback; sprites and window planned subsequently.
- Principle IV (Simplicity): Minimal public API (`tick(cycles)` and register accessors); clear separation of concerns between render and presentation.
- Principle V (Technology Policy): C++20 and SDL2 allowed by project; no external emulation cores.

Gate evaluation: PASS. No violations detected.

## Project Structure

### Documentation (this feature)

```text
specs/002-gb-ppu/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── contracts/
    └── ppu.md          # PPU class interface contract
```

### Source Code (repository root)

```text
src/
├── ppu/
│   ├── PPU.hpp          # PPU class interface (tick, register access, frame callback)
│   └── PPU.cpp          # Implementation: modes, background pipeline, timing, interrupts
└── video/
    ├── Display.hpp      # Presentation abstraction backed by SDL2
    └── Display.cpp      # Present 160x144 buffer (nearest-neighbor if scaled)

tests/
└── manual/
    └── tetris.md        # Manual validation steps (see quickstart.md)
```

Structure Decision: Separate core PPU from presentation; contracts documented under `specs/002-gb-ppu/contracts/`.

## Complexity Tracking

No constitution violations; section not applicable.

## Post-Design Constitution Check

Re-evaluated after Phase 0 and Phase 1 artifacts: PASS. Decisions include tile addressing rules, STAT interrupt gating, and pixel format mapping consistent with accuracy and simplicity principles.
