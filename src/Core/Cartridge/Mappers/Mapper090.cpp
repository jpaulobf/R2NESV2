#include "Core/Cartridge/Mappers/Mapper090.h"

namespace R2NES::Core
{
    Mapper090::Mapper090(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {}

    Mapper090::~Mapper090() {}

    bool Mapper090::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x8000 && addr <= 0xFFFF)
        {
            // Mapeamento fixo em 8KB por banco
            int bankIndex = (addr - 0x8000) / 0x2000;
            mapped_addr = prgBanks[bankIndex] * 0x2000 + (addr % 0x2000);
            return true;
        }

        // Multiplicador de Hardware (Registradores $D000-$D003)
        if (addr >= 0xD000 && addr <= 0xD003)
        {
            uint32_t result = (uint32_t)multA * (uint32_t)multB;
            switch (addr)
            {
            case 0xD000:
                data = (uint8_t)(result & 0xFF);
                break;
            case 0xD001:
                data = (uint8_t)((result >> 8) & 0xFF);
                break;
            case 0xD002:
                data = (uint8_t)((result >> 16) & 0xFF);
                break;
            case 0xD003:
                data = (uint8_t)((result >> 24) & 0xFF);
                break;
            }
            return true;
        }

        return false;
    }

    bool Mapper090::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
    {
        // Bancos PRG ($8000-$8003)
        if (addr >= 0x8000 && addr <= 0x8003)
        {
            prgBanks[addr & 0x03] = data;
        }
        // Bancos CHR Low ($9000-$9007)
        else if (addr >= 0x9000 && addr <= 0x9007)
        {
            chrLow[addr & 0x07] = data;
            chrBanks[addr & 0x07] = (chrHigh[addr & 0x07] << 8) | chrLow[addr & 0x07];
        }
        // Bancos CHR High ($A000-$A007)
        else if (addr >= 0xA000 && addr <= 0xA007)
        {
            chrHigh[addr & 0x07] = data;
            chrBanks[addr & 0x07] = (chrHigh[addr & 0x07] << 8) | chrLow[addr & 0x07];
        }
        // Controle de Espelhamento e VRAM ($B000)
        else if (addr == 0xB000)
        {
            mirrorControl = data;
        }
        // IRQ Control ($C002-$C005)
        else if (addr == 0xC002)
            irqEnabled = false;
        else if (addr == 0xC003)
            irqEnabled = true;
        else if (addr == 0xC004)
            irqCounter = data; // Em algumas versões é o Latch

        // Multiplicador ($D000, $D001)
        else if (addr == 0xD000)
            multA = data;
        else if (addr == 0xD001)
            multB = data;

        return false;
    }

    bool Mapper090::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF)
        {
            int bankIndex = addr / 0x0400; // 8 bancos de 1KB
            mapped_addr = chrBanks[bankIndex] * 0x0400 + (addr % 0x0400);
            return true;
        }
        return false;
    }

    bool Mapper090::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data)
    {
        if (addr >= 0x0000 && addr <= 0x1FFF && nCHRBanks == 0) // CHR-RAM
        {
            mapped_addr = addr;
            return true;
        }
        return false;
    }

    void Mapper090::saveState(std::ostream &os)
    {
        os.write(reinterpret_cast<const char *>(prgBanks), sizeof(prgBanks));
        os.write(reinterpret_cast<const char *>(chrLow), sizeof(chrLow));
        os.write(reinterpret_cast<const char *>(chrHigh), sizeof(chrHigh));
        os.write(reinterpret_cast<const char *>(chrBanks), sizeof(chrBanks));
        os.write(reinterpret_cast<const char *>(&multA), sizeof(multA));
        os.write(reinterpret_cast<const char *>(&multB), sizeof(multB));
        os.write(reinterpret_cast<const char *>(&irqEnabled), sizeof(irqEnabled));
        os.write(reinterpret_cast<const char *>(&irqCounter), sizeof(irqCounter));
        os.write(reinterpret_cast<const char *>(&irqLatch), sizeof(irqLatch));
        os.write(reinterpret_cast<const char *>(&mirrorControl), sizeof(mirrorControl));
    }

    void Mapper090::loadState(std::istream &is)
    {
        is.read(reinterpret_cast<char *>(prgBanks), sizeof(prgBanks));
        is.read(reinterpret_cast<char *>(chrLow), sizeof(chrLow));
        is.read(reinterpret_cast<char *>(chrHigh), sizeof(chrHigh));
        is.read(reinterpret_cast<char *>(chrBanks), sizeof(chrBanks));
        is.read(reinterpret_cast<char *>(&multA), sizeof(multA));
        is.read(reinterpret_cast<char *>(&multB), sizeof(multB));
        is.read(reinterpret_cast<char *>(&irqEnabled), sizeof(irqEnabled));
        is.read(reinterpret_cast<char *>(&irqCounter), sizeof(irqCounter));
        is.read(reinterpret_cast<char *>(&irqLatch), sizeof(irqLatch));
        is.read(reinterpret_cast<char *>(&mirrorControl), sizeof(mirrorControl));
    }
}