# Feature Specification: gb-ppu

**Feature Branch**: `[002-gb-ppu]`

**Created**: 2026-10-24

**Status**: Draft

**Input**: User description: "Реализовать PPU (Picture Processing Unit) для Game Boy (DMG). Регистры LCDC, STAT, SCY, SCX, LY, LYC, BGP, OBP0, OBP1, WY, WX. Режимы PPU: OAM scan (mode 2), Drawing (mode 3), HBlank (mode 0), VBlank (mode 1). Синхронизация с CPU по тактам (456 тактов на линию). Тайловый рендер: 160x144 экран, фон (background), окно (window), спрайты (8x8 и 8x16). Прерывания: VBlank (IF bit 0), LCD STAT (IF bit 1) — LYC=LY, mode 0, mode 1, mode 2. Связка с SDL2: вывод кадра 160x144 (пока без масштабирования или с простым nearest-neighbor). Критерий готовности: Tetris запускается, отрисовывается игровое поле, управление работает."

## User Scenarios & Testing (mandatory)

### User Story 1 - Render Tetris Gameplay (Priority: P1)

As a player running a retail Tetris ROM, I can see the title screen and gameplay field rendered correctly at 160x144 so that I can start and play a round without visual glitches.

Why this priority: Tetris is a practical benchmark confirming end-to-end display, scrolling, and sprite overlay.

Independent Test: Launch Tetris and reach gameplay; the field, pieces, score, and UI render correctly and remain stable during play.

Acceptance Scenarios:

1. Given the game at the title screen, when the user starts a game, then the playfield background scrolls and the tetromino sprites appear over it with correct priority and transparency.
2. Given a running game, when multiple sprites are on the same scanline, then no more than 10 sprites are rendered on that line and the selection priority follows documented behavior; visuals remain coherent (no stray pixels or flicker beyond hardware-accurate limits).

---

### User Story 2 - Accurate PPU Timing & Modes (Priority: P1)

As a developer validating display timing, I can observe the PPU progressing through modes (OAM scan, Drawing, HBlank, VBlank) per scanline cadence so that timing-dependent software behaves correctly.

Why this priority: Many games depend on mode timings for effects and interrupts.

Independent Test: Observe LY progression 0–153, with ~456 cycles per visible line and VBlank period lines 144–153; mode flags in the status register reflect the current phase.

Acceptance Scenarios:

1. Given display enabled, when a frame begins, then lines 0–143 render with mode sequence 2→3→0 and lines 144–153 are in mode 1; LY increments each line.
2. Given LYC set to a value, when LY equals LYC, then the coincidence flag is set and the configured status interrupt can occur.

---

### User Story 3 - Interrupt Signaling (Priority: P1)

As a developer relying on display events, I receive vertical blanking and status interrupts at appropriate times so that the main loop can synchronize non-display work and register updates safely.

Why this priority: Games commonly perform updates during VBlank and use status events for split effects.

Independent Test: With interrupts enabled in software, vertical blanking triggers once per frame; status interrupts can be configured for modes 0/1/2 and LYC=LY.

Acceptance Scenarios:

1. Given VBlank enabled, when a frame completes (entering mode 1), then a vertical blanking interrupt becomes pending.
2. Given status interrupt configuration, when the PPU enters the selected mode or LYC matches LY, then a status interrupt becomes pending with proper priority order.

### Edge Cases

- Window overlay starts at its X/Y with documented X offset behavior, covering background pixels where enabled.
- Sprite priority: background-over-sprite and sprite-over-background rules follow DMG behavior, including sprite-to-sprite priority by X position and OAM order when equal.
- Sprite limits: maximum 10 sprites per visible scanline; sprite size selectable (8x8 or 8x16) where configured.
- Palette application: background palette maps 2-bit colors; object palettes apply per-sprite attributes with transparency for color 0.
- Scroll and window interactions: SCX/SCY scrolling independent of window position; window may be disabled or enabled mid-frame via control.

## Requirements (mandatory)

### Functional Requirements

- FR-001: The display produces a 160x144 visible image composed of background, optional window, and sprites, with tile-based rendering consistent with the DMG model.
- FR-002: The system supports display control and status via display control and status registers (enable/disable, tile map selections, window enable, sprite size, mode flags, and coincidence flag behavior).
- FR-003: Scrolling registers adjust background position (horizontal and vertical) seamlessly without tearing; window position registers place the window region appropriately on the screen.
- FR-004: Background rendering uses 8x8 tiles with correct addressing and per-pixel color selection; window uses the same tile system and overlays as enabled.
- FR-005: Sprites render with 8x8 or 8x16 dimensions based on configuration, honoring per-sprite attributes (priority to background/window, palette select, flip), and enforcing the 10-sprites-per-scanline limit.
- FR-006: The PPU advances through modes per scanline cadence: OAM scan, pixel drawing, HBlank for visible lines; VBlank for lines after the last visible line; approximately 456 cycles per visible line.
- FR-007: The current line indicator increments from the first visible line through the VBlank period; a line-compare register sets a coincidence flag when equal and can raise a status interrupt per configuration.
- FR-008: Vertical blanking raises a frame interrupt once per frame upon entering the VBlank period; status interrupts can be generated for modes 0/1/2 and line coincidence as configured by software.
- FR-009: Color mapping applies background and object palettes consistently, with color index 0 on sprites treated as transparent; resulting image remains visually consistent frame-to-frame.
- FR-010: A complete frame buffer is available each frame for presentation, containing exactly 160x144 pixels; if scaling is applied externally, nearest-neighbor or equivalent preserves pixel intent.
- FR-011: Interaction with the central processor is synchronized so that writes to display registers mid-frame produce effects consistent with hardware expectations (e.g., split scroll using status interrupts).
- FR-012: When the display is disabled via control, the screen clears appropriately and timing resets per documented behavior; enabling restarts timing from the top of the frame.

### Key Entities

- Display State: display enable flag, control flags for background/window/sprites, sprite size, tile map selections, palette values, scroll positions, window position, current line, line-compare value, mode indicator, and coincidence flag.
- Frame Buffer: 160x144 pixel buffer updated line-by-line during drawing modes and consumed by the presentation layer once per frame.
- Interrupt Sources: vertical blanking and configurable status events (modes 0/1/2 and line coincidence) that can become pending for the processor to service.

## Success Criteria (mandatory)

### Measurable Outcomes

- SC-001: The Tetris ROM reaches and displays the title screen and gameplay field within 5 seconds of launch; visuals remain stable for at least 10 minutes of continuous play.
- SC-002: Sprite overlay, priority, and transparency behaviors match expected gameplay visuals in Tetris across at least 100 lines with active sprites without visual artifacts beyond hardware-accurate limits.
- SC-003: Vertical blanking occurs once per frame and can be detected by software; status interrupts can be observed for each configured mode and for line coincidence within a single test run.
- SC-004: The on-screen image is 160x144 pixels with legible tiles; scrolling and window movements appear continuous without visible tearing during normal gameplay.
- SC-005: Over three consecutive runs of the same ROM, rendered frames for identical input sequences are visually consistent (no divergent artifacts).

## Assumptions

- The baseline model is the original handheld variant; color-specific features are out of scope for this feature.
- Presentation to a display surface occurs outside this feature; this feature ensures a valid 160x144 frame is produced each frame.
- Input handling and central processor functionality are provided by existing components; this feature relies on their correctness for overall behavior.
