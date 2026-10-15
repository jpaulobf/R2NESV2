#include "Core/Cartridge/Mappers/Mapper007.h"
#include <stdexcept>

namespace R2NES::Core
{
	bool Mapper007::supports(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror)
	{
		// AxROM usa CHR-RAM fixa de 8 KiB. Mantém as extensões de PRG de
		// 16 KiB (espelhada) e 512 KiB (bit 3), sem inventar linhas de banco.
		return prgBanks >= 1 && prgBanks <= 32 && (prgBanks & (prgBanks - 1)) == 0 &&
			chrBanks == 0 && (mirror == MirrorMode::HORIZONTAL || mirror == MirrorMode::VERTICAL);
	}

	Mapper007::Mapper007(uint16_t prgBanks, uint16_t chrBanks, MirrorMode mirror) : Mapper(prgBanks, chrBanks)
	{
		if (!supports(prgBanks, chrBanks, mirror))
			throw std::invalid_argument("Unsupported Mapper 7 memory configuration");
		reset();
	}

	Mapper007::~Mapper007() {}

	bool Mapper007::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
	{
		if (addr >= 0x8000 && addr <= 0xFFFF)
		{
			// AxROM (Mapper 7) chaveia janelas de 32KB.
			// nPRGBanks é a contagem de chunks de 16KB (padrão iNES).
			const uint32_t totalSize = static_cast<uint32_t>(nPRGBanks) * 0x4000;
			// As linhas de endereço ausentes espelham a memória física.
			mapped_addr = (static_cast<uint32_t>(nPRGBankSelect) * 0x8000 + (addr & 0x7FFF)) & (totalSize - 1);
			return true;
		}
		return false;
	}

	bool Mapper007::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
	{
		if (addr >= 0x8000 && addr <= 0xFFFF)
		{
			// Formato do registrador: [xxxMxPPP]
			// Bits 0-2 (PPP): Seleciona o banco de 32KB da PRG ROM (ou bits 0-3 para ROMs oversized de 512KB)
			// Bit 4 (M): Seleciona a nametable para single-screen mirroring
			//   0 = ONESCREEN_LO (nametable inferior, $2000)
			//   1 = ONESCREEN_HI (nametable superior, $2400)

			// Até 256 KiB, apenas PPP está conectado; a extensão de 512 KiB
			// usa também o bit 3. O bit 4 continua independente do banco PRG.
			const uint8_t bankMask = nPRGBanks >= 2 ? nPRGBanks / 2 - 1 : 0;
			nPRGBankSelect = data & bankMask;

			// Bit 4 controla o mirroring single-screen
			mirrorMode =
				(data & 0x10) ? MirrorMode::ONESCREEN_HI : MirrorMode::ONESCREEN_LO;

			mapped_addr = 0xFFFFFFFF; // Registrador tratado; não escreve na PRG ROM.
			return true;
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
		// Preserva os cinco bytes do formato anterior no Windows, sem depender
		// da representação nativa do enum ou da ordem de bytes da plataforma.
		const uint32_t mirror = static_cast<uint32_t>(mirrorMode);
		for (unsigned shift = 0; shift < 32; shift += 8)
			os.put(static_cast<char>((mirror >> shift) & 0xFF));
	}

	void Mapper007::loadState(std::istream &is)
	{
		uint8_t bank = 0;
		uint32_t mirror = 0;
		is.read(reinterpret_cast<char *>(&bank), sizeof(bank));
		for (unsigned shift = 0; shift < 32; shift += 8)
		{
			const int byte = is.get();
			if (byte == std::char_traits<char>::eof())
				return;
			mirror |= static_cast<uint32_t>(static_cast<uint8_t>(byte)) << shift;
		}
		const unsigned bankCount = nPRGBanks >= 2 ? nPRGBanks / 2 : 1;
		if (!is || bank >= bankCount ||
			(mirror != static_cast<uint32_t>(MirrorMode::ONESCREEN_LO) &&
			 mirror != static_cast<uint32_t>(MirrorMode::ONESCREEN_HI)))
		{
			is.setstate(std::ios::failbit);
			return;
		}
		// Só altera o mapper depois de validar todo o estado.
		nPRGBankSelect = bank;
		mirrorMode = static_cast<MirrorMode>(mirror);
	}

	MirrorMode Mapper007::getMirrorMode() { return mirrorMode; }
}
