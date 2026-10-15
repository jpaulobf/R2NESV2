#include "Core/Cartridge/Mappers/Mapper004.h"
#include <algorithm>
#include <stdexcept>

namespace R2NES::Core
{
    bool Mapper004::supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, const Configuration &config)
    {
        // Capacidades em potências de dois permitem espelhar bancos pelas
        // linhas de endereço realmente conectadas. Rejeitar zero PRG também
        // evita divisão por zero nos cálculos de bancos.
        const uint64_t ramSize = uint64_t(config.prgRamSize) + config.prgNvRamSize;
        const bool ramValid = config.submapper == 1 ? ramSize == 1024 : (ramSize == 0 || ramSize == 8192);
        return prgBanks != 0 && prgBanks <= 32 && (prgBanks & (prgBanks - 1)) == 0 &&
               chrBanks <= (config.extendedChr ? 64 : 32) &&
               (chrBanks == 0 || (chrBanks & (chrBanks - 1)) == 0) && config.submapper <= 5 && ramValid &&
               !(config.prgRamSize && config.prgNvRamSize) &&
               (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL || mirror == MirrorMode::FOUR_SCREEN);
    }

    Mapper004::Mapper004(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror)
        : Mapper004(prgBanks, chrBanks, mirror, Configuration{}) {}

    Mapper004::Mapper004(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror, const Configuration &config)
        : Mapper(prgBanks, chrBanks), configuration(config)
    {
        if (!supports(prgBanks, chrBanks, mirror, config))
            throw std::invalid_argument("Unsupported Mapper 4 memory/submapper configuration");
        mirrorMode = mirror;
        ogMirrorMode = mirror; // Armazena o modo de espelhamento original para reset
        for (int i = 0; i < 8192; i++)
            vPRGRAM[i] = 0x00;
        reset();
    }

    Mapper004::~Mapper004() {}

    void Mapper004::loadTrainer(const uint8_t *data)
    {
        // Instala o trainer sem alterar as permissões elétricas do MMC6.
        const unsigned offset = configuration.submapper == 1 ? 0 : 0x1000;
        std::copy_n(data, 512, vPRGRAM + offset);
    }

    void Mapper004::reset()
    {
        // Inicialização determinística do emulador. Não limpa PRG-RAM nem
        // a RAM adicional de nametables, que pertencem ao cartucho.
        nTargetRegister = 0;
        bPRGBankMode = false;
        bCHRInversion = false;
        mirrorMode = ogMirrorMode;

        bIRQEnabled = false;
        bIRQActive = false;
        bIRQReload = false;
        nIRQLatch = 0x00;
        nIRQCounter = 0x00;
        bPRGRAMEnabled = true;
        bPRGRAMWriteProtected = false;
        mmc6RamEnabled = false;
        mmc6RamPermissions = 0;
        mcAccPrescaler = 0;
        scramblingPattern = 0;
        nLastA12 = 0xFFFF; // A primeira observação estabelece o nível inicial de A12.
        nA12LowM2Edges = 0;

        // Inicialização padrão segura do MMC3
        pRegister[0] = 0;
        pRegister[1] = 2;
        pRegister[2] = 4;
        pRegister[3] = 5;
        pRegister[4] = 6;
        pRegister[5] = 7;
        pRegister[6] = 0;
        pRegister[7] = 1;

        updateBanks();
    }

    bool Mapper004::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            if (configuration.submapper == 1)
            {
                // MMC6: nenhum leitor habilitado deixa o barramento aberto.
                // Com apenas uma metade habilitada, a outra retorna zero.
                // A máscara 0x03FF espelha o chip de 1 KiB em $7000-$7FFF.
                if (addr < 0x7000 || !mmc6RamEnabled || (mmc6RamPermissions & 0xA0) == 0)
                    return false;
                const uint8_t readBit = (addr & 0x0200) ? 0x80 : 0x20;
                data = (mmc6RamPermissions & readBit) ? vPRGRAM[addr & 0x03FF] : 0;
            }
            else
            {
                // Retornar false permite ao Bus preservar o último dado
                // quando não existe RAM ou o chip está desabilitado.
                if (!bPRGRAMEnabled || configuration.prgRamSize + configuration.prgNvRamSize == 0)
                    return false;
                data = vPRGRAM[addr & 0x1FFF];
            }
            mapped_addr = 0xFFFFFFFF;
            return true;
        }

        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            uint16_t offset = addr & 0x1FFF;
            uint8_t bank = (addr - 0x8000) / 0x2000;
            mapped_addr = pPRGMode[bank] * 0x2000 + offset;
            mapped_addr = scrambleAddress(mapped_addr, false) % (uint32_t(nPRGBanks) * 0x4000);
            return true;
        }

        return false;
    }

    bool Mapper004::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        if (configuration.submapper == 5 && (addr & 0xF000) == 0x5000)
        {
            scramblingPattern = data & 7;
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        if (addr >= 0x6000 && addr <= 0x7FFF)
        {
            mapped_addr = 0xFFFFFFFF;
            if (configuration.submapper == 1)
            {
                // No MMC6, a escrita exige leitura e escrita habilitadas
                // para a mesma metade de 512 bytes; só o bit de escrita não basta.
                const uint8_t permissions = (addr & 0x0200) ? 0xC0 : 0x30;
                if (addr >= 0x7000 && mmc6RamEnabled && (mmc6RamPermissions & permissions) == permissions)
                    vPRGRAM[addr & 0x03FF] = data;
            }
            else if (configuration.prgRamSize + configuration.prgNvRamSize != 0 &&
                     bPRGRAMEnabled && !bPRGRAMWriteProtected)
                vPRGRAM[addr & 0x1FFF] = data;
            return true;
        }

        if (addr < 0x8000)
            return false;
        // As escritas seguintes alteram registradores, não a PRG-ROM.
        // O sentinela informa ao cartucho que a operação já foi atendida.
        mapped_addr = 0xFFFFFFFF;

        if (addr >= 0x8000 && addr <= 0x9FFF)
        {
            if (!(addr & 0x0001))
            {
                nTargetRegister = data & 0x07;
                bPRGBankMode = (data & 0x40);
                bCHRInversion = (data & 0x80);
                if (configuration.submapper == 1)
                {
                    // Desabilitar $8000.5 mantém $A001 zerado. Reabilitar
                    // a RAM exige que o jogo programe novamente suas permissões.
                    mmc6RamEnabled = (data & 0x20) != 0;
                    if (!mmc6RamEnabled)
                        mmc6RamPermissions = 0;
                }
                updateBanks();
            }
            else
            {
                if (nTargetRegister <= 1)
                {
                    pRegister[nTargetRegister] = data & 0xFE;
                }
                else
                {
                    pRegister[nTargetRegister] = data;
                }
                updateBanks();
            }
            return true;
        }

        if (addr >= 0xA000 && addr <= 0xBFFF)
        {
            if (!(addr & 0x0001))
            {
                if (ogMirrorMode != MirrorMode::FOUR_SCREEN && configuration.submapper != 2)
                    mirrorMode = (data & 0x01) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL;
            }
            else
            {
                if (configuration.submapper == 1)
                    mmc6RamPermissions = mmc6RamEnabled ? data & 0xF0 : 0;
                else
                {
                    bPRGRAMEnabled = (data & 0x80) != 0;
                    bPRGRAMWriteProtected = (data & 0x40) != 0;
                }
            }
            return true;
        }

        if (addr >= 0xC000 && addr <= 0xDFFF)
        {
            if (!(addr & 0x0001))
                nIRQLatch = data;
            else
            {
                nIRQCounter = 0;
                bIRQReload = true;
                mcAccPrescaler = 0;
            }

            return true;
        }

        if (addr >= 0xE000 && addr <= 0xFFFF)
        {
            if (!(addr & 0x0001))
            {
                bIRQEnabled = false;
                bIRQActive = false;
            }
            else
                bIRQEnabled = true;
            return true;
        }

        return false;
    }

    void Mapper004::onPpuAddress(uint16_t addr, uint32_t)
    {
        handleA12Edge(addr);
    }

    void Mapper004::tick()
    {
        constexpr uint8_t A12_LOW_FILTER_M2_CYCLES = 3;
        if (nLastA12 == 0x0000 && nA12LowM2Edges < A12_LOW_FILTER_M2_CYCLES)
            ++nA12LowM2Edges;
    }

    // Avança o contador a partir das transições dos pinos de endereço da PPU.
    void Mapper004::handleA12Edge(uint16_t addr)
    {
        constexpr uint16_t A12_UNINITIALIZED = 0xFFFF;
        constexpr uint16_t A12_LOW = 0x0000;
        constexpr uint16_t A12_HIGH = 0x1000;
        // O filtro exige A12 baixo durante três bordas de descida de M2.
        constexpr uint8_t A12_LOW_FILTER_M2_CYCLES = 3;

        const uint16_t currentA12 = addr & A12_HIGH;

        // A primeira observação estabelece o nível sem inventar uma borda.
        if (nLastA12 == A12_UNINITIALIZED)
        {
            nLastA12 = currentA12;
            nA12LowM2Edges = 0;
            return;
        }

        // Acessos repetidos no mesmo nível não geram novas bordas de A12.
        if (currentA12 == nLastA12)
            return;

        if (currentA12 == A12_LOW)
        {
            if (configuration.submapper == 3)
            {
                // MC-ACC conta pulsos sem o filtro M2: o primeiro pulso
                // avança o contador, e os sete seguintes apenas o prescaler.
                if (mcAccPrescaler == 0)
                    clockIrqCounter();
                mcAccPrescaler = (mcAccPrescaler + 1) & 7;
            }
            // A12 caiu: reinicia a contagem M2 do intervalo em nível baixo.
            nA12LowM2Edges = 0;
            nLastA12 = A12_LOW;
            return;
        }

        // A12 subiu: no MMC3, só um intervalo baixo qualificado avança o contador.
        nLastA12 = A12_HIGH;
        if (configuration.submapper == 3 || nA12LowM2Edges < A12_LOW_FILTER_M2_CYCLES)
            return;

        clockIrqCounter();
    }

    void Mapper004::clockIrqCounter()
    {
        const uint16_t previous = nIRQCounter;

        if (nIRQCounter == 0 || bIRQReload)
            nIRQCounter = nIRQLatch;
        else
            nIRQCounter--;

        // Sharp gera IRQ se o resultado do clock for zero, inclusive na
        // recarga natural. NEC/MMC6 exigem uma transição de valor não zero
        // ou uma recarga explícita; isso evita repetir IRQ na recarga natural zero.
        // Uma IRQ já ativa permanece assim até a escrita em $E000.
        const bool alternate = configuration.submapper == 4 || configuration.submapper == 1;
        if (nIRQCounter == 0 && bIRQEnabled && (!alternate || previous != 0 || bIRQReload))
            bIRQActive = true;

        bIRQReload = false;
    }

    bool Mapper004::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t)
    {
        // Four-screen: CIRAM da PPU atende as duas primeiras nametables;
        // o cartucho fornece as duas restantes, inclusive seus espelhos $3000.
        if (ogMirrorMode == MirrorMode::FOUR_SCREEN && addr >= 0x2000 && addr <= 0x3EFF && (addr & 0x0800))
        {
            data = extraNametables[addr & 0x07FF];
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            uint16_t offset = addr & 0x03FF;
            uint8_t bank = addr / 0x0400;
            mapped_addr = pCHRMode[bank] * 0x0400 + offset;
            mapped_addr = scrambleAddress(mapped_addr, true) % (nCHRBanks ? uint32_t(nCHRBanks) * 0x2000 : 8192);
            return true;
        }
        return false;
    }

    bool Mapper004::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t)
    {
        if (ogMirrorMode == MirrorMode::FOUR_SCREEN && addr >= 0x2000 && addr <= 0x3EFF && (addr & 0x0800))
        {
            extraNametables[addr & 0x07FF] = data;
            mapped_addr = 0xFFFFFFFF;
            return true;
        }
        if (addr >= 0x0000 && addr <= 0x1FFF && nCHRBanks == 0)
        {
            uint16_t offset = addr & 0x03FF;
            uint8_t bank = addr / 0x0400;
            mapped_addr = pCHRMode[bank] * 0x0400 + offset;
            mapped_addr = scrambleAddress(mapped_addr, true) % 8192;
            return true;
        }
        return false;
    }

    MirrorMode Mapper004::getMirrorMode()
    {
        return mirrorMode;
    }

    bool Mapper004::getIrqFlag() const
    {
        return bIRQActive;
    }

    void Mapper004::clearIrqFlag()
    {
        // Aceitar a interrupção na CPU não limpa a saída IRQ do MMC3.
        // A escrita em $E000 desabilita a fonte e reconhece a IRQ pendente.
    }

    void Mapper004::updateBanks()
    {
        uint32_t nPRG8 = nPRGBanks * 2;
        uint32_t nCHR1 = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

        if (bPRGBankMode)
        {
            pPRGMode[0] = (nPRG8 - 2) % nPRG8;
            pPRGMode[1] = pRegister[7] % nPRG8;
            pPRGMode[2] = pRegister[6] % nPRG8;
            pPRGMode[3] = (nPRG8 - 1) % nPRG8;
        }
        else
        {
            pPRGMode[0] = pRegister[6] % nPRG8;
            pPRGMode[1] = pRegister[7] % nPRG8;
            pPRGMode[2] = (nPRG8 - 2) % nPRG8;
            pPRGMode[3] = (nPRG8 - 1) % nPRG8;
        }

        if (bCHRInversion)
        {
            pCHRMode[0] = pRegister[2] % nCHR1;
            pCHRMode[1] = pRegister[3] % nCHR1;
            pCHRMode[2] = pRegister[4] % nCHR1;
            pCHRMode[3] = pRegister[5] % nCHR1;
            pCHRMode[4] = (pRegister[0] & 0xFE) % nCHR1;
            pCHRMode[5] = ((pRegister[0] & 0xFE) + 1) % nCHR1;
            pCHRMode[6] = (pRegister[1] & 0xFE) % nCHR1;
            pCHRMode[7] = ((pRegister[1] & 0xFE) + 1) % nCHR1;
        }
        else
        {
            pCHRMode[0] = (pRegister[0] & 0xFE) % nCHR1;
            pCHRMode[1] = ((pRegister[0] & 0xFE) + 1) % nCHR1;
            pCHRMode[2] = (pRegister[1] & 0xFE) % nCHR1;
            pCHRMode[3] = ((pRegister[1] & 0xFE) + 1) % nCHR1;
            pCHRMode[4] = pRegister[2] % nCHR1;
            pCHRMode[5] = pRegister[3] % nCHR1;
            pCHRMode[6] = pRegister[4] % nCHR1;
            pCHRMode[7] = pRegister[5] % nCHR1;
        }
    }

    uint32_t Mapper004::scrambleAddress(uint32_t address, bool chr) const
    {
        if (configuration.submapper != 5)
            return address;
        // NesDev T9552: Mapper 4.5 armazena os bancos na ordem da coluna 2.
        static constexpr uint8_t prgBits[4][4] = {
            {16,17,15,14}, {17,16,14,15}, {14,15,16,17}, {15,14,17,16}
        };
        static constexpr uint8_t chrBits[8][6] = {
            {15,12,16,17,14,13}, {14,15,13,12,17,16}, {12,13,14,15,16,17},
            {16,14,12,13,17,15}, {15,13,17,16,12,14}, {14,12,15,16,17,13},
            {13,16,14,15,12,17}, {12,15,16,17,13,14}
        };
        const uint8_t *bits = chr ? chrBits[scramblingPattern] : prgBits[scramblingPattern & 3];
        // Só as linhas presentes nas tabelas são permutadas. Os bits de
        // endereço restantes conservam seu valor, inclusive o deslocamento no banco.
        const unsigned firstBit = chr ? 12 : 14;
        uint32_t result = address & ~(chr ? 0x3F000u : 0x3C000u);
        for (unsigned i = 0; i < (chr ? 6u : 4u); ++i)
            if (address & (1u << bits[i]))
                result |= 1u << (firstBit + i);
        return result;
    }

    void Mapper004::saveState(std::ostream &os)
    {
        const auto write = [&](uint32_t value, unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) os.put(static_cast<char>((value >> (8 * i)) & 0xFF));
        };
        // Layout legado Windows: 8219 bytes para MMC3 sem four-screen.
        write(nTargetRegister, 1); write(bPRGBankMode, 1); write(bCHRInversion, 1);
        write(static_cast<uint32_t>(mirrorMode), 4);
        os.write(reinterpret_cast<const char *>(pRegister), sizeof(pRegister));
        write(bIRQEnabled, 1); write(bIRQActive, 1); write(bIRQReload, 1);
        write(nIRQLatch, 2); write(nIRQCounter, 2);
        write(bPRGRAMEnabled, 1); write(bPRGRAMWriteProtected, 1);
        write(nLastA12, 2); write(nA12LowM2Edges, 1);
        os.write(reinterpret_cast<const char *>(vPRGRAM), sizeof(vPRGRAM));
        // Extensões vêm depois do payload legado. A configuração fixa da
        // ROM determina quais dados adicionais precisam ser salvos e lidos.
        if (configuration.submapper == 1) { write(mmc6RamEnabled, 1); write(mmc6RamPermissions, 1); }
        if (configuration.submapper == 3) write(mcAccPrescaler, 1);
        if (configuration.submapper == 5) write(scramblingPattern, 1);
        if (ogMirrorMode == MirrorMode::FOUR_SCREEN)
            os.write(reinterpret_cast<const char *>(extraNametables.data()), extraNametables.size());
    }

    void Mapper004::loadState(std::istream &is)
    {
        // Toda a leitura ocorre numa cópia: stream truncado ou campo inválido
        // não pode alterar bancos, IRQ, permissões ou RAM do mapper ativo.
        Mapper004 next = *this;
        const auto read = [&](unsigned bytes) {
            uint32_t value = 0;
            for (unsigned i = 0; i < bytes; ++i)
            {
                const int byte = is.get();
                if (byte == std::char_traits<char>::eof()) return uint32_t(0);
                value |= uint32_t(static_cast<uint8_t>(byte)) << (8 * i);
            }
            return value;
        };
        const auto readBool = [&](bool &value) {
            // Valida o byte antes de convertê-lo, evitando representações
            // inválidas de bool provenientes de estados corrompidos.
            const uint32_t byte = read(1);
            if (byte > 1) is.setstate(std::ios::failbit);
            value = byte == 1;
        };
        next.nTargetRegister = read(1); readBool(next.bPRGBankMode); readBool(next.bCHRInversion);
        const uint32_t mirror = read(4);
        is.read(reinterpret_cast<char *>(next.pRegister), sizeof(next.pRegister));
        readBool(next.bIRQEnabled); readBool(next.bIRQActive); readBool(next.bIRQReload);
        next.nIRQLatch = read(2); next.nIRQCounter = read(2);
        readBool(next.bPRGRAMEnabled); readBool(next.bPRGRAMWriteProtected);
        next.nLastA12 = read(2); next.nA12LowM2Edges = read(1);
        is.read(reinterpret_cast<char *>(next.vPRGRAM), sizeof(next.vPRGRAM));
        if (configuration.submapper == 1) { readBool(next.mmc6RamEnabled); next.mmc6RamPermissions = read(1); }
        if (configuration.submapper == 3) next.mcAccPrescaler = read(1);
        if (configuration.submapper == 5) next.scramblingPattern = read(1);
        if (ogMirrorMode == MirrorMode::FOUR_SCREEN)
            is.read(reinterpret_cast<char *>(next.extraNametables.data()), next.extraNametables.size());
        const bool fixedMirror = ogMirrorMode == MirrorMode::FOUR_SCREEN || configuration.submapper == 2;
        if (!is || next.nTargetRegister > 7 || next.nIRQLatch > 255 || next.nIRQCounter > 255 ||
            (mirror > 1 && mirror != static_cast<uint32_t>(MirrorMode::FOUR_SCREEN)) ||
            (fixedMirror ? mirror != static_cast<uint32_t>(ogMirrorMode) : mirror > 1) ||
            (next.pRegister[0] & 1) || (next.pRegister[1] & 1) ||
            (next.bIRQActive && !next.bIRQEnabled) || next.nA12LowM2Edges > 3 ||
            (next.nLastA12 != 0 && next.nLastA12 != 0x1000 && next.nLastA12 != 0xFFFF) ||
            (next.nLastA12 == 0xFFFF && next.nA12LowM2Edges != 0) ||
            (next.mmc6RamPermissions & 0x0F) || (!next.mmc6RamEnabled && next.mmc6RamPermissions) ||
            next.mcAccPrescaler > 7 || next.scramblingPattern > 7)
        {
            is.setstate(std::ios::failbit);
            return;
        }
        next.mirrorMode = static_cast<MirrorMode>(mirror);
        // Os mapas são derivados dos registradores; recalculá-los evita
        // restaurar tabelas de bancos inconsistentes com o estado validado.
        next.updateBanks();
        *this = next;
    }
}