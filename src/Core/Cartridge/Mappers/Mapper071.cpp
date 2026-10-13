#include "Core/Cartridge/Mappers/Mapper071.h"

namespace R2NES::Core
{
    Mapper071::Mapper071(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror,
                         uint8_t submapper, bool legacyMirroringDetection)
        : Mapper(prgBanks, chrBanks),
          singleScreenMirroring(submapper == 1),
          autoDetectMirroring(submapper == 0 && legacyMirroringDetection),
          initialMirrorMode(singleScreenMirroring ? MirrorMode::ONESCREEN_LO : mirror),
          mirrorMode(initialMirrorMode)
    {
        reset();
    }

    bool Mapper071::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr < 0x8000 || nPRGBanks == 0)
            return false;

        // $8000-$BFFF: banco selecionável; $C000-$FFFF: último banco fixo.
        const uint8_t bank = addr < 0xC000 ? nPRGBankSelect : nPRGBanks - 1;
        mapped_addr = static_cast<uint32_t>(bank) * 0x4000 + (addr & 0x3FFF);
        return true;
    }

    bool Mapper071::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr < 0x8000)
            return false;

        if (addr >= 0xC000)
        {
            // BF9093 usa quatro bits; BF9097 (Fire Hawk) usa três.
            const uint8_t bankMask = singleScreenMirroring ? 0x07 : 0x0F;
            nPRGBankSelect = nPRGBanks > 0 ? (data & bankMask) % nPRGBanks : 0;
        }
        else if (addr <= 0x9FFF &&
                 (singleScreenMirroring || (autoDetectMirroring && addr >= 0x9000)))
        {
            // iNES antigo: ignora $8000 para preservar H/V em outros jogos.
            // NES 2.0 submapper 1: todo o intervalo $8000-$9FFF controla CIRAM A10.
            mirrorMode = (data & 0x10) ? MirrorMode::ONESCREEN_HI : MirrorMode::ONESCREEN_LO;
        }

        mapped_addr = 0xFFFFFFFF; // Escrita consumida pelo mapper, sem alterar PRG-ROM.
        return true;
    }

    bool Mapper071::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr >= 0x2000)
            return false;

        mapped_addr = addr;
        return true;
    }

    bool Mapper071::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr >= 0x2000 || nCHRBanks != 0)
            return false;

        // CHR-RAM de 8 KiB sem troca de bancos.
        mapped_addr = addr;
        return true;
    }

    MirrorMode Mapper071::getMirrorMode() { return mirrorMode; }

    void Mapper071::reset()
    {
        nPRGBankSelect = 0;
        mirrorMode = initialMirrorMode;
    }

    void Mapper071::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        os.write(reinterpret_cast<const char *>(&mirrorMode), sizeof(mirrorMode));
    }

    void Mapper071::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
        is.read(reinterpret_cast<char *>(&mirrorMode), sizeof(mirrorMode));
    }
}