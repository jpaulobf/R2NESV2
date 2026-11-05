#include "Core/Cartridge/Mappers/Mapper069.h"
#include <algorithm>
#include <iterator>

namespace R2NES::Core
{
    Mapper069::Mapper069(uint16_t p, uint16_t c, bool r, bool b, MirrorMode m)
        : Mapper(p, c), hasRam(r), battery(b), initialMirror(m), mirror(m) { reset(); }

    void Mapper069::reset()
    {
        chr.fill(0);
        prg.fill(0);
        command = control = 0;
        counter = 0;
        irq = false;
        mirror = initialMirror;
        audioRegs.fill(0);
        toneCounter.fill(0);
        toneOutput.fill(0);
        audioSelect = audioDivider = noiseCounter = 0;
        envelopeCounter = 0;
        noiseLfsr = 1;
        envelopeStep = 31;
        envelopeAttack = 0;
        envelopeHolding = false;
    }
    bool Mapper069::cpuMapRead(uint16_t a, uint32_t &m, uint8_t &d)
    {
        if (a < 0x6000)
            return false;
        // Em $6000-$7FFF, o bit 6 seleciona RAM; o bit 7 habilita seu acesso.
        if (a < 0x8000 && (prg[0] & 0x40))
        {
            if (!(prg[0] & 0x80) || !hasRam)
                return false;
            m = 0xffffffff; // A leitura foi atendida pelo próprio mapper.
            d = ram[a & 0x1fff];
            return true;
        }
        // $E000-$FFFF usa o último banco; as demais janelas usam os registradores.
        unsigned bank = a >= 0xe000 ? nPRGBanks * 2 - 1 : a < 0x8000 ? prg[0] & 0x3f
                                                                     : prg[1 + (a - 0x8000) / 0x2000];
        m = (bank & (nPRGBanks * 2 - 1)) * 0x2000 + (a & 0x1fff);
        return true;
    }
    bool Mapper069::cpuMapWrite(uint16_t a, uint32_t &m, uint8_t d, uint32_t)
    {
        if (a >= 0x6000 && a < 0x8000)
        {
            if ((prg[0] & 0xc0) == 0xc0 && hasRam)
            {
                ram[a & 0x1fff] = d;
                m = 0xffffffff;
                return true;
            }
            return false;
        }
        if (a < 0x8000)
            return false;
        m = 0xffffffff;
        // $8000-$9FFF seleciona o comando; $A000-$BFFF grava seu valor.
        if (a < 0xa000)
            command = d & 15;
        else if (a < 0xc000)
        {
            // Comandos 0-7: CHR; 8-11: PRG; 12: espelhamento.
            // Comandos 13-15: controle de IRQ e bytes baixo/alto do contador.
            if (command < 8)
                chr[command] = d;
            else if (command < 12)
                prg[command - 8] = command == 8 ? d : d & 0x3f;
            else if (command == 12)
            {
                const MirrorMode modes[] = {MirrorMode::VERTICAL, MirrorMode::HORIZONTAL, MirrorMode::ONESCREEN_LO, MirrorMode::ONESCREEN_HI};
                mirror = modes[d & 3];
            }
            else if (command == 13)
            {
                control = d & 0x81;
                irq = false;
            }
            else if (command == 14)
                counter = (counter & 0xff00) | d;
            else
                counter = (counter & 0x00ff) | (uint16_t(d) << 8);
        }
        else if (a < 0xe000)
            audioSelect = d;
        else if ((audioSelect & 0xf0) == 0)
            audioWrite(d);
        return true;
    }
    bool Mapper069::ppuMapRead(uint16_t a, uint32_t &m, uint8_t &, uint32_t)
    {
        if (a >= 0x2000)
            return false;
        const unsigned banks = nCHRBanks ? nCHRBanks * 8 : 8;
        m = (chr[a / 0x400] & (banks - 1)) * 0x400 + (a & 0x3ff);
        return true;
    }
    bool Mapper069::ppuMapWrite(uint16_t a, uint32_t &m, uint8_t d, uint32_t t)
    {
        return !nCHRBanks && ppuMapRead(a, m, d, t);
    }
    MirrorMode Mapper069::getMirrorMode() { return mirror; }
    void Mapper069::tick()
    {
        audioTick();
        // Bit 7 faz o contador decrementar; bit 0 permite gerar IRQ no underflow.
        // O contador continua contando mesmo quando a geração de IRQ está desabilitada.
        if (control & 0x80)
        {
            if (counter-- == 0 && (control & 1))
                irq = true;
        }
    }
    void Mapper069::audioWrite(uint8_t value)
    {
        const unsigned reg = audioSelect & 15;
        static constexpr uint8_t masks[16] = {255, 15, 255, 15, 255, 15, 31, 63, 31, 31, 31, 255, 255, 15, 255, 255};
        audioRegs[reg] = value & masks[reg];
        if (reg == 13)
        {
            envelopeStep = 31;
            envelopeAttack = (value & 4) ? 31 : 0;
            envelopeHolding = false;
            envelopeCounter = 0;
        }
    }
    void Mapper069::audioTick()
    {
        audioDivider = (audioDivider + 1) & 31;
        if ((audioDivider & 15) != 0)
            return;
        for (unsigned i = 0; i < 3; ++i)
        {
            const unsigned period = audioRegs[i * 2] | (unsigned(audioRegs[i * 2 + 1]) << 8);
            if (++toneCounter[i] >= std::max(1u, period))
            {
                toneCounter[i] = 0;
                toneOutput[i] ^= 1;
            }
        }
        if (audioDivider == 0 && ++noiseCounter >= std::max(1u, unsigned(audioRegs[6])))
        {
            noiseCounter = 0;
            const uint32_t feedback = (noiseLfsr ^ (noiseLfsr >> 3)) & 1;
            noiseLfsr = (noiseLfsr >> 1) | (feedback << 16);
        }
        const unsigned period = audioRegs[11] | (unsigned(audioRegs[12]) << 8);
        if (++envelopeCounter >= std::max(1u, period))
        {
            envelopeCounter = 0;
            if (!envelopeHolding)
            {
                if (envelopeStep)
                    --envelopeStep;
                else
                {
                    const uint8_t shape = audioRegs[13];
                    if (!(shape & 8))
                    {
                        envelopeAttack = 0;
                        envelopeHolding = true;
                    }
                    else if (shape & 1)
                    {
                        if (shape & 2)
                            envelopeAttack ^= 31;
                        envelopeHolding = true;
                    }
                    else
                    {
                        if (shape & 2)
                            envelopeAttack ^= 31;
                        envelopeStep = 31;
                    }
                }
            }
        }
    }
    float Mapper069::expansionAudio() const
    {
        // O DAC do YM2149 tem passos de aproximadamente 1,5 dB.
        static constexpr float levels[32] = {
            0, 0, 0.00668f, 0.00794f, 0.00944f, 0.01122f, 0.01334f, 0.01585f,
            0.01884f, 0.02239f, 0.02661f, 0.03162f, 0.03758f, 0.04467f, 0.05309f, 0.06310f,
            0.07499f, 0.08913f, 0.10593f, 0.12589f, 0.14962f, 0.17783f, 0.21135f, 0.25119f,
            0.29854f, 0.35481f, 0.42170f, 0.50119f, 0.59566f, 0.70795f, 0.84140f, 1.0f};
        float sum = 0;
        for (unsigned i = 0; i < 3; ++i)
        {
            const bool tone = (audioRegs[7] & (1 << i)) || toneOutput[i];
            const bool noise = (audioRegs[7] & (8 << i)) || (noiseLfsr & 1);
            const uint8_t volume = audioRegs[8 + i];
            const unsigned level = (volume & 16) ? (envelopeStep ^ envelopeAttack) : (volume & 15) ? (volume & 15) * 2 + 1
                                                                                                   : 0;
            if (tone && noise)
                sum += levels[level];
        }
        return sum / 3.0f;
    }
    std::vector<uint8_t> Mapper069::getBatteryData() const
    {
        return battery ? std::vector<uint8_t>(ram.begin(), ram.end()) : std::vector<uint8_t>{};
    }
    bool Mapper069::setBatteryData(const std::vector<uint8_t> &d)
    {
        if (d.size() != (battery ? ram.size() : 0))
            return false;
        if (battery)
            std::copy(d.begin(), d.end(), ram.begin());
        return true;
    }
    void Mapper069::saveState(std::ostream &s)
    {
        s.write(reinterpret_cast<const char *>(chr.data()), chr.size());
        s.write(reinterpret_cast<const char *>(prg.data()), prg.size());
        s.put(command);
        s.put(control);
        s.put(counter & 255);
        s.put(counter >> 8);
        s.put(irq ? 1 : 0);
        s.put(static_cast<uint8_t>(mirror));
        s.put(battery ? 2 : hasRam ? 1
                                   : 0);
        s.write(reinterpret_cast<const char *>(ram.data()), ram.size());
        s.write("S5B1", 4);
        s.write(reinterpret_cast<const char *>(audioRegs.data()), audioRegs.size());
        for (auto value : toneCounter)
        {
            s.put(value & 255);
            s.put(value >> 8);
        }
        for (auto value : toneOutput)
            s.put(value);
        s.put(audioSelect);
        s.put(audioDivider);
        s.put(noiseCounter);
        s.put(envelopeCounter & 255);
        s.put(envelopeCounter >> 8);
        s.put(noiseLfsr & 255);
        s.put((noiseLfsr >> 8) & 255);
        s.put(noiseLfsr >> 16);
        s.put(envelopeStep);
        s.put(envelopeAttack);
        s.put(envelopeHolding ? 1 : 0);
    }
    void Mapper069::loadState(std::istream &s) { loadStateImpl(s, false); }
    void Mapper069::loadLegacyState(std::istream &s) { loadStateImpl(s, true); }
    void Mapper069::loadStateImpl(std::istream &s, bool legacy)
    {
        // Lê em temporários para não alterar o mapper caso o estado seja inválido.
        std::array<uint8_t, 8> c{};
        std::array<uint8_t, 4> p{};
        std::array<uint8_t, 7> v{};
        std::array<uint8_t, 8192> r{};
        s.read(reinterpret_cast<char *>(c.data()), c.size());
        s.read(reinterpret_cast<char *>(p.data()), p.size());
        s.read(reinterpret_cast<char *>(v.data()), v.size());
        s.read(reinterpret_cast<char *>(r.data()), r.size());
        if (!s || v[0] > 15 || (v[1] & ~0x81) || v[4] > 1 ||
            (v[5] != uint8_t(MirrorMode::VERTICAL) && v[5] != uint8_t(MirrorMode::HORIZONTAL) &&
             v[5] != uint8_t(MirrorMode::ONESCREEN_LO) && v[5] != uint8_t(MirrorMode::ONESCREEN_HI)) ||
            v[6] != (battery ? 2 : hasRam ? 1
                                          : 0) ||
            p[1] > 63 || p[2] > 63 || p[3] > 63)
        {
            s.setstate(std::ios::failbit);
            return;
        }
        if (legacy)
        {
            audioRegs.fill(0);
            toneCounter.fill(0);
            toneOutput.fill(0);
            audioSelect = audioDivider = noiseCounter = 0;
            envelopeCounter = 0;
            noiseLfsr = 1;
            envelopeStep = 31;
            envelopeAttack = 0;
            envelopeHolding = false;
            chr = c;
            prg = p;
            command = v[0];
            control = v[1];
            counter = v[2] | (uint16_t(v[3]) << 8);
            irq = v[4];
            mirror = static_cast<MirrorMode>(v[5]);
            ram = r;
            return;
        }
        std::array<uint8_t, 40> audio{};
        s.read(reinterpret_cast<char *>(audio.data()), audio.size());
        const uint32_t lfsr = audio[34] | (uint32_t(audio[35]) << 8) | (uint32_t(audio[36]) << 16);
        if (!s || audio[0] != 'S' || audio[1] != '5' || audio[2] != 'B' || audio[3] != '1' ||
            audio[26] > 1 || audio[27] > 1 || audio[28] > 1 || audio[30] > 31 ||
            audio[31] > 31 || audio[37] > 31 || (audio[38] != 0 && audio[38] != 31) ||
            audio[39] > 1 || !lfsr || lfsr > 0x1ffff)
        {
            s.setstate(std::ios::failbit);
            return;
        }
        std::copy_n(audio.begin() + 4, 16, audioRegs.begin());
        for (unsigned i = 0; i < 3; ++i)
        {
            toneCounter[i] = audio[20 + i * 2] | (uint16_t(audio[21 + i * 2]) << 8);
            toneOutput[i] = audio[26 + i];
        }
        audioSelect = audio[29];
        audioDivider = audio[30];
        noiseCounter = audio[31];
        envelopeCounter = audio[32] | (uint16_t(audio[33]) << 8);
        noiseLfsr = lfsr;
        envelopeStep = audio[37];
        envelopeAttack = audio[38];
        envelopeHolding = audio[39] != 0;
        chr = c;
        prg = p;
        command = v[0];
        control = v[1];
        counter = v[2] | (uint16_t(v[3]) << 8);
        irq = v[4];
        mirror = static_cast<MirrorMode>(v[5]);
        ram = r;
    }
}