#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <cstdint>
#include <iostream>
#include <istream>

namespace R2NES::Core
{
	class Mapper005 : public Mapper
	{
	public:
		Mapper005(uint8_t prgBanks, uint8_t chrBanks);
		~Mapper005() override;

		bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
		bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
		bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
		bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
		void setPpuReadIsSprite(bool isSprite) override;

		bool getIrqFlag() const override;
		void clearIrqFlag() override;
		void tick() override;
		void reset() override;

		// Serialização para Save / Load states
		void saveState(std::ostream &os) override;
		void loadState(std::istream &is) override;

	private:
		// Registers
		uint8_t prgMode = 3;		// $5100 (0: 32KB, 1: 16KB, 2: 16KB+8KB+8KB, 3: 8KB)
		uint8_t chrMode = 0;		// $5101 (0: 8KB, 1: 4KB, 2: 2KB, 3: 1KB)
		uint8_t prgRamProtect1 = 0; // $5102 (Requires 0x02 to enable write)
		uint8_t prgRamProtect2 = 0; // $5103 (Requires 0x01 to enable write)
		uint8_t exRamMode =
			0; // $5104 (0: Extra NT, 1: Ext Attr, 2: CPU RAM, 3: Read-Only)
		uint8_t nametableTileMode =
			0;				   // $5105 (2 bits per NT: 0=CIRAM0, 1=CIRAM1, 2=ExRAM, 3=Fill)
		uint8_t fillTile = 0;  // $5106
		uint8_t fillColor = 0; // $5107

		uint8_t prgBankRegs[5] = {0, 0, 0, 0, 0xFF}; // $5113 - $5117
		uint8_t chrSetA[8] = {0};					 // $5120 - $5127 (Sprites)
		uint8_t chrSetB[4] = {0};					 // $5128 - $512B (Background)
		uint8_t chrHigh = 0;						 // $5130 (Upper bits for CHR)

		// Multiplier
		uint8_t multA = 0; // $5205
		uint8_t multB = 0; // $5206

		// Scanline IRQ
		uint8_t irqTargetLine = 0; // $5203
		bool irqEnable = false;	   // $5204 bit 7 write
		bool irqPending = false;   // $5204 bit 7 read
		bool irqActive = false;
		bool inFrame = false;
		uint8_t scanlineCounter = 0;

		// Audio registers ($5000 - $5015)
		uint8_t audioRegs[22] = {0};

		// Memory buffers
		uint8_t vPRGRAM[128 * 1024] = {0}; // 128KB PRG RAM
		uint8_t vExRAM[1024] = {0};		   // 1KB ExRAM
		uint8_t vRAM[2048] = {0};		   // 2KB Internal CIRAM (Page 0 & Page 1)

		// Fetch tracking for Dual CHR / Extended Attributes / IRQ
		uint16_t lastPpuAddr = 0;
		uint8_t sameAddrReadCount = 0;
		uint32_t idleCycles = 0;

		uint16_t lastNtAddr = 0;
		uint8_t lastExRamByte = 0;
		uint8_t chrReadsLeft = 0;
		bool ppuReadIsSprite = false;

		uint32_t getChrBankIndex(uint8_t page, bool isSprite) const;
		void updateScanlineIRQ(uint16_t addr);
	};
}