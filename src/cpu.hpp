#pragma once
#include <cstdint>
#include <iostream>

#include "memory.hpp"

struct CPU {
    // Accès à la mémoire (branché depuis main)
    Memory* mem = nullptr;

    // Registres 8 bits (valeurs post-boot DMG)
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

    // Flags
    static constexpr uint8_t FLAG_Z = 1 << 7;
    static constexpr uint8_t FLAG_N = 1 << 6;
    static constexpr uint8_t FLAG_H = 1 << 5;
    static constexpr uint8_t FLAG_C = 1 << 4;

    // Paires 16 bits
    uint16_t af() const { return (a << 8) | f; }
    uint16_t bc() const { return (b << 8) | c; }
    uint16_t de() const { return (d << 8) | e; }
    uint16_t hl() const { return (h << 8) | l; }

    void set_af(uint16_t v) { a = v >> 8; f = v & 0xF0; }
    void set_bc(uint16_t v) { b = v >> 8; c = v & 0xFF; }
    void set_de(uint16_t v) { d = v >> 8; e = v & 0xFF; }
    void set_hl(uint16_t v) { h = v >> 8; l = v & 0xFF; }

    // Flags
    bool get_flag(uint8_t mask) const { return (f & mask) != 0; }
    void set_flag(uint8_t mask, bool on) {
        if (on) f |= mask;
        else    f &= ~mask;
    }

    // Lit l'octet à PC, puis avance PC
    uint8_t fetch8() {
        uint8_t value = mem->read(pc);
        pc++;
        return value;
    }

    // Lit 2 octets : le bas d'abord, puis le haut (little-endian)
    uint16_t fetch16() {
        uint8_t lo = fetch8();
        uint8_t hi = fetch8();
        return (hi << 8) | lo;
    }

    // Exécute une instruction
    void step() {
        uint8_t opcode = fetch8();
        switch (opcode) {
            case 0x00:  // NOP : ne rien faire
                break;
            case 0xC3:  // JP a16 : sauter à l'adresse donnée
                pc = fetch16();
                break;
            default:
                std::cerr << "Opcode inconnu : 0x" << std::hex
                          << static_cast<int>(opcode) << std::dec << "\n";
                break;
        }
    }
};