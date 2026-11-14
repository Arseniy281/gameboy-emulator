# Data Model: gb-ppu

## Entities

### PPU Registers

- LCDC: display enable, BG/window/sprite enable, window enable, tile map selections, tile data sign mode, sprite size
- STAT: mode flags (0/1/2/3), coincidence flag, interrupt enables (modes 0/1/2 and LYC)
- SCY/SCX: background scroll Y/X
- LY/LYC: current line and compare line
- BGP: background palette (4 color indices)
- OBP0/OBP1: object palettes (deferred to Milestone 2)
- WY/WX: window position (deferred to Milestone 2)

### PPU State

- Mode: current PPU mode (0=HBlank,1=VBlank,2=OAM,3=Drawing)
- Cycle Counter: accumulated cycles within the current line/mode
- Line Buffer: 160-pixel buffer for the current scanline (background-only in Milestone 1)
- Frame Buffer: 160x144 buffer; updated line-by-line
- Interrupt Latches: VBlank pending; STAT pending per enables

### Memory Regions (via bus)

- Tile Data: two regions (0x8000 base unsigned indices; 0x8800 base signed indices)
- BG Tile Maps: two regions (0x9800 and 0x9C00)
- OAM: deferred to Milestone 2

## Relationships and Rules

- LY increments after each full line duration; LYC coincidence set/cleared at line start and checked against STAT enables.
- Background pixel fetch uses SCX/SCY to determine tile row/column and within-tile line; palette mapping via BGP to grayscale.
- Display enable (LCDC) controls PPU activity; disabling clears frame buffer and resets LY/mode per documentation.

## Validation Rules

- LY must progress 0..143 for visible lines and 144..153 during VBlank; mode transitions align with cycle thresholds (OAM→Drawing→HBlank→VBlank).
- Exactly 160 pixels produced per visible line; off-screen fetches wrap per tilemap addressing.
