---

description: "Task list for gb-cpu-core"
---


# Tasks: gb-cpu-core

**Input**: Design documents from `/specs/001-gb-cpu-core/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Tests are OPTIONAL. For this feature, test harness integration via CTest is included as tasks because readiness criteria depend on Blargg PASS.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and basic structure

- [X] T001 Update root CMakeLists.txt to add CPU library target in src/cpu (path: CMakeLists.txt) — Done when: `add_library(gb_cpu ...)` present and project configures without errors
- [X] T002 Create src/cpu/CMakeLists.txt with target sources and include paths (path: src/cpu/CMakeLists.txt) — Done when: library builds with empty CPU.cpp/CPU.hpp
- [X] T003 Create tests/cpu/CMakeLists.txt enabling CTest and a cpu_tests executable (path: tests/cpu/CMakeLists.txt) — Done when: `ctest -N` lists cpu tests target
- [X] T004 [P] Seed test harness file for Blargg runner (path: tests/cpu/cpu_tests.cpp) — Done when: builds and runs a placeholder test via CTest
- [X] T005 [P] Add stderr logging toggle macro/env support for PC/register trace (path: src/cpu/CPU.hpp) — Done when: logging can be enabled/disabled without compile errors

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T006 Define CPU interface and registers in header (path: src/cpu/CPU.hpp) — Done when: A,F,B,C,D,E,H,L, SP, PC, IME, halted fields and `step()`, IF/IE accessors exist
- [X] T007 Implement CPU skeleton and fetch/decode/dispatch loop (path: src/cpu/CPU.cpp) — Done when: NOP executes, PC advances, cycles returned deterministically
- [X] T008 [P] Add register pair helpers and flag utilities (path: src/cpu/CPU.hpp) — Done when: get/set AF,BC,DE,HL and Z/N/H/C setters/getters compile and unit-stub passes
- [X] T009 [P] Establish bus abstraction (read8/write8) and integrate into CPU (path: src/cpu/Bus.hpp, src/cpu/CPU.hpp) — Done when: CPU reads/writes via bus pointers/callbacks
- [X] T010 Wire tests/roms discovery and PASS detection stub (path: tests/cpu/cpu_tests.cpp) — Done when: harness can load a ROM file from tests/roms/ and check for PASS marker
    - Intercepts FF01/FF02 writes and accumulates output; detects "Passed"/"Failed"; 10s emulated timeout; runs 01-special.gb as smoke test

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - Execute Base Instructions Correctly (Priority: P1) 🎯 MVP

**Goal**: Execute all base (non-CB) instructions with correct semantics, flags, and cycle counts.

**Independent Test**: Run Blargg cpu_instrs base sections; ROM reports PASS.

### Implementation for User Story 1

- [X] T011 [P] [US1] Implement 8-bit ALU ops ADD/ADC/SUB/SBC/AND/OR/XOR/CP (path: src/cpu/CPU.cpp) — Done when: unit traces show correct Z/N/H/C and Blargg ALU subtests pass
- [X] T012 [P] [US1] Implement INC/DEC for registers and (HL) with correct H behavior (path: src/cpu/CPU.cpp) — Done when: H toggles per nibble overflow/borrow and subtests pass
- [X] T013 [P] [US1] Implement 16-bit arithmetic (ADD HL, ADD SP,e8, LDHL SP+e8, INC/DEC rr) (path: src/cpu/CPU.cpp) — Done when: cycles/flags per spec and related subtests pass
- [X] T014 [P] [US1] Implement LD group (reg↔reg, (HL)↔reg, (nn)↔A, FF00+n) (path: src/cpu/CPU.cpp) — Done when: loads/stores verified and cpu_instrs load subtests pass
- [X] T015 [P] [US1] Implement control flow JP/JR/CALL/RET/RST (incl. conditionals) (path: src/cpu/CPU.cpp) — Done when: PC/stack cycles correct and control-flow subtests pass
- [X] T016 [US1] Implement DAA, CPL, SCF, CCF (path: src/cpu/CPU.cpp) — Done when: DAA conforms and flag ops verified by subtests
- [X] T017 [US1] Implement PUSH/POP and SP adjust instructions (path: src/cpu/CPU.cpp) — Done when: stack integrity verified and related subtests pass
- [X] T018 [US1] Integrate Blargg cpu_instrs run into CTest (path: tests/cpu/CMakeLists.txt, tests/cpu/cpu_tests.cpp) — Done when: `ctest` includes cpu_instrs and reports PASS

---

## Phase 4: User Story 2 - Execute CB-Prefix Instructions Correctly (Priority: P1)

**Goal**: Execute all CB-prefixed bit ops and shifts/rotates with exact flag behavior and timing.

**Independent Test**: Run Blargg cpu_instrs CB sections; ROM reports PASS.

### Implementation for User Story 2

- [ ] T019 [P] [US2] Implement rotates/rotates-through-carry: RLCA/RLA/RRCA/RRA and RLC/RL/RRC/RR on r,(HL) (path: src/cpu/CPU.cpp) — Done when: flags/values match and CB rotate subtests pass
- [ ] T020 [P] [US2] Implement shifts and swap: SLA/SRA/SRL/SWAP on r,(HL) (path: src/cpu/CPU.cpp) — Done when: flags/values match and CB shift subtests pass
- [ ] T021 [P] [US2] Implement BIT/RES/SET on r,(HL) (path: src/cpu/CPU.cpp) — Done when: Z/H/N flags correct and CB bit subtests pass
- [ ] T022 [US2] Validate CB prefix coverage in CTest (path: tests/cpu/cpu_tests.cpp) — Done when: ctest shows CB sections PASS

---

## Phase 5: User Story 3 - Interrupt Handling and HALT/STOP (Priority: P1)

**Goal**: Correctly handle interrupts (VBlank, LCD STAT, Timer, Serial, Joypad) and HALT/STOP states.

**Independent Test**: Run Blargg instr_timing and targeted interrupt scenarios; observe correct service order and timing.

### Implementation for User Story 3

- [ ] T023 [P] [US3] Implement IE/IF accessors and IME with EI delayed-enable (path: src/cpu/CPU.cpp) — Done when: IME sets after next instruction and IF/IE mask respected
- [ ] T024 [P] [US3] Implement interrupt priority select and vector jump with cycles (path: src/cpu/CPU.cpp) — Done when: service order VBlank>STAT>Timer>Serial>Joypad and cycles match
- [ ] T025 [US3] Implement HALT and HALT bug behavior (path: src/cpu/CPU.cpp) — Done when: CPU halts and exhibits halt bug on IME=0 with pending disabled IF per spec
- [ ] T026 [US3] Implement STOP entry/exit per spec (path: src/cpu/CPU.cpp) — Done when: CPU enters STOP and resumes with correct preconditions
- [ ] T027 [US3] Integrate Blargg instr_timing run into CTest (path: tests/cpu/CMakeLists.txt, tests/cpu/cpu_tests.cpp) — Done when: `ctest` includes instr_timing and reports PASS

---

## Phase N: Polish & Cross-Cutting Concerns

**Purpose**: Improvements across stories

- [ ] T028 [P] Align documentation in specs/001-gb-cpu-core/* with final CPU.hpp contract — Done when: contract and header match
- [ ] T029 Code cleanup and unify flag helpers (path: src/cpu/CPU.cpp, src/cpu/CPU.hpp) — Done when: no duplication and helpers cover all uses
- [ ] T030 Performance profiling checkpoint (no changes unless hotspots identified) — Done when: baseline instruction/sec measured on target machine
- [ ] T031 [P] Optional focused unit tests for tricky opcodes (path: tests/cpu/unit_tests.cpp) — Done when: tests added for DAA, ADD SP,e8, BIT (HL)
- [ ] T032 Run quickstart validation (path: specs/001-gb-cpu-core/quickstart.md) — Done when: all steps complete and PASS observed

---

## Dependencies & Execution Order

### Phase Dependencies

- Setup (Phase 1): No dependencies
- Foundational (Phase 2): Depends on Setup; BLOCKS all user stories
- User Stories (Phase 3+): Depend on Foundational; US1, US2, US3 are independent and can proceed in parallel after Phase 2
- Polish (Final): Depends on user story phases

### User Story Dependencies

- User Story 1 (P1): No dependencies on other stories
- User Story 2 (P1): No dependencies on other stories
- User Story 3 (P1): No dependencies on other stories

### Within Each User Story

- Implement tasks; validate independently with the specified tests
- Maintain determinism and accurate cycle accounting

### Parallel Opportunities

- [P]-marked tasks are parallel-safe (different files, minimal coupling)
- US1 opcode groups (ALU, INC/DEC, 16-bit ops, loads, control flow) can proceed in parallel
- US2 opcode groups (rotates, shifts, bit ops) can proceed in parallel
- US3 interrupt components (IME/IF/IE, service, HALT/STOP) can proceed in parallel

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 and Phase 2
2. Implement US1 tasks T011–T018
3. Validate Blargg cpu_instrs base PASS

### Incremental Delivery

1. Add US2 tasks T019–T022 → validate CB PASS
2. Add US3 tasks T023–T027 → validate instr_timing PASS
3. Polish tasks T028–T032



















