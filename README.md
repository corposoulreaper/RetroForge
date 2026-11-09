# retrocore

Modular retro console emulator written in C++. Game Boy first, more cores to come. Runs on desktop and in the browser via WebAssembly.

> 🚧 **Work in progress**: currently working on the Game Boy CPU.

## Goals

- One emulation core per console, behind a common interface
- Desktop frontend (SDL2) and web frontend (WebAssembly)
- Built-in debugger (registers, VRAM, disassembler, step-by-step)

## Roadmap

- [ ] Game Boy CPU (passes Blargg `cpu_instrs`)
- [ ] Memory map, interrupts, timers
- [ ] PPU (passes `dmg-acid2`)
- [ ] Input and cartridges (MBC1, MBC3)
- [ ] Audio (APU)
- [ ] WebAssembly frontend
- [ ] Debugger
- [ ] More consoles (CHIP-8, Game Boy Color, NES)

## Build

Requirements: a C++17 compiler, CMake.

```bash
cmake -B build
cmake --build build
```

## Resources

- [Pan Docs](https://gbdev.io/pandocs/)

## Legal

This project does not provide, and does not support the distribution of, commercial ROMs. Test ROMs and homebrew only.

## License

MIT
