# Quickstart: gb-ppu Background-Only

## Prerequisites

- Build the project (CMake) with SDL2 available on the system.
- Place a retail Tetris ROM in the configured ROMs directory.

## Steps

1. Launch the emulator with the Tetris ROM.
2. Observe the title screen and start a game; verify background tiles render and scroll correctly.
3. Confirm LY increments 0..153 and VBlank occurs at lines 144..153 by inspecting debug output or on-screen indicators.
4. Verify STAT interrupts can be toggled in software and occur at the expected mode transitions (modes 0/1/2 and LYC).

## Expected Outcomes

- A 160x144 image is displayed; background tiles are visible with appropriate grayscale mapping.
- Vertical blanking interrupt observed once per frame; status interrupts observable when enabled.
- No sprites or window elements are rendered in this milestone; visuals still coherent.
