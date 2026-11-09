#pragma once
#include <cstdint>

struct CPU {
    // Registres 8 bits, avec leur valeur après le boot ROM (DMG)
    uint8_t a = 0x01;
    uint8_t f = 0xB0;
    uint8_t b = 0x00;
    uint8_t c = 0x13;
    uint8_t d = 0x00;
    uint8_t e = 0xD8;
    uint8_t h = 0x01;
    uint8_t l = 0x4D;

    // Registres 16 bits
    uint16_t sp = 0xFFFE;
    uint16_t pc = 0x0100;

    // Flags : position de chaque bit dans F
    static constexpr uint8_t FLAG_Z = 1 << 7;
    static constexpr uint8_t FLAG_N = 1 << 6;
    static constexpr uint8_t FLAG_H = 1 << 5;
    static constexpr uint8_t FLAG_C = 1 << 4;

    // Paires 16 bits : calculées à partir des registres 8 bits
    uint16_t af() const { return (a << 8) | f; }
    uint16_t bc() const { return (b << 8) | c; }
    uint16_t de() const { return (d << 8) | e; }
    uint16_t hl() const { return (h << 8) | l; }

    void set_af(uint16_t v) { a = v >> 8; f = v & 0xF0; }  // 0xF0 : les 4 bits du bas de F restent à 0
    void set_bc(uint16_t v) { b = v >> 8; c = v & 0xFF; }
    void set_de(uint16_t v) { d = v >> 8; e = v & 0xFF; }
    void set_hl(uint16_t v) { h = v >> 8; l = v & 0xFF; }

    // Lire / écrire un flag
    bool get_flag(uint8_t mask) const { return (f & mask) != 0; }
    void set_flag(uint8_t mask, bool on) {
        if (on) f |= mask;
        else    f &= ~mask;
    }
};