#include "CPU.hpp"
#include "PPU.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

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

    // Memory and I/O stubs
    std::vector<uint8_t> mem(0x10000, 0x00);
    for (size_t i = 0; i < rom.size() && i < mem.size(); ++i) mem[i] = rom[i];

    struct SerialState {
        uint8_t SB{0}; // FF01
        uint8_t SC{0}; // FF02
        std::string out;
    } serial;

    uint8_t IF = 0xE1; // FF0F post-boot default per request
    uint8_t IE = 0x00; // FFFF
    uint8_t DIV = 0xAB; // FF04
    uint8_t TIMA = 0, TMA = 0, TAC = 0;

    Bus bus{};
    bus.read8 = [&](uint16_t addr) -> uint8_t {
        if (addr == 0xFF01) return serial.SB;
        if (addr == 0xFF02) return serial.SC;
        if (addr == 0xFF0F) return static_cast<uint8_t>(IF | 0xE0);
        if (addr == 0xFFFF) return IE;
        if (addr == 0xFF04) return DIV;
        if (addr == 0xFF44) return 0x90; // temporary: force LY=0x90 for CPU tests
        if (addr == 0xFF05) return TIMA;
        if (addr == 0xFF06) return TMA;
        if (addr == 0xFF07) return TAC;
        if (addr == 0xFF40 || (addr >= 0xFF41 && addr <= 0xFF4B)) {
            if (bus.ppu) {
                return static_cast<PPU*>(bus.ppu)->read(addr);
            }
        }
        return mem[addr];
    };
    bus.write8 = [&](uint16_t addr, uint8_t value) {
        if (addr == 0xFF01) { serial.SB = value; return; }
        if (addr == 0xFF02) {
            serial.SC = value;
            if (value & 0x80) {
                serial.out.push_back(static_cast<char>(serial.SB));
                serial.SC &= ~0x80;
            }
            return;
        }
        if (addr == 0xFF0F) { IF = static_cast<uint8_t>(value & 0x1F); return; }
        if (addr == 0xFFFF) { IE = static_cast<uint8_t>(value & 0x1F); return; }
        if (addr == 0xFF04) { DIV = 0; return; }
        if (addr == 0xFF05) { TIMA = value; return; }
        if (addr == 0xFF06) { TMA = value; return; }
        if (addr == 0xFF07) { TAC = value; return; }
        if (addr == 0xFF40 || (addr >= 0xFF41 && addr <= 0xFF4B)) {
            if (bus.ppu) {
                static_cast<PPU*>(bus.ppu)->write(addr, value);
                return;
            }
        }
        if (addr < 0x8000) return; // ignore writes to ROM area
        mem[addr] = value;
    };

    // Initialize PPU and hook into bus
    PPU ppu(&bus);
    bus.ppu = &ppu;
    bus.ppu_tick = [&](uint32_t cyc){ ppu.tick(cyc); };

    CPU cpu(&bus);
    cpu.setAF(0x01B0);
    cpu.setBC(0x0013);
    cpu.setDE(0x00D8);
    cpu.setHL(0x014D);
    cpu.SP = 0xFFFE;
    cpu.PC = 0x0100;

    // First log line before executing any instruction (PC should be 0100)
    {
        uint8_t m0 = bus.read8(cpu.PC);
        uint8_t m1 = bus.read8(static_cast<uint16_t>(cpu.PC + 1));
        uint8_t m2 = bus.read8(static_cast<uint16_t>(cpu.PC + 2));
        uint8_t m3 = bus.read8(static_cast<uint16_t>(cpu.PC + 3));
        std::fprintf(stderr,
                     "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
                     cpu.A, cpu.F, cpu.B, cpu.C, cpu.D, cpu.E, cpu.H, cpu.L, cpu.SP, cpu.PC, m0, m1, m2, m3);
    }

    const uint64_t max_steps = 10000000ULL; // increase step limit to 10,000,000
    bool seen_pass = false;
    uint32_t post_pass_remaining = 0;
    // Collect loop trace entries for PC in [0xC000, 0xC010]
    std::vector<std::string> loop_logs;
    // Collect detailed trace for PC in [0xC000, 0xC020] and selected register writes
    std::vector<std::string> trace_logs;
    // Rebuild bus with trace access for register writes
    bus.write8 = [&](uint16_t addr, uint8_t value) {
        if (addr == 0xFF01) { serial.SB = value; return; }
        if (addr == 0xFF02) {
            serial.SC = value;
            if (value & 0x80) {
                serial.out.push_back(static_cast<char>(serial.SB));
                serial.SC &= ~0x80;
            }
            return;
        }
        if (addr == 0xFF0F) { IF = static_cast<uint8_t>(value & 0x1F); return; }
        if (addr == 0xFFFF) {
            IE = static_cast<uint8_t>(value & 0x1F);
            char buf[64]; std::snprintf(buf, sizeof(buf), "REG: write IE=%02X", IE);
            trace_logs.emplace_back(buf);
            return;
        }
        if (addr == 0xFF04) { DIV = 0; return; }
        if (addr == 0xFF05) { TIMA = value; return; }
        if (addr == 0xFF06) { TMA = value; return; }
        if (addr == 0xFF07) { TAC = value; char buf[64]; std::snprintf(buf, sizeof(buf), "REG: write TAC=%02X", TAC); trace_logs.emplace_back(buf); return; }
        if (addr == 0xFF40 || (addr >= 0xFF41 && addr <= 0xFF4B)) {
            if (bus.ppu) { static_cast<PPU*>(bus.ppu)->write(addr, value); return; }
        }
        if (addr < 0x8000) return; // ignore writes to ROM area
        mem[addr] = value;
    };
    for (uint64_t i = 0; i < max_steps; ++i) {
        // Pre-step snapshot for PC-range trace
        uint16_t prevPC = cpu.PC;
        uint8_t pm0 = bus.read8(prevPC);
        uint8_t pm1 = bus.read8(static_cast<uint16_t>(prevPC + 1));
        uint8_t pm2 = bus.read8(static_cast<uint16_t>(prevPC + 2));
        uint8_t pm3 = bus.read8(static_cast<uint16_t>(prevPC + 3));

        uint32_t cyc = cpu.step();

        // Лог только если реально выполнена инструкция (а не холостой цикл HALT)
        // У нас нет прямого флага из CPU, поэтому используем эвристику: если были 4 цикла и PC/регистры не менялись, это может быть HALT.
        // Более надёжно: логируем всегда после step, но для 02-interrupts достаточно фильтровать явные холостые спины по PCMEM=76 (HALT) и отсутствию IF→service.

        // Для простоты: если opcode HALT (0x76) на PC перед step и IME=0 и IF!=0, step только снимает HALT без fetch — пропустим лог.
        // Здесь у нас нет prefetch, поэтому просто логируем всегда — Gameboy Doctor допускает линии на границе инструкций.

        uint8_t m0 = bus.read8(cpu.PC);
        uint8_t m1 = bus.read8(static_cast<uint16_t>(cpu.PC + 1));
        uint8_t m2 = bus.read8(static_cast<uint16_t>(cpu.PC + 2));
        uint8_t m3 = bus.read8(static_cast<uint16_t>(cpu.PC + 3));
        std::fprintf(stderr,
                     "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
                     cpu.A, cpu.F, cpu.B, cpu.C, cpu.D, cpu.E, cpu.H, cpu.L, cpu.SP, cpu.PC, m0, m1, m2, m3);

        // Specific loop trace: only when PC in range 0xC000-0xC010 (post-step PC)
        if (cpu.PC >= 0xC000 && cpu.PC <= 0xC010) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "LOOP: PC=%04X A=%02X F=%02X", cpu.PC, cpu.A, cpu.F);
            loop_logs.emplace_back(buf);
        }

        // Detailed trace: log all instructions when prevPC in 0xC000-0xC020 including PCMEM
        if (prevPC >= 0xC000 && prevPC <= 0xC020) {
            char tbuf[160];
            std::snprintf(tbuf, sizeof(tbuf),
                          "TRACE: PC=%04X A=%02X F=%02X PCMEM:%02X,%02X,%02X,%02X",
                          prevPC, cpu.A, cpu.F, pm0, pm1, pm2, pm3);
            trace_logs.emplace_back(tbuf);
        }

        if (!seen_pass && serial.out.find("Passed") != std::string::npos) {
            seen_pass = true;
            post_pass_remaining = 1000; // continue for 1000 more instructions
        } else if (seen_pass) {
            if (post_pass_remaining > 0) {
                --post_pass_remaining;
            }
            if (post_pass_remaining == 0) {
                return 0; // done after extra logging window
            }
        }
    }
    // At end, print first and last 100 LOOP trace lines as requested
    std::fprintf(stderr, "===== LOOP TRACE (first 100) =====\n");
    size_t firstN = loop_logs.size() < 100 ? loop_logs.size() : 100;
    for (size_t i = 0; i < firstN; ++i) {
        std::fprintf(stderr, "%s\n", loop_logs[i].c_str());
    }
    std::fprintf(stderr, "===== LOOP TRACE (last 100) =====\n");
    size_t start = loop_logs.size() > 100 ? (loop_logs.size() - 100) : 0;
    for (size_t i = start; i < loop_logs.size(); ++i) {
        std::fprintf(stderr, "%s\n", loop_logs[i].c_str());
    }

    // Also print first 200 detailed trace lines
    std::fprintf(stderr, "===== TRACE (first 200) =====\n");
    size_t firstT = trace_logs.size() < 200 ? trace_logs.size() : 200;
    for (size_t i = 0; i < firstT; ++i) {
        std::fprintf(stderr, "%s\n", trace_logs[i].c_str());
    }

    std::fprintf(stderr, "Max steps reached without 'Passed'.\n");
    return 1;
}
