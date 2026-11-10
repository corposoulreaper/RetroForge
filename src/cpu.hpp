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

    // Interruptions : IME + délai d'activation de EI (effet après l'instruction suivante)
    bool ime = false;
    bool ime_pending = false;

    // HALT : le CPU dort jusqu'à ce qu'une interruption soit demandée (IF & IE)
    bool halted = false;
    bool halt_bug = false;  // HALT avec IME=0 et interruption déjà en attente : l'octet suivant est lu 2 fois

    // M-cycles de l'instruction en cours (1 M-cycle = 4 T-cycles)
    int cycles_m = 0;

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

    // Registre 8 bits par numéro : 0=B 1=C 2=D 3=E 4=H 5=L 6=(HL) 7=A
    uint8_t get_r8(int i) {
        switch (i) {
            case 0: return b;
            case 1: return c;
            case 2: return d;
            case 3: return e;
            case 4: return h;
            case 5: return l;
            case 6: return mem->read(hl());
            default: return a;  // 7
        }
    }

    void set_r8(int i, uint8_t v) {
        switch (i) {
            case 0: b = v; break;
            case 1: c = v; break;
            case 2: d = v; break;
            case 3: e = v; break;
            case 4: h = v; break;
            case 5: l = v; break;
            case 6: mem->write(hl(), v); break;
            default: a = v; break;  // 7
        }
    }

    // INC r8 : Z selon le résultat, N=0, H si retenue du bit 3, C inchangé
    void inc8(uint8_t& r) {
        uint8_t old = r;
        r = old + 1;
        set_flag(FLAG_Z, r == 0);
        set_flag(FLAG_N, false);
        set_flag(FLAG_H, (old & 0x0F) == 0x0F);
    }

    // DEC r8 : Z selon le résultat, N=1, H si emprunt du bit 4, C inchangé
    void dec8(uint8_t& r) {
        uint8_t old = r;
        r = old - 1;
        set_flag(FLAG_Z, r == 0);
        set_flag(FLAG_N, true);
        set_flag(FLAG_H, (old & 0x0F) == 0x00);
    }

    // ALU sur A : op 0=ADD 1=ADC 2=SUB 3=SBC 4=AND 5=XOR 6=OR 7=CP
    void alu(int op, uint8_t v) {
        int cy = get_flag(FLAG_C) ? 1 : 0;
        switch (op) {
            case 0:
            case 1: {  // ADD / ADC
                int carry = (op == 1) ? cy : 0;
                int r = a + v + carry;
                set_flag(FLAG_Z, (r & 0xFF) == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, ((a & 0x0F) + (v & 0x0F) + carry) > 0x0F);
                set_flag(FLAG_C, r > 0xFF);
                a = r & 0xFF;
                break;
            }
            case 2:
            case 3:
            case 7: {  // SUB / SBC / CP
                int carry = (op == 3) ? cy : 0;
                int r = a - v - carry;
                set_flag(FLAG_Z, (r & 0xFF) == 0);
                set_flag(FLAG_N, true);
                set_flag(FLAG_H, (a & 0x0F) < ((v & 0x0F) + carry));
                set_flag(FLAG_C, r < 0);
                if (op != 7) a = r & 0xFF;  // CP ne modifie pas A
                break;
            }
            case 4:  // AND
                a &= v;
                set_flag(FLAG_Z, a == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, true);
                set_flag(FLAG_C, false);
                break;
            case 5:  // XOR
                a ^= v;
                set_flag(FLAG_Z, a == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, false);
                set_flag(FLAG_C, false);
                break;
            case 6:  // OR
                a |= v;
                set_flag(FLAG_Z, a == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, false);
                set_flag(FLAG_C, false);
                break;
        }
    }

    // Saut relatif : l'offset est signé, ajouté à PC (déjà après l'instruction)
    // Saut pris : +1 M-cycle
    void jr(bool condition) {
        int8_t offset = static_cast<int8_t>(fetch8());  // toujours lu
        if (condition) {
            pc += offset;
            cycles_m += 1;
        }
    }

    // Condition : 0=NZ 1=Z 2=NC 3=C
    bool cond(int cc) const {
        switch (cc) {
            case 0: return !get_flag(FLAG_Z);
            case 1: return get_flag(FLAG_Z);
            case 2: return !get_flag(FLAG_C);
            default: return get_flag(FLAG_C);
        }
    }

    // ADD HL, r16 : Z inchangé, N=0, H = retenue du bit 11, C = retenue du bit 15
    void add_hl(uint16_t v) {
        uint32_t r = static_cast<uint32_t>(hl()) + v;
        set_flag(FLAG_N, false);
        set_flag(FLAG_H, ((hl() & 0x0FFF) + (v & 0x0FFF)) > 0x0FFF);
        set_flag(FLAG_C, r > 0xFFFF);
        set_hl(r & 0xFFFF);
    }

    // Rotations de A : 0=RLCA 1=RRCA 2=RLA 3=RRA (Z=0, N=0, H=0, C = bit sorti)
    void rotate_a(int kind) {
        bool old_c = get_flag(FLAG_C);
        bool carry = false;
        switch (kind) {
            case 0:  // RLCA
                carry = (a & 0x80) != 0;
                a = static_cast<uint8_t>((a << 1) | (carry ? 1 : 0));
                break;
            case 1:  // RRCA
                carry = (a & 0x01) != 0;
                a = static_cast<uint8_t>((a >> 1) | (carry ? 0x80 : 0));
                break;
            case 2:  // RLA
                carry = (a & 0x80) != 0;
                a = static_cast<uint8_t>((a << 1) | (old_c ? 1 : 0));
                break;
            default:  // RRA
                carry = (a & 0x01) != 0;
                a = static_cast<uint8_t>((a >> 1) | (old_c ? 0x80 : 0));
                break;
        }
        set_flag(FLAG_Z, false);
        set_flag(FLAG_N, false);
        set_flag(FLAG_H, false);
        set_flag(FLAG_C, carry);
    }

    // DAA : ajustement décimal de A après une addition / soustraction BCD
    void daa() {
        int adj = 0;
        bool carry = get_flag(FLAG_C);
        if (!get_flag(FLAG_N)) {
            if (get_flag(FLAG_H) || (a & 0x0F) > 9) adj |= 0x06;
            if (carry || a > 0x99) {
                adj |= 0x60;
                carry = true;
            }
            a = static_cast<uint8_t>(a + adj);
        } else {
            if (get_flag(FLAG_H)) adj |= 0x06;
            if (carry) adj |= 0x60;
            a = static_cast<uint8_t>(a - adj);
        }
        set_flag(FLAG_Z, a == 0);
        set_flag(FLAG_H, false);
        set_flag(FLAG_C, carry);
    }

    // SP + e8 signé (ADD SP, e8 et LD HL, SP+e8) : Z=0, N=0, H/C calculés sur les octets bas
    uint16_t sp_plus_e8() {
        int8_t off = static_cast<int8_t>(fetch8());
        set_flag(FLAG_Z, false);
        set_flag(FLAG_N, false);
        set_flag(FLAG_H, ((sp & 0x0F) + (off & 0x0F)) > 0x0F);
        set_flag(FLAG_C, ((sp & 0xFF) + (off & 0xFF)) > 0xFF);
        return static_cast<uint16_t>(sp + off);
    }

    // Pile : la pile descend, l'octet haut est écrit en premier
    void push16(uint16_t v) {
        sp--;
        mem->write(sp, v >> 8);
        sp--;
        mem->write(sp, v & 0xFF);
    }

    uint16_t pop16() {
        uint8_t lo = mem->read(sp);
        sp++;
        uint8_t hi = mem->read(sp);
        sp++;
        return (hi << 8) | lo;
    }

    // M-cycles de base d'un opcode (cas "non pris" pour les sauts/appels/retours conditionnels).
    // 0 = opcode invalide ou préfixe CB (géré à part).
    static int base_cycles(uint8_t op) {
        static constexpr uint8_t table[256] = {
            // 0x00
            1, 3, 2, 2, 1, 1, 2, 1,  5, 2, 2, 2, 1, 1, 2, 1,
            // 0x10
            1, 3, 2, 2, 1, 1, 2, 1,  3, 2, 2, 2, 1, 1, 2, 1,
            // 0x20
            2, 3, 2, 2, 1, 1, 2, 1,  2, 2, 2, 2, 1, 1, 2, 1,
            // 0x30
            2, 3, 2, 2, 3, 3, 3, 1,  2, 2, 2, 2, 1, 1, 2, 1,
            // 0x40 - 0xBF : calculés ci-dessous
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
            // 0xC0
            2, 3, 3, 4, 3, 4, 2, 4,  2, 4, 3, 0, 3, 6, 2, 4,
            // 0xD0
            2, 3, 3, 0, 3, 4, 2, 4,  2, 4, 3, 0, 3, 0, 2, 4,
            // 0xE0
            3, 3, 2, 0, 0, 4, 2, 4,  4, 1, 4, 0, 0, 0, 2, 4,
            // 0xF0
            3, 3, 2, 1, 0, 4, 2, 4,  3, 2, 4, 1, 0, 0, 2, 4,
        };
        if (op >= 0x40 && op <= 0x7F) {                       // LD r8, r8' (et HALT)
            if (op == 0x76) return 1;
            return (((op >> 3) & 7) == 6 || (op & 7) == 6) ? 2 : 1;
        }
        if (op >= 0x80 && op <= 0xBF) return (op & 7) == 6 ? 2 : 1;  // ALU A, r8
        return table[op];
    }

    // Déclenche l'interruption la plus prioritaire parmi celles en attente (bits de `pending`).
    // Coût : 5 M-cycles = 20 T-cycles.
    int service_interrupt(uint8_t pending) {
        for (int i = 0; i < 5; i++) {
            if (pending & (1 << i)) {
                ime = false;
                uint8_t iflag = mem->read(0xFF0F);
                mem->write(0xFF0F, static_cast<uint8_t>(iflag & ~(1 << i) & 0x1F));
                push16(pc);
                pc = static_cast<uint16_t>(0x0040 + 8 * i);  // VBlank, STAT, Timer, Serial, Joypad
                return 20;
            }
        }
        return 0;
    }

    // Exécute une instruction (ou une interruption, ou un cycle de HALT).
    // Renvoie le nombre de T-cycles consommés, ou -1 si l'opcode est inconnu.
    int step() {
        uint8_t pending = mem->read(0xFFFF) & mem->read(0xFF0F) & 0x1F;  // IE & IF

        if (halted) {
            if (!pending) return 4;  // on dort : le temps passe quand même
            halted = false;
        }

        if (ime && pending) return service_interrupt(pending);

        bool enable_ime = ime_pending;  // EI pris en compte avant cette instruction
        ime_pending = false;
        if (!execute()) return -1;
        if (enable_ime) ime = true;     // activé après l'instruction suivant EI
        return cycles_m * 4;
    }

    // Instructions préfixées 0xCB : 00 = rotations/décalages, 01 = BIT, 10 = RES, 11 = SET
    bool execute_cb() {
        uint8_t op = fetch8();
        int r = op & 7;
        int bit = (op >> 3) & 7;
        uint8_t v = get_r8(r);

        // 2 M-cycles sur registre ; sur (HL) : 3 pour BIT, 4 pour les autres
        cycles_m = (r != 6) ? 2 : (((op >> 6) == 1) ? 3 : 4);

        switch (op >> 6) {
            case 0: {
                uint8_t res = 0;
                bool carry = false;
                switch (bit) {
                    case 0:  // RLC
                        carry = (v & 0x80) != 0;
                        res = static_cast<uint8_t>((v << 1) | (carry ? 1 : 0));
                        break;
                    case 1:  // RRC
                        carry = (v & 0x01) != 0;
                        res = static_cast<uint8_t>((v >> 1) | (carry ? 0x80 : 0));
                        break;
                    case 2:  // RL
                        carry = (v & 0x80) != 0;
                        res = static_cast<uint8_t>((v << 1) | (get_flag(FLAG_C) ? 1 : 0));
                        break;
                    case 3:  // RR
                        carry = (v & 0x01) != 0;
                        res = static_cast<uint8_t>((v >> 1) | (get_flag(FLAG_C) ? 0x80 : 0));
                        break;
                    case 4:  // SLA
                        carry = (v & 0x80) != 0;
                        res = static_cast<uint8_t>(v << 1);
                        break;
                    case 5:  // SRA (le bit 7 est conservé)
                        carry = (v & 0x01) != 0;
                        res = static_cast<uint8_t>((v >> 1) | (v & 0x80));
                        break;
                    case 6:  // SWAP
                        carry = false;
                        res = static_cast<uint8_t>((v << 4) | (v >> 4));
                        break;
                    default:  // SRL
                        carry = (v & 0x01) != 0;
                        res = static_cast<uint8_t>(v >> 1);
                        break;
                }
                set_flag(FLAG_Z, res == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, false);
                set_flag(FLAG_C, carry);
                set_r8(r, res);
                break;
            }
            case 1:  // BIT n, r : Z = bit à 0, N=0, H=1, C inchangé
                set_flag(FLAG_Z, (v & (1 << bit)) == 0);
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, true);
                break;
            case 2:  // RES n, r
                set_r8(r, static_cast<uint8_t>(v & ~(1 << bit)));
                break;
            default:  // SET n, r
                set_r8(r, static_cast<uint8_t>(v | (1 << bit)));
                break;
        }
        return true;
    }

    // Exécute une instruction. Renvoie false si l'opcode est inconnu.
    bool execute() {
        uint16_t addr = pc;
        uint8_t opcode;
        if (halt_bug) {
            opcode = mem->read(pc);  // bug du HALT : PC n'avance pas pour cet octet
            halt_bug = false;
        } else {
            opcode = fetch8();
        }

        cycles_m = base_cycles(opcode);

        // Famille LD r8, r8' : 01 DDD SSS (0x76 est HALT, pas un LD)
        if (opcode >= 0x40 && opcode <= 0x7F && opcode != 0x76) {
            set_r8((opcode >> 3) & 7, get_r8(opcode & 7));
            return true;
        }

        // Famille ALU A, r8 : 10 OOO SSS
        if (opcode >= 0x80 && opcode <= 0xBF) {
            alu((opcode >> 3) & 7, get_r8(opcode & 7));
            return true;
        }

        // Instructions régulières décodées par bits (gère aussi (HL))
        switch (opcode & 0xC7) {
            case 0x04: {  // INC r8 / INC (HL)
                int r = (opcode >> 3) & 7;
                uint8_t v = get_r8(r);
                inc8(v);
                set_r8(r, v);
                return true;
            }
            case 0x05: {  // DEC r8 / DEC (HL)
                int r = (opcode >> 3) & 7;
                uint8_t v = get_r8(r);
                dec8(v);
                set_r8(r, v);
                return true;
            }
            case 0x06:    // LD r8, n8 / LD (HL), n8
                set_r8((opcode >> 3) & 7, fetch8());
                return true;
            case 0xC6:    // ALU A, n8 (ADD/ADC/SUB/SBC/AND/XOR/OR/CP)
                alu((opcode >> 3) & 7, fetch8());
                return true;
            case 0xC7:    // RST n : appel vers 0x00, 0x08, ... 0x38
                push16(pc);
                pc = opcode & 0x38;
                return true;
        }

        // Sauts / appels / retours conditionnels (cc dans les bits 3-4)
        switch (opcode & 0xE7) {
            case 0xC0:    // RET cc
                if (cond((opcode >> 3) & 3)) {
                    pc = pop16();
                    cycles_m += 3;
                }
                return true;
            case 0xC2: {  // JP cc, a16
                uint16_t target = fetch16();
                if (cond((opcode >> 3) & 3)) {
                    pc = target;
                    cycles_m += 1;
                }
                return true;
            }
            case 0xC4: {  // CALL cc, a16
                uint16_t target = fetch16();
                if (cond((opcode >> 3) & 3)) {
                    push16(pc);
                    pc = target;
                    cycles_m += 3;
                }
                return true;
            }
        }

        switch (opcode) {
            case 0x00: break;                       // NOP
            case 0x10: fetch8(); break;             // STOP (traité comme un NOP de 2 octets)

            // LD r16, n16
            case 0x01: set_bc(fetch16()); break;    // LD BC, n16
            case 0x11: set_de(fetch16()); break;    // LD DE, n16
            case 0x21: set_hl(fetch16()); break;    // LD HL, n16
            case 0x31: sp = fetch16(); break;       // LD SP, n16

            // Accès mémoire via BC / DE
            case 0x02: mem->write(bc(), a); break;  // LD (BC), A
            case 0x12: mem->write(de(), a); break;  // LD (DE), A
            case 0x0A: a = mem->read(bc()); break;  // LD A, (BC)
            case 0x1A: a = mem->read(de()); break;  // LD A, (DE)

            // Accès mémoire via HL, avec incrément / décrément de HL
            case 0x2A:                              // LD A, (HL+)
                a = mem->read(hl());
                set_hl(hl() + 1);
                break;
            case 0x22:                              // LD (HL+), A
                mem->write(hl(), a);
                set_hl(hl() + 1);
                break;
            case 0x3A:                              // LD A, (HL-)
                a = mem->read(hl());
                set_hl(hl() - 1);
                break;
            case 0x32:                              // LD (HL-), A
                mem->write(hl(), a);
                set_hl(hl() - 1);
                break;

            // Sauts relatifs
            case 0x18: jr(true); break;                      // JR e8
            case 0x20: jr(!get_flag(FLAG_Z)); break;         // JR NZ, e8
            case 0x28: jr(get_flag(FLAG_Z)); break;          // JR Z, e8
            case 0x30: jr(!get_flag(FLAG_C)); break;         // JR NC, e8
            case 0x38: jr(get_flag(FLAG_C)); break;          // JR C, e8

            // Sauts absolus
            case 0xC3: pc = fetch16(); break;       // JP a16

            // Accès mémoire absolus et "hauts" (0xFF00 + offset)
            case 0xEA: mem->write(fetch16(), a); break;   // LD (a16), A
            case 0xFA: a = mem->read(fetch16()); break;   // LD A, (a16)
            case 0xE0:                                    // LDH (a8), A
                mem->write(static_cast<uint16_t>(0xFF00 + fetch8()), a);
                break;
            case 0xF0:                                    // LDH A, (a8)
                a = mem->read(static_cast<uint16_t>(0xFF00 + fetch8()));
                break;
            case 0xE2:                                    // LD (C), A
                mem->write(static_cast<uint16_t>(0xFF00 + c), a);
                break;
            case 0xF2:                                    // LD A, (C)
                a = mem->read(static_cast<uint16_t>(0xFF00 + c));
                break;

            // Pile
            case 0xC5: push16(bc()); break;               // PUSH BC
            case 0xD5: push16(de()); break;               // PUSH DE
            case 0xE5: push16(hl()); break;               // PUSH HL
            case 0xF5: push16(af()); break;               // PUSH AF
            case 0xC1: set_bc(pop16()); break;            // POP BC
            case 0xD1: set_de(pop16()); break;            // POP DE
            case 0xE1: set_hl(pop16()); break;            // POP HL
            case 0xF1: set_af(pop16()); break;            // POP AF (F masqué)
            case 0xF9: sp = hl(); break;                  // LD SP, HL

            // Appels et retours
            case 0xCD: {                                  // CALL a16
                uint16_t target = fetch16();
                push16(pc);
                pc = target;
                break;
            }
            case 0xC9: pc = pop16(); break;               // RET
            case 0xE9: pc = hl(); break;                  // JP HL

            // INC / DEC 16 bits (aucun flag modifié)
            case 0x03: set_bc(bc() + 1); break;           // INC BC
            case 0x13: set_de(de() + 1); break;           // INC DE
            case 0x23: set_hl(hl() + 1); break;           // INC HL
            case 0x33: sp++; break;                       // INC SP
            case 0x0B: set_bc(bc() - 1); break;           // DEC BC
            case 0x1B: set_de(de() - 1); break;           // DEC DE
            case 0x2B: set_hl(hl() - 1); break;           // DEC HL
            case 0x3B: sp--; break;                       // DEC SP

            // ADD HL, r16
            case 0x09: add_hl(bc()); break;               // ADD HL, BC
            case 0x19: add_hl(de()); break;               // ADD HL, DE
            case 0x29: add_hl(hl()); break;               // ADD HL, HL
            case 0x39: add_hl(sp); break;                 // ADD HL, SP

            // Rotations de A
            case 0x07: rotate_a(0); break;                // RLCA
            case 0x0F: rotate_a(1); break;                // RRCA
            case 0x17: rotate_a(2); break;                // RLA
            case 0x1F: rotate_a(3); break;                // RRA

            // Divers sur A et les flags
            case 0x27: daa(); break;                      // DAA
            case 0x2F:                                    // CPL
                a = static_cast<uint8_t>(~a);
                set_flag(FLAG_N, true);
                set_flag(FLAG_H, true);
                break;
            case 0x37:                                    // SCF
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, false);
                set_flag(FLAG_C, true);
                break;
            case 0x3F:                                    // CCF
                set_flag(FLAG_N, false);
                set_flag(FLAG_H, false);
                set_flag(FLAG_C, !get_flag(FLAG_C));
                break;

            // SP
            case 0x08: {                                  // LD (a16), SP
                uint16_t target = fetch16();
                mem->write(target, sp & 0xFF);
                mem->write(static_cast<uint16_t>(target + 1), sp >> 8);
                break;
            }
            case 0xE8: sp = sp_plus_e8(); break;          // ADD SP, e8
            case 0xF8: set_hl(sp_plus_e8()); break;       // LD HL, SP+e8

            // Retour d'interruption et préfixe CB
            case 0xD9: pc = pop16(); ime = true; break;   // RETI
            case 0xCB: return execute_cb();               // préfixe CB

            // HALT : dort jusqu'à une interruption. Si IME=0 et qu'une interruption est déjà
            // en attente, le CPU ne dort pas et l'octet suivant est lu deux fois (bug du HALT).
            case 0x76: {
                uint8_t pending = mem->read(0xFFFF) & mem->read(0xFF0F) & 0x1F;
                if (!ime && pending) halt_bug = true;
                else                 halted = true;
                break;
            }

            // Interruptions
            case 0xF3: ime = false; break;          // DI
            case 0xFB: ime_pending = true; break;   // EI

            default:
                std::cerr << "Opcode inconnu : 0x" << std::hex
                          << static_cast<int>(opcode) << " a l'adresse 0x"
                          << addr << std::dec << "\n";
                return false;
        }
        return true;
    }
};