#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct Memory {
    std::array<uint8_t, 0x10000> cells{};

    // Texte envoyé par la ROM sur le port série (ROMs de test de Blargg)
    std::string serial_output;

    // Compteur interne du timer : DIV (0xFF04) en est l'octet haut.
    // Il avance de 1 à chaque T-cycle (4 194 304 fois par seconde).
    uint16_t div_counter = 0xABCC;

    // Manette : 0 = touche pressée, 1 = relâchée (4 bits utiles chacun)
    uint8_t joy_dpad = 0x0F;     // bit 0 Droite, 1 Gauche, 2 Haut, 3 Bas
    uint8_t joy_buttons = 0x0F;  // bit 0 A, 1 B, 2 Select, 3 Start

    // --- Cartouche -------------------------------------------------------
    std::vector<uint8_t> rom;      // contenu complet de la ROM (multiple de 16 Ko)
    std::vector<uint8_t> ext_ram;  // RAM de la cartouche (0xA000-0xBFFF), vide si aucune
    int mbc = 0;                   // 0 = sans MBC, 1 = MBC1, 3 = MBC3 (sans horloge), 5 = MBC5
    size_t rom_banks = 2;          // nombre de banques de 16 Ko
    size_t rom_bank = 1;           // banque visible en 0x4000-0x7FFF
    size_t ram_bank = 0;           // banque de RAM visible en 0xA000-0xBFFF
    bool ram_enabled = false;
    int mbc1_low = 1;              // MBC1 : 5 bits bas du numéro de banque (jamais 0)
    int mbc1_high = 0;             // MBC1 : 2 bits hauts
    int mbc1_mode = 0;             // MBC1 : mode de banking (0 ou 1)

    // Sauvegarde : la RAM des cartouches à pile est stockée dans un fichier .sav à côté de la ROM
    std::string save_path;
    bool has_battery = false;
    bool ram_dirty = false;        // la RAM a été modifiée depuis la dernière sauvegarde

    // Valeurs des registres après le boot de la console (on n'émule pas la boot ROM)
    Memory() {
        cells[0xFF00] = 0x30;  // JOYP : aucune ligne sélectionnée
        cells[0xFF0F] = 0xE1;  // IF
        cells[0xFF40] = 0x91;  // LCDC : écran allumé, fond actif, tuiles en 0x8000
        cells[0xFF41] = 0x85;  // STAT
        cells[0xFF47] = 0xFC;  // BGP
        cells[0xFF48] = 0xFF;  // OBP0
        cells[0xFF49] = 0xFF;  // OBP1
    }

    // --- Lecture ---------------------------------------------------------
    uint8_t read(uint16_t pos) const {
        if (pos < 0x4000) return rom.empty() ? 0xFF : rom[pos];
        if (pos < 0x8000) {
            if (rom.empty()) return 0xFF;
            return rom[(rom_bank % rom_banks) * 0x4000 + (pos - 0x4000)];
        }
        if (pos >= 0xA000 && pos < 0xC000 && !ext_ram.empty()) {  // RAM de la cartouche
            if (!ram_enabled) return 0xFF;
            if (mbc == 3 && ram_bank >= 8) return 0x00;  // registres d'horloge MBC3 : non gérés
            return ext_ram[(ram_bank * 0x2000 + (pos - 0xA000)) % ext_ram.size()];
        }
        if (pos >= 0xE000 && pos < 0xFE00) pos -= 0x2000;  // Echo RAM : miroir de 0xC000-0xDDFF

        switch (pos) {
            case 0xFF00: {  // JOYP : les lignes sélectionnées (bits 4-5 à 0) donnent l'état des touches
                uint8_t select = cells[0xFF00] & 0x30;
                uint8_t low = 0x0F;
                if (!(select & 0x10)) low &= joy_dpad;
                if (!(select & 0x20)) low &= joy_buttons;
                return 0xC0 | select | low;
            }
            case 0xFF04: return static_cast<uint8_t>(div_counter >> 8);  // DIV
            case 0xFF07: return cells[pos] | 0xF8;                       // TAC (3 bits utiles)
            case 0xFF0F: return cells[pos] | 0xE0;                       // IF (5 bits utiles)
            default:     return cells[pos];
        }
    }

    // --- Écriture --------------------------------------------------------
    void write(uint16_t pos, uint8_t v) {
        // Écrire dans la zone ROM ne modifie pas la ROM : ça configure le MBC
        if (pos < 0x8000) {
            mbc_write(pos, v);
            return;
        }
        if (pos >= 0xA000 && pos < 0xC000 && !ext_ram.empty()) {  // RAM de la cartouche
            if (ram_enabled && !(mbc == 3 && ram_bank >= 8)) {
                ext_ram[(ram_bank * 0x2000 + (pos - 0xA000)) % ext_ram.size()] = v;
                ram_dirty = true;
            }
            return;
        }
        if (pos >= 0xE000 && pos < 0xFE00) pos -= 0x2000;  // Echo RAM

        switch (pos) {
            case 0xFF00:  // JOYP : seuls les bits 4-5 (choix des lignes) sont écrivables
                cells[pos] = v & 0x30;
                return;
            case 0xFF04: {  // écrire dans DIV remet le compteur à 0
                bool before = timer_signal();
                div_counter = 0;
                if (before) increment_tima();  // le signal retombe : front descendant
                return;
            }
            case 0xFF07: {  // TAC : changer la fréquence peut aussi produire un front descendant
                bool before = timer_signal();
                cells[pos] = static_cast<uint8_t>(v & 0x07);
                if (before && !timer_signal()) increment_tima();
                return;
            }
            case 0xFF02:  // Port série : la ROM met un caractère en 0xFF01, puis écrit 0x81 ici
                cells[pos] = v;
                if (v == 0x81) {
                    char ch = static_cast<char>(cells[0xFF01]);
                    serial_output += ch;
                    std::cout << ch << std::flush;
                }
                return;
            case 0xFF41:  // STAT : les 3 bits bas (mode, coïncidence) sont gérés par le PPU
                cells[pos] = static_cast<uint8_t>((cells[pos] & 0x07) | (v & 0x78) | 0x80);
                return;
            case 0xFF44:  // LY : lecture seule
                return;
            case 0xFF46: {  // DMA : copie 160 octets de (v << 8) vers l'OAM (0xFE00-0xFE9F)
                uint16_t src = static_cast<uint16_t>(v << 8);
                for (int i = 0; i < 0xA0; i++) {
                    cells[0xFE00 + i] = read(static_cast<uint16_t>(src + i));
                }
                cells[pos] = v;
                return;
            }
            default:
                break;
        }
        cells[pos] = v;
    }

    // --- MBC : registres de banking (écritures dans 0x0000-0x7FFF) ---------
    void mbc_write(uint16_t pos, uint8_t v) {
        switch (mbc) {
            case 1:  // MBC1
                if (pos < 0x2000)      ram_enabled = (v & 0x0F) == 0x0A;
                else if (pos < 0x4000) { mbc1_low = v & 0x1F; if (mbc1_low == 0) mbc1_low = 1; }
                else if (pos < 0x6000) mbc1_high = v & 0x03;
                else                   mbc1_mode = v & 1;
                rom_bank = (static_cast<size_t>(mbc1_high) << 5) | static_cast<size_t>(mbc1_low);
                ram_bank = mbc1_mode ? static_cast<size_t>(mbc1_high) : 0;
                break;
            case 3:  // MBC3 (sans horloge temps réel)
                if (pos < 0x2000)      ram_enabled = (v & 0x0F) == 0x0A;
                else if (pos < 0x4000) { rom_bank = v & 0x7F; if (rom_bank == 0) rom_bank = 1; }
                else if (pos < 0x6000) ram_bank = v & 0x0F;  // 0-3 : RAM, 8-12 : horloge (non gérée)
                break;
            case 5:  // MBC5
                if (pos < 0x2000)      ram_enabled = (v & 0x0F) == 0x0A;
                else if (pos < 0x3000) rom_bank = (rom_bank & 0x100) | v;
                else if (pos < 0x4000) rom_bank = (rom_bank & 0x0FF) | (static_cast<size_t>(v & 1) << 8);
                else if (pos < 0x6000) ram_bank = v & 0x0F;
                break;
            default:  // sans MBC : rien à configurer
                break;
        }
    }

    // --- Manette ---------------------------------------------------------
    // dpad = true pour la croix, false pour A/B/Select/Start. bit : voir joy_dpad / joy_buttons.
    void set_key(bool dpad, int bit, bool pressed) {
        uint8_t& reg = dpad ? joy_dpad : joy_buttons;
        bool was_released = (reg & (1 << bit)) != 0;
        if (pressed) reg = static_cast<uint8_t>(reg & ~(1 << bit));
        else         reg = static_cast<uint8_t>(reg | (1 << bit));
        if (pressed && was_released) {
            cells[0xFF0F] = static_cast<uint8_t>(cells[0xFF0F] | 0x10);  // interruption Joypad
        }
    }

    // --- Timer -----------------------------------------------------------
    // Fait avancer le timer de `cycles` T-cycles (à appeler après chaque instruction)
    void tick(int cycles) {
        for (int i = 0; i < cycles; i++) {
            bool before = timer_signal();
            div_counter++;
            if (before && !timer_signal()) increment_tima();
        }
    }

    // Signal du timer : TAC bit 2 (activation) ET un bit de div_counter choisi par TAC bits 0-1.
    // TIMA s'incrémente quand ce signal passe de 1 à 0.
    bool timer_signal() const {
        static constexpr uint16_t masks[4] = {0x0200, 0x0008, 0x0020, 0x0080};  // 4096/262144/65536/16384 Hz
        uint8_t tac = cells[0xFF07];
        return (tac & 0x04) && (div_counter & masks[tac & 3]);
    }

    void increment_tima() {
        if (cells[0xFF05] == 0xFF) {
            cells[0xFF05] = cells[0xFF06];                           // TIMA = TMA
            cells[0xFF0F] = static_cast<uint8_t>(cells[0xFF0F] | 0x04);  // demande l'interruption Timer
        } else {
            cells[0xFF05]++;
        }
    }

    // --- Chargement de la ROM ---------------------------------------------
    // Lit le fichier, repère le type de cartouche dans l'en-tête (octet 0x147) et prépare le MBC.
    bool load_rom(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Fichier introuvable ou illisible : " << path << "\n";
            return false;
        }

        std::streamsize size = file.tellg();
        if (size < 0x150 || size > 8 * 1024 * 1024) {
            std::cerr << "Taille de ROM invalide : " << size << " octets (attendu : 336 octets a 8 Mo). "
                      << "Verifie que c'est bien une ROM Game Boy (.gb), pas Game Boy Advance (.gba).\n";
            return false;
        }

        rom.assign(static_cast<size_t>(size), 0xFF);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(rom.data()), size);

        // Complète à un multiple de 16 Ko (32 Ko minimum) pour que les banques soient entières
        size_t padded = (static_cast<size_t>(size) + 0x3FFF) / 0x4000 * 0x4000;
        if (padded < 0x8000) padded = 0x8000;
        rom.resize(padded, 0xFF);
        rom_banks = rom.size() / 0x4000;

        // Type de cartouche (octet 0x147)
        uint8_t type = rom[0x147];
        if (type == 0x00 || type == 0x08 || type == 0x09)      mbc = 0;
        else if (type >= 0x01 && type <= 0x03)                 mbc = 1;
        else if (type >= 0x0F && type <= 0x13)                 mbc = 3;
        else if (type >= 0x19 && type <= 0x1E)                 mbc = 5;
        else {
            std::cerr << "Type de cartouche non supporte : 0x" << std::hex << static_cast<int>(type)
                      << std::dec << " (supportes : sans MBC, MBC1, MBC3 sans horloge, MBC5)\n";
            return false;
        }

        // Taille de la RAM de la cartouche (octet 0x149)
        size_t ram_size = 0;
        switch (rom[0x149]) {
            case 0x01: ram_size = 0x0800; break;
            case 0x02: ram_size = 0x2000; break;
            case 0x03: ram_size = 0x8000; break;
            case 0x04: ram_size = 0x20000; break;
            case 0x05: ram_size = 0x10000; break;
            default:   ram_size = 0; break;
        }
        ext_ram.assign(ram_size, 0);

        // Cartouche à pile de sauvegarde (types 0x03, 0x09, 0x0F, 0x10, 0x13, 0x1B, 0x1E) ?
        has_battery = !ext_ram.empty() &&
                      (type == 0x03 || type == 0x09 || type == 0x0F || type == 0x10 ||
                       type == 0x13 || type == 0x1B || type == 0x1E);

        // Fichier de sauvegarde : même nom que la ROM, extension .sav
        size_t dot = path.find_last_of('.');
        size_t slash = path.find_last_of("/\\");
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
            save_path = path.substr(0, dot) + ".sav";
        } else {
            save_path = path + ".sav";
        }
        ram_dirty = false;
        if (has_battery) load_save();

        rom_bank = 1;
        ram_bank = 0;
        ram_enabled = false;
        mbc1_low = 1;
        mbc1_high = 0;
        mbc1_mode = 0;
        return true;
    }

    // --- Sauvegarde ---------------------------------------------------------
    // Charge la sauvegarde existante, s'il y en a une
    void load_save() {
        std::ifstream f(save_path, std::ios::binary);
        if (!f) return;
        f.read(reinterpret_cast<char*>(ext_ram.data()), static_cast<std::streamsize>(ext_ram.size()));
        std::cerr << "Sauvegarde chargee : " << save_path << "\n";
    }

    // Écrit la RAM de la cartouche dans le fichier .sav (sans effet si la cartouche n'a pas de pile)
    bool save_ram() {
        if (!has_battery) return true;
        std::ofstream f(save_path, std::ios::binary | std::ios::trunc);
        if (!f) {
            std::cerr << "Impossible d'ecrire la sauvegarde : " << save_path << "\n";
            return false;
        }
        f.write(reinterpret_cast<const char*>(ext_ram.data()), static_cast<std::streamsize>(ext_ram.size()));
        ram_dirty = false;
        return true;
    }
};