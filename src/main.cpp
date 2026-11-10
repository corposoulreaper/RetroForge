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

    int steps = 0;
    while (steps < 100000 && cpu.step()) {
        steps++;
    }

    std::cout << std::hex << std::setfill('0');
    std::cout << "Arret apres " << std::dec << steps << " instructions\n";
    std::cout << std::hex;
    std::cout << "PC = " << std::setw(4) << cpu.pc << "\n";
    std::cout << "AF = " << std::setw(4) << cpu.af()
              << "  BC = " << std::setw(4) << cpu.bc() << "\n";
    std::cout << "DE = " << std::setw(4) << cpu.de()
              << "  HL = " << std::setw(4) << cpu.hl() << "\n";

    return 0;
}