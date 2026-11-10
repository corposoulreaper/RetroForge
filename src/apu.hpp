#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "memory.hpp"

// APU de la Game Boy (DMG) : 4 canaux sonores.
//   Canal 1 : onde carrée + balayage de fréquence (sweep)   0xFF10-0xFF14
//   Canal 2 : onde carrée                                    0xFF16-0xFF19
//   Canal 3 : onde échantillonnée (RAM d'onde 0xFF30-0xFF3F)  0xFF1A-0xFF1E
//   Canal 4 : bruit (registre à décalage LFSR)               0xFF20-0xFF23
//   Contrôle : 0xFF24 (volume général), 0xFF25 (panoramique), 0xFF26 (marche/arrêt, état)
//
// Le "frame sequencer" tourne à 512 Hz et cadence la durée (256 Hz), le sweep (128 Hz)
// et l'enveloppe de volume (64 Hz). Les échantillons sont produits à `sample_rate`.
struct APU : ApuHook {
    static constexpr int CPU_HZ = 4194304;

    // Sortie : échantillons stéréo 16 bits entrelacés (gauche, droite, gauche, droite, ...).
    // Le programme principal les vide à chaque image.
    std::vector<int16_t> samples;
    int sample_rate = 44100;
    float master_gain = 0.5f;  // volume de sortie global (1.0 = plein volume, risque de saturation)

    // Registres 0xFF10-0xFF2F (index = adresse - 0xFF10) et RAM d'onde 0xFF30-0xFF3F
    uint8_t nr[0x20] = {};
    uint8_t wave_ram[16] = {};
    bool power = true;  // NR52 bit 7

    struct Channel {
        bool enabled = false;
        int length = 0;      // compteur de durée
        int freq_timer = 0;  // décompte avant le prochain pas de la forme d'onde
        int volume = 0;      // volume courant de l'enveloppe (canaux 1, 2, 4)
        int env_timer = 0;
        int pos = 0;         // canaux 1-2 : pas de l'onde (0-7) ; canal 3 : échantillon (0-31)
    };
    Channel ch[4];

    // Sweep (canal 1)
    int sweep_timer = 0;
    bool sweep_enabled = false;
    int shadow_freq = 0;

    uint16_t lfsr = 0x7FFF;  // canal 4

    int fs_counter = 0;  // frame sequencer : cycles écoulés dans le pas courant
    int fs_step = 0;     // pas courant (0-7)
    int sample_acc = 0;  // accumulateur pour produire un échantillon tous les CPU_HZ / sample_rate cycles

    // Filtre passe-haut (retire la composante continue, comme le condensateur de sortie d'une vraie console)
    float cap_l = 0.0f, cap_r = 0.0f;
    float hp_charge = 0.996f;

    APU() {
        // Valeurs des registres après le boot de la console
        nr[0x00] = 0x80; nr[0x01] = 0xBF; nr[0x02] = 0xF3; nr[0x03] = 0xFF; nr[0x04] = 0xBF;  // NR10-NR14
        nr[0x06] = 0x3F; nr[0x07] = 0x00; nr[0x08] = 0xFF; nr[0x09] = 0xBF;                  // NR21-NR24
        nr[0x0A] = 0x7F; nr[0x0B] = 0xFF; nr[0x0C] = 0x9F; nr[0x0D] = 0xFF; nr[0x0E] = 0xBF;  // NR30-NR34
        nr[0x10] = 0xFF; nr[0x11] = 0x00; nr[0x12] = 0x00; nr[0x13] = 0xBF;                  // NR41-NR44
        nr[0x14] = 0x77; nr[0x15] = 0xF3;                                                    // NR50, NR51
        set_sample_rate(44100);
    }

    void set_sample_rate(int rate) {
        sample_rate = rate;
        hp_charge = static_cast<float>(std::pow(0.999958, static_cast<double>(CPU_HZ) / rate));
    }

    // --- Accès aux registres (appelés par Memory pour 0xFF10-0xFF3F) --------------
    uint8_t reg(uint16_t addr) const { return nr[addr - 0xFF10]; }

    uint8_t apu_read(uint16_t pos) const override {
        if (pos >= 0xFF30) return wave_ram[pos - 0xFF30];
        if (pos == 0xFF26) {
            return static_cast<uint8_t>(0x70 | (power ? 0x80 : 0) | (ch[0].enabled ? 1 : 0) |
                                        (ch[1].enabled ? 2 : 0) | (ch[2].enabled ? 4 : 0) |
                                        (ch[3].enabled ? 8 : 0));
        }
        if (pos > 0xFF26) return 0xFF;  // 0xFF27-0xFF2F : inutilisés
        // Les bits en écriture seule sont lus à 1
        static const uint8_t mask[0x16] = {
            0x80, 0x3F, 0x00, 0xFF, 0xBF,  // NR10-NR14
            0xFF, 0x3F, 0x00, 0xFF, 0xBF,  // (0xFF15 inutilisé), NR21-NR24
            0x7F, 0xFF, 0x9F, 0xFF, 0xBF,  // NR30-NR34
            0xFF, 0xFF, 0x00, 0x00, 0xBF,  // (0xFF1F inutilisé), NR41-NR44
            0x00, 0x00                     // NR50, NR51
        };
        return static_cast<uint8_t>(nr[pos - 0xFF10] | mask[pos - 0xFF10]);
    }

    void apu_write(uint16_t pos, uint8_t v) override {
        if (pos >= 0xFF30) {
            wave_ram[pos - 0xFF30] = v;
            return;
        }
        if (pos == 0xFF26) {
            write_nr52(v);
            return;
        }
        if (!power || pos > 0xFF26) return;  // APU éteint : seuls NR52 et la RAM d'onde sont écrivables

        nr[pos - 0xFF10] = v;
        switch (pos) {
            case 0xFF11: ch[0].length = 64 - (v & 0x3F); break;
            case 0xFF16: ch[1].length = 64 - (v & 0x3F); break;
            case 0xFF1B: ch[2].length = 256 - v; break;
            case 0xFF20: ch[3].length = 64 - (v & 0x3F); break;

            // Écrire un DAC éteint coupe le canal
            case 0xFF12: if (!dac_on(0)) ch[0].enabled = false; break;
            case 0xFF17: if (!dac_on(1)) ch[1].enabled = false; break;
            case 0xFF1A: if (!dac_on(2)) ch[2].enabled = false; break;
            case 0xFF21: if (!dac_on(3)) ch[3].enabled = false; break;

            // Bit 7 de NRx4 : déclenchement du son
            case 0xFF14: if (v & 0x80) trigger(0); break;
            case 0xFF19: if (v & 0x80) trigger(1); break;
            case 0xFF1E: if (v & 0x80) trigger(2); break;
            case 0xFF23: if (v & 0x80) trigger(3); break;
            default: break;
        }
    }

    void write_nr52(uint8_t v) {
        bool on = (v & 0x80) != 0;
        if (on && !power) {
            power = true;
            fs_step = 0;
        } else if (!on && power) {
            for (int i = 0; i <= 0x15; i++) nr[i] = 0;  // NR10-NR51 remis à 0
            for (auto& c : ch) c.enabled = false;
            power = false;
        }
    }

    // --- Utilitaires -------------------------------------------------------
    bool dac_on(int c) const {
        switch (c) {
            case 0:  return (reg(0xFF12) & 0xF8) != 0;
            case 1:  return (reg(0xFF17) & 0xF8) != 0;
            case 2:  return (reg(0xFF1A) & 0x80) != 0;
            default: return (reg(0xFF21) & 0xF8) != 0;
        }
    }

    uint8_t env_reg(int c) const { return c == 0 ? reg(0xFF12) : (c == 1 ? reg(0xFF17) : reg(0xFF21)); }

    bool length_enabled(int c) const {
        static const uint16_t addr[4] = {0xFF14, 0xFF19, 0xFF1E, 0xFF23};
        return (reg(addr[c]) & 0x40) != 0;
    }

    int freq_of(int c) const {  // fréquence 11 bits des canaux 1, 2, 3
        static const uint16_t lo[3] = {0xFF13, 0xFF18, 0xFF1D};
        return reg(lo[c]) | ((reg(static_cast<uint16_t>(lo[c] + 1)) & 7) << 8);
    }

    // Durée (en T-cycles) d'un pas de la forme d'onde
    int period(int c) const {
        switch (c) {
            case 0:
            case 1:  return (2048 - freq_of(c)) * 4;
            case 2:  return (2048 - freq_of(2)) * 2;
            default: {
                uint8_t r = reg(0xFF22);
                int code = r & 7;
                int divisor = code ? code * 16 : 8;
                return divisor << (r >> 4);
            }
        }
    }

    // --- Déclenchement d'un canal ---------------------------------------------
    void trigger(int c) {
        Channel& k = ch[c];
        k.enabled = true;
        if (k.length == 0) k.length = (c == 2) ? 256 : 64;
        k.freq_timer = period(c);

        if (c != 2) {  // enveloppe de volume
            uint8_t r = env_reg(c);
            k.volume = r >> 4;
            k.env_timer = (r & 7) ? (r & 7) : 8;
        }
        if (c == 0) {  // sweep
            uint8_t s = reg(0xFF10);
            int pace = (s >> 4) & 7;
            int shift = s & 7;
            shadow_freq = freq_of(0);
            sweep_timer = pace ? pace : 8;
            sweep_enabled = (pace != 0 || shift != 0);
            if (shift != 0) sweep_calc();  // contrôle de débordement immédiat
        }
        if (c == 2) k.pos = 0;
        if (c == 3) lfsr = 0x7FFF;

        if (!dac_on(c)) k.enabled = false;
    }

    int sweep_calc() {
        uint8_t s = reg(0xFF10);
        int delta = shadow_freq >> (s & 7);
        int nf = (s & 0x08) ? shadow_freq - delta : shadow_freq + delta;
        if (nf > 2047) ch[0].enabled = false;  // débordement : le canal 1 se coupe
        return nf;
    }

    // --- Frame sequencer -----------------------------------------------------
    void clock_length() {
        for (int c = 0; c < 4; c++) {
            if (length_enabled(c) && ch[c].length > 0) {
                if (--ch[c].length == 0) ch[c].enabled = false;
            }
        }
    }

    void clock_sweep() {
        if (--sweep_timer > 0) return;
        uint8_t s = reg(0xFF10);
        int pace = (s >> 4) & 7;
        sweep_timer = pace ? pace : 8;
        if (sweep_enabled && pace != 0) {
            int nf = sweep_calc();
            if (nf <= 2047 && (s & 7) != 0) {
                shadow_freq = nf;
                nr[0xFF13 - 0xFF10] = static_cast<uint8_t>(nf & 0xFF);
                nr[0xFF14 - 0xFF10] = static_cast<uint8_t>((nr[0xFF14 - 0xFF10] & 0xF8) | (nf >> 8));
                sweep_calc();  // second contrôle de débordement
            }
        }
    }

    void clock_envelope() {
        for (int c : {0, 1, 3}) {
            uint8_t r = env_reg(c);
            int pace = r & 7;
            if (pace == 0 || !ch[c].enabled) continue;  // pace 0 : enveloppe inactive
            if (--ch[c].env_timer <= 0) {
                ch[c].env_timer = pace;
                if (r & 0x08) { if (ch[c].volume < 15) ch[c].volume++; }
                else          { if (ch[c].volume > 0)  ch[c].volume--; }
            }
        }
    }

    void clock_frame_sequencer() {
        switch (fs_step) {
            case 0: case 4: clock_length(); break;
            case 2: case 6: clock_length(); clock_sweep(); break;
            case 7: clock_envelope(); break;
            default: break;
        }
        fs_step = (fs_step + 1) & 7;
    }

    // --- Avance d'un T-cycle -----------------------------------------------------
    void step_cycle() {
        if (power) {
            if (++fs_counter >= 8192) {  // 512 Hz
                fs_counter = 0;
                clock_frame_sequencer();
            }
            for (int c = 0; c < 2; c++) {  // canaux carrés
                if (ch[c].enabled && --ch[c].freq_timer <= 0) {
                    ch[c].freq_timer += period(c);
                    ch[c].pos = (ch[c].pos + 1) & 7;
                }
            }
            if (ch[2].enabled && --ch[2].freq_timer <= 0) {  // onde
                ch[2].freq_timer += period(2);
                ch[2].pos = (ch[2].pos + 1) & 31;
            }
            if (ch[3].enabled && --ch[3].freq_timer <= 0) {  // bruit
                ch[3].freq_timer += period(3);
                int x = (lfsr & 1) ^ ((lfsr >> 1) & 1);
                lfsr = static_cast<uint16_t>((lfsr >> 1) | (x << 14));
                if (reg(0xFF22) & 0x08) lfsr = static_cast<uint16_t>((lfsr & ~0x40) | (x << 6));  // mode 7 bits
            }
        }

        sample_acc += sample_rate;
        if (sample_acc >= CPU_HZ) {
            sample_acc -= CPU_HZ;
            generate_sample();
        }
    }

    void tick(int cycles) {
        for (int i = 0; i < cycles; i++) step_cycle();
    }

    // --- Mixage ------------------------------------------------------------------
    // Valeur numérique (0-15) produite par un canal à cet instant
    int digital(int c) const {
        if (!ch[c].enabled) return 0;
        switch (c) {
            case 0:
            case 1: {
                static const uint8_t duty_patterns[4] = {0x01, 0x81, 0x87, 0x7E};  // 12,5 %, 25 %, 50 %, 75 %
                uint8_t duty = (reg(c == 0 ? 0xFF11 : 0xFF16) >> 6) & 3;
                return ((duty_patterns[duty] >> (7 - ch[c].pos)) & 1) ? ch[c].volume : 0;
            }
            case 2: {
                uint8_t b = wave_ram[ch[2].pos >> 1];
                int s = (ch[2].pos & 1) ? (b & 0x0F) : (b >> 4);
                static const int shift[4] = {4, 0, 1, 2};  // muet, 100 %, 50 %, 25 %
                return s >> shift[(reg(0xFF1C) >> 5) & 3];
            }
            default:
                return (lfsr & 1) ? 0 : ch[3].volume;
        }
    }

    void generate_sample() {
        float left = 0.0f, right = 0.0f;
        if (power) {
            uint8_t pan = reg(0xFF25);
            uint8_t vol = reg(0xFF24);
            for (int c = 0; c < 4; c++) {
                if (!dac_on(c)) continue;                          // DAC éteint : silence
                float a = digital(c) / 7.5f - 1.0f;                 // 0..15 -> -1..+1
                if (pan & (0x10 << c)) left += a;
                if (pan & (0x01 << c)) right += a;
            }
            left  = left  / 4.0f * (((vol >> 4) & 7) + 1) / 8.0f * master_gain;
            right = right / 4.0f * ((vol & 7) + 1) / 8.0f * master_gain;
        }

        // Filtre passe-haut
        float out_l = left - cap_l;
        cap_l = left - out_l * hp_charge;
        float out_r = right - cap_r;
        cap_r = right - out_r * hp_charge;

        samples.push_back(to_pcm(out_l));
        samples.push_back(to_pcm(out_r));
    }

    static int16_t to_pcm(float x) {
        x = std::max(-1.0f, std::min(1.0f, x));
        return static_cast<int16_t>(x * 32767.0f);
    }
};