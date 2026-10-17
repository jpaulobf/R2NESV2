#pragma once
#include "Core/Cartridge/Mappers/Mapper.h"
#include <cstdint>
#include <iostream>
#include <istream>
#include <optional>
#include <vector>
#include "Core/Memory/VRAM/VRAM.h"

namespace R2NES::Core
{
	class Mapper005 : public Mapper
	{
	public:
		enum class Revision : uint8_t { MMC5, MMC5A };
		struct Configuration
		{
			uint32_t prgRamSize = 65536; // Superset legado, sem inferência por CRC.
			uint32_t prgNvRamSize = 0;   // Capacidade emulada; persistência em disco ainda ausente.
			uint8_t submapper = 0;
			Revision revision = Revision::MMC5; // iNES/NES 2.0 não identificam a revisão do silício.
		};
		Mapper005(uint16_t prgBanks, uint16_t chrBanks);
		Mapper005(uint16_t prgBanks, uint16_t chrBanks, Configuration configuration);
		static bool supports(uint16_t prgBanks, uint16_t chrBanks, const Configuration &configuration);
		void loadTrainer(const uint8_t *trainer);
		VRAM *getCiram() override { return &vCIRAM; }
		~Mapper005() override;

		bool cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
		bool cpuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data) override;
		void onCpuRead(uint16_t addr, uint8_t data) override;
		void onPpuRead(uint16_t addr) override;
		void onPpuBackgroundTile(int16_t scanline, uint8_t column) override;
		bool cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
		bool ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
		bool ppuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter) override;
		bool ppuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter) override;
		void setPpuReadContext(PpuReadContext context) override;

		bool getIrqFlag() const override;
		void clearIrqFlag() override;
		void tick() override;
		void reset() override;
		// Evento já detectado de reset elétrico. reset() continua a inicialização completa.
		void hardwareReset();
		// Modelo digital dos pinos SL3/CL3 (bits 7/6), sem periférico externo instalado.
		void setExpansionInputs(uint8_t levels) { expansionInputs = levels & 0xC0; }
		uint8_t expansionPins(uint16_t address = 0, bool write = false, bool m2High = false) const;

		// Serialização para Save / Load states
		void saveState(std::ostream &os) override;
		void loadState(std::istream &is) override;

	private:
		// Registers
		uint8_t prgMode = 3;		// $5100 (0: 32KB, 1: 16KB, 2: 16KB+8KB+8KB, 3: 8KB)
		uint8_t chrMode = 0;		// $5101 (0: 8KB, 1: 4KB, 2: 2KB, 3: 1KB)
		uint8_t prgRamProtect1 = 1; // $5102 (Requires 0x02 to enable write)
		uint8_t prgRamProtect2 = 2; // $5103 (Requires 0x01 to enable write)
		uint8_t exRamMode =
			3; // $5104 (0: Extra NT, 1: Ext Attr, 2: CPU RAM, 3: Read-Only)
		uint8_t nametableTileMode =
			0;				   // $5105 (2 bits per NT: 0=CIRAM0, 1=CIRAM1, 2=ExRAM, 3=Fill)
		uint8_t fillTile = 0;  // $5106
		uint8_t fillColor = 0; // $5107

		uint8_t prgBankRegs[5] = {0, 0, 0, 0, 0xFF}; // $5113 - $5117
		// Registradores internos de 10 bits, já capturados em unidades de 1 KiB.
		uint16_t chrSetA[8] = {0}; // $5120 - $5127 (Sprites)
		uint16_t chrSetB[4] = {0}; // $5128 - $512B (Background)
		uint8_t chrHigh = 0;						 // $5130 (Upper bits for CHR)

		// Multiplier
		uint8_t multA = 0; // $5205
		uint8_t multB = 0; // $5206

		// Scanline IRQ
		uint8_t irqTargetLine = 0; // $5203
		bool irqEnable = false;	   // $5204 bit 7 write
		bool irqPending = false;   // $5204 bit 7 read
		bool inFrame = false;
		uint8_t scanlineCounter = 0;

		// Audio registers ($5000 - $5015)
		uint8_t audioRegs[22] = {0};
		uint8_t expansionControl = 0xC0, expansionLatch = 0, expansionInputs = 0;
		uint16_t timerCounter = 0;
		bool timerRunning = false, timerPending = false;

		// Memory buffers
		Configuration configuration;
		std::vector<uint8_t> vPRGRAM;
		uint8_t vExRAM[1024] = {0};		   // 1KB ExRAM
		// CIRAM autoritativa do MMC5. A VRAM da PPU não atende suas nametables.
		VRAM vCIRAM;

		// Fetch tracking for Dual CHR / Extended Attributes / IRQ
		uint16_t lastPpuAddr = 0xFFFF;
		uint8_t sameAddrReadCount = 0;
		uint8_t idleCycles = 0;
		uint8_t ppuReadSinceTick = 0;

		uint16_t lastNtAddr = 0;
		uint8_t lastExRamByte = 0;
		uint8_t observedPpuCtrl = 0; // Somente PPUCTRL.5 observado em $2000 exato.
		uint8_t observedPpuMask = 0; // Somente PPUMASK.3/.4 observado em $2001 exato.
		uint8_t lastChrSetB = 0; // Última escrita em $5120-$512B, para PPUDATA.
		uint8_t splitControl = 0; // $5200: habilitação, lado e limite em tiles.
		uint8_t splitScroll = 0;  // $5201: origem vertical carregada no começo do quadro.
		uint8_t splitBank = 0;    // $5202: banco de 4 KiB, independente de $5130.
		int16_t splitLine = -1;
		uint8_t splitY = 0;
		uint8_t splitColumn = 0;
		uint8_t splitTileActive = 0;
		uint16_t splitTileOffset = 0;
		PpuReadContext ppuReadContext = PpuReadContext::Cpu; // Transitório, fora do estado.

		uint32_t getChrBankIndex(uint8_t page, bool isSprite) const;
		uint16_t captureChrBank(uint8_t data) const;
		void loadStateData(std::istream &is);
		bool mapCpuRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, bool readOnly);
		void getPrgMapping(uint16_t addr, uint8_t &bank, bool &ram) const;
		std::optional<uint32_t> getRamOffset(uint8_t bank, uint16_t offset) const;
		void resetScanlineState();
		void clockScanline();
		bool useChrSetA(PpuReadContext context) const;
		bool mapPpuRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, PpuReadContext context);
	};
}
