# Tasks: gb-ppu Implementation

## Phase 1: Setup

- [ ] T001 Create src/ppu and src/video directories per plan
- [ ] T002 Add SDL2 find_package and link targets in CMakeLists.txt at project root
- [ ] T003 Add grayscale palette constants in a shared header src/video/Palette.hpp

## Phase 2: Foundational

- [ ] T004 Define PPU public interface in src/ppu/PPU.hpp (tick, read/write, onFrame callback)
- [ ] T005 Implement Display abstraction in src/video/Display.hpp and src/video/Display.cpp (init window, present 160x144 buffer)
- [ ] T006 Wire CPU to call PPU::tick(cycles) within main execution loop in src/doctor/main.cpp

## Phase 3: User Story 1 (P1) - Render Tetris Gameplay (Background-only milestone)

Goal: First picture via background tiles; verify scrolling and frame cadence.
Independent test criteria: Launch Tetris, reach gameplay, observe stable background rendering.

- [ ] T007 [US1] Implement LCDC/STAT/SCX/SCY/LY/LYC/BGP registers in src/ppu/PPU.cpp
- [ ] T008 [US1] Implement scanline timing and mode transitions (2→3→0, then 1) with 456 cycles per line in src/ppu/PPU.cpp
- [ ] T009 [US1] Implement background tile fetch and pixel pipeline (tile map selection, signed/unsigned tile index) in src/ppu/PPU.cpp
- [ ] T010 [US1] Map background pixel indices to grayscale via BGP in src/ppu/PPU.cpp using src/video/Palette.hpp
- [ ] T011 [US1] Raise VBlank and STAT interrupts at correct times via IF writes in src/ppu/PPU.cpp
- [ ] T012 [US1] Present frame buffer once per frame via Display in src/video/Display.cpp

## Phase 4: User Story 2 (P1) - Accurate PPU Timing & Modes

Goal: Validate mode flags and LY/LYC behavior observable to software.
Independent test criteria: Observe correct mode transitions and LY progression; LYC coincidence interrupts fire as configured.

- [ ] T013 [US2] Update STAT mode bits and coincidence flag timely at line start in src/ppu/PPU.cpp
- [ ] T014 [US2] Implement LYC match logic and configurable STAT interrupts in src/ppu/PPU.cpp
- [ ] T015 [US2] Add debug hooks/logging toggles to verify LY and mode changes in src/ppu/PPU.cpp

## Phase 5: User Story 3 (P1) - Interrupt Signaling

Goal: Ensure VBlank and STAT interrupts integrate with CPU interrupt system.
Independent test criteria: Software can observe and handle interrupts reliably once per frame and per configured modes.

- [ ] T016 [US3] Integrate IF writes and ensure priority order with CPU’s interrupt system in src/cpu/CPU.cpp and src/ppu/PPU.cpp
- [ ] T017 [US3] Validate interrupt service timing at instruction boundaries by coordinating CPU stepping in src/cpu/CPU.cpp

## Final Phase: Polish & Cross-Cutting

- [ ] T018 Add nearest-neighbor scaling toggle in src/video/Display.cpp
- [ ] T019 Document manual validation steps in tests/manual/tetris.md
- [ ] T020 Review and refactor for clarity; add comments in complex timing logic in src/ppu/PPU.cpp

## Dependencies (Story Order)

US1 → US2 → US3

## Parallel Execution Examples

- [ ] T005 [P] Implement Display abstraction in src/video/Display.hpp and src/video/Display.cpp
- [ ] T004 [P] Define PPU public interface in src/ppu/PPU.hpp
- [ ] T006 [P] Wire CPU to call PPU::tick(cycles) in src/doctor/main.cpp

## Implementation Strategy

MVP first: Complete Phase 3 (US1) to achieve background-only first picture. Then proceed to timing verification (US2) and full interrupt integration (US3).
