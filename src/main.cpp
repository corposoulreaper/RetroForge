#include <iostream>
#include "cpu.hpp"

int main() {
    CPU cpu;

    std::cout << std::hex;
    std::cout << "PC = " << cpu.pc << "\n";
    std::cout << "SP = " << cpu.sp << "\n";
    std::cout << "AF = " << cpu.af() << "\n";
    std::cout << "HL = " << cpu.hl() << "\n";

    cpu.b = 0x12;
    cpu.c = 0x34;
    std::cout << "BC = " << cpu.bc() << "\n";   // attendu : 1234

    cpu.set_af(0xFFFF);
    std::cout << "AF = " << cpu.af() << "\n";   // attendu : ffff0 ? non, regarde bien

    std::cout << "Z = " << cpu.get_flag(CPU::FLAG_Z) << "\n";
}