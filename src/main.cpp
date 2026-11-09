#include <iomanip>
#include <iostream>

#include "memory.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: retroforge <rom.gb>\n";
        return 1;
    }

    Memory mem;
    if (!mem.load_rom(argv[1])) {
        std::cerr << "Impossible de charger la ROM : " << argv[1] << "\n";
        return 1;
    }

    std::cout << "ROM chargee. Octets a partir de 0x0100 :\n";
    std::cout << std::hex << std::setfill('0');
    for (int i = 0; i < 4; i++) {
        std::cout << std::setw(2) << static_cast<int>(mem.read(0x0100 + i)) << " ";
    }
    std::cout << "\n";

    return 0;
}