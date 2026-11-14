# Contract: CPU Class Interface

**Date**: 2026-10-22
**Spec**: specs/001-gb-cpu-core/spec.md

## Overview

The CPU class encapsulates LR35902 state and provides deterministic stepping
with cycle reporting and interrupt handling.

## Public Interface (contract)

- Constructor: initializes registers to boot state or provided initial state.
- `void reset(bool cgb_mode=false)`: Reset to model-appropriate initial state.
- `uint32_t step()`: Execute one instruction (including CB opcodes); returns cycles consumed.
- `void requestInterrupt(uint8_t source_bit)`: Raise IF bit for a source.
- `void setIE(uint8_t value)`: Set interrupt enable register.
- `uint8_t getIE() const`: Get interrupt enable.
- `void setIF(uint8_t value)`: Set interrupt flags register.
- `uint8_t getIF() const`: Get interrupt flags.
- `void setIME(bool enabled)`: Set IME; EI semantics are handled internally.
- `bool getIME() const`: Query IME.
- `void write8(uint16_t addr, uint8_t value)`: Memory write (abstracted through bus).
- `uint8_t read8(uint16_t addr) const`: Memory read (abstracted through bus).
- `void connectBus(/* abstract bus interface */)`: Attach memory/I/O bus abstraction.

## Behavior Guarantees

- Flags Z/N/H/C updated per documented instruction semantics.
- Cycle counts returned are exact for the executed instruction path.
- Interrupt service order respects priority: VBlank, LCD STAT, Timer, Serial, Joypad.
- HALT and STOP states behave per documented rules; HALT bug is implemented.
- Logging of PC and registers to stderr is optional and disableable; logging is
  non-intrusive and does not alter timing when disabled.
