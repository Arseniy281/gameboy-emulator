# Feature Specification: gb-cpu-core

**Feature Branch**: `[001-gb-cpu-core]`

**Created**: 2026-10-22

**Status**: Draft

**Input**: User description: "Реализовать CPU Game Boy (Sharp LR35902): все базовые инструкции, CB-prefix инструкции, флаги Z/N/H/C, обработку прерываний (VBlank, Timer, Serial, Joypad), состояния HALT и STOP. Критерий готовности: прохождение Blargg cpu_instrs (все тесты) и instr_timing. "

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Execute Base Instructions Correctly (Priority: P1)

As a developer running CPU validation ROMs, I can execute all base (non-CB) instructions with correct semantics, flag effects (Z/N/H/C), and cycle counts so that conformance tests pass.

**Why this priority**: Base instructions are the foundation; without them, no other validation is meaningful.

**Independent Test**: Run Blargg cpu_instrs ROM subset targeting base opcodes; the ROM reports success without hangs or mismatches.


1. Given a ROM that exercises arithmetic/logic/load/jump instructions, When executed from reset state, Then for every tested opcode, the flags and registers match expected values and the ROM signals PASS.
2. Given instructions that modify HL/SP and memory, When run, Then memory read/write side effects and cycles match expectations reported by the test harness.

---

### User Story 2 - Execute CB-Prefix Instructions Correctly (Priority: P1)

As a developer running CPU validation ROMs, I can execute all CB-prefixed bit operations and shifts/rotates with exact flag behavior and timing.

**Why this priority**: Bit operations are heavily used and are explicitly tested by Blargg.

**Independent Test**: Run Blargg cpu_instrs CB test sections; PASS is reported with no discrepancies.

**Acceptance Scenarios**:

1. Given a ROM focusing on CB opcodes, When run, Then for every CB instruction the resulting register/memory values and Z/N/H/C flags match the expected reference.

---

### User Story 3 - Interrupt Handling and HALT/STOP (Priority: P1)

As a developer validating control flow, I can enable/disable and service VBlank, Timer, Serial, and Joypad interrupts correctly, and observe HALT/STOP states per specification.

**Why this priority**: Correct interrupt behavior and low-power states are required by timing and control-flow tests.

**Independent Test**: Run Blargg instr_timing and relevant Mooneye-style checks that rely on IF/IE, EI/DI timing, IME behavior, and HALT edge cases.

**Acceptance Scenarios**:

1. Given IF and IE with a pending interrupt and IME=1, When an instruction completes, Then the highest-priority enabled interrupt is serviced, PC jumps to the vector, IME is cleared, and cycles match expectations.
2. Given HALT with IME=0 and IF has a pending but disabled interrupt, When executed, Then the CPU exhibits the documented halt bug behavior until the condition changes, after which normal execution resumes.
3. Given STOP and correct key/clock preconditions, When executed, Then the CPU enters STOP and resumes only per specification.

### Edge Cases

- Behavior on EI followed immediately by an interrupt source becoming pending before next instruction boundary.
- Trigger order when multiple IF bits set; priority resolution on same cycle.
- HALT with IME=0 and IF has a disabled interrupt (halt bug behavior).
- STOP with/without speed switch context (CGB-only details are out of scope for DMG unless specified).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Implement all base (non-CB) LR35902 instructions with exact semantics and documented cycle counts.
- **FR-002**: Implement all CB-prefixed instructions with exact semantics and documented cycle counts.
- **FR-003**: Update Z, N, H, C flags for every instruction exactly as documented; undefined bits remain unchanged where applicable.
- **FR-004**: Implement interrupt enable/disable mechanics (IME), IE/IF registers, and service order (VBlank, LCD STAT, Timer, Serial, Joypad) with correct priority and timing.
- **FR-005**: Implement HALT and STOP states, including entry/exit conditions and timing effects.
- **FR-006**: Implement EI/DI timing semantics (IME becomes enabled after the instruction following EI completes).
- **FR-007**: Provide a stepping/execution loop that advances CPU state deterministically and exposes consumed cycles for PPU/Timer synchronization.
- **FR-008**: Pass Blargg cpu_instrs test suite (all subtests report PASS).
- **FR-009**: Pass Blargg instr_timing test suite.
- **FR-010**: Document any hardware-undefined behaviors encountered with references to Pan Docs.
- **FR-011**: Implement the HALT bug behavior and document the specific conditions under which it manifests.

### Key Entities

- CPU State: registers (A, F, B, C, D, E, H, L, SP, PC), IME flag, pending enable flag after EI, HALT/STOP states, IE/IF bits (by source), and cycle counter.
- Instruction: opcode byte(s), addressing mode, operand(s), cycles, flag effects.
- Interrupt Source: VBlank, LCD STAT, Timer, Serial, Joypad with priority order and vector addresses.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Blargg cpu_instrs test ROM completes with PASS.
- **SC-002**: Blargg instr_timing test ROM completes with PASS.
- **SC-003**: For targeted interrupt tests, observed service order and timing match documented behavior across at least 1000 instruction boundaries without discrepancy.
- **SC-004**: Execution is deterministic given identical inputs; repeated runs produce identical traces for instruction, PC, flags, and cycles.

## Assumptions

- Target model baseline is DMG (original Game Boy); CGB-specific features are out of scope for this feature unless required by tests.
- Timer and PPU synchronization are handled elsewhere but consume the cycle counts exposed by the CPU execution loop.
- Test ROMs are available to the developer and are executed in an environment where PASS/FAIL can be detected from ROM output or memory flags.
