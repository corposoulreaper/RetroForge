#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "apu.hpp"
#include "cpu.hpp"
#include "memory.hpp"
#include "ppu.hpp"

static void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

static void test_cpu_executes_and_writes_memory() {
    Memory mem;
    mem.rom.assign(0x8000, 0);
    mem.rom[0x0100] = 0x3E;  // LD A, 0x42
    mem.rom[0x0101] = 0x42;
    mem.rom[0x0102] = 0xEA;  // LD (0xC000), A
    mem.rom[0x0103] = 0x00;
    mem.rom[0x0104] = 0xC0;
    CPU cpu;
    cpu.mem = &mem;

    require(cpu.step() == 8, "LD A,n cycle count");
    require(cpu.a == 0x42, "LD A,n register value");
    require(cpu.step() == 16, "LD (nn),A cycle count");
    require(mem.read(0xC000) == 0x42, "CPU write reaches memory");
}

static void test_timer_reloads_on_overflow() {
    Memory mem;
    mem.div_counter = 0;
    mem.cells[0xFF05] = 0xFF;
    mem.cells[0xFF06] = 0xA7;
    mem.cells[0xFF07] = 0x05;

    mem.tick(16);

    require(mem.read(0xFF05) == 0xA7, "TIMA reloads TMA on overflow");
    require((mem.read(0xFF0F) & 0x04) != 0, "timer overflow requests interrupt");
}

static void test_apu_registers_and_sample_output() {
    Memory mem;
    APU apu;
    mem.apu = &apu;
    mem.write(0xFF12, 0xF0);
    mem.write(0xFF14, 0x80);

    require(apu.ch[0].enabled, "channel 1 trigger enables its DAC-backed channel");
    require((mem.read(0xFF26) & 0x01) != 0, "NR52 reports channel 1 enabled");

    apu.tick(96);
    require(apu.samples.size() == 2, "APU emits one interleaved stereo sample");

    mem.write(0xFF26, 0x00);
    require(!apu.ch[0].enabled, "NR52 power-off disables channel 1");
}

static void test_ppu_enters_vblank() {
    Memory mem;
    mem.cells[0xFF40] = 0x80;
    PPU ppu;
    ppu.mem = &mem;

    for (int cycle = 0; cycle < 456 * 144; cycle += 4) ppu.tick(4);

    require(ppu.frame_ready, "PPU signals the first VBlank");
    require(ppu.ly == 144, "PPU enters VBlank at line 144");
    require((mem.read(0xFF0F) & 0x01) != 0, "VBlank requests its interrupt");
}

int main() {
    test_cpu_executes_and_writes_memory();
    test_timer_reloads_on_overflow();
    test_apu_registers_and_sample_output();
    test_ppu_enters_vblank();
    std::cout << "All core regression tests passed.\n";
}