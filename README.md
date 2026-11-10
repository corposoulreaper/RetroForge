RetroForge

A multi-system retro console emulator written in C++17 from scratch, starting with the original Game Boy (DMG). The goal is to build one emulator in which each system has its own core and shares a common frontend, and to grow it system by system: Game Boy Color next, then Game Boy Advance.

Status
System	Status
Game Boy (DMG)	Playable: CPU, timer, interrupts, video, audio, cartridges, saves
Game Boy Color (CGB)	Planned, next milestone
Game Boy Advance (GBA)	Planned, long-term goal (separate ARM core)

Game Boy (DMG) features
CPU: the full Sharp LR35902 instruction set, including the CB-prefixed opcodes, DAA, HALT (with the halt bug) and cycle counting per instruction.
Interrupts and timer: IME/IE/IF with the EI delay, plus DIV, TIMA, TMA and TAC.
Video (PPU): scanline-based renderer for background, window and sprites (8x8 and 8x16), LCDC/STAT/LY/LYC, the VBlank and STAT interrupts, and OAM DMA.
Audio (APU): the four sound channels (two pulse channels with sweep and envelope, wave, noise), mixed to stereo through SDL2.
Cartridges: no MBC, MBC1, MBC3 (without the real-time clock) and MBC5, with battery-backed saves written to a .sav file next to the ROM.
Input: keyboard mapped to the Game Boy pad.
Debug aids: FPS shown in the window title, F1 dumps the CPU state to the terminal, and a headless --test mode for test ROMs.
Accuracy

The CPU passes all 11 individual ROMs of Blargg's cpu_instrs test suite (01-special to 11-op a,(hl)), run in headless mode with the serial output printed to the terminal.

Known simplifications, which I plan to improve:

TIMA reloads immediately on overflow (real hardware delays the reload by one M-cycle).
Each scanline is drawn once, when it enters HBlank, so effects that change registers mid-scanline are not reproduced.
The MBC3 real-time clock is not emulated.
Build

Developed and tested on Linux (Linux Mint / Ubuntu).

bash
sudo apt install build-essential cmake libsdl2-dev
cmake -S . -B build
cmake --build build -j
Usage
bash
./build/retroforge path/to/game.gb

Headless mode for test ROMs (prints the serial output and stops on "Passed" or "Failed"):

bash
./build/retroforge path/to/test.gb --test

Run all the cpu_instrs ROMs in one go:

bash
for rom in path/to/cpu_instrs/individual/*.gb; do
  echo "=== $(basename "$rom") ==="
  timeout 60 ./build/retroforge "$rom" --test | grep -E "Passed|Failed|Fin d'execution"
done

Controls
Game Boy	Keyboard
D-pad	Arrow keys
A	Z or W
B	X
Start	Enter or Space
Select	Backspace or Right Shift
Mute / unmute	M
Print CPU state	F1
Quit	Esc
Project layout
src/
  main.cpp     SDL2 frontend, main loop, headless test mode
  cpu.hpp      CPU: registers, instruction decoding, interrupts, HALT
  memory.hpp   Memory map, cartridge and MBC, timer, joypad, saves
  ppu.hpp      Video: modes, STAT/VBlank, scanline renderer
  apu.hpp      Audio: four channels and mixing
CMakeLists.txt
Roadmap
Game Boy Color

The CGB runs the same CPU as the DMG with extra features, so it builds directly on the current core:

 Detect CGB cartridges from the header (0x143)
 Banked VRAM (2 banks) and WRAM (8 banks), tile attributes
 Color palettes (BCPS/BCPD, OCPS/OCPD) and the CGB sprite and background priority rules
 Double-speed mode (KEY1 / STOP)
 General-purpose and HBlank DMA (HDMA)
 Backward compatibility: DMG games with a CGB palette
Game Boy Advance

A much larger step with a different architecture, so it will be a separate core sharing only the frontend:

 ARM7TDMI CPU (ARM and Thumb instruction sets)
 Memory map, BIOS handling and DMA
 Video modes (tiled and bitmap) and the GBA PPU
 Timers, interrupts and input
 Audio (direct sound and the legacy Game Boy channels)
Cross-system work
 Split a common system interface (load ROM, run one frame, get the video buffer, push input, drain audio) from the per-system cores
 Choose the system automatically from the ROM header or file extension
 Automated regression tests (Blargg, dmg-acid2, instr_timing)
Legal

RetroForge does not include any ROM or BIOS. Use only games and test ROMs that you own or that are freely distributed (homebrew, test suites).

Acknowledgements
Pan Docs for the hardware reference
Blargg's Game Boy test ROMs
The Game Boy homebrew community
License

MIT
