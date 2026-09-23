#include "Core/Cartridge/Mappers/Mapper005.h"

namespace R2NES::Core
{
	Mapper005::Mapper005(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks)
	{
		reset();
	}

	Mapper005::~Mapper005() {}

	void Mapper005::reset()
	{
		prgMode = 3;
		chrMode = 0;
		prgRamProtect1 = 0;
		prgRamProtect2 = 0;
		exRamMode = 0;
		nametableTileMode = 0;
		fillTile = 0;
		fillColor = 0;

		prgBankRegs[0] = 0;
		prgBankRegs[1] = 0;
		prgBankRegs[2] = 0;
		prgBankRegs[3] = 0;
		prgBankRegs[4] = 0xFF; // Top bank fixed to last ROM bank

		for (int i = 0; i < 8; i++) chrSetA[i] = 0;
		for (int i = 0; i < 4; i++) chrSetB[i] = 0;
		chrHigh = 0;

		multA = 0;
		multB = 0;

		irqTargetLine = 0;
		irqEnable = false;
		irqPending = false;
		irqActive = false;
		inFrame = false;
		scanlineCounter = 0;

		for (int i = 0; i < 22; i++) audioRegs[i] = 0;

		lastPpuAddr = 0;
		sameAddrReadCount = 0;
		idleCycles = 0;
		lastNtAddr = 0;
		lastExRamByte = 0;
		chrReadsLeft = 0;
		ppuReadIsSprite = false;
	}

	void Mapper005::setPpuReadIsSprite(bool isSprite)
	{
		ppuReadIsSprite = isSprite;
	}

	bool Mapper005::getIrqFlag() const
	{
		return irqActive;
	}

	void Mapper005::clearIrqFlag()
	{
		irqActive = false;
	}

	void Mapper005::tick()
	{
		idleCycles++;
		if (idleCycles > 200)
		{
			inFrame = false;
			scanlineCounter = 0;
			sameAddrReadCount = 0;
		}
	}

	bool Mapper005::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
	{
		// Audio Registers ($5000 - $5015)
		if (addr >= 0x5000 && addr <= 0x5015)
		{
			data = audioRegs[addr - 0x5000];
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		// Multiplier & IRQ status ($5200 - $5206)
		if (addr == 0x5204)
		{
			data = (irqPending ? 0x80 : 0x00) | (inFrame ? 0x40 : 0x00);
			irqPending = false;
			irqActive = false;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		if (addr == 0x5205)
		{
			uint16_t res = static_cast<uint16_t>(multA) * static_cast<uint16_t>(multB);
			data = static_cast<uint8_t>(res & 0xFF);
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		if (addr == 0x5206)
		{
			uint16_t res = static_cast<uint16_t>(multA) * static_cast<uint16_t>(multB);
			data = static_cast<uint8_t>((res >> 8) & 0xFF);
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		// ExRAM ($5C00 - $5FFF)
		if (addr >= 0x5C00 && addr <= 0x5FFF)
		{
			mapped_addr = 0xFFFFFFFF;
			data = vExRAM[addr & 0x03FF];
			return true;
		}

		// PRG RAM ($6000 - $7FFF)
		if (addr >= 0x6000 && addr <= 0x7FFF)
		{
			mapped_addr = 0xFFFFFFFF;
			uint8_t ramBank = prgBankRegs[0] & 0x07;
			data = vPRGRAM[(ramBank * 8192) + (addr & 0x1FFF)];
			return true;
		}

		// PRG ROM / RAM Mapping ($8000 - $FFFF)
		if (addr >= 0x8000 && addr <= 0xFFFF)
		{
			uint32_t prg8Count = nPRGBanks * 2;
			uint8_t slot = (addr - 0x8000) / 0x2000;
			uint16_t offset = addr & 0x1FFF;

			switch (prgMode)
			{
			case 0: // 32KB Mode
			{
				uint32_t bank = ((prgBankRegs[4] & 0x7C) + slot) % prg8Count;
				mapped_addr = bank * 8192 + offset;
				return true;
			}

			case 1: // 16KB Mode
			{
				if (slot < 2)
				{
					uint8_t reg = prgBankRegs[2];
					if (!(reg & 0x80))
					{
						// PRG RAM
						mapped_addr = 0xFFFFFFFF;
						uint8_t ramBank = (reg & 0x06) + (slot & 1);
						data = vPRGRAM[(ramBank * 8192) + offset];
					}
					else
					{
						// PRG ROM
						uint32_t bank = ((reg & 0x7E) + (slot & 1)) % prg8Count;
						mapped_addr = bank * 8192 + offset;
					}
				}
				else
				{
					uint8_t reg = prgBankRegs[4];
					uint32_t bank = ((reg & 0x7E) + (slot & 1)) % prg8Count;
					mapped_addr = bank * 8192 + offset;
				}
				return true;
			}

			case 2: // 16KB + 8KB + 8KB
			{
				if (slot < 2)
				{
					uint8_t reg = prgBankRegs[2];
					if (!(reg & 0x80))
					{
						mapped_addr = 0xFFFFFFFF;
						uint8_t ramBank = (reg & 0x06) + (slot & 1);
						data = vPRGRAM[(ramBank * 8192) + offset];
					}
					else
					{
						uint32_t bank = ((reg & 0x7E) + (slot & 1)) % prg8Count;
						mapped_addr = bank * 8192 + offset;
					}
				}
				else if (slot == 2)
				{
					uint8_t reg = prgBankRegs[3];
					if (!(reg & 0x80))
					{
						mapped_addr = 0xFFFFFFFF;
						uint8_t ramBank = reg & 0x07;
						data = vPRGRAM[(ramBank * 8192) + offset];
					}
					else
					{
						uint32_t bank = (reg & 0x7F) % prg8Count;
						mapped_addr = bank * 8192 + offset;
					}
				}
				else
				{
					uint8_t reg = prgBankRegs[4];
					uint32_t bank = (reg & 0x7F) % prg8Count;
					mapped_addr = bank * 8192 + offset;
				}
				return true;
			}

			case 3: // 8KB Mode
			{
				uint8_t reg = prgBankRegs[slot + 1];
				if (slot < 3 && !(reg & 0x80))
				{
					mapped_addr = 0xFFFFFFFF;
					uint8_t ramBank = reg & 0x07;
					data = vPRGRAM[(ramBank * 8192) + offset];
				}
				else
				{
					uint32_t bank = (reg & 0x7F) % prg8Count;
					mapped_addr = bank * 8192 + offset;
				}
				return true;
			}
			}
		}

		return false;
	}

	bool Mapper005::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
	{
		// Audio Registers ($5000 - $5015)
		if (addr >= 0x5000 && addr <= 0x5015)
		{
			audioRegs[addr - 0x5000] = data;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		// Mapper Registers ($5100 - $5130)
		if (addr >= 0x5100 && addr <= 0x5130)
		{
			switch (addr)
			{
			case 0x5100: prgMode = data & 0x03; break;
			case 0x5101: chrMode = data & 0x03; break;
			case 0x5102: prgRamProtect1 = data & 0x03; break;
			case 0x5103: prgRamProtect2 = data & 0x03; break;
			case 0x5104: exRamMode = data & 0x03; break;
			case 0x5105: nametableTileMode = data; break;
			case 0x5106: fillTile = data; break;
			case 0x5107: fillColor = data & 0x03; break;
			case 0x5130: chrHigh = data & 0x03; break;
			default:
				if (addr >= 0x5113 && addr <= 0x5117)
					prgBankRegs[addr - 0x5113] = data;
				else if (addr >= 0x5120 && addr <= 0x5127)
					chrSetA[addr - 0x5120] = data;
				else if (addr >= 0x5128 && addr <= 0x512B)
					chrSetB[addr - 0x5128] = data;
				break;
			}
			return false;
		}

		// IRQ & Multiplier ($5200 - $5206)
		if (addr == 0x5203)
		{
			irqTargetLine = data;
			return false;
		}

		if (addr == 0x5204)
		{
			irqEnable = (data & 0x80) != 0;
			return false;
		}

		if (addr == 0x5205)
		{
			multA = data;
			return false;
		}

		if (addr == 0x5206)
		{
			multB = data;
			return false;
		}

		// ExRAM ($5C00 - $5FFF)
		if (addr >= 0x5C00 && addr <= 0x5FFF)
		{
			mapped_addr = 0xFFFFFFFF;
			if (exRamMode == 0 || exRamMode == 1 || exRamMode == 2)
			{
				vExRAM[addr & 0x03FF] = data;
			}
			return true;
		}

		// PRG RAM Write ($6000 - $7FFF)
		if (addr >= 0x6000 && addr <= 0x7FFF)
		{
			mapped_addr = 0xFFFFFFFF;
			if (prgRamProtect1 == 0x02 && prgRamProtect2 == 0x01)
			{
				uint8_t ramBank = prgBankRegs[0] & 0x07;
				vPRGRAM[(ramBank * 8192) + (addr & 0x1FFF)] = data;
			}
			return true;
		}

		// PRG RAM Write at $8000 - $FFFF
		if (addr >= 0x8000 && addr <= 0xFFFF)
		{
			if (prgRamProtect1 == 0x02 && prgRamProtect2 == 0x01)
			{
				uint8_t slot = (addr - 0x8000) / 0x2000;
				uint16_t offset = addr & 0x1FFF;

				bool isRam = false;
				uint8_t ramBank = 0;

				if (prgMode == 1 && slot < 2 && !(prgBankRegs[2] & 0x80))
				{
					isRam = true;
					ramBank = (prgBankRegs[2] & 0x06) + (slot & 1);
				}
				else if (prgMode == 2)
				{
					if (slot < 2 && !(prgBankRegs[2] & 0x80))
					{
						isRam = true;
						ramBank = (prgBankRegs[2] & 0x06) + (slot & 1);
					}
					else if (slot == 2 && !(prgBankRegs[3] & 0x80))
					{
						isRam = true;
						ramBank = prgBankRegs[3] & 0x07;
					}
				}
				else if (prgMode == 3 && slot < 3 && !(prgBankRegs[slot + 1] & 0x80))
				{
					isRam = true;
					ramBank = prgBankRegs[slot + 1] & 0x07;
				}

				if (isRam)
				{
					vPRGRAM[(ramBank * 8192) + offset] = data;
					mapped_addr = 0xFFFFFFFF;
					return true;
				}
			}
			return false;
		}

		return false;
	}

	uint32_t Mapper005::getChrBankIndex(uint8_t page, bool isSprite) const
	{
		uint32_t bank = 0;
		if (isSprite)
		{
			switch (chrMode)
			{
			case 0: bank = (chrSetA[7] & ~7) + page; break;
			case 1: bank = (chrSetA[(page < 4) ? 3 : 7] & ~3) + (page & 3); break;
			case 2: bank = (chrSetA[(page >> 1) * 2 + 1] & ~1) + (page & 1); break;
			case 3: bank = chrSetA[page]; break;
			}
		}
		else
		{
			switch (chrMode)
			{
			case 0: bank = (chrSetB[3] & ~7) + page; break;
			case 1: bank = (chrSetB[3] & ~3) + (page & 3); break;
			case 2: bank = (chrSetB[(page & 2) + 1] & ~1) + (page & 1); break;
			case 3: bank = chrSetB[page & 3]; break;
			}
		}
		return bank | (static_cast<uint32_t>(chrHigh & 0x03) << 8);
	}

	void Mapper005::updateScanlineIRQ(uint16_t addr)
	{
		idleCycles = 0;
		if (addr == lastPpuAddr)
		{
			sameAddrReadCount++;
		}
		else
		{
			sameAddrReadCount = 1;
			lastPpuAddr = addr;
		}

		if (sameAddrReadCount == 3)
		{
			scanlineCounter++;
			inFrame = true;
			if (scanlineCounter == irqTargetLine)
			{
				irqPending = true;
				if (irqEnable)
					irqActive = true;
			}
		}
	}

	bool Mapper005::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
	{
		if (addr >= 0x0000 && addr <= 0x1FFF)
		{
			bool isSprite = ppuReadIsSprite;
			if (!isSprite && chrReadsLeft > 0)
			{
				isSprite = false;
				chrReadsLeft--;
			}
			else if (!isSprite)
			{
				isSprite = true;
			}

			uint32_t totalChr1k = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

			if (exRamMode == 1 && !isSprite)
			{
				uint32_t bank = (lastExRamByte & 0x3F) | (static_cast<uint32_t>(chrHigh & 0x03) << 6);
				mapped_addr = (bank % totalChr1k) * 1024 + (addr & 0x03FF);
				return true;
			}

			uint8_t page = (addr >> 10) & 7;
			uint32_t bank = getChrBankIndex(page, isSprite);
			mapped_addr = (bank % totalChr1k) * 1024 + (addr & 0x03FF);
			return true;
		}

		if (addr >= 0x2000 && addr <= 0x3EFF)
		{
			uint16_t normAddr = 0x2000 + (addr & 0x0FFF);
			updateScanlineIRQ(normAddr);

			uint8_t ntIndex = (normAddr >> 10) & 3;
			uint8_t ntSource = (nametableTileMode >> (ntIndex * 2)) & 0x03;
			uint16_t offset = normAddr & 0x03FF;

			lastNtAddr = offset;
			chrReadsLeft = 2;

			mapped_addr = 0xFFFFFFFF;

			switch (ntSource)
			{
			case 0: data = vRAM[offset]; return true;
			case 1: data = vRAM[1024 + offset]; return true;
			case 2:
				if (exRamMode == 0 || exRamMode == 1)
				{
					lastExRamByte = vExRAM[offset];
					if (exRamMode == 1 && offset >= 0x03C0)
					{
						uint8_t pal = (lastExRamByte >> 6) & 0x03;
						data = (pal << 6) | (pal << 4) | (pal << 2) | pal;
					}
					else
					{
						data = vExRAM[offset];
					}
				}
				else
				{
					data = 0x00;
				}
				return true;
			case 3:
				if (offset < 0x03C0)
				{
					data = fillTile;
				}
				else
				{
					uint8_t pal = fillColor & 0x03;
					data = (pal << 6) | (pal << 4) | (pal << 2) | pal;
				}
				return true;
			}
		}

		return false;
	}

	bool Mapper005::ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
	{
		if (addr >= 0x0000 && addr <= 0x1FFF && nCHRBanks == 0)
		{
			uint8_t page = (addr >> 10) & 7;
			uint32_t bank = getChrBankIndex(page, true);
			mapped_addr = (bank % 8) * 1024 + (addr & 0x03FF);
			return true;
		}

		if (addr >= 0x2000 && addr <= 0x3EFF)
		{
			uint16_t normAddr = 0x2000 + (addr & 0x0FFF);
			uint8_t ntIndex = (normAddr >> 10) & 3;
			uint8_t ntSource = (nametableTileMode >> (ntIndex * 2)) & 0x03;
			uint16_t offset = normAddr & 0x03FF;

			mapped_addr = 0xFFFFFFFF;
			if (ntSource == 0)
				vRAM[offset] = data;
			else if (ntSource == 1)
				vRAM[1024 + offset] = data;
			else if (ntSource == 2 && exRamMode <= 2)
				vExRAM[offset] = data;

			return true;
		}

		return false;
	}

	void Mapper005::saveState(std::ostream &os)
	{
		os.write(reinterpret_cast<const char *>(&prgMode), sizeof(prgMode));
		os.write(reinterpret_cast<const char *>(&chrMode), sizeof(chrMode));
		os.write(reinterpret_cast<const char *>(&prgRamProtect1), sizeof(prgRamProtect1));
		os.write(reinterpret_cast<const char *>(&prgRamProtect2), sizeof(prgRamProtect2));
		os.write(reinterpret_cast<const char *>(&exRamMode), sizeof(exRamMode));
		os.write(reinterpret_cast<const char *>(&nametableTileMode), sizeof(nametableTileMode));
		os.write(reinterpret_cast<const char *>(&fillTile), sizeof(fillTile));
		os.write(reinterpret_cast<const char *>(&fillColor), sizeof(fillColor));

		os.write(reinterpret_cast<const char *>(prgBankRegs), sizeof(prgBankRegs));
		os.write(reinterpret_cast<const char *>(chrSetA), sizeof(chrSetA));
		os.write(reinterpret_cast<const char *>(chrSetB), sizeof(chrSetB));
		os.write(reinterpret_cast<const char *>(&chrHigh), sizeof(chrHigh));

		os.write(reinterpret_cast<const char *>(&multA), sizeof(multA));
		os.write(reinterpret_cast<const char *>(&multB), sizeof(multB));

		os.write(reinterpret_cast<const char *>(&irqTargetLine), sizeof(irqTargetLine));
		os.write(reinterpret_cast<const char *>(&irqEnable), sizeof(irqEnable));
		os.write(reinterpret_cast<const char *>(&irqPending), sizeof(irqPending));
		os.write(reinterpret_cast<const char *>(&irqActive), sizeof(irqActive));
		os.write(reinterpret_cast<const char *>(&inFrame), sizeof(inFrame));
		os.write(reinterpret_cast<const char *>(&scanlineCounter), sizeof(scanlineCounter));

		os.write(reinterpret_cast<const char *>(audioRegs), sizeof(audioRegs));
		os.write(reinterpret_cast<const char *>(vPRGRAM), sizeof(vPRGRAM));
		os.write(reinterpret_cast<const char *>(vExRAM), sizeof(vExRAM));
		os.write(reinterpret_cast<const char *>(vRAM), sizeof(vRAM));

		os.write(reinterpret_cast<const char *>(&lastPpuAddr), sizeof(lastPpuAddr));
		os.write(reinterpret_cast<const char *>(&sameAddrReadCount), sizeof(sameAddrReadCount));
		os.write(reinterpret_cast<const char *>(&idleCycles), sizeof(idleCycles));
		os.write(reinterpret_cast<const char *>(&lastNtAddr), sizeof(lastNtAddr));
		os.write(reinterpret_cast<const char *>(&lastExRamByte), sizeof(lastExRamByte));
		os.write(reinterpret_cast<const char *>(&chrReadsLeft), sizeof(chrReadsLeft));
	}

	void Mapper005::loadState(std::istream &is)
	{
		is.read(reinterpret_cast<char *>(&prgMode), sizeof(prgMode));
		is.read(reinterpret_cast<char *>(&chrMode), sizeof(chrMode));
		is.read(reinterpret_cast<char *>(&prgRamProtect1), sizeof(prgRamProtect1));
		is.read(reinterpret_cast<char *>(&prgRamProtect2), sizeof(prgRamProtect2));
		is.read(reinterpret_cast<char *>(&exRamMode), sizeof(exRamMode));
		is.read(reinterpret_cast<char *>(&nametableTileMode), sizeof(nametableTileMode));
		is.read(reinterpret_cast<char *>(&fillTile), sizeof(fillTile));
		is.read(reinterpret_cast<char *>(&fillColor), sizeof(fillColor));

		is.read(reinterpret_cast<char *>(prgBankRegs), sizeof(prgBankRegs));
		is.read(reinterpret_cast<char *>(chrSetA), sizeof(chrSetA));
		is.read(reinterpret_cast<char *>(chrSetB), sizeof(chrSetB));
		is.read(reinterpret_cast<char *>(&chrHigh), sizeof(chrHigh));

		is.read(reinterpret_cast<char *>(&multA), sizeof(multA));
		is.read(reinterpret_cast<char *>(&multB), sizeof(multB));

		is.read(reinterpret_cast<char *>(&irqTargetLine), sizeof(irqTargetLine));
		is.read(reinterpret_cast<char *>(&irqEnable), sizeof(irqEnable));
		is.read(reinterpret_cast<char *>(&irqPending), sizeof(irqPending));
		is.read(reinterpret_cast<char *>(&irqActive), sizeof(irqActive));
		is.read(reinterpret_cast<char *>(&inFrame), sizeof(inFrame));
		is.read(reinterpret_cast<char *>(&scanlineCounter), sizeof(scanlineCounter));

		is.read(reinterpret_cast<char *>(audioRegs), sizeof(audioRegs));
		is.read(reinterpret_cast<char *>(vPRGRAM), sizeof(vPRGRAM));
		is.read(reinterpret_cast<char *>(vExRAM), sizeof(vExRAM));
		is.read(reinterpret_cast<char *>(vRAM), sizeof(vRAM));

		is.read(reinterpret_cast<char *>(&lastPpuAddr), sizeof(lastPpuAddr));
		is.read(reinterpret_cast<char *>(&sameAddrReadCount), sizeof(sameAddrReadCount));
		is.read(reinterpret_cast<char *>(&idleCycles), sizeof(idleCycles));
		is.read(reinterpret_cast<char *>(&lastNtAddr), sizeof(lastNtAddr));
		is.read(reinterpret_cast<char *>(&lastExRamByte), sizeof(lastExRamByte));
		is.read(reinterpret_cast<char *>(&chrReadsLeft), sizeof(chrReadsLeft));
	}
}