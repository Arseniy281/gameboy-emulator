// Simple SDL2 frontend to run BG-only PPU and display frames
#include "CPU.hpp"
#include "PPU.hpp"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>

using namespace gb;

static std::vector<uint8_t> load_rom(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data;
    data.resize(static_cast<size_t>(size));
    if (!data.empty()) f.read(reinterpret_cast<char*>(data.data()), data.size());
    return data;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <rom.gb>\n", argv[0]);
        return 2;
    }
    const std::string rom_path = argv[1];
    auto rom = load_rom(rom_path);
    if (rom.empty()) {
        std::fprintf(stderr, "Failed to load ROM: %s\n", rom_path.c_str());
        return 2;
    }

    // Memory map with simple MBC1-like banking stub: switchable 16KB at 0x4000-0x7FFF
    std::vector<uint8_t> mem(0x10000, 0x00);
    // Store whole ROM for banked reads
    std::vector<uint8_t> rom_full = rom;
    uint8_t mbc1_rom_bank = 1; // initial bank 1 at 0x4000
    auto read_rom_banked = [&](uint16_t addr) -> uint8_t {
        if (addr < 0x4000) {
            return (addr < rom_full.size()) ? rom_full[addr] : 0xFF;
        } else if (addr < 0x8000) {
            size_t base = static_cast<size_t>(mbc1_rom_bank) * 0x4000;
            size_t idx = base + (addr - 0x4000);
            return (idx < rom_full.size()) ? rom_full[idx] : 0xFF;
        }
        return 0xFF;
    };
    // Initialize bank 0 and bank 1 view into mem for simplicity
    for (uint16_t a = 0; a < 0x8000 && a < mem.size(); ++a) mem[a] = read_rom_banked(a);

    // Simple IO state
    uint8_t IF = 0xE1, IE = 0x00;
    uint8_t DIV = 0xAB, TIMA = 0, TMA = 0, TAC = 0;
    uint8_t DMA = 0; // FF46 OAM DMA
    // Joypad: JOYP at FF00
    uint8_t JOYP_select = 0x30; // bits 5-4 default to 1 (no group selected)
    uint8_t JOYP_buttons = 0x0F; // A,B,Select,Start (bit 0..3), 1=not pressed, 0=pressed
    uint8_t JOYP_dpad = 0x0F;    // Right,Left,Up,Down (bit 0..3), 1=not pressed, 0=pressed

    Bus bus{};
    PPU ppu(&bus);
    bus.ppu = &ppu;
    bus.ppu_tick = [&](uint32_t cyc){ ppu.tick(cyc); };

    bus.read8 = [&](uint16_t addr) -> uint8_t {
        if (addr == 0xFF00) {
            // JOYP: bits 5-4 select groups (0=select), bits 3-0 are active-low
            uint8_t res = static_cast<uint8_t>(0xC0 | (JOYP_select & 0x30));
            uint8_t out = 0x0F;
            if ((JOYP_select & 0x10) == 0) out &= JOYP_dpad;
            if ((JOYP_select & 0x20) == 0) out &= JOYP_buttons;
            return static_cast<uint8_t>(res | out);
        }
        if (addr == 0xFF0F) return static_cast<uint8_t>(IF | 0xE0);
        if (addr == 0xFFFF) return IE;
        if (addr == 0xFF04) return DIV;
        if (addr == 0xFF05) return TIMA;
        if (addr == 0xFF06) return TMA;
        if (addr == 0xFF07) return TAC;
        if (addr == 0xFF46) return DMA;
        if (addr >= 0xFF40 && addr <= 0xFF4B) return ppu.read(addr);
        if (addr < 0x8000) return read_rom_banked(addr);
        return mem[addr];
    };
    auto vram_locked = [&](uint16_t a) -> bool {
        // VRAM (0x8000-0x9FFF) and OAM (0xFE00-0xFE9F) lockouts during MODE 3 (or OAM during modes 2/3)
        uint8_t stat = ppu.read(0xFF41);
        uint8_t mode = stat & 0x03;
        // Mode 3 blocks VRAM and OAM; Mode 2 blocks OAM
        return ( (a >= 0x8000 && a <= 0x9FFF && mode == 3) ||
                 (a >= 0xFE00 && a <= 0xFE9F && (mode == 2 || mode == 3)) );
    };
    // OAM DMA stall tracking
    uint32_t dma_stall_cycles = 0;
    bus.take_stall = [&]() -> uint32_t {
        if (dma_stall_cycles > 0) {
            uint32_t chunk = 4; // process in 4-cycle steps
            if (dma_stall_cycles < chunk) chunk = dma_stall_cycles;
            dma_stall_cycles -= chunk;
            return chunk;
        }
        return 0;
    };
    bus.write8 = [&](uint16_t addr, uint8_t value) {
        if (addr == 0xFF00) {
            // Only bits 5 and 4 writable; lower bits reflect state
            JOYP_select = static_cast<uint8_t>(value & 0x30);
            return;
        }
        if (addr == 0xFF0F) { IF = static_cast<uint8_t>(value & 0x1F); return; }
        if (addr == 0xFFFF) { IE = static_cast<uint8_t>(value & 0x1F); return; }
        if (addr == 0xFF04) { DIV = 0; return; }
        if (addr == 0xFF05) { TIMA = value; return; }
        if (addr == 0xFF06) { TMA = value; return; }
        if (addr == 0xFF07) { TAC = value; return; }
        if (addr == 0xFF46) {
            // OAM DMA: copy 160 bytes from (value * 0x100) to FE00-FE9F
            DMA = value;
            uint16_t src = static_cast<uint16_t>(value) << 8;
            // Schedule DMA stall for 160 M-cycles (we simulate as 160 * 4 CPU cycles = 640 cycles)
            dma_stall_cycles = 640;
            for (int i = 0; i < 160; ++i) {
                uint16_t s = static_cast<uint16_t>(src + i);
                uint16_t d = static_cast<uint16_t>(0xFE00 + i);
                if (s < mem.size() && d < mem.size()) mem[d] = bus.read8 ? bus.read8(s) : mem[s];
            }
            return;
        }
        if (addr >= 0xFF40 && addr <= 0xFF4B) { ppu.write(addr, value); return; }
        // VRAM/OAM lockout during PPU draw/OAM modes
        if (vram_locked(addr)) return;
        // MBC1-like control: 0000-1FFF (RAM enable), 2000-3FFF (ROM bank low 5 bits)
        if (addr <= 0x1FFF) {
            return;
        }
        if (addr >= 0x2000 && addr <= 0x3FFF) {
            uint8_t bank = static_cast<uint8_t>(value & 0x1F);
            if (bank == 0) bank = 1; // MBC1 maps 0 -> 1
            mbc1_rom_bank = bank;
            // Update live view into mem for 0x4000-0x7FFF
            for (uint16_t a = 0x4000; a < 0x8000 && a < mem.size(); ++a) mem[a] = read_rom_banked(a);
            return;
        }
        if (addr < 0x8000) return; // ignore ROM writes
        mem[addr] = value;
    };

    CPU cpu(&bus);
    cpu.setAF(0x01B0);
    cpu.setBC(0x0013);
    cpu.setDE(0x00D8);
    cpu.setHL(0x014D);
    cpu.SP = 0xFFFE;
    cpu.PC = 0x0100;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 3;
    }
    SDL_Window* win = SDL_CreateWindow("GB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 160 * 3, 144 * 3, SDL_WINDOW_SHOWN);
    if (!win) { std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return 3; }
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError()); return 3; }
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    if (!tex) { std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError()); return 3; }

    bool running = true;
    uint64_t frame_start = SDL_GetPerformanceCounter();
    const uint64_t freq = SDL_GetPerformanceFrequency();

    auto set_button = [&](bool pressed, bool is_button_group, int bit_index) {
        uint8_t* group = is_button_group ? &JOYP_buttons : &JOYP_dpad;
        uint8_t prev = *group;
        if (pressed) {
            *group = static_cast<uint8_t>(*group & static_cast<uint8_t>(~(1u << bit_index)));
        } else {
            *group = static_cast<uint8_t>(*group | static_cast<uint8_t>(1u << bit_index));
        }
        // Request Joypad interrupt on 1->0 transition
        if (((prev >> bit_index) & 1u) == 1u && ((static_cast<int>(*group >> bit_index) & 1) == 0)) {
            IF = static_cast<uint8_t>(IF | 0x10);
        }
    };

    const double target_hz = 59.73; // DMG ~59.73 Hz
    const double target_dt = 1.0 / target_hz;
    const uint32_t cycles_per_frame = 70224; // 456*154

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                bool pressed = (e.type == SDL_KEYDOWN);
                SDL_Scancode sc = e.key.keysym.scancode;
                switch (sc) {
                    // D-Pad
                    case SDL_SCANCODE_RIGHT: set_button(pressed, false, 0); break; // Right
                    case SDL_SCANCODE_LEFT:  set_button(pressed, false, 1); break; // Left
                    case SDL_SCANCODE_UP:    set_button(pressed, false, 2); break; // Up
                    case SDL_SCANCODE_DOWN:  set_button(pressed, false, 3); break; // Down
                    // Buttons
                    case SDL_SCANCODE_Z:     set_button(pressed, true, 0); break; // A
                    case SDL_SCANCODE_X:     set_button(pressed, true, 1); break; // B
                    case SDL_SCANCODE_LSHIFT:
                    case SDL_SCANCODE_RSHIFT:set_button(pressed, true, 2); break; // Select
                    case SDL_SCANCODE_RETURN:set_button(pressed, true, 3); break; // Start
                    default: break;
                }
            }
        }

        // Run CPU until a frame is ready (VBlank) or for a fixed instruction budget (~50k steps)
        uint64_t cycles_accum = 0;
        uint64_t steps_accum = 0;
        while (!ppu.frameReady() && cycles_accum < (cycles_per_frame * 4ULL) && steps_accum < 50000ULL) {
            cycles_accum += cpu.step();
            ++steps_accum;
        }
        if (ppu.frameReady()) {
            // Upload framebuffer into texture
            void* pixels = nullptr; int pitch = 0;
            if (SDL_LockTexture(tex, nullptr, &pixels, &pitch) == 0) {
                const uint32_t* fb = ppu.frameBuffer();
                // pitch is in bytes; our width is 160 * 4
                for (int y = 0; y < 144; ++y) {
                    std::memcpy(static_cast<uint8_t*>(pixels) + y * pitch, fb + y * 160, 160 * sizeof(uint32_t));
                }
                SDL_UnlockTexture(tex);
            }
            ppu.clearFrameReady();

            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);

            // Pace to ~59.73 FPS if vsync doesn't lock us
            double elapsed = static_cast<double>(SDL_GetPerformanceCounter() - frame_start) / static_cast<double>(freq);
            if (elapsed < target_dt) {
                SDL_Delay(static_cast<uint32_t>((target_dt - elapsed) * 1000.0));
            }
            frame_start = SDL_GetPerformanceCounter();
        }
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
