#include "CPU.hpp"
#include "PPU.hpp"
#include <cassert>
#include <vector>

using namespace gb;

int main() {
    std::vector<uint8_t> mem(0x10000, 0x00);
    uint8_t IF = 0, IE = 0, DIV = 0, TIMA = 0, TMA = 0, TAC = 0;
    Bus bus{};
    bus.read8 = [&](uint16_t addr) -> uint8_t {
        if (addr == 0xFF0F) return static_cast<uint8_t>(IF | 0xE0);
        if (addr == 0xFFFF) return IE;
        if (addr == 0xFF04) return DIV;
        if (addr == 0xFF05) return TIMA;
        if (addr == 0xFF06) return TMA;
        if (addr == 0xFF07) return TAC;
        if (addr == 0xFF40 || (addr >= 0xFF41 && addr <= 0xFF4B)) {
            if (bus.ppu) return static_cast<PPU*>(bus.ppu)->read(addr);
        }
        return mem[addr];
    };
    bus.write8 = [&](uint16_t addr, uint8_t v) {
        if (addr == 0xFF0F) { IF = static_cast<uint8_t>(v & 0x1F); return; }
        if (addr == 0xFFFF) { IE = static_cast<uint8_t>(v & 0x1F); return; }
        if (addr == 0xFF04) { DIV = 0; return; }
        if (addr == 0xFF05) { TIMA = v; return; }
        if (addr == 0xFF06) { TMA = v; return; }
        if (addr == 0xFF07) { TAC = v; return; }
        if (addr == 0xFF44) {
            // LY is read-only; ignore mirrored writes from PPU.tick
            return;
        }
        if (addr == 0xFF40 || (addr >= 0xFF41 && addr <= 0xFF4B)) {
            if (bus.ppu) { static_cast<PPU*>(bus.ppu)->write(addr, v); return; }
        }
        mem[addr] = v;
    };

    PPU ppu(&bus);
    bus.ppu = &ppu;
    bus.ppu_tick = [&](uint32_t cyc){ ppu.tick(cyc); };

    // Enable LCDC (bit7=1)
    static_cast<PPU*>(bus.ppu)->write(0xFF40, 0x80);

    // Verify LY increments every 456 cycles
    uint8_t ly_before = static_cast<PPU*>(bus.ppu)->read(0xFF44);
    // Advance 1 line
    bus.ppu_tick(456);
    uint8_t ly_after1 = static_cast<PPU*>(bus.ppu)->read(0xFF44);
    assert(ly_after1 == static_cast<uint8_t>(ly_before + 1));

    // Advance 9 more lines
    for (int i = 0; i < 9; ++i) bus.ppu_tick(456);
    uint8_t ly_after10 = static_cast<PPU*>(bus.ppu)->read(0xFF44);
    assert(ly_after10 == static_cast<uint8_t>(ly_before + 10));

    return 0;
}
