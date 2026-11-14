// PPU implementation

#include "PPU.hpp"
#include "CPU.hpp" // for Bus declaration and IF/IE access via bus
#include <cstdio>

namespace gb {

// We reuse Bus read/write callbacks for register mirroring
struct Bus;

PPU::PPU(Bus* bus) : bus_(bus) {
    // Initialize STAT mode to 2 (OAM search) at power-on
    stat_ = static_cast<uint8_t>((stat_ & 0xFC) | 0x02);
    // LY reset
    ly_ = 0;
}

uint8_t PPU::read(uint16_t addr) const {
    switch (addr) {
        case 0xFF40: return lcdc_;
        case 0xFF41: return static_cast<uint8_t>((stat_ & 0xF8) | (mode_ & 0x03) | ((ly_ == lyc_) ? 0x04 : 0));
        case 0xFF42: return scy_;
        case 0xFF43: return scx_;
        case 0xFF44: return ly_;
        case 0xFF45: return lyc_;
        case 0xFF47: return bgp_;
        case 0xFF48: return obp0_;
        case 0xFF49: return obp1_;
        case 0xFF4A: return wy_;
        case 0xFF4B: return wx_;
        default: return 0xFF;
    }
}

void PPU::write(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0xFF40: {
            // Detect LCD enable transitions on bit7
            uint8_t old = lcdc_;
            lcdc_ = value;
            bool was_on = (old & 0x80) != 0;
            bool now_on = (lcdc_ & 0x80) != 0;
            if (!was_on && now_on) {
                // Turning LCD ON: reset LY and timing, enter OAM (mode 2)
                ly_ = 0;
                line_cycle_ = 0;
                frame_ready_ = false;
                setMode(2);
                updateCoincidence();
            } else if (was_on && !now_on) {
                // Turning LCD OFF: stop ticking, LY=0, HBlank mode (0)
                ly_ = 0;
                line_cycle_ = 0;
                frame_ready_ = false;
                setMode(0);
                updateCoincidence();
            }
            break;
        }
        case 0xFF41: stat_ = static_cast<uint8_t>((stat_ & 0x07) | (value & 0x78)); break; // writable: bits 6-3; lower reflect status
        case 0xFF42: scy_ = value; break;
        case 0xFF43: scx_ = value; break;
        case 0xFF44: /* LY is read-only; writing resets LY to 0 on some docs */ ly_ = 0; break;
        case 0xFF45: lyc_ = value; updateCoincidence(); break;
        case 0xFF47: bgp_ = value; break;
        case 0xFF48: obp0_ = value; break;
        case 0xFF49: obp1_ = value; break;
        case 0xFF4A: wy_ = value; break;
        case 0xFF4B: wx_ = value; break;
        default: break;
    }
}

void PPU::setMode(uint8_t m) {
    mode_ = static_cast<uint8_t>(m & 0x03);
    // Reflect mode in STAT low bits (read path ORs it)
    // Trigger STAT interrupt when entering certain modes if enabled
    requestSTAT();
}

void PPU::updateCoincidence() {
    // Coincidence flag is bit 2; set if LY==LYC
    if (ly_ == lyc_) {
        stat_ |= 0x04;
        // Coincidence interrupt enable is STAT bit 6
        if (stat_ & 0x40) requestSTAT();
    } else {
        stat_ &= static_cast<uint8_t>(~0x04);
    }
}

void PPU::requestSTAT() {
    // STAT interrupt enable bits: 3=mode0,4=mode1,5=mode2,6=LYC
    bool trigger = false;
    switch (mode_) {
        case 0: if (stat_ & 0x08) trigger = true; break; // HBlank
        case 1: if (stat_ & 0x10) trigger = true; break; // VBlank
        case 2: if (stat_ & 0x20) trigger = true; break; // OAM
        case 3: /* no STAT interrupt for mode 3 */ break;
    }
    // LYC=LY handled in updateCoincidence (bit 6)
    if (trigger && bus_ && bus_->read8 && bus_->write8) {
        uint8_t IF = bus_->read8(0xFF0F);
        bus_->write8(0xFF0F, static_cast<uint8_t>(IF | 0x02));
    }
}

void PPU::requestVBlank() {
    if (bus_ && bus_->read8 && bus_->write8) {
        uint8_t IF = bus_->read8(0xFF0F);
        bus_->write8(0xFF0F, static_cast<uint8_t>(IF | 0x01));
    }
}

void PPU::tick(uint32_t cycles) {
    if (!bus_) return;
    // If LCD off, LY=0 and mode=0; ignore timing for now
    if ((lcdc_ & 0x80) == 0) {
        ly_ = 0; line_cycle_ = 0; setMode(0);
        return;
    }
    line_cycle_ += cycles;
    // Visible lines 0..143: OAM(80), Transfer(~172), HBlank(~204) total 456
    while (line_cycle_ >= 0) {
        if (ly_ <= 143) {
            if (line_cycle_ < 80) { setMode(2); rendered_line_ = false; break; }
            else if (line_cycle_ < 80 + 172) {
                setMode(3);
                // Render at the start of Mode 3 once per scanline to use current SCX/SCY values
                if (!rendered_line_) {
                    renderBGLine(ly_);
                    renderOBJLine(ly_);
                    rendered_line_ = true;
                }
                break;
            }
            else if (line_cycle_ < 456) { setMode(0); break; }
            else {
                // End of line
                line_cycle_ -= 456;
                ly_ = static_cast<uint8_t>(ly_ + 1);
                if (ly_ == 144) {
                    setMode(1);
                    requestVBlank();
                    // Mark frame ready when entering VBlank
                    frame_ready_ = true;
                }
                updateCoincidence();
            }
        } else {
            // VBlank lines 144..153
            setMode(1);
            if (line_cycle_ >= 456) {
                line_cycle_ -= 456;
                ly_ = static_cast<uint8_t>(ly_ + 1);
                if (ly_ > 153) {
                    ly_ = 0;
                    setMode(2); // next frame starts in OAM
                    // Clear frame_ready_ at the start of a new frame
                    frame_ready_ = false;
                }
                updateCoincidence();
            } else {
                break;
            }
        }
    }
    // LY is exposed via read() path; do not mirror via bus write to avoid unintended resets
}

// Map BGP palette value to ARGB8888 grayscale
inline uint32_t PPU::mapBgpShade(uint8_t color_id) const {
    // BGP encodes 2-bit colors for BG: bits [1:0]=color0, [3:2]=color1, [5:4]=color2, [7:6]=color3
    uint8_t shade = 0xFF; // default white
    // DMG colors: 0->white, 1->light gray, 2->dark gray, 3->black
    switch (color_id & 0x03) {
        case 0: shade = 0xE0; break; // light
        case 1: shade = 0xC0; break;
        case 2: shade = 0x80; break;
        case 3: shade = 0x20; break; // dark
    }
    return 0xFF000000u | (static_cast<uint32_t>(shade) << 16) | (static_cast<uint32_t>(shade) << 8) | shade;
}

void PPU::renderBGLine(uint8_t line) {
    // Render if BG enabled (LCDC.0). If disabled, fill white and proceed to Window if enabled.
    bool bg_enabled = (lcdc_ & 0x01) != 0;
    for (int x = 0; x < 160; ++x) { framebuffer_[line * 160 + x] = 0xFFFFFFFFu; bg_color_[line*160+x] = 0; }
    // Determine tile map base from LCDC.3
    uint16_t tilemap_base = (lcdc_ & 0x08) ? 0x9C00 : 0x9800;
    // Determine tile data mode from LCDC.4: 1=0x8000 unsigned, 0=0x8800 signed
    bool tiledata_unsigned = (lcdc_ & 0x10) != 0;
    // uint16_t tiledata_base = tiledata_unsigned ? 0x8000 : 0x8800; // use base in addressing below

    // Effective scrolled Y within BG (0..255)
    // Y = (LY + SCY) % 256
    uint8_t scrolled_y = static_cast<uint8_t>(scy_ + line);
    uint8_t tile_y = static_cast<uint8_t>(scrolled_y >> 3); // /8
    uint8_t fine_y = static_cast<uint8_t>(scrolled_y & 7);   // %8

    if (bg_enabled) {
        for (int x = 0; x < 160; ++x) {
            // X = (pixel_x + SCX) % 256
            uint8_t scrolled_x = static_cast<uint8_t>(scx_ + x);
            uint8_t tile_x = static_cast<uint8_t>(scrolled_x >> 3);
            // Bitplanes are MSB-left, so fine bit index is inverse of x within tile
            uint8_t fine_x = static_cast<uint8_t>(7 - (scrolled_x & 7));
            
            // Fetch tile index from map (32x32 entries)
            uint16_t map_addr = static_cast<uint16_t>(tilemap_base + (static_cast<uint16_t>(tile_y) * 32u) + tile_x);
            uint8_t tile_index = bus_->read8 ? bus_->read8(map_addr) : 0;
            
            // Compute tile pattern address
            uint16_t tile_addr;
            if (tiledata_unsigned) {
                tile_addr = static_cast<uint16_t>(0x8000 + tile_index * 16 + fine_y * 2);
            } else {
                int8_t signed_index = static_cast<int8_t>(tile_index);
                tile_addr = static_cast<uint16_t>(0x9000 + signed_index * 16 + fine_y * 2);
            }
            
            // Read two bitplanes for this row
            uint8_t lo = bus_->read8 ? bus_->read8(tile_addr) : 0;
            uint8_t hi = bus_->read8 ? bus_->read8(static_cast<uint16_t>(tile_addr + 1)) : 0;
            // Extract color id for pixel
            uint8_t bit = 1u << fine_x;
            uint8_t b0 = (lo & bit) ? 1 : 0;
            uint8_t b1 = (hi & bit) ? 1 : 0;
            uint8_t color_id = static_cast<uint8_t>((b1 << 1) | b0);
            
            // Apply BGP mapping
            uint8_t shade_idx;
            switch (color_id) {
                case 0: shade_idx = (bgp_ >> 0) & 0x03; break;
                case 1: shade_idx = (bgp_ >> 2) & 0; shade_idx += (bgp_ >> 2) & 0x03; break; // consistent mapping
                case 2: shade_idx = (bgp_ >> 4) & 0x03; break;
                default: shade_idx = (bgp_ >> 6) & 0x03; break;
            }
            
            uint32_t pixel = mapBgpShade(shade_idx);
            framebuffer_[line * 160 + x] = pixel;
            bg_color_[line * 160 + x] = color_id;
        }
    }
    // TODO: Window layer (LCDC.5) — reserved for future; ensure we don't overwrite BG unless window is enabled and X/Y match
}

// Map object palette shade to ARGB
inline uint32_t PPU::mapObjpShade(uint8_t palette_bits, uint8_t color_id) const {
    uint8_t shade_idx;
    switch (color_id) {
        case 0: shade_idx = (palette_bits >> 0) & 0x03; break;
        case 1: shade_idx = (palette_bits >> 2) & 0x03; break;
        case 2: shade_idx = (palette_bits >> 4) & 0x03; break;
        default: shade_idx = (palette_bits >> 6) & 0x03; break;
    }
    // Reuse grayscale mapping for simplicity
    return mapBgpShade(shade_idx);
}

void PPU::renderOBJLine(uint8_t line) {
    // Sprites enabled? LCDC.1
    if ((lcdc_ & 0x02) == 0) return;
    bool obj_8x16 = (lcdc_ & 0x04) != 0;

    struct Sprite { uint8_t y,x,tile,attr; };
    Sprite objs[40];
    // Read OAM into local array
    for (int i = 0; i < 40; ++i) {
        uint16_t base = static_cast<uint16_t>(0xFE00 + i * 4);
        objs[i].y = bus_->read8 ? bus_->read8(base + 0) : 0;
        objs[i].x = bus_->read8 ? bus_->read8(base + 1) : 0;
        objs[i].tile = bus_->read8 ? bus_->read8(base + 2) : 0;
        objs[i].attr = bus_->read8 ? bus_->read8(base + 3) : 0;
    }
    // Collect sprites that appear on this line, up to 10 by X order
    int count = 0;
    struct Slot { int idx; uint8_t x; } slots[40];
    for (int i = 0; i < 40; ++i) {
        int sy = static_cast<int>(objs[i].y) - 16;
        if (line >= 144) break;
        int height = obj_8x16 ? 16 : 8;
        if (line >= sy && line < sy + height) {
            slots[count].idx = i; slots[count].x = objs[i].x; ++count;
        }
    }
    // Sort by X coordinate ascending (stable for equal X)
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (slots[j].x < slots[i].x) { auto t = slots[i]; slots[i] = slots[j]; slots[j] = t; }
        }
    }
    if (count > 10) count = 10;

    for (int n = 0; n < count; ++n) {
        int i = slots[n].idx;
        const auto& s = objs[i];
        int sy = static_cast<int>(s.y) - 16;
        int sx = static_cast<int>(s.x) - 8;
        bool yflip = (s.attr & 0x40) != 0;
        bool xflip = (s.attr & 0x20) != 0;
        bool priority_behind_bg = (s.attr & 0x80) != 0; // 1=behind non-zero BG
        uint8_t palette_bits = (s.attr & 0x10) ? obp1_ : obp0_;
        int line_in_sprite = static_cast<int>(line) - sy;
        int tile_index = s.tile;
        if (obj_8x16) {
            // In 8x16, tile index ignores bit 0 (even/odd pair)
            tile_index &= 0xFE;
            // Upper or lower tile selection depends on line_in_sprite and Y flip
            bool upper = (line_in_sprite < 8);
            bool use_upper = yflip ? !upper : upper;
            if (!use_upper) tile_index += 1;
        }
        int line_in_tile = obj_8x16 ? (line_in_sprite % 8) : line_in_sprite;
        if (yflip) line_in_tile = 7 - line_in_tile;
        // Each tile row is two bytes at 0x8000 + tile*16 + line*2
        uint16_t addr = static_cast<uint16_t>(0x8000 + tile_index * 16 + line_in_tile * 2);
        uint8_t lo = bus_->read8 ? bus_->read8(addr) : 0;
        uint8_t hi = bus_->read8 ? bus_->read8(static_cast<uint16_t>(addr + 1)) : 0;
        for (int px = 0; px < 8; ++px) {
            int xx = xflip ? px : (7 - px);
            uint8_t bit = static_cast<uint8_t>(1u << xx);
            uint8_t b0 = (lo & bit) ? 1 : 0;
            uint8_t b1 = (hi & bit) ? 1 : 0;
            uint8_t color_id = static_cast<uint8_t>((b1 << 1) | b0);
            if (color_id == 0) continue; // color 0 transparent for sprites
            int screen_x = sx + px;
            if (screen_x < 0 || screen_x >= 160) continue;
            // Priority: behind BG if BG color != 0 and priority bit set
            if (priority_behind_bg && bg_color_[line * 160 + screen_x] != 0) continue;
            framebuffer_[line * 160 + screen_x] = mapObjpShade(palette_bits, color_id);
        }
    }
}

} // namespace gb
