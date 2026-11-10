#pragma once
#include <array>
#include <cstdint>

#include "memory.hpp"

// PPU de la Game Boy (DMG) : timing par ligne + rendu ligne par ligne.
//   - 456 T-cycles par ligne, 154 lignes par image (144 visibles + 10 de VBlank)
//   - modes : 2 (recherche OAM, 80), 3 (dessin, 172), 0 (HBlank, 204), 1 (VBlank)
//   - chaque ligne est dessinée à l'entrée en HBlank avec les registres de ce moment-là
struct PPU {
    static constexpr int WIDTH = 160;
    static constexpr int HEIGHT = 144;

    Memory* mem = nullptr;

    // Image courante : une teinte (0 = clair ... 3 = foncé) par pixel
    std::array<uint8_t, WIDTH * HEIGHT> framebuffer{};
    bool frame_ready = false;  // passe à true à chaque début de VBlank (à remettre à false côté main)

    int dot = 0;           // position dans la ligne (0-455)
    uint8_t ly = 0;        // ligne courante (0-153)
    int mode = 0;          // mode courant (0-3)
    bool lcd_on = true;
    bool stat_line = false;  // état de la ligne d'interruption STAT (détection de front montant)
    int window_line = 0;     // ligne interne de la fenêtre (avance seulement quand elle est dessinée)

    // Demande une interruption (bit de IF) : 0 = VBlank, 1 = STAT
    void request(int bit) {
        mem->cells[0xFF0F] = static_cast<uint8_t>(mem->cells[0xFF0F] | (1 << bit));
    }

    // Met à jour STAT (mode + indicateur LY==LYC) et déclenche l'interruption STAT si besoin
    void update_stat() {
        uint8_t stat = mem->cells[0xFF41];
        bool coincidence = (ly == mem->cells[0xFF45]);
        stat = static_cast<uint8_t>((stat & 0x78) | 0x80 | (coincidence ? 0x04 : 0x00) | mode);
        mem->cells[0xFF41] = stat;

        bool line = (coincidence && (stat & 0x40)) ||
                    (mode == 0 && (stat & 0x08)) ||
                    (mode == 1 && (stat & 0x10)) ||
                    (mode == 2 && (stat & 0x20));
        if (line && !stat_line) request(1);
        stat_line = line;
    }

    // Fait avancer le PPU de `cycles` T-cycles (à appeler après chaque instruction)
    void tick(int cycles) {
        if (!(mem->cells[0xFF40] & 0x80)) {  // LCDC bit 7 : écran éteint
            if (lcd_on) {
                lcd_on = false;
                dot = 0;
                ly = 0;
                mode = 0;
                window_line = 0;
                stat_line = false;
                mem->cells[0xFF44] = 0;
                mem->cells[0xFF41] = static_cast<uint8_t>((mem->cells[0xFF41] & 0x78) | 0x80);
                framebuffer.fill(0);
            }
            return;
        }
        if (!lcd_on) {  // l'écran vient d'être rallumé : on repart de la ligne 0
            lcd_on = true;
            dot = 0;
            ly = 0;
            mode = 0;
            window_line = 0;
            stat_line = false;
        }

        dot += cycles;
        if (dot >= 456) {  // fin de ligne (cycles <= 24, donc au plus une fois par appel)
            dot -= 456;
            ly++;
            if (ly > 153) {
                ly = 0;
                window_line = 0;
            }
            mem->cells[0xFF44] = ly;
        }

        int new_mode;
        if (ly >= 144)      new_mode = 1;
        else if (dot < 80)  new_mode = 2;
        else if (dot < 252) new_mode = 3;
        else                new_mode = 0;

        if (new_mode != mode) {
            mode = new_mode;
            if (mode == 0) {
                render_scanline();
            } else if (mode == 1) {
                request(0);
                frame_ready = true;
            }
        }
        update_stat();
    }

    // Dessine la ligne `ly` : fond, fenêtre, puis sprites
    void render_scanline() {
        const auto& c = mem->cells;
        const uint8_t lcdc = c[0xFF40];
        const uint8_t scy = c[0xFF42], scx = c[0xFF43];
        const uint8_t bgp = c[0xFF47], obp0 = c[0xFF48], obp1 = c[0xFF49];
        const int wy = c[0xFF4A], wx = c[0xFF4B];

        uint8_t bg_id[WIDTH] = {};  // numéro de couleur (0-3) du fond, pour la priorité des sprites
        uint8_t* row = &framebuffer[ly * WIDTH];

        // --- Fond et fenêtre (LCDC bit 0 = fond actif) ---
        if (lcdc & 0x01) {
            bool window_visible = (lcdc & 0x20) && ly >= wy && wx <= 166;
            bool window_drawn = false;

            for (int x = 0; x < WIDTH; x++) {
                int px, py, map_base;
                if (window_visible && x >= wx - 7) {
                    px = x - (wx - 7);
                    py = window_line;
                    map_base = (lcdc & 0x40) ? 0x9C00 : 0x9800;
                    window_drawn = true;
                } else {
                    px = (x + scx) & 0xFF;
                    py = (ly + scy) & 0xFF;
                    map_base = (lcdc & 0x08) ? 0x9C00 : 0x9800;
                }

                uint8_t tile_id = c[map_base + (py / 8) * 32 + (px / 8)];
                int tile_addr = (lcdc & 0x10) ? 0x8000 + tile_id * 16
                                              : 0x9000 + static_cast<int8_t>(tile_id) * 16;
                int line = (py % 8) * 2;
                uint8_t lo = c[tile_addr + line];
                uint8_t hi = c[tile_addr + line + 1];
                int bit = 7 - (px % 8);
                uint8_t id = static_cast<uint8_t>((((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));

                bg_id[x] = id;
                row[x] = (bgp >> (id * 2)) & 3;
            }
            if (window_drawn) window_line++;
        } else {
            for (int x = 0; x < WIDTH; x++) row[x] = 0;
        }

        // --- Sprites (LCDC bit 1 = sprites actifs) ---
        if (lcdc & 0x02) {
            const int height = (lcdc & 0x04) ? 16 : 8;

            // Au plus 10 sprites par ligne, dans l'ordre de l'OAM
            int sel[10];
            int count = 0;
            for (int i = 0; i < 40 && count < 10; i++) {
                int sy = c[0xFE00 + i * 4] - 16;
                if (ly >= sy && ly < sy + height) sel[count++] = i;
            }

            // Priorité : le sprite le plus à gauche gagne (puis l'indice OAM le plus petit).
            // Tri par insertion, stable : les égalités gardent l'ordre de l'OAM.
            for (int i = 1; i < count; i++) {
                int cur = sel[i];
                int j = i - 1;
                while (j >= 0 && c[0xFE00 + sel[j] * 4 + 1] > c[0xFE00 + cur * 4 + 1]) {
                    sel[j + 1] = sel[j];
                    j--;
                }
                sel[j + 1] = cur;
            }

            bool drawn[WIDTH] = {};
            for (int s = 0; s < count; s++) {
                int base = 0xFE00 + sel[s] * 4;
                int sy = c[base] - 16;
                int sx = c[base + 1] - 8;
                uint8_t tile = c[base + 2];
                uint8_t flags = c[base + 3];

                int row_in = ly - sy;
                if (flags & 0x40) row_in = height - 1 - row_in;  // retournement vertical
                if (height == 16) tile = static_cast<uint8_t>((tile & 0xFE) | (row_in >= 8 ? 1 : 0));

                int addr = 0x8000 + tile * 16 + (row_in % 8) * 2;
                uint8_t lo = c[addr];
                uint8_t hi = c[addr + 1];
                uint8_t palette = (flags & 0x10) ? obp1 : obp0;

                for (int p = 0; p < 8; p++) {
                    int x = sx + p;
                    if (x < 0 || x >= WIDTH || drawn[x]) continue;

                    int bit = (flags & 0x20) ? p : 7 - p;  // retournement horizontal
                    uint8_t id = static_cast<uint8_t>((((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));
                    if (id == 0) continue;  // couleur 0 = transparent

                    drawn[x] = true;
                    if ((flags & 0x80) && bg_id[x] != 0) continue;  // sprite derrière le fond (couleurs 1-3)
                    row[x] = (palette >> (id * 2)) & 3;
                }
            }
        }
    }
};