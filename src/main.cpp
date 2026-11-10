#include <iomanip>
#include <iostream>
#include <string>

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

    // Garde-fou : on s'arrête si la ROM ne se termine jamais
    const long long max_steps = 200000000;

    long long steps = 0;
    long long total_cycles = 0;
    bool unknown_opcode = false;
    std::string verdict = "limite d'instructions atteinte";

    while (steps < max_steps) {
        int cycles = cpu.step();  // T-cycles consommés, ou -1 si opcode inconnu
        if (cycles < 0) {
            unknown_opcode = true;
            verdict = "opcode inconnu";
            break;
        }

        mem.tick(cycles);  // fait avancer le timer (DIV / TIMA)
        steps++;
        total_cycles += cycles;

        // Les ROMs de test de Blargg annoncent leur résultat sur le port série
        if (mem.serial_output.find("Passed") != std::string::npos) {
            verdict = "test reussi (Passed)";
            break;
        }
        if (mem.serial_output.find("Failed") != std::string::npos) {
            verdict = "test echoue (Failed)";
            break;
        }
    }

    std::cout << "\n--- Fin d'execution : " << verdict << " ---\n";
    std::cout << "Pas executes : " << std::dec << steps
              << "  |  Cycles : " << total_cycles << "\n";
    std::cout << std::hex << std::setfill('0');
    std::cout << "PC = " << std::setw(4) << cpu.pc << "\n";
    std::cout << "AF = " << std::setw(4) << cpu.af()
              << "  BC = " << std::setw(4) << cpu.bc() << "\n";
    std::cout << "DE = " << std::setw(4) << cpu.de()
              << "  HL = " << std::setw(4) << cpu.hl() << "\n";

    return unknown_opcode ? 1 : 0;
}