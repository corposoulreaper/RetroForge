#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

struct Memory {
    std::array<uint8_t, 0x10000> cells{};

    // Texte envoyé par la ROM sur le port série (ROMs de test de Blargg)
    std::string serial_output;

    // Compteur interne du timer : DIV (0xFF04) en est l'octet haut.
    // Il avance de 1 à chaque T-cycle (4 194 304 fois par seconde).
    uint16_t div_counter = 0xABCC;

    // --- Lecture ---------------------------------------------------------
    uint8_t read(uint16_t pos) const {
        switch (pos) {
            case 0xFF04: return static_cast<uint8_t>(div_counter >> 8);  // DIV
            case 0xFF07: return cells[pos] | 0xF8;                       // TAC (3 bits utiles)
            case 0xFF0F: return cells[pos] | 0xE0;                       // IF (5 bits utiles)
            default:     return cells[pos];
        }
    }

    // --- Écriture --------------------------------------------------------
    void write(uint16_t pos, uint8_t v) {
        // ROM : écriture ignorée (plus tard, ce seront les registres du MBC)
        if (pos < 0x8000) return;

        switch (pos) {
            case 0xFF04: {  // écrire dans DIV remet le compteur à 0
                bool before = timer_signal();
                div_counter = 0;
                if (before) increment_tima();  // le signal retombe : front descendant
                return;
            }
            case 0xFF07: {  // TAC : changer la fréquence peut aussi produire un front descendant
                bool before = timer_signal();
                cells[pos] = static_cast<uint8_t>(v & 0x07);
                if (before && !timer_signal()) increment_tima();
                return;
            }
            case 0xFF02:  // Port série : la ROM met un caractère en 0xFF01, puis écrit 0x81 ici
                cells[pos] = v;
                if (v == 0x81) {
                    char ch = static_cast<char>(cells[0xFF01]);
                    serial_output += ch;
                    std::cout << ch << std::flush;
                }
                return;
            default:
                break;
        }
        cells[pos] = v;
    }

    // --- Timer -----------------------------------------------------------
    // Fait avancer le timer de `cycles` T-cycles (à appeler après chaque instruction)
    void tick(int cycles) {
        for (int i = 0; i < cycles; i++) {
            bool before = timer_signal();
            div_counter++;
            if (before && !timer_signal()) increment_tima();
        }
    }

    // Signal du timer : TAC bit 2 (activation) ET un bit de div_counter choisi par TAC bits 0-1.
    // TIMA s'incrémente quand ce signal passe de 1 à 0.
    bool timer_signal() const {
        static constexpr uint16_t masks[4] = {0x0200, 0x0008, 0x0020, 0x0080};  // 4096/262144/65536/16384 Hz
        uint8_t tac = cells[0xFF07];
        return (tac & 0x04) && (div_counter & masks[tac & 3]);
    }

    void increment_tima() {
        if (cells[0xFF05] == 0xFF) {
            cells[0xFF05] = cells[0xFF06];                           // TIMA = TMA
            cells[0xFF0F] = static_cast<uint8_t>(cells[0xFF0F] | 0x04);  // demande l'interruption Timer
        } else {
            cells[0xFF05]++;
        }
    }

    // --- Chargement de la ROM ---------------------------------------------
    bool load_rom(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return false;

        std::streamsize size = file.tellg();
        file.seekg(0);

        if (size > 0x8000) return false;

        file.read(reinterpret_cast<char*>(cells.data()), size);
        return true;
    }
};