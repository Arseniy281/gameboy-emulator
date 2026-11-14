# Contract: PPU Public Interface

## Purpose

Define the public interface for the `PPU` class to integrate with the CPU and presentation.

## Interface

### Class: PPU

- Constructor: accepts references to memory/bus and a frame callback or presentation sink
- Method: `void tick(int cycles)` — advance PPU by CPU-provided cycles
- Method: `void write(uint16_t addr, uint8_t value)` — memory-mapped I/O for PPU registers
- Method: `uint8_t read(uint16_t addr)` — memory-mapped reads for PPU registers
- Callback: `onFrame(const uint32_t* frame, int width, int height)` — invoked at end of frame

Notes:

- Milestone 1 requires only background rendering; sprite/window writes are accepted but have no effect until Milestone 2.
- Interrupt signaling is exposed via bus/IF register integration; STAT/VBlank events are latched according to mode transitions.
