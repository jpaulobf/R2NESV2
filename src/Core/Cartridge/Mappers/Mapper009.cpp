#include "Core/Cartridge/Mappers/Mapper009.h"
#include <array>
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper009::supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror)
    {
        // MMC2: 4 bits de PRG em bancos de 8 KiB e 5 de CHR em bancos de 4 KiB.
        // A janela fixa de 24 KiB exige pelo menos 32 KiB de PRG física.
        return prgBanks >= 2 && prgBanks <= 8 && (prgBanks & (prgBanks - 1)) == 0 &&
               chrBanks >= 1 && chrBanks <= 16 && (chrBanks & (chrBanks - 1)) == 0 &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
    }

    Mapper009::Mapper009(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror)
        : Mapper(prgBanks, chrBanks), mirrorMode(mirror), ogMirrorMode(mirror)
    {
        if (!supports(prgBanks, chrBanks, mirror))
            throw std::invalid_argument("Unsupported Mapper 9 memory configuration");
        reset();
    }

    Mapper009::~Mapper009() {}

    void Mapper009::reset()
    {
        nPRGBankSelect = 0;
        nCHRBankSelect_0_FD = nCHRBankSelect_0_FE = 0;
        nCHRBankSelect_1_FD = nCHRBankSelect_1_FE = 0;
        latch0 = latch1 = LatchState::FE;
        mirrorMode = ogMirrorMode;
    }

    bool Mapper009::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        // PxROM não possui PRG-RAM; $6000-$7FFF permanece em barramento aberto.
        if (addr < 0x8000)
            return false;
        const uint32_t bankCount = nPRGBanks * 2;
        const uint32_t slot = (addr - 0x8000) / 0x2000;
        const uint32_t bank = slot == 0 ? nPRGBankSelect & (bankCount - 1) : bankCount - 4 + slot;
        mapped_addr = bank * 0x2000 + (addr & 0x1FFF);
        return true;
    }

    bool Mapper009::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (addr < 0xA000)
            return false;
        switch (addr & 0xF000)
        {
        case 0xA000: nPRGBankSelect = data & 0x0F; break;
        case 0xB000: nCHRBankSelect_0_FD = data & 0x1F; break;
        case 0xC000: nCHRBankSelect_0_FE = data & 0x1F; break;
        case 0xD000: nCHRBankSelect_1_FD = data & 0x1F; break;
        case 0xE000: nCHRBankSelect_1_FE = data & 0x1F; break;
        case 0xF000: mirrorMode = (data & 1) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL; break;
        }
        mapped_addr = 0xFFFFFFFF; // Registrador atendido; nunca escreve na PRG-ROM.
        return true;
    }

    bool Mapper009::ppuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (addr > 0x1FFF)
            return false;
        const uint8_t bank = addr < 0x1000
            ? (latch0 == LatchState::FD ? nCHRBankSelect_0_FD : nCHRBankSelect_0_FE)
            : (latch1 == LatchState::FD ? nCHRBankSelect_1_FD : nCHRBankSelect_1_FE);
        mapped_addr = (bank & (nCHRBanks * 2 - 1)) * 0x1000 + (addr & 0x0FFF);
        return true;
    }

    bool Mapper009::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
    {
        if (!ppuMapPeek(addr, mapped_addr, data, systemClockCounter))
            return false;
        // Calcula a leitura com o latch anterior. A mudança vale para a próxima.
        // No MMC2, a metade inferior compara endereços exatos; o MMC4 difere aqui.
        if (addr == 0x0FD8) latch0 = LatchState::FD;
        else if (addr == 0x0FE8) latch0 = LatchState::FE;
        else if (addr >= 0x1FD8 && addr <= 0x1FDF) latch1 = LatchState::FD;
        else if (addr >= 0x1FE8 && addr <= 0x1FEF) latch1 = LatchState::FE;
        return true;
    }

    bool Mapper009::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        // A placa emulada tem CHR-ROM: escritas não alteram memória nem latches.
        return false;
    }

    MirrorMode Mapper009::getMirrorMode() { return mirrorMode; }

    void Mapper009::saveState(std::ostream &os)
    {
        const std::array<uint8_t, 7> registers{{nPRGBankSelect, nCHRBankSelect_0_FD, nCHRBankSelect_0_FE,
            nCHRBankSelect_1_FD, nCHRBankSelect_1_FE, static_cast<uint8_t>(latch0), static_cast<uint8_t>(latch1)}};
        os.write(reinterpret_cast<const char *>(registers.data()), registers.size());
        for (MirrorMode mode : {mirrorMode, ogMirrorMode})
            for (unsigned shift = 0; shift < 32; shift += 8)
                os.put(static_cast<char>((static_cast<uint32_t>(mode) >> shift) & 0xFF));
        // Mantém os 8207 bytes do formato Windows anterior. Estes 8 KiB eram
        // RAM fictícia; agora são preenchimento reservado, sem mapeamento na CPU.
        const std::array<char, 8192> padding{};
        os.write(padding.data(), padding.size());
    }

    void Mapper009::loadState(std::istream &is)
    {
        std::array<uint8_t, 7> registers{};
        std::array<uint32_t, 2> mirrors{};
        std::array<char, 8192> padding{};
        is.read(reinterpret_cast<char *>(registers.data()), registers.size());
        for (auto &mode : mirrors)
            for (unsigned shift = 0; shift < 32; shift += 8)
            {
                const int byte = is.get();
                if (byte == std::char_traits<char>::eof()) return;
                mode |= static_cast<uint32_t>(static_cast<uint8_t>(byte)) << shift;
            }
        is.read(padding.data(), padding.size());
        if (!is || registers[0] > 15 || registers[1] > 31 || registers[2] > 31 ||
            registers[3] > 31 || registers[4] > 31 || registers[5] > 1 || registers[6] > 1 ||
            mirrors[0] > static_cast<uint32_t>(MirrorMode::VERTICAL) ||
            mirrors[1] != static_cast<uint32_t>(ogMirrorMode))
        {
            is.setstate(std::ios::failbit);
            return;
        }
        // Os registradores retêm seus bits mesmo se linhas físicas estiverem ausentes.
        // Valida o payload completo antes de aplicar qualquer alteração ao mapper.
        nPRGBankSelect = registers[0];
        nCHRBankSelect_0_FD = registers[1]; nCHRBankSelect_0_FE = registers[2];
        nCHRBankSelect_1_FD = registers[3]; nCHRBankSelect_1_FE = registers[4];
        latch0 = static_cast<LatchState>(registers[5]); latch1 = static_cast<LatchState>(registers[6]);
        mirrorMode = static_cast<MirrorMode>(mirrors[0]);
    }
}
