#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <fstream>

struct Memory {
    std::array<uint8_t, 0x10000> cells{};

    uint8_t read(uint16_t pos) const { return cells[pos]; }
    void write(uint16_t pos, uint8_t v) { cells[pos] = v; }
    
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