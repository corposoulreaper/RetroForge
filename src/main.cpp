#include <SDL2/SDL.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "memory.hpp"
#include "ppu.hpp"

// ---------------------------------------------------------------------------
// Mode test (--test) : sans fenêtre, s'arrête sur "Passed" / "Failed" (ROMs de Blargg)
// ---------------------------------------------------------------------------
static int run_test(CPU& cpu, Memory& mem, PPU& ppu) {
    const long long max_steps = 200000000;  // garde-fou

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

        mem.tick(cycles);  // timer
        ppu.tick(cycles);  // écran
        steps++;
        total_cycles += cycles;

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

// ---------------------------------------------------------------------------
// Mode fenêtre (SDL2)
// ---------------------------------------------------------------------------

// Touches du clavier (plusieurs touches possibles par bouton) :
//   Croix  : flèches
//   A      : Z ou W (les deux, pour que ce soit pratique en AZERTY comme en QWERTY)
//   B      : X
//   Start  : Entrée ou Espace
//   Select : Retour arrière ou Maj droite
//   F1     : affiche l'état du CPU dans le terminal (aide au debug)
//   Échap  : quitter
// Pour changer une touche, modifie simplement les "case" ci-dessous.
static void handle_key(Memory& mem, const SDL_Keysym& k, bool down) {
    // Par position physique (identique en QWERTY et AZERTY)
    switch (k.scancode) {
        case SDL_SCANCODE_RIGHT:     mem.set_key(true, 0, down); break;
        case SDL_SCANCODE_LEFT:      mem.set_key(true, 1, down); break;
        case SDL_SCANCODE_UP:        mem.set_key(true, 2, down); break;
        case SDL_SCANCODE_DOWN:      mem.set_key(true, 3, down); break;
        case SDL_SCANCODE_Z:         mem.set_key(false, 0, down); break;  // A (touche W en AZERTY)
        case SDL_SCANCODE_X:         mem.set_key(false, 1, down); break;  // B
        case SDL_SCANCODE_SPACE:
        case SDL_SCANCODE_RETURN:    mem.set_key(false, 3, down); break;  // Start
        case SDL_SCANCODE_BACKSPACE:
        case SDL_SCANCODE_RSHIFT:    mem.set_key(false, 2, down); break;  // Select
        default: break;
    }
    // Par lettre imprimée sur la touche : la touche "Z" d'un clavier AZERTY est aussi A
    if (k.sym == SDLK_z) mem.set_key(false, 0, down);
}

// F1 : affiche l'état de la machine, pour comprendre pourquoi un jeu est bloqué
static void dump_state(const CPU& cpu, const Memory& mem, const PPU& ppu) {
    std::cout << std::hex << std::setfill('0');
    std::cout << "\n[F1] PC=" << std::setw(4) << cpu.pc << " SP=" << std::setw(4) << cpu.sp
              << " AF=" << std::setw(4) << cpu.af() << " BC=" << std::setw(4) << cpu.bc()
              << " DE=" << std::setw(4) << cpu.de() << " HL=" << std::setw(4) << cpu.hl() << "\n";
    std::cout << "     IME=" << cpu.ime << " HALT=" << cpu.halted
              << " IE=" << std::setw(2) << static_cast<int>(mem.read(0xFFFF))
              << " IF=" << std::setw(2) << static_cast<int>(mem.read(0xFF0F))
              << " LCDC=" << std::setw(2) << static_cast<int>(mem.read(0xFF40))
              << " STAT=" << std::setw(2) << static_cast<int>(mem.read(0xFF41))
              << " LY=" << std::setw(2) << static_cast<int>(ppu.ly)
              << " JOYP=" << std::setw(2) << static_cast<int>(mem.read(0xFF00))
              << " banque ROM=" << mem.rom_bank << " (MBC" << std::dec << mem.mbc << std::hex << ")\n";
    std::cout << "     octets a PC :";
    for (int i = 0; i < 8; i++) {
        std::cout << " " << std::setw(2) << static_cast<int>(mem.read(static_cast<uint16_t>(cpu.pc + i)));
    }
    std::cout << std::dec << std::setfill(' ') << "\n" << std::flush;
}

static int run_window(CPU& cpu, Memory& mem, PPU& ppu, const std::string& title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Erreur SDL_Init : " << SDL_GetError() << "\n";
        return 1;
    }

    const int scale = 4;
    SDL_Window* window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          PPU::WIDTH * scale, PPU::HEIGHT * scale, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Erreur SDL_CreateWindow : " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        std::cerr << "Erreur SDL_CreateRenderer : " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // pixels nets (pas de lissage)
    SDL_RenderSetLogicalSize(renderer, PPU::WIDTH, PPU::HEIGHT);  // garde le ratio 160:144

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                             PPU::WIDTH, PPU::HEIGHT);
    if (!texture) {
        std::cerr << "Erreur SDL_CreateTexture : " << SDL_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Teintes 0 (clair) à 3 (foncé), au format ARGB
    const uint32_t palette[4] = {0xFFE0F8D0, 0xFF88C070, 0xFF346856, 0xFF081820};
    std::vector<uint32_t> pixels(PPU::WIDTH * PPU::HEIGHT);

    // Une image = 70224 T-cycles, soit environ 59,7 images par seconde
    const int cycles_per_frame = 70224;
    const Uint64 freq = SDL_GetPerformanceFrequency();
    const Uint64 frame_ticks = static_cast<Uint64>(freq * (cycles_per_frame / 4194304.0));
    Uint64 next_frame = SDL_GetPerformanceCounter();
    Uint64 fps_start = next_frame;  // pour afficher les FPS dans le titre de la fenêtre
    int fps_frames = 0;

    bool running = true;
    bool error = false;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
                if (ev.key.keysym.scancode == SDL_SCANCODE_F1) dump_state(cpu, mem, ppu);
                handle_key(mem, ev.key.keysym, true);
            } else if (ev.type == SDL_KEYUP) {
                handle_key(mem, ev.key.keysym, false);
            }
        }

        // Exécute le CPU jusqu'au VBlank (ou 70224 cycles si l'écran est éteint)
        int frame_cycles = 0;
        while (!ppu.frame_ready && frame_cycles < cycles_per_frame) {
            int cycles = cpu.step();
            if (cycles < 0) {  // opcode inconnu
                running = false;
                error = true;
                break;
            }
            mem.tick(cycles);
            ppu.tick(cycles);
            frame_cycles += cycles;
        }
        ppu.frame_ready = false;

        // Affichage
        for (size_t i = 0; i < pixels.size(); i++) pixels[i] = palette[ppu.framebuffer[i] & 3];
        SDL_UpdateTexture(texture, nullptr, pixels.data(), PPU::WIDTH * static_cast<int>(sizeof(uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // FPS dans le titre (environ 59.7 si tout va bien ; bien moins = émulateur trop lent)
        fps_frames++;
        Uint64 t = SDL_GetPerformanceCounter();
        if (t - fps_start >= freq) {
            double fps = fps_frames * static_cast<double>(freq) / static_cast<double>(t - fps_start);
            std::ostringstream oss;
            oss << title << " - " << std::fixed << std::setprecision(1) << fps << " fps";
            SDL_SetWindowTitle(window, oss.str().c_str());
            fps_start = t;
            fps_frames = 0;
        }

        // Cadence : attend le temps restant pour tenir ~59,7 images par seconde
        next_frame += frame_ticks;
        Uint64 now = SDL_GetPerformanceCounter();
        if (next_frame > now) {
            SDL_Delay(static_cast<Uint32>((next_frame - now) * 1000 / freq));
        } else if (now - next_frame > freq / 10) {
            next_frame = now;  // trop de retard : on se resynchronise
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return error ? 1 : 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: retroforge <rom.gb> [--test]\n"
                  << "  --test : mode sans fenetre pour les ROMs de test de Blargg\n";
        return 1;
    }

    bool test_mode = false;
    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--test") == 0) test_mode = true;
    }

    Memory mem;
    if (!mem.load_rom(argv[1])) {
        std::cerr << "Impossible de charger la ROM : " << argv[1] << "\n";
        return 1;
    }

    CPU cpu;
    cpu.mem = &mem;

    PPU ppu;
    ppu.mem = &mem;

    if (test_mode) return run_test(cpu, mem, ppu);
    return run_window(cpu, mem, ppu, "RetroForge - " + std::string(argv[1]));
}