#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

struct Memory {
    std::array<uint8_t, 0x10000> cells{};

    // Texte envoyé par la ROM sur le port série (utilisé par les ROMs de test de Blargg)
    std::string serial_output;

    uint8_t read(uint16_t pos) const { return cells[pos]; }

    void write(uint16_t pos, uint8_t v) {
        cells[pos] = v;

        // Port série : la ROM met un caractère en 0xFF01, puis écrit 0x81 en 0xFF02
        // pour "l'envoyer". On l'affiche dans le terminal et on le garde en mémoire.
        if (pos == 0xFF02 && v == 0x81) {
            char ch = static_cast<char>(cells[0xFF01]);
            serial_output += ch;
            std::cout << ch << std::flush;
        }
    }

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