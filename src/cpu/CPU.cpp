#include "CPU.hpp"

namespace gb {

static inline uint8_t read8(Bus* bus, uint16_t addr) {
    if (bus && bus->read8) return bus->read8(addr);
    return 0x00; // default open bus placeholder
}

static inline void write8(Bus* bus, uint16_t addr, uint8_t val) {
    if (bus && bus->write8) bus->write8(addr, val);
}

static inline uint16_t read16(Bus* bus, uint16_t addr) {
    uint8_t lo = read8(bus, addr);
    uint8_t hi = read8(bus, addr + 1);
    return static_cast<uint16_t>(lo | (static_cast<uint16_t>(hi) << 8));
}

// Helpers for half-carry/carry calculations
static inline void set_flags_add8(CPU& cpu, uint8_t a, uint8_t b, uint8_t res, bool carry) {
    cpu.setZ(res == 0);
    cpu.setN(false);
    cpu.setH(((a & 0x0F) + (b & 0x0F)) > 0x0F);
    cpu.setC(carry);
}

static inline void set_flags_adc8(CPU& cpu, uint8_t a, uint8_t b, uint8_t res, uint8_t carry_in) {
    cpu.setZ(res == 0);
    cpu.setN(false);
    cpu.setH(((a & 0x0F) + (b & 0x0F) + carry_in) > 0x0F);
    cpu.setC(static_cast<uint16_t>(a) + static_cast<uint16_t>(b) + carry_in > 0xFF);
}

static inline void set_flags_sub8(CPU& cpu, uint8_t a, uint8_t b, uint8_t res, bool carry) {
    cpu.setZ(res == 0);
    cpu.setN(true);
    cpu.setH((a & 0x0F) < (b & 0x0F));
    cpu.setC(carry);
}

static inline void set_flags_sbc8(CPU& cpu, uint8_t a, uint8_t b, uint8_t res, uint8_t carry_in) {
    cpu.setZ(res == 0);
    cpu.setN(true);
    cpu.setH((a & 0x0F) < ((b & 0x0F) + carry_in));
    cpu.setC(static_cast<int>(a) - static_cast<int>(b) - static_cast<int>(carry_in) < 0);
}

static inline void set_flags_logical(CPU& cpu, uint8_t res, char kind) {
    cpu.setZ(res == 0);
    cpu.setN(false);
    cpu.setH(kind == 'A'); // AND sets H=1, OR/XOR set H=0
    cpu.setC(false);
}

// DAA per DMG rules
static inline void op_daa(CPU& cpu) {
    uint8_t a = cpu.A;
    uint8_t adj = 0;
    bool c = cpu.getC();
    if (!cpu.getN()) {
        if (cpu.getH() || (a & 0x0F) > 9) adj |= 0x06;
        if (c || a > 0x99) { adj |= 0x60; c = true; }
        a = static_cast<uint8_t>(a + adj);
    } else {
        if (cpu.getH()) adj |= 0x06;
        if (c) adj |= 0x60;
        a = static_cast<uint8_t>(a - adj);
    }
    cpu.A = a;
    cpu.setZ(cpu.A == 0);
    cpu.setH(false);
    cpu.setC(c);
}

// Register indexing helpers: 0:B 1:C 2:D 3:E 4:H 5:L 6:(HL) 7:A
static inline uint8_t read_r(CPU& cpu, int idx) {
    switch (idx) {
        case 0: return cpu.B;
        case 1: return cpu.C;
        case 2: return cpu.D;
        case 3: return cpu.E;
        case 4: return cpu.H;
        case 5: return cpu.L;
        case 6: return read8(cpu.bus(), cpu.getHL());
        case 7: return cpu.A;
    }
    return 0;
}

static inline void write_r(CPU& cpu, int idx, uint8_t v) {
    switch (idx) {
        case 0: cpu.B = v; break;
        case 1: cpu.C = v; break;
        case 2: cpu.D = v; break;
        case 3: cpu.E = v; break;
        case 4: cpu.H = v; break;
        case 5: cpu.L = v; break;
        case 6: write8(cpu.bus(), cpu.getHL(), v); break;
        case 7: cpu.A = v; break;
    }
}

static inline uint8_t fetch8(CPU& cpu) {
    uint8_t v = read8(cpu.bus(), cpu.PC);
    cpu.PC++;
    return v;
}

static inline int8_t fetch8s(CPU& cpu) {
    return static_cast<int8_t>(fetch8(cpu));
}

static inline uint16_t fetch16(CPU& cpu) {
    uint16_t v = read16(cpu.bus(), cpu.PC);
    cpu.PC += 2;
    return v;
}

uint32_t CPU::step() {
    // Honor an external stall if present (e.g., OAM DMA). Burn 4 cycles at a minimum to keep timers/PPU progressing
    if (bus_ && bus_->take_stall) {
        uint32_t stall = bus_->take_stall();
        if (stall) {
            tickTimers(stall);
            tickPPU(stall);
            return stall;
        }
    }
    // Optional register dump retained behind GB_CPU_LOGGING; default is off via tests' CMake
    #if GB_CPU_LOGGING
    std::fprintf(stderr, "PC=%04X A=%02X F=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X\n",
                 PC, A, F, B, C, D, E, H, L);
    #endif
    auto service_interrupt = [&]() -> uint32_t {
        // Determine highest priority pending interrupt
        uint8_t IF = bus() && bus()->read8 ? bus()->read8(0xFF0F) : 0;
        uint8_t IE = bus() && bus()->read8 ? bus()->read8(0xFFFF) : 0;
        uint8_t pending = static_cast<uint8_t>(IF & IE & 0x1F);
        if (!pending) return 0;
        uint16_t vector = 0;
        uint8_t bit = 0;
        if (pending & 0x01) { vector = 0x40; bit = 0x01; }
        else if (pending & 0x02) { vector = 0x48; bit = 0x02; }
        else if (pending & 0x04) { vector = 0x50; bit = 0x04; }
        else if (pending & 0x08) { vector = 0x58; bit = 0x08; }
        else if (pending & 0x10) { vector = 0x60; bit = 0x10; }
        else return 0;
        // Acknowledge: clear IF bit
        if (bus() && bus()->write8) {
            bus()->write8(0xFF0F, static_cast<uint8_t>(IF & static_cast<uint8_t>(~bit)));
        }
        // Push PC
        uint16_t ret = PC;
        SP = static_cast<uint16_t>(SP - 2);
        write8(bus(), SP, static_cast<uint8_t>(ret & 0xFF));
        write8(bus(), static_cast<uint16_t>(SP + 1), static_cast<uint8_t>(ret >> 8));
        // Jump and clear IME
        PC = vector;
        // Trace when entering a low-memory vector region or PC==0x0038
#if GB_CPU_LOGGING
        if (PC <= 0x0100 || PC == 0x0038) {
            uint8_t m0 = read8(bus(), PC);
            uint8_t m1 = read8(bus(), static_cast<uint16_t>(PC + 1));
            uint8_t m2 = read8(bus(), static_cast<uint16_t>(PC + 2));
            uint8_t m3 = read8(bus(), static_cast<uint16_t>(PC + 3));
            std::fprintf(stderr,
                         "TRACE: PC=%04X A:%02X F:%02X PCMEM:%02X,%02X,%02X,%02X (INT)\n",
                         PC, A, F, m0, m1, m2, m3);
        }
#endif
        IME = false;
        // Consume 20 cycles
        tickTimers(20);
        tickPPU(20);
        return 20;
    };

    // Handle HALT: consume cycles until IF has any bit set; unhalt regardless of IE/IME
    if (halted) {
        uint8_t IF_any = bus() && bus()->read8 ? bus()->read8(0xFF0F) : 0;
        if ((IF_any & 0x1F) == 0) {
            tickTimers(4);
            tickPPU(4);
            return 4;
        }
        // IF has a pending source: unhalt
        {
            uint8_t IE_mask = bus() && bus()->read8 ? bus()->read8(0xFFFF) : 0;
            bool any_enabled = (IF_any & IE_mask & 0x1F) != 0;
            (void)IE_mask; (void)IF_any; (void)any_enabled; // no debug logging in harness
        }
        halted = false;
        // If IME=1 and IE enables a pending IF, service immediately; otherwise continue to fetch
        uint8_t IE_mask = bus() && bus()->read8 ? bus()->read8(0xFFFF) : 0;
        if (IME && (IF_any & IE_mask & 0x1F)) {
            uint32_t cyc = service_interrupt();
            if (cyc) return cyc;
        }
    }
    // Normal boundary-based service when IME is enabled
    if (IME && pendingInterrupt()) {
        uint32_t cyc = service_interrupt();
        if (cyc) return cyc;
    }

    uint16_t pc0 = PC;
    // Keep a small rolling buffer of recent instructions for debugging (last 16)
    struct TraceEntry { uint16_t pc; uint8_t a,f,m0,m1,m2,m3; uint16_t sp; uint16_t spval; };
    static TraceEntry tracebuf[16];
    static int traceidx = 0;
    auto push_trace = [&](uint16_t pc_for_entry){
        TraceEntry& te = tracebuf[traceidx & 15];
        te.pc = pc_for_entry;
        te.a = A; te.f = F;
        te.m0 = read8(bus(), pc_for_entry);
        te.m1 = read8(bus(), static_cast<uint16_t>(pc_for_entry + 1));
        te.m2 = read8(bus(), static_cast<uint16_t>(pc_for_entry + 2));
        te.m3 = read8(bus(), static_cast<uint16_t>(pc_for_entry + 3));
        te.sp = SP;
        uint8_t slo = read8(bus(), SP);
        uint8_t shi = read8(bus(), static_cast<uint16_t>(SP + 1));
        te.spval = static_cast<uint16_t>(slo | (static_cast<uint16_t>(shi) << 8));
        ++traceidx;
    };
    // Record pre-instruction trace for game code region only (>=0x0100) to reduce noise
    if (pc0 >= 0x0100) push_trace(pc0);
    // Pre-trace for boot/interrupt region
#if GB_CPU_LOGGING
    if (pc0 <= 0x0100) {
        uint8_t m0 = read8(bus(), pc0);
        uint8_t m1 = read8(bus(), static_cast<uint16_t>(pc0 + 1));
        uint8_t m2 = read8(bus(), static_cast<uint16_t>(pc0 + 2));
        uint8_t m3 = read8(bus(), static_cast<uint16_t>(pc0 + 3));
        std::fprintf(stderr,
                     "TRACE: PC=%04X A:%02X F:%02X PCMEM:%02X,%02X,%02X,%02X\n",
                     pc0, A, F, m0, m1, m2, m3);
    }
#endif
    uint8_t op = fetch8(*this);

    // Default cycles for unknown opcodes: treat as NOP
    uint32_t cycles = 4;

    switch (op) {
        case 0x00: // NOP
            cycles = 4; break;

        case 0xCB: { // CB-prefixed instructions
            uint8_t cb = fetch8(*this);
            int r = cb & 7;           // register index
            uint8_t v = read_r(*this, r);
            auto write_back = [&](uint8_t val){ write_r(*this, r, val); };
            auto set_common = [&](){ setN(false); setH(false); };
            // Default cycles for CB groups
            auto reg_cycles = (r == 6) ? 16u : 8u; // (HL) is 16 for most, BIT on (HL) is 12

            if (cb <= 0x07) { // RLC r
                uint8_t c = (v >> 7) & 1;
                uint8_t res = static_cast<uint8_t>((v << 1) | c);
                setZ(res == 0); set_common(); setC(c);
                write_back(res); cycles = reg_cycles; break;
            }
            if (cb >= 0x08 && cb <= 0x0F) { // RRC r
                uint8_t c = v & 1;
                uint8_t res = static_cast<uint8_t>((v >> 1) | (c << 7));
                setZ(res == 0); set_common(); setC(c);
                write_back(res); cycles = reg_cycles; break;
            }
            if (cb >= 0x10 && cb <= 0x17) { // RL r
                uint8_t oldC = getC() ? 1 : 0;
                uint8_t c = (v >> 7) & 1;
                uint8_t res = static_cast<uint8_t>((v << 1) | oldC);
                setZ(res == 0); set_common(); setC(c);
                write_back(res); cycles = reg_cycles; break;
            }
            if (cb >= 0x18 && cb <= 0x1F) { // RR r
                uint8_t oldC = getC() ? 1 : 0;
                uint8_t c = v & 1;
                uint8_t res = static_cast<uint8_t>((v >> 1) | (oldC << 7));
                setZ(res == 0); set_common(); setC(c);
                write_back(res); cycles = reg_cycles; break;
            }
            if (cb >= 0x20 && cb <= 0x27) { // SLA r
                uint8_t c = (v >> 7) & 1;
                uint8_t res = static_cast<uint8_t>(v << 1);
                setZ(res == 0); set_common(); setC(c);
                write_back(res); cycles = reg_cycles; break;
            }
            if (cb >= 0x28 && cb <= 0x2F) { // SRA r (keep bit7)
                uint8_t c = v & 1;
                uint8_t msb = v & 0x80;
                uint8_t res = static_cast<uint8_t>((v >> 1) | msb);
                setZ(res == 0); set_common(); setC(c);
                write_back(res); cycles = reg_cycles; break;
            }
            if (cb >= 0x30 && cb <= 0x37) { // SWAP r
                uint8_t res = static_cast<uint8_t>(((v & 0xF) << 4) | ((v & 0xF0) >> 4));
                setZ(res == 0); setN(false); setH(false); setC(false);
                write_back(res); cycles = reg_cycles; break;
            }
            if (cb >= 0x38 && cb <= 0x3F) { // SRL r
                uint8_t c = v & 1;
                uint8_t res = static_cast<uint8_t>(v >> 1);
                setZ(res == 0); set_common(); setC(c);
                write_back(res); cycles = reg_cycles; break;
            }

            // BIT b,r (0x40-0x7F)
            if (cb >= 0x40 && cb <= 0x7F) {
                uint8_t b = (cb >> 3) & 7;
                bool bit = (v >> b) & 1;
                setZ(!bit); setN(false); setH(true); // C is preserved
                cycles = (r == 6) ? 12 : 8; // (HL) is 12 cycles for BIT
                break;
            }
            // RES b,r (0x80-0xBF)
            if (cb >= 0x80 && cb <= 0xBF) {
                uint8_t b = (cb >> 3) & 7;
                uint8_t res = static_cast<uint8_t>(v & static_cast<uint8_t>(~(1u << b)));
                write_back(res);
                cycles = reg_cycles; // (HL)=16, reg=8
                break;
            }
            // SET b,r (0xC0-0xFF)
            if (cb >= 0xC0 && cb <= 0xFF) {
                uint8_t b = (cb >> 3) & 7;
                uint8_t res = static_cast<uint8_t>(v | static_cast<uint8_t>(1u << b));
                write_back(res);
                cycles = reg_cycles; // (HL)=16, reg=8
                break;
            }
            cycles = 4; // fallback
            break;
        }

        // LD rr,imm16
        case 0x01: setBC(fetch16(*this)); cycles = 12; break;
        case 0x11: setDE(fetch16(*this)); cycles = 12; break;
        case 0x21: setHL(fetch16(*this)); cycles = 12; break;
        case 0x31: SP = fetch16(*this); cycles = 12; break;

        // INC rr / DEC rr
        case 0x03: setBC(getBC() + 1); cycles = 8; break;
        case 0x13: setDE(getDE() + 1); cycles = 8; break;
        case 0x23: setHL(getHL() + 1); cycles = 8; break;
        case 0x33: SP = static_cast<uint16_t>(SP + 1); cycles = 8; break;
        case 0x0B: setBC(getBC() - 1); cycles = 8; break;
        case 0x1B: setDE(getDE() - 1); cycles = 8; break;
        case 0x2B: setHL(getHL() - 1); cycles = 8; break;
        case 0x3B: SP = static_cast<uint16_t>(SP - 1); cycles = 8; break;

        // INC r / DEC r (incl (HL))
        case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x34: case 0x3C: {
            int r = (op >> 3) & 7;
            uint8_t v = read_r(*this, r);
            uint8_t res = static_cast<uint8_t>(v + 1);
            setZ(res == 0); setN(false); setH((v & 0x0F) == 0x0F);
            write_r(*this, r, res);
            cycles = (r == 6) ? 12 : 4;
            break;
        }
        case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x35: case 0x3D: {
            int r = (op >> 3) & 7;
            uint8_t v = read_r(*this, r);
            uint8_t res = static_cast<uint8_t>(v - 1);
            setZ(res == 0); setN(true); setH((v & 0x0F) == 0x00);
            write_r(*this, r, res);
            cycles = (r == 6) ? 12 : 4;
            break;
        }

        // LD r,r' (incl (HL))
        default: {
            if (op >= 0x40 && op <= 0x7F) {
                int dst = (op >> 3) & 7;
                int src = op & 7;
                uint8_t v = read_r(*this, src);
                write_r(*this, dst, v);
                cycles = (dst == 6 || src == 6) ? 8 : 4;
                break;
            }
            // LD r, n and LD (HL), n
            if (op == 0x06 || op == 0x0E || op == 0x16 || op == 0x1E || op == 0x26 || op == 0x2E || op == 0x3E) {
                uint8_t n = fetch8(*this);
                int dst = (op >> 3) & 7;
                write_r(*this, dst, n);
                cycles = 8; break;
            }
            if (op == 0x36) { uint8_t n = fetch8(*this); write_r(*this, 6, n); cycles = 12; break; }
            // LD (nn),A / LD A,(nn)
            if (op == 0xEA) { uint16_t nn = fetch16(*this); write8(bus(), nn, A); cycles = 16; break; }
            if (op == 0xFA) { uint16_t nn = fetch16(*this); A = read8(bus(), nn); cycles = 16; break; }

            // LD (HL+/-),A and LD A,(HL+/-)
            if (op == 0x22) { write8(bus(), getHL(), A); setHL(getHL() + 1); cycles = 8; break; }
            if (op == 0x32) { write8(bus(), getHL(), A); setHL(getHL() - 1); cycles = 8; break; }
            if (op == 0x2A) { A = read8(bus(), getHL()); setHL(getHL() + 1); cycles = 8; break; }
            if (op == 0x3A) { A = read8(bus(), getHL()); setHL(getHL() - 1); cycles = 8; break; }

            // LD A,(BC)/(DE) and LD (BC)/(DE),A
            if (op == 0x0A) { A = read8(bus(), getBC()); cycles = 8; break; }
            if (op == 0x1A) { A = read8(bus(), getDE()); cycles = 8; break; }
            if (op == 0x02) { write8(bus(), getBC(), A); cycles = 8; break; }
            if (op == 0x12) { write8(bus(), getDE(), A); cycles = 8; break; }

            // LDH (FF00+n),A and LDH A,(FF00+n)
            if (op == 0xE0) { uint8_t n = fetch8(*this); write8(bus(), static_cast<uint16_t>(0xFF00 | n), A); cycles = 12; break; }
            if (op == 0xF0) { uint8_t n = fetch8(*this); A = read8(bus(), static_cast<uint16_t>(0xFF00 | n)); cycles = 12; break; }
            if (op == 0xE2) { write8(bus(), static_cast<uint16_t>(0xFF00 | C), A); cycles = 8; break; }
            if (op == 0xF2) { A = read8(bus(), static_cast<uint16_t>(0xFF00 | C)); cycles = 8; break; }

            // 8-bit ALU A,r
            if (op >= 0x80 && op <= 0x87) { // ADD A,r
                int r = op & 7; uint8_t b = read_r(*this, r);
                uint16_t t = static_cast<uint16_t>(A) + b;
                uint8_t res = static_cast<uint8_t>(t);
                set_flags_add8(*this, A, b, res, t > 0xFF);
                A = res; cycles = (r == 6) ? 8 : 4; break;
            }
            if (op >= 0x88 && op <= 0x8F) { // ADC A,r
                int r = op & 7; uint8_t b = read_r(*this, r); uint8_t ci = getC() ? 1 : 0;
                uint16_t t = static_cast<uint16_t>(A) + b + ci;
                uint8_t res = static_cast<uint8_t>(t);
                set_flags_adc8(*this, A, b, res, ci);
                A = res; cycles = (r == 6) ? 8 : 4; break;
            }
            if (op >= 0x90 && op <= 0x97) { // SUB r
                int r = op & 7; uint8_t b = read_r(*this, r);
                int t = static_cast<int>(A) - static_cast<int>(b);
                uint8_t res = static_cast<uint8_t>(t);
                set_flags_sub8(*this, A, b, res, t < 0);
                A = res; cycles = (r == 6) ? 8 : 4; break;
            }
            if (op >= 0x98 && op <= 0x9F) { // SBC r
                int r = op & 7; uint8_t b = read_r(*this, r); uint8_t ci = getC() ? 1 : 0;
                int t = static_cast<int>(A) - static_cast<int>(b) - static_cast<int>(ci);
                uint8_t res = static_cast<uint8_t>(t);
                set_flags_sbc8(*this, A, b, res, ci);
                A = res; cycles = (r == 6) ? 8 : 4; break;
            }
            if (op >= 0xA0 && op <= 0xA7) { // AND r
                int r = op & 7; uint8_t res = static_cast<uint8_t>(A & read_r(*this, r));
                set_flags_logical(*this, res, 'A'); A = res; cycles = (r == 6) ? 8 : 4; break;
            }
            if (op >= 0xA8 && op <= 0xAF) { // XOR r
                int r = op & 7; uint8_t res = static_cast<uint8_t>(A ^ read_r(*this, r));
                set_flags_logical(*this, res, 'X'); A = res; cycles = (r == 6) ? 8 : 4; break;
            }
            if (op >= 0xB0 && op <= 0xB7) { // OR r
                int r = op & 7; uint8_t res = static_cast<uint8_t>(A | read_r(*this, r));
                set_flags_logical(*this, res, 'O'); A = res; cycles = (r == 6) ? 8 : 4; break;
            }
            if (op >= 0xB8 && op <= 0xBF) { // CP r
                int r = op & 7; uint8_t b = read_r(*this, r);
                int t = static_cast<int>(A) - static_cast<int>(b);
                uint8_t res = static_cast<uint8_t>(t);
                set_flags_sub8(*this, A, b, res, t < 0); cycles = (r == 6) ? 8 : 4; break;
            }

            // Immediate ALU
            if (op == 0xC6) { uint8_t n = fetch8(*this); uint16_t t = static_cast<uint16_t>(A) + n; uint8_t r = static_cast<uint8_t>(t);
                set_flags_add8(*this, A, n, r, t > 0xFF); A = r; cycles = 8; break; }
            if (op == 0xCE) { uint8_t n = fetch8(*this); uint8_t ci = getC() ? 1 : 0; uint16_t t = static_cast<uint16_t>(A) + n + ci; uint8_t r = static_cast<uint8_t>(t);
                set_flags_adc8(*this, A, n, r, ci); A = r; cycles = 8; break; }
            if (op == 0xD6) { uint8_t n = fetch8(*this); int t = static_cast<int>(A) - static_cast<int>(n); uint8_t r = static_cast<uint8_t>(t);
                set_flags_sub8(*this, A, n, r, t < 0); A = r; cycles = 8; break; }
            if (op == 0xDE) { uint8_t n = fetch8(*this); uint8_t ci = getC() ? 1 : 0; int t = static_cast<int>(A) - static_cast<int>(n) - static_cast<int>(ci); uint8_t r = static_cast<uint8_t>(t);
                set_flags_sbc8(*this, A, n, r, ci); A = r; cycles = 8; break; }
            if (op == 0xE6) { uint8_t n = fetch8(*this); uint8_t r = static_cast<uint8_t>(A & n); set_flags_logical(*this, r, 'A'); A = r; cycles = 8; break; }
            if (op == 0xEE) { uint8_t n = fetch8(*this); uint8_t r = static_cast<uint8_t>(A ^ n); set_flags_logical(*this, r, 'X'); A = r; cycles = 8; break; }
            if (op == 0xF6) { uint8_t n = fetch8(*this); uint8_t r = static_cast<uint8_t>(A | n); set_flags_logical(*this, r, 'O'); A = r; cycles = 8; break; }
            if (op == 0xFE) { uint8_t n = fetch8(*this); int t = static_cast<int>(A) - static_cast<int>(n); uint8_t r = static_cast<uint8_t>(t); set_flags_sub8(*this, A, n, r, t < 0); cycles = 8; break; }

            // ADD HL,rr
            if (op == 0x09 || op == 0x19 || op == 0x29 || op == 0x39) {
                uint16_t src = (op == 0x09) ? getBC() : (op == 0x19) ? getDE() : (op == 0x29) ? getHL() : SP;
                uint32_t t = static_cast<uint32_t>(getHL()) + src;
                setN(false);
                setH(((getHL() & 0x0FFF) + (src & 0x0FFF)) > 0x0FFF);
                setC(t > 0xFFFF);
                setHL(static_cast<uint16_t>(t));
                cycles = 8; break;
            }

            // ADD SP, e8
            if (op == 0xE8) {
                int8_t e = fetch8s(*this);
                uint16_t sp = SP;
                uint16_t res = static_cast<uint16_t>(sp + e);
                setZ(false); setN(false);
                setH(((sp & 0x0F) + (e & 0x0F)) > 0x0F);
                setC(((sp & 0xFF) + (static_cast<uint8_t>(e))) > 0xFF);
                SP = res; cycles = 16; break;
            }

            // LDHL SP+e8
            if (op == 0xF8) {
                int8_t e = fetch8s(*this);
                uint16_t sp = SP; uint16_t res = static_cast<uint16_t>(sp + e);
                setZ(false); setN(false);
                setH(((sp & 0x0F) + (e & 0x0F)) > 0x0F);
                setC(((sp & 0xFF) + (static_cast<uint8_t>(e))) > 0xFF);
                setHL(res); cycles = 12; break;
            }

            // LD (nn),SP and LD SP,HL
            if (op == 0x08) { uint16_t nn = fetch16(*this); write8(bus(), nn, static_cast<uint8_t>(SP & 0xFF)); write8(bus(), static_cast<uint16_t>(nn + 1), static_cast<uint8_t>(SP >> 8)); cycles = 20; break; }
            if (op == 0xF9) { SP = getHL(); cycles = 8; break; }

            // JP nn / JR e and conditionals
            if (op == 0xC3) { uint16_t nn = fetch16(*this); PC = nn; cycles = 16; break; }
            if (op == 0xE9) { PC = getHL(); cycles = 4; break; } // JP (HL)
            if (op == 0x18) { int8_t e = fetch8s(*this); PC = static_cast<uint16_t>(PC + e); cycles = 12; break; }
            if (op == 0xC2 || op == 0xCA || op == 0xD2 || op == 0xDA) {
                uint16_t nn = fetch16(*this);
                bool cond = (op == 0xC2) ? !getZ() : (op == 0xCA) ? getZ() : (op == 0xD2) ? !getC() : getC();
                if (cond) { PC = nn; cycles = 16; } else { cycles = 12; }
                break;
            }
            if (op == 0x20 || op == 0x28 || op == 0x30 || op == 0x38) {
                int8_t e = fetch8s(*this);
                bool cond = (op == 0x20) ? !getZ() : (op == 0x28) ? getZ() : (op == 0x30) ? !getC() : getC();
                if (cond) { PC = static_cast<uint16_t>(PC + e); cycles = 12; } else { cycles = 8; }
                break;
            }

            // CALL/RET and conditionals
            if (op == 0xCD) { uint16_t nn = fetch16(*this); uint16_t ret = PC; SP -= 2; write8(bus(), SP, static_cast<uint8_t>(ret & 0xFF)); write8(bus(), static_cast<uint16_t>(SP + 1), static_cast<uint8_t>(ret >> 8)); PC = nn; cycles = 24; break; }
            if (op == 0xC9) { // RET
                uint8_t lo = read8(bus(), SP);
                uint8_t hi = read8(bus(), static_cast<uint16_t>(SP + 1));
                SP += 2;
                PC = static_cast<uint16_t>(lo | (hi << 8));
                cycles = 16; break;
            }
            if (op == 0xD9) { // RETI
                uint8_t lo = read8(bus(), SP);
                uint8_t hi = read8(bus(), static_cast<uint16_t>(SP + 1));
                SP += 2;
                PC = static_cast<uint16_t>(lo | (hi << 8));
                IME = true; // Enable interrupts after returning
                cycles = 16; break;
            }
            if (op == 0xC0 || op == 0xC8 || op == 0xD0 || op == 0xD8) {
                bool cond = (op == 0xC0) ? !getZ() : (op == 0xC8) ? getZ() : (op == 0xD0) ? !getC() : getC();
                if (cond) { uint8_t lo = read8(bus(), SP); uint8_t hi = read8(bus(), static_cast<uint16_t>(SP + 1)); SP += 2; PC = static_cast<uint16_t>(lo | (hi << 8)); cycles = 20; } else { cycles = 8; }
                break;
            }
            if (op == 0xC4 || op == 0xCC || op == 0xD4 || op == 0xDC) {
                uint16_t nn = fetch16(*this);
                bool cond = (op == 0xC4) ? !getZ() : (op == 0xCC) ? getZ() : (op == 0xD4) ? !getC() : getC();
                if (cond) { uint16_t ret = PC; SP -= 2; write8(bus(), SP, static_cast<uint8_t>(ret & 0xFF)); write8(bus(), static_cast<uint16_t>(SP + 1), static_cast<uint8_t>(ret >> 8)); PC = nn; cycles = 24; } else { cycles = 12; }
                break;
            }

            // RST t
            if ((op & 0xC7) == 0xC7) { uint16_t vec = static_cast<uint16_t>(op & 0x38); uint16_t ret = PC; SP -= 2; write8(bus(), SP, static_cast<uint8_t>(ret & 0xFF)); write8(bus(), static_cast<uint16_t>(SP + 1), static_cast<uint8_t>(ret >> 8)); PC = vec; cycles = 16; break; }

            // DAA, CPL, SCF, CCF
            if (op == 0x27) { op_daa(*this); cycles = 4; break; }
            if (op == 0x2F) { A ^= 0xFF; setN(true); setH(true); cycles = 4; break; }
            if (op == 0x37) { setN(false); setH(false); setC(true); cycles = 4; break; }
            if (op == 0x3F) { setN(false); setH(false); setC(!getC()); cycles = 4; break; }

            // RLCA, RRCA, RLA, RRA (non-CB rotates on A)
            if (op == 0x07) { // RLCA
                uint8_t c = (A & 0x80) ? 1 : 0;
                A = static_cast<uint8_t>((A << 1) | c);
                setZ(false); setN(false); setH(false); setC(c);
                cycles = 4; break;
            }
            if (op == 0x0F) { // RRCA
                uint8_t c = (A & 0x01);
                A = static_cast<uint8_t>((A >> 1) | (c << 7));
                setZ(false); setN(false); setH(false); setC(c);
                cycles = 4; break;
            }
            if (op == 0x17) { // RLA
                uint8_t oldC = getC() ? 1 : 0;
                uint8_t c = (A & 0x80) ? 1 : 0;
                A = static_cast<uint8_t>((A << 1) | oldC);
                setZ(false); setN(false); setH(false); setC(c);
                cycles = 4; break;
            }
            if (op == 0x1F) { // RRA
                uint8_t oldC = getC() ? 1 : 0;
                uint8_t c = (A & 0x01);
                A = static_cast<uint8_t>((A >> 1) | (oldC << 7));
                setZ(false); setN(false); setH(false); setC(c);
                cycles = 4; break;
            }

            // PUSH/POP rr (AF,BC,DE,HL)
            if (op == 0xC5 || op == 0xD5 || op == 0xE5 || op == 0xF5) {
                uint16_t rr = (op == 0xC5) ? getBC() : (op == 0xD5) ? getDE() : (op == 0xE5) ? getHL() : getAF();
                SP -= 2; write8(bus(), SP, static_cast<uint8_t>(rr & 0xFF)); write8(bus(), static_cast<uint16_t>(SP + 1), static_cast<uint8_t>(rr >> 8)); cycles = 16; break;
            }
            if (op == 0xC1 || op == 0xD1 || op == 0xE1 || op == 0xF1) {
                uint8_t lo = read8(bus(), SP); uint8_t hi = read8(bus(), static_cast<uint16_t>(SP + 1)); SP += 2; uint16_t rr = static_cast<uint16_t>(lo | (hi << 8));
                if (op == 0xC1) setBC(rr); else if (op == 0xD1) setDE(rr); else if (op == 0xE1) setHL(rr); else setAF(rr);
                cycles = 12; break;
            }

            // EI/DI
            if (op == 0xFB) { // EI
                ime_enable_after_step_ = true; cycles = 4; break;
            }
            if (op == 0xF3) { // DI
                IME = false; cycles = 4; break;
            }

            // HALT minimal
            if (op == 0x76) { // HALT
                // Enter HALT; suppress debug logging in test harness
                if (!IME || !pendingInterrupt()) { halted = true; }
                cycles = 4; break;
            }

            // Fallback: treat unknown as NOP to keep progress
            cycles = 4; break;
        }
    }
    // Post-step IME delayed enable
    if (ime_enable_after_step_) { IME = true; ime_enable_after_step_ = false; }
    tickTimers(cycles);
    tickPPU(cycles);
    // Post-trace when PC changes to 0x0038
#if GB_CPU_LOGGING
    if (PC == 0x0038 && pc0 != PC) {
        // Dump last 10 entries prior to entering 0x0038
        std::fprintf(stderr, "===== LAST 10 BEFORE 0038 =====\n");
        int count = 10;
        for (int i = count; i > 0; --i) {
            int idx = (traceidx - i) & 15;
            const TraceEntry& te = tracebuf[idx];
            std::fprintf(stderr,
                         "TRACE: PC=%04X A:%02X F:%02X PCMEM:%02X,%02X,%02X,%02X SP:%04X [SP]=%04X\n",
                         te.pc, te.a, te.f, te.m0, te.m1, te.m2, te.m3, te.sp, te.spval);
        }
        uint8_t m0 = read8(bus(), PC);
        uint8_t m1 = read8(bus(), static_cast<uint16_t>(PC + 1));
        uint8_t m2 = read8(bus(), static_cast<uint16_t>(PC + 2));
        uint8_t m3 = read8(bus(), static_cast<uint16_t>(PC + 3));
        std::fprintf(stderr,
                     "TRACE: PC=%04X A:%02X F:%02X PCMEM:%02X,%02X,%02X,%02X\n",
                     PC, A, F, m0, m1, m2, m3);
    }
#endif
    return cycles;
}

} // namespace gb
