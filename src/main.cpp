#include <iomanip>
#include <iostream>

#include "cpu.hpp"
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

    CPU cpu;
    cpu.mem = &mem;

    std::cout << std::hex;
    for (int i = 0; i < 3; i++) {
        cpu.step();
        std::cout << "PC = " << cpu.pc << "\n";
    }

    return 0;
}