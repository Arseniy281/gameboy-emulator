# Research: gb-ppu

## Decisions and Clarifications

### LCDC Tile Data and Map Selection (Milestone 1)

- Decision: Support both BG tile map base addresses and both tile data addressing modes controlled by LCDC.
- Rationale: Many games (including Tetris) may rely on either tile map region and signed/unsigned tile index addressing. Implementing both ensures early compatibility.
- Alternatives considered: Fix to a single map/data mode for first frame; rejected due to compatibility risk.

### STAT Interrupt Enables and Coincidence Timing

- Decision: Implement STAT interrupt enables per documented bits; coincidence flag (LY==LYC) updated at the start of OAM scan for each line with immediate STAT check.
- Rationale: Aligns with common documentation and avoids mid-pixel ambiguity.
- Alternatives considered: Update at end of line; could shift interrupt timing and break split effects.

### Pixel Format Mapping

- Decision: Map DMG 2-bit color indices 0..3 to a grayscale palette: [255, 192, 96, 0].
- Rationale: Readable on typical displays; preserves ordering and contrast.
- Alternatives considered: [255, 170, 85, 0]; slightly lower contrast.

## Best Practices

- Maintain a strict 456-cycles-per-visible-line cadence; accumulate cycles in `tick(cycles)` and step through modes accordingly.
- Defer sprites and window to Milestone 2; keep background-only path simple and verifiable.
- Isolate memory reads via a bus interface to avoid tight coupling; cache tile line fetches per scanline for efficiency, but prioritize correctness.

## Open Items (Resolved here)

- Unknowns from Technical Context: addressed in decisions above; no pending clarifications remain for Milestone 1.
