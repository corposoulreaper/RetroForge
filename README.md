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

## Tests

Run the core regression tests with CTest:

```bash
ctest --test-dir build --output-on-failure
```

GitHub Actions runs the native regression tests and builds the WebAssembly frontend on pushes and pull requests to `main`. Pushes to `main` are deployed to GitHub Pages after both jobs pass. Enable GitHub Pages with the **GitHub Actions** build and deployment source in the repository settings.

## WebAssembly

With the Emscripten SDK activated in the shell:

```bash
emcmake cmake -S . -B build-wasm
cmake --build build-wasm
python3 -m http.server --directory build-wasm 8000
```

Open `http://localhost:8000/retroforge.html` and choose a Game Boy ROM. The page includes touch controls for mobile browsers, pause/resume, ROM switching without reloading, and a restart button; press `P` to toggle pause from the keyboard.
Battery-backed saves are stored in the browser's IndexedDB, separately for each ROM filename and browser origin.

## Resources

- [Pan Docs](https://gbdev.io/pandocs/)

## Legal

This project does not provide, and does not support the distribution of, commercial ROMs. Test ROMs and homebrew only.

## License

MIT
