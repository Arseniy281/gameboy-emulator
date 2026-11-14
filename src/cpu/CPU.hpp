#ifndef GB_CPU_CPU_HPP
#define GB_CPU_CPU_HPP

#include <cstdint>
#include <cstdio>
#include <functional>

namespace gb {

struct Bus {
    // Simple bus callbacks; can be wired by the emulator later
    std::function<void(uint16_t, uint8_t)> write8;
    std::function<uint8_t(uint16_t)> read8;
    // Optional PPU hook
    void* ppu{nullptr};
    std::function<void(uint32_t)> ppu_tick; // if set, CPU delegates PPU ticking
    // Optional stall provider (e.g., OAM DMA). Returns cycles to burn this step; CPU will tick timers/PPU and return
    std::function<uint32_t()> take_stall;
};

class CPU {
public:
    // 8-bit registers
    uint8_t A{0}, F{0}, B{0}, C{0}, D{0}, E{0}, H{0}, L{0};
    // 16-bit special registers
    uint16_t SP{0}, PC{0};

    // Control
    bool IME{false};
    bool halted{false};

    explicit CPU(Bus* bus = nullptr) : bus_(bus) {}

    void connectBus(Bus* bus) { bus_ = bus; }

    // One instruction step; returns cycles consumed
    uint32_t step();

    // IF/IE accessors will be wired via bus; placeholders for interface completeness
    void setIE(uint8_t v) { (void)v; }
    uint8_t getIE() const { return 0; }
    void setIF(uint8_t v) { (void)v; }
    uint8_t getIF() const { return 0; }

    // Logging toggle is controlled via compile definition GB_CPU_LOGGING

    // Internal bus accessor for helpers
    Bus* bus() const { return bus_; }

    // Register pair helpers (note: low 4 bits of F are always 0 on hardware)
    uint16_t getAF() const { return static_cast<uint16_t>((A << 8) | (F & 0xF0)); }
    void setAF(uint16_t v) {
        A = static_cast<uint8_t>(v >> 8);
        F = static_cast<uint8_t>(v & 0xF0); // mask off undefined low flag bits
    }

    uint16_t getBC() const { return static_cast<uint16_t>((B << 8) | C); }
    void setBC(uint16_t v) {
        B = static_cast<uint8_t>(v >> 8);
        C = static_cast<uint8_t>(v);
    }

    uint16_t getDE() const { return static_cast<uint16_t>((D << 8) | E); }
    void setDE(uint16_t v) {
        D = static_cast<uint8_t>(v >> 8);
        E = static_cast<uint8_t>(v);
    }

    uint16_t getHL() const { return static_cast<uint16_t>((H << 8) | L); }
    void setHL(uint16_t v) {
        H = static_cast<uint8_t>(v >> 8);
        L = static_cast<uint8_t>(v);
    }

    // Flag utilities: Z N H C are bits 7..4 in F
    static constexpr uint8_t FLAG_Z = 0x80;
    static constexpr uint8_t FLAG_N = 0x40;
    static constexpr uint8_t FLAG_H = 0x20;
    static constexpr uint8_t FLAG_C = 0x10;

    bool getZ() const { return (F & FLAG_Z) != 0; }
    bool getN() const { return (F & FLAG_N) != 0; }
    bool getH() const { return (F & FLAG_H) != 0; }
    bool getC() const { return (F & FLAG_C) != 0; }

    void setZ(bool v) { if (v) F |= FLAG_Z; else F &= static_cast<uint8_t>(~FLAG_Z); }
    void setN(bool v) { if (v) F |= FLAG_N; else F &= static_cast<uint8_t>(~FLAG_N); }
    void setH(bool v) { if (v) F |= FLAG_H; else F &= static_cast<uint8_t>(~FLAG_H); }
    void setC(bool v) { if (v) F |= FLAG_C; else F &= static_cast<uint8_t>(~FLAG_C); }

private:
    Bus* bus_{};
    bool ime_enable_after_step_{false};
    // Internal 16-bit divider counter (increments every CPU cycle)
    uint16_t div_counter_{0};
    // Last sampled bit value for falling-edge detection
    bool last_timer_bit_{false};
    // Pending TIMA reload delay (in CPU cycles). When >0, after it elapses, TIMA := TMA once.
    uint32_t tima_reload_delay_{0};
    bool tima_reload_pending_{false};
    uint8_t last_tac_{0};
    // PPU/LY tracking
    uint32_t ly_cycle_accum_{0};
    uint8_t ly_{0};
    // Debug: tickTimers call counter
    uint32_t timer_debug_counter_{0};
    // Debug: PPU tick counter
    uint32_t ppu_debug_counter_{0};

    // Helpers
    inline bool pendingInterrupt() const {
        // IF=$FF0F, IE=$FFFF; only lower 5 bits valid
        uint8_t IF = bus_ && bus_->read8 ? bus_->read8(0xFF0F) : 0;
        uint8_t IE = bus_ && bus_->read8 ? bus_->read8(0xFFFF) : 0;
        return (IF & IE & 0x1F) != 0;
    }
    inline void tickTimers(uint32_t cycles) {
        if (!(bus_ && bus_->read8 && bus_->write8)) return;
        // Helper to apply TIMA increment/overflow with delayed reload of TMA
        auto inc_tima = [&](bool timer_enabled) {
            if (!timer_enabled) return; // If timer disabled, do not increment or signal IF
            if (tima_reload_pending_) return; // During pending reload, ignore further increments
            uint8_t tima = bus_->read8(0xFF05);
            if (tima == 0xFF) {
                bus_->write8(0xFF05, 0x00);
                // Request Timer interrupt immediately
                uint8_t IF = bus_->read8(0xFF0F);
                bus_->write8(0xFF0F, static_cast<uint8_t>(IF | 0x04));
                // Schedule reload of TMA after 1 M-cycle (~4 CPU cycles)
                tima_reload_pending_ = true;
                tima_reload_delay_ = 4;
            } else {
                bus_->write8(0xFF05, static_cast<uint8_t>(tima + 1));
            }
        };

        // Check for external writes to DIV: if FF04 != (div_counter_>>8), resync divider
        uint8_t div_mem = bus_->read8(0xFF04);
        if (div_mem != static_cast<uint8_t>(div_counter_ >> 8)) {
            // Resync internal divider to match external high byte; clear edge to avoid false increment
            div_counter_ = static_cast<uint16_t>(div_mem) << 8;
        }

        // Determine selected bit based on TAC, and resync last bit on TAC changes
        uint8_t tac = bus_->read8(0xFF07);
        if (tac != last_tac_) {
            static const uint8_t bit_index_lut_resync[4] = {7, 1, 3, 5};
            uint8_t resync_bit = bit_index_lut_resync[tac & 0x03];
            last_timer_bit_ = ((div_counter_ >> resync_bit) & 1u) != 0;
            last_tac_ = tac;
        }
        bool enabled = (tac & 0x04) != 0;
        static const uint8_t bit_index_lut[4] = {7, 1, 3, 5};
        uint8_t bit_index = bit_index_lut[tac & 0x03];

        // Remove noisy periodic debug logs

        for (uint32_t i = 0; i < cycles; ++i) {
            // Before increment, sample DIV high byte and selected bit
            uint8_t prev_div = static_cast<uint8_t>(div_counter_ >> 8);
            bool prev_bit = ((div_counter_ >> bit_index) & 1u) != 0;

            // Increment internal divider counter by 1 cycle
            div_counter_ = static_cast<uint16_t>(div_counter_ + 1);

            // Update DIV register to reflect high byte of divider
            uint8_t new_div = static_cast<uint8_t>(div_counter_ >> 8);
            if (new_div != prev_div) {
                bus_->write8(0xFF04, new_div);
            }

            // Timer falling-edge detection
            if (enabled) {
                bool curr_bit = ((div_counter_ >> bit_index) & 1u) != 0;
                // Falling edge: 1 -> 0
                if (prev_bit && !curr_bit) {
                    inc_tima(true);
                }
                last_timer_bit_ = curr_bit;
            }
            // Handle delayed reload timeline
            if (tima_reload_pending_) {
                if (tima_reload_delay_ > 0) {
                    --tima_reload_delay_;
                }
                if (tima_reload_delay_ == 0) {
                    uint8_t tma = bus_->read8(0xFF06);
                    bus_->write8(0xFF05, tma);
                    tima_reload_pending_ = false;
                }
            }
        }
    }

    inline void tickPPU(uint32_t cycles) {
        // Delegate to external PPU if present; otherwise no-op to avoid spurious interrupts in CPU-only tests
        if (bus_ && bus_->ppu_tick) { bus_->ppu_tick(cycles); }
    }
};

} // namespace gb

#endif // GB_CPU_CPU_HPP
