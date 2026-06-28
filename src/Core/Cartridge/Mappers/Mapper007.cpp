#include "Core/Cartridge/Mappers/Mapper007.h"

namespace R2NES::Core
{
	Mapper007::Mapper007(uint8_t prgBanks, uint8_t chrBanks, MirrorMode mirror) : Mapper(prgBanks, chrBanks)
	{
		mirrorMode = MirrorMode::ONESCREEN_LO;
		reset();
	}

	Mapper007::~Mapper007() {}

	bool Mapper007::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
	{
		if (addr >= 0x8000 && addr <= 0xFFFF)
		{
			// AxROM (Mapper 7) chaveia janelas de 32KB.
			// nPRGBanks é a contagem de chunks de 16KB (padrão iNES).
			uint32_t offset = addr & 0x7FFF;
			uint32_t totalSize = static_cast<uint32_t>(nPRGBanks) * 16384;
			if (totalSize > 0)
			{
				mapped_addr = (static_cast<uint32_t>(nPRGBankSelect) * 0x8000 + offset) % totalSize;
			}
			else
			{
				mapped_addr = 0;
			}
			return true;
		}
		return false;
	}

	bool Mapper007::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
	{
		if (addr >= 0x8000 && addr <= 0xFFFF)
		{
			// Formato do registrador: [....MPPP]
			// Bits 0-2 (PPP): Seleciona o banco de 32KB da PRG ROM (ou bits 0-3 para ROMs oversized de 512KB)
			// Bit 4 (M): Seleciona a nametable para single-screen mirroring
			//   0 = ONESCREEN_LO (nametable inferior, $2000)
			//   1 = ONESCREEN_HI (nametable superior, $2400)

			uint8_t nPRG32kChunks = nPRGBanks / 2;
			if (nPRG32kChunks > 0)
			{
				// Suporte a ROMs oversized (512KB) usando bit 3 se necessário
				uint8_t bankMask = (nPRG32kChunks > 8) ? 0x0F : 0x07;
				nPRGBankSelect = (data & bankMask) % nPRG32kChunks;
			}
			else
			{
				nPRGBankSelect = 0;
			}

			// Bit 4 controla o mirroring single-screen
			mirrorMode =
				(data & 0x10) ? MirrorMode::ONESCREEN_HI : MirrorMode::ONESCREEN_LO;

			return false; // Retorna false: não há escrita na PRG ROM
		}

		return false;
	}

	bool Mapper007::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
	{
		if (addr >= 0x0000 && addr <= 0x1FFF)
		{
			// AxROM usa CHR-RAM de 8KB (não tem CHR ROM)
			mapped_addr = addr;
			return true;
		}
		return false;
	}

	bool Mapper007::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
	{
		if (addr >= 0x0000 && addr <= 0x1FFF)
		{
			// CHR-RAM: permite escrita
			mapped_addr = addr;
			return true;
		}
		return false;
	}

	void Mapper007::reset()
	{
		nPRGBankSelect = 0;
		mirrorMode = MirrorMode::ONESCREEN_LO;
	}

	void Mapper007::saveState(std::ostream &os)
	{
		os.write(reinterpret_cast<const char *>(&nPRGBankSelect),
				 sizeof(nPRGBankSelect));
		os.write(reinterpret_cast<const char *>(&mirrorMode), sizeof(mirrorMode));
	}

	void Mapper007::loadState(std::istream &is)
	{
		is.read(reinterpret_cast<char *>(&nPRGBankSelect), sizeof(nPRGBankSelect));
		is.read(reinterpret_cast<char *>(&mirrorMode), sizeof(mirrorMode));
	}

	MirrorMode Mapper007::getMirrorMode() { return mirrorMode; }
}