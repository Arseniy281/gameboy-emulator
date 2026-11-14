# Data Model: gb-cpu-core

**Date**: 2026-10-22
**Spec**: specs/001-gb-cpu-core/spec.md

## Entities

### CPU State

- Registers: A, F, B, C, D, E, H, L
- Special: SP, PC
- Flags in F: Z (bit 7), N (bit 6), H (bit 5), C (bit 4)
- Control: IME (interrupt master enable), halted (HALT state), stopped (STOP state)
- Interrupt Registers: IE (enable bits), IF (flag bits)
- Cycle Counter: total cycles consumed (for synchronization)

### Instruction

- Opcode: 8-bit (with optional CB prefix)
- Operands: immediate (8/16-bit), register, memory via addressing modes
- Addressing Modes: register, immediate, indirect via HL, absolute via 16-bit address
- Metadata: cycles, flag effects (Z/N/H/C per instruction), description

### Interrupt Source

- Sources: VBlank, LCD STAT, Timer, Serial, Joypad
- Priority: VBlank > LCD STAT > Timer > Serial > Joypad
- Vector Addresses: 0x40, 0x48, 0x50, 0x58, 0x60
- Enable: IE bits; Pending: IF bits

## Relationships

- CPU executes Instruction; Instruction updates CPU State and consumes cycles.
- Interrupt Source interacts with CPU via IF/IE; CPU services the highest-priority
  pending enabled interrupt at instruction boundaries when IME=1.

## Rules

- EI sets a delayed IME enable that takes effect after the following instruction completes.
- HALT: when IME=1 and no interrupts pending, CPU halts until an interrupt occurs.
  HALT bug: when IME=0 and a pending but disabled interrupt exists, CPU halts but
  continues to increment PC under specific conditions until the state changes.
- STOP: enters low-power state; resumes per documented preconditions.
