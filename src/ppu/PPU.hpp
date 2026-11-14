#ifndef GB_PPU_PPU_HPP
#define GB_PPU_PPU_HPP

#include <cstdint>
#include <array>

namespace gb {

struct Bus; // forward decl

// Minimal DMG PPU timing and interrupt unit (no pixel rendering yet)
class PPU {
public:
    explicit PPU(Bus* bus);

    // Advance PPU by CPU cycles
    void tick(uint32_t cycles);

    // Memory-mapped register access (FF40-FF4B)
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t value);

    // BG framebuffer access (ARGB8888). Returns pointer to 160x144 pixels.
    const uint32_t* frameBuffer() const { return framebuffer_.data(); }
    // True when a new frame just started (entered VBlank); cleared by consumer
    bool frameReady() const { return frame_ready_; }
    void clearFrameReady() { frame_ready_ = false; }

private:
    Bus* bus_{};

    // Registers
    uint8_t lcdc_{0};   // FF40
    uint8_t stat_{0};   // FF41
    uint8_t scy_{0};    // FF42
    uint8_t scx_{0};    // FF43
    uint8_t ly_{0};     // FF44 (read-only)
    uint8_t lyc_{0};    // FF45
    uint8_t bgp_{0xFC}; // FF47 (DMG boot default)
    uint8_t obp0_{0};   // FF48
    uint8_t obp1_{0};   // FF49
    uint8_t wy_{0};     // FF4A
    uint8_t wx_{0};     // FF4B

    // Timing
    uint32_t line_cycle_{0};
    // Mode: 0=HBlank,1=VBlank,2=OAM,3=Transfer
    uint8_t mode_{2};
    bool rendered_line_{false}; // whether current LY line has been rendered in Mode 3

    // Simple BG framebuffer
    std::array<uint32_t, 160 * 144> framebuffer_{};
    // Keep BG color ids (0..3) per pixel for sprite priority evaluation
    std::array<uint8_t, 160 * 144> bg_color_{};
    bool frame_ready_{false};

    // Helpers
    void setMode(uint8_t m);
    void updateCoincidence();
    void requestSTAT();
    void requestVBlank();

    // Rendering helpers
    void renderBGLine(uint8_t line);
    inline uint32_t mapBgpShade(uint8_t color_id) const;
    void renderOBJLine(uint8_t line);
    inline uint32_t mapObjpShade(uint8_t palette_bits, uint8_t color_id) const;
};

} // namespace gb

#endif // GB_PPU_PPU_HPP
