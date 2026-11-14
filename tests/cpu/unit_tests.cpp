#include "CPU.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace gb;

int main() {
    // Test LD (HL),r and LD r,(HL)
    std::vector<uint8_t> mem(0x10000, 0x00);
    Bus bus{};
    bus.read8 = [&](uint16_t addr) -> uint8_t { return mem[addr]; };
    bus.write8 = [&](uint16_t addr, uint8_t v) { mem[addr] = v; };

    CPU cpu(&bus);
    cpu.PC = 0x0100;
    cpu.setHL(0xC000);
    cpu.B = 0x42;

    // Program: LD (HL),B; LD A,(HL)
    mem[0x0100] = 0x70; // LD (HL),B
    mem[0x0101] = 0x7E; // LD A,(HL)

    auto c1 = cpu.step();
    auto c2 = cpu.step();

    assert(mem[0xC000] == 0x42);
    assert(cpu.A == 0x42);
    // cycles: 8 + 8
    assert(c1 == 8 && c2 == 8);

    std::fprintf(stderr, "unit_tests: LD (HL),r and LD r,(HL) OK\n");

    // Test RRA: A=0xFE, C=1 -> A=0xFF, C=0
    {
        std::vector<uint8_t> mem2(0x10000, 0x00);
        Bus bus2{};
        bus2.read8 = [&](uint16_t addr) -> uint8_t { return mem2[addr]; };
        bus2.write8 = [&](uint16_t addr, uint8_t v) { mem2[addr] = v; };
        CPU cpu2(&bus2);
        cpu2.PC = 0x3000;
        cpu2.A = 0xFE;
        cpu2.setC(true);
        mem2[0x3000] = 0x1F; // RRA
        cpu2.step();
        assert(cpu2.A == 0xFF);
        assert(cpu2.getC() == false);
        assert(cpu2.getZ() == false);
        assert(cpu2.getN() == false);
        assert(cpu2.getH() == false);
        std::fprintf(stderr, "unit_tests: RRA case1 OK\n");
    }

    // Test RRA: A=0x03, C=0 -> A=0x01, C=1
    {
        std::vector<uint8_t> mem3(0x10000, 0x00);
        Bus bus3{};
        bus3.read8 = [&](uint16_t addr) -> uint8_t { return mem3[addr]; };
        bus3.write8 = [&](uint16_t addr, uint8_t v) { mem3[addr] = v; };
        CPU cpu3(&bus3);
        cpu3.PC = 0x3100;
        cpu3.A = 0x03;
        cpu3.setC(false);
        mem3[0x3100] = 0x1F; // RRA
        cpu3.step();
        assert(cpu3.A == 0x01);
        assert(cpu3.getC() == true);
        assert(cpu3.getZ() == false);
        assert(cpu3.getN() == false);
        assert(cpu3.getH() == false);
        std::fprintf(stderr, "unit_tests: RRA case2 OK\n");
    }

    // Test SUB borrow sets C and H, and JR NC does not jump when C=1
    {
        std::vector<uint8_t> rom(0x10000, 0x00);
        Bus bus2{};
        bus2.read8 = [&](uint16_t addr) -> uint8_t { return rom[addr]; };
        bus2.write8 = [&](uint16_t addr, uint8_t v) { rom[addr] = v; };

        CPU cpu2(&bus2);
        cpu2.PC = 0x2000;
        // Program:
        // LD A,0x04       (0x3E 0x04)
        // SUB 0x05        (0xD6 0x05) -> A=0xFF, C=1, H=1, N=1
        // JR NC, +2       (0x30 0x02) -> since C=1, should NOT jump
        // NOP             (0x00)
        // NOP             (0x00)
        rom[0x2000] = 0x3E; rom[0x2001] = 0x04;
        rom[0x2002] = 0xD6; rom[0x2003] = 0x05;
        rom[0x2004] = 0x30; rom[0x2005] = 0x02;
        rom[0x2006] = 0x00;
        rom[0x2007] = 0x00;

        // Execute LD A,0x04
        cpu2.step();
        assert(cpu2.A == 0x04);

        // Execute SUB 0x05
        cpu2.step();
        assert(cpu2.A == 0xFF);
        assert(cpu2.getC() == true);
        assert(cpu2.getH() == true);
        assert(cpu2.getN() == true);

        // Note PC should now be at 0x2004
        assert(cpu2.PC == 0x2004);

        // Execute JR NC, +2: since C=1, NC=false, must NOT jump
        cpu2.step();
        // When not jumping, PC advances by 2 only (opcode+imm)
        assert(cpu2.PC == 0x2006);

        std::fprintf(stderr, "unit_tests: SUB borrow flags and JR NC no-jump OK\n");
    }

    return 0;
}
