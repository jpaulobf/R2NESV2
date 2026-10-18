#include "Core/Cartridge/Mappers/Mapper005.h"
#include <memory>
#include <algorithm>
#include <stdexcept>

namespace R2NES::Core
{
	namespace
	{
		void writeWord(std::ostream &os, uint16_t value)
		{
			os.put(value & 255);
			os.put(value >> 8);
		}
		uint16_t readWord(std::istream &is)
		{
			uint8_t bytes[2] = {};
			is.read(reinterpret_cast<char *>(bytes), 2);
			return bytes[0] | (uint16_t(bytes[1]) << 8);
		}
		void writeDword(std::ostream &os, uint32_t value)
		{
			writeWord(os, value & 0xFFFF);
			writeWord(os, value >> 16);
		}
		uint32_t readDword(std::istream &is)
		{
			const uint32_t low = readWord(is), high = readWord(is);
			return low | (high << 16);
		}
	}
	bool Mapper005::supports(uint16_t prgBanks, uint16_t chrBanks, const Configuration &config)
	{
		const auto powerOfTwo = [](uint32_t value)
		{ return value && !(value & (value - 1)); };
		if (!powerOfTwo(prgBanks) || prgBanks > 64 || chrBanks > 128 ||
			(chrBanks && !powerOfTwo(chrBanks)) || config.submapper ||
			(config.revision != Revision::MMC5 && config.revision != Revision::MMC5A))
			return false;
		const uint64_t total = uint64_t(config.prgRamSize) + config.prgNvRamSize;
		if (config.prgRamSize && config.prgNvRamSize)
			return (config.prgRamSize == 8192 || config.prgRamSize == 32768) &&
				   (config.prgNvRamSize == 8192 || config.prgNvRamSize == 32768);
		return total == 0 || total == 8192 || total == 16384 || total == 32768 || total == 65536 || total == 131072;
	}

	Mapper005::Mapper005(uint16_t prgBanks, uint16_t chrBanks) : Mapper005(prgBanks, chrBanks, Configuration{}) {}

	Mapper005::Mapper005(uint16_t prgBanks, uint16_t chrBanks, Configuration config) : Mapper(prgBanks, chrBanks), configuration(config)
	{
		if (!supports(prgBanks, chrBanks, config))
			throw std::invalid_argument("Unsupported MMC5 memory configuration");
		vPRGRAM.resize(config.prgRamSize + config.prgNvRamSize);
		configuration.batteryBackedExRam = config.batteryBackedExRam || config.prgNvRamSize != 0;
		reset();
	}

	std::vector<uint8_t> Mapper005::getBatteryData() const
	{
		// ETROM mantém o primeiro chip. O segundo é RAM de trabalho.
		std::vector<uint8_t> data(vPRGRAM.begin(), vPRGRAM.begin() + configuration.prgNvRamSize);
		if (configuration.batteryBackedExRam)
			data.insert(data.end(), std::begin(vExRAM), std::end(vExRAM));
		return data;
	}

	bool Mapper005::setBatteryData(const std::vector<uint8_t> &data)
	{
		const size_t size = configuration.prgNvRamSize + (configuration.batteryBackedExRam ? 1024 : 0);
		if (data.size() != size)
			return false;
		std::copy_n(data.begin(), configuration.prgNvRamSize, vPRGRAM.begin());
		if (configuration.batteryBackedExRam)
			std::copy_n(data.begin() + configuration.prgNvRamSize, 1024, vExRAM);
		return true;
	}

	bool Mapper005::convertBatteryData(const std::vector<uint8_t> &source, std::vector<uint8_t> &converted,
									   std::string &description) const
	{
		if (!configuration.batteryBackedExRam || !configuration.prgNvRamSize)
			return false;
		size_t sourceNv = 0;
		bool hasExRam = false;
		for (size_t size : {size_t(8192), size_t(16384), size_t(32768), size_t(65536), size_t(131072)})
		{
			if (source.size() == size)
			{
				sourceNv = size;
				break;
			}
			if (source.size() == size + 1024)
			{
				sourceNv = size;
				hasExRam = true;
				break;
			}
		}
		if (!sourceNv)
			return false;
		const size_t targetNv = configuration.prgNvRamSize;
		if (sourceNv > targetNv)
		{
			// Só reduz perfis maiores se a cauda estiver vazia ou repetir o banco
			// mantido. Dados distintos exigem análise manual, nunca são descartados.
			bool zero = true, mirrored = true;
			for (size_t i = targetNv; i < sourceNv; ++i)
			{
				zero &= source[i] == 0;
				mirrored &= source[i] == source[i % targetNv];
			}
			if (!zero && !mirrored)
			{
				description = "A conversao descartaria bancos com dados diferentes. O arquivo foi preservado.";
				return false;
			}
		}
		std::vector<uint8_t> result(targetNv + 1024, 0);
		std::copy_n(source.begin(), std::min(sourceNv, targetNv), result.begin());
		if (hasExRam)
			std::copy_n(source.begin() + sourceNv, 1024, result.begin() + targetNv);
		description = "Arquivo MMC5 com " + std::to_string(sourceNv) + " bytes de PRG-NVRAM" +
					  (hasExRam ? " e ExRAM." : ", sem ExRAM. A ExRAM sera inicializada com zeros.") +
					  "\nDestino: " + std::to_string(targetNv) + " bytes de PRG-NVRAM e 1024 bytes de ExRAM." +
					  (sourceNv < targetNv ? "\nOs bancos adicionais serao inicializados com zeros." : "") +
					  "\nO arquivo original sera copiado para um backup antes da conversao.";
		converted.swap(result);
		return true;
	}

	std::optional<uint32_t> Mapper005::getRamOffset(uint8_t bank, uint16_t offset) const
	{
		const uint32_t total = vPRGRAM.size();
		if (!total)
			return std::nullopt;
		// Extensão de um chip de 128 KiB: os quatro bits selecionam dezesseis bancos.
		if (total == 131072)
			return (bank & 15) * 8192u + offset;
		uint32_t first = total, second = 0;
		if (configuration.prgRamSize && configuration.prgNvRamSize)
		{
			// ETROM: primeiro chip não volátil, segundo volátil.
			first = configuration.prgNvRamSize;
			second = configuration.prgRamSize;
		}
		else if (total == 16384 || total == 65536)
			first = second = total / 2;
		const bool chip = bank & 4;
		const uint32_t size = chip ? second : first;
		if (!size)
			return std::nullopt; // /CE seleciona um chip ausente: barramento aberto.
		return (chip ? first : 0) + ((bank & 3) * 8192u + offset) % size;
	}

	void Mapper005::getPrgMapping(uint16_t addr, uint8_t &bank, bool &ram) const
	{
		if (addr < 0x8000)
		{
			bank = prgBankRegs[0];
			ram = true;
			return;
		}
		const uint8_t slot = (addr - 0x8000) / 8192;
		uint8_t reg = prgBankRegs[slot + 1];
		if (prgMode == 0)
		{
			bank = (prgBankRegs[4] & 0x7C) + slot;
			ram = false;
			return;
		}
		if (prgMode == 1 || (prgMode == 2 && slot < 2))
		{
			reg = prgBankRegs[slot < 2 ? 2 : 4];
			bank = (reg & 0x7E) + (slot & 1);
		}
		else
			bank = reg & 0x7F;
		ram = slot < (prgMode == 1 ? 2 : 3) && !(reg & 0x80);
	}

	void Mapper005::loadTrainer(const uint8_t *trainer)
	{
		const auto offset = getRamOffset(0, 0x1000);
		if (!offset)
			throw std::invalid_argument("MMC5 trainer requires PRG RAM");
		// Instalação física, sem desbloquear a proteção nem simular escritas da CPU.
		std::copy_n(trainer, 512, vPRGRAM.begin() + *offset);
	}

	Mapper005::~Mapper005() {}

	void Mapper005::reset()
	{
		prgMode = 3;
		chrMode = 0;
		// Valores de reset documentados: WRAM protegida e ExRAM somente leitura pela CPU.
		prgRamProtect1 = 1;
		prgRamProtect2 = 2;
		exRamMode = 3;
		nametableTileMode = 0;
		fillTile = 0;
		fillColor = 0;

		prgBankRegs[0] = 0;
		prgBankRegs[1] = 0;
		prgBankRegs[2] = 0;
		prgBankRegs[3] = 0;
		prgBankRegs[4] = 0xFF; // Top bank fixed to last ROM bank

		for (int i = 0; i < 8; i++)
			chrSetA[i] = 0;
		for (int i = 0; i < 4; i++)
			chrSetB[i] = 0;
		chrHigh = 0;

		multA = 0;
		multB = 0;

		irqTargetLine = 0;
		irqEnable = false;
		irqPending = false;
		inFrame = false;
		scanlineCounter = 0;

		for (int i = 0; i < 22; i++)
			audioRegs[i] = 0;
		expansionControl = 0xC0;
		expansionLatch = 0;
		timerCounter = 0;
		timerRunning = timerPending = false;

		lastPpuAddr = 0xFFFF;
		sameAddrReadCount = 0;
		idleCycles = 0;
		ppuReadSinceTick = 0;
		lastNtAddr = 0;
		lastExRamByte = 0;
		observedPpuCtrl = 0;
		observedPpuMask = 0;
		lastChrSetB = 0;
		ppuReadContext = PpuReadContext::Cpu;
		splitControl = splitScroll = splitBank = 0;
		splitLine = -1;
		splitY = splitColumn = splitTileActive = 0;
		splitTileOffset = 0;
	}

	void Mapper005::hardwareReset()
	{
		// Reset detectado pelo chip, não uma nova instância da máquina. Não limpa
		// RAM, bancos CHR, multiplicador, alvo/pendência/contador da IRQ de vídeo.
		prgMode = 3;
		prgRamProtect1 = 1;
		prgRamProtect2 = 2;
		exRamMode = 3;
		prgBankRegs[4] = 0xFF;
		chrHigh = 0;
		splitControl = splitScroll = 0;
		splitTileActive = 0;
		irqEnable = false;
		expansionControl = 0xC0;
		expansionLatch = 0;
		timerCounter = 0;
		timerRunning = timerPending = false;
		// Áudio e a detecção analógica de ausência de M2 ficam para etapas próprias.
	}

	uint8_t Mapper005::expansionPins(uint16_t address, bool write, bool m2High) const
	{
		if (configuration.revision != Revision::MMC5A)
			return 0;
		uint8_t levels = 0;
		for (unsigned pin = 0; pin < 2; ++pin)
		{
			const uint8_t mask = uint8_t(0x40 << pin);
			if (expansionControl & (1 << pin))
			{
				// Função /M2 sobrepõe direção/latch; não há RAM interna em $5800.
				const bool access = address >= 0x5800 && address <= 0x5BFF && write == (pin == 1);
				if (!(access && m2High))
					levels |= mask;
			}
			else
				levels |= (expansionControl & mask ? expansionInputs : expansionLatch) & mask;
		}
		return levels;
	}

	void Mapper005::setPpuReadContext(PpuReadContext context)
	{
		ppuReadContext = context;
	}

	bool Mapper005::getIrqFlag() const
	{
		return (irqEnable && irqPending) || timerPending;
	}

	void Mapper005::clearIrqFlag()
	{
		// Aceitar na CPU não reconhece a fonte: $5204 é vídeo; $5209 é timer MMC5A.
	}

	void Mapper005::tick()
	{
		// Um tick representa uma subida sistêmica de M2, inclusive em DMA.
		if (timerRunning)
		{
			if (timerCounter && --timerCounter == 0)
				timerPending = true;
			if (!timerCounter)
				timerRunning = false;
		}
		// Amostra a atividade de /RD em cada ciclo M2, inclusive durante DMA.
		if (ppuReadSinceTick)
			idleCycles = 0;
		else if (idleCycles < 3)
			++idleCycles;
		ppuReadSinceTick = 0;
		if (idleCycles == 3)
		{
			inFrame = false;
			lastPpuAddr = 0xFFFF;
			sameAddrReadCount = 0;
		}
	}

	bool Mapper005::cpuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
	{
		return mapCpuRead(addr, mapped_addr, data, false);
	}

	bool Mapper005::cpuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data)
	{
		return mapCpuRead(addr, mapped_addr, data, true);
	}

	void Mapper005::onCpuRead(uint16_t addr, uint8_t data)
	{
		// Observa o valor antes de a PPU limpar VBlank; espelhos não são decodificados.
		if (addr == 0x2002 && (data & 0x80))
			resetScanlineState();
	}

	bool Mapper005::mapCpuRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, bool readOnly)
	{
		if (!readOnly && (addr == 0xFFFA || addr == 0xFFFB))
			resetScanlineState();
		if (configuration.revision == Revision::MMC5A && (addr == 0x5208 || addr == 0x5209))
		{
			// Bits não documentados ficam determinísticos em zero; peek não reconhece IRQ.
			data = addr == 0x5208 ? expansionPins() : (!timerRunning && timerPending ? 0x80 : 0);
			if (addr == 0x5209 && !readOnly)
				timerPending = false;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}
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
			if (!readOnly)
				irqPending = false;
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
			// Nos modos de vídeo a CPU não dirige a leitura: conserva o barramento aberto.
			if (exRamMode >= 2)
				data = vExRAM[addr & 0x03FF];
			return true;
		}

		if (addr >= 0x6000)
		{
			uint8_t bank = 0;
			bool ram = false;
			getPrgMapping(addr, bank, ram);
			if (ram)
			{
				mapped_addr = 0xFFFFFFFF;
				if (const auto offset = getRamOffset(bank, addr & 0x1FFF))
					data = vPRGRAM[*offset];
			}
			else
				mapped_addr = (bank % (nPRGBanks * 2)) * 8192u + (addr & 0x1FFF);
			return true;
		}

		return false;
	}

	bool Mapper005::cpuMapWrite(uint16_t addr, uint32_t &mapped_addr, uint8_t data, uint32_t systemClockCounter)
	{
		if (configuration.revision == Revision::MMC5A)
		{
			if (addr >= 0x5207 && addr <= 0x520A)
			{
				if (addr == 0x5207)
					expansionControl = data & 0xC3;
				if (addr == 0x5208)
					expansionLatch = data & 0xC0;
				if (addr == 0x520A)
					timerCounter = (timerCounter & 0xFF) | (uint16_t(data) << 8);
				if (addr == 0x5209)
				{
					timerCounter = (timerCounter & 0xFF00) | data;
					timerRunning = timerCounter != 0;
				}
				mapped_addr = 0xFFFFFFFF;
				return true;
			}
			if (addr >= 0x5800 && addr <= 0x5BFF)
			{
				mapped_addr = 0xFFFFFFFF; // Acesso de periférico sem armazenamento instalado.
				return true;
			}
		}
		// O MMC5 observa os endereços exatos; os espelhos pertencem apenas à PPU.
		// Retornar false permite que o barramento entregue a mesma escrita à PPU.
		if (addr == 0x2000)
			observedPpuCtrl = data & 0x20;
		if (addr == 0x2001)
		{
			const uint8_t nextMask = data & 0x18;
			if (!observedPpuMask && nextMask)
				resetScanlineState();
			if (!nextMask)
			{
				// Desabilitar rendering encerra o quadro, mas não reconhece a IRQ pendente.
				inFrame = false;
				lastPpuAddr = 0xFFFF;
				sameAddrReadCount = 0;
			}
			observedPpuMask = nextMask;
		}
		// O NesDev documenta este reset; a confirmação elétrica de OAMDMA é parcial.
		if (addr == 0x4014)
			resetScanlineState();

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
			case 0x5100:
				prgMode = data & 0x03;
				break;
			case 0x5101:
				chrMode = data & 0x03;
				break;
			case 0x5102:
				prgRamProtect1 = data & 0x03;
				break;
			case 0x5103:
				prgRamProtect2 = data & 0x03;
				break;
			case 0x5104:
				exRamMode = data & 0x03;
				break;
			case 0x5105:
				nametableTileMode = data;
				break;
			case 0x5106:
				fillTile = data;
				break;
			case 0x5107:
				fillColor = data & 0x03;
				break;
			case 0x5130:
				chrHigh = data & 0x03;
				break;
			default:
				if (addr >= 0x5113 && addr <= 0x5117)
					prgBankRegs[addr - 0x5113] = data;
				else if (addr >= 0x5120 && addr <= 0x5127)
				{
					chrSetA[addr - 0x5120] = captureChrBank(data);
					lastChrSetB = 0;
				}
				else if (addr >= 0x5128 && addr <= 0x512B)
				{
					chrSetB[addr - 0x5128] = captureChrBank(data);
					lastChrSetB = 1;
				}
				else
					return false; // Lacunas não são registradores do MMC5.
				break;
			}
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		// IRQ & Multiplier ($5200 - $5206)
		if (addr >= 0x5200 && addr <= 0x5202)
		{
			if (addr == 0x5200)
				splitControl = data & 0xDF;
			if (addr == 0x5201)
				splitScroll = data;
			if (addr == 0x5202)
				splitBank = data;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}
		if (addr == 0x5203)
		{
			irqTargetLine = data;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		if (addr == 0x5204)
		{
			irqEnable = (data & 0x80) != 0;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		if (addr == 0x5205)
		{
			multA = data;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		if (addr == 0x5206)
		{
			multB = data;
			mapped_addr = 0xFFFFFFFF;
			return true;
		}

		// ExRAM ($5C00 - $5FFF)
		if (addr >= 0x5C00 && addr <= 0x5FFF)
		{
			mapped_addr = 0xFFFFFFFF;
			// Fora de quadro, o hardware pode ignorar/corromper escritas nos modos
			// de vídeo. Adotamos a opção determinística de ignorá-las, sem simular ruído.
			if (exRamMode == 2 || (exRamMode <= 1 && inFrame))
			{
				vExRAM[addr & 0x03FF] = data;
			}
			return true;
		}

		if (addr >= 0x6000)
		{
			mapped_addr = 0xFFFFFFFF;
			uint8_t bank = 0;
			bool ram = false;
			getPrgMapping(addr, bank, ram);
			if (ram && prgRamProtect1 == 2 && prgRamProtect2 == 1)
				if (const auto offset = getRamOffset(bank, addr & 0x1FFF))
					vPRGRAM[*offset] = data;
			return true; // ROM, RAM protegida ou chip ausente também consomem a escrita.
		}

		return false;
	}

	uint16_t Mapper005::captureChrBank(uint8_t data) const
	{
		// O tamanho ativo define a unidade da escrita; $5130 é capturado agora.
		// A tabela do NesDev documenta D0 também no bit interno 0 em todos os modos.
		// Bits 1/2 marcados como desconhecidos nos modos maiores ficam em zero.
		const uint16_t value = data | (uint16_t(chrHigh) << 8);
		return ((value << (3 - chrMode)) & 0x3FF) | (data & 1);
	}

	uint32_t Mapper005::getChrBankIndex(uint8_t page, bool isSprite) const
	{
		uint32_t bank = 0;
		if (isSprite)
		{
			switch (chrMode)
			{
			case 0:
				bank = (chrSetA[7] & ~7) + page;
				break;
			case 1:
				bank = (chrSetA[(page < 4) ? 3 : 7] & ~3) + (page & 3);
				break;
			case 2:
				bank = (chrSetA[(page >> 1) * 2 + 1] & ~1) + (page & 1);
				break;
			case 3:
				bank = chrSetA[page];
				break;
			}
		}
		else
		{
			switch (chrMode)
			{
			case 0:
				bank = (chrSetB[3] & ~7) + page;
				break;
			case 1:
				bank = (chrSetB[3] & ~3) + (page & 3);
				break;
			case 2:
				bank = (chrSetB[(page & 2) + 1] & ~1) + (page & 1);
				break;
			case 3:
				bank = chrSetB[page & 3];
				break;
			}
		}
		// Mudar $5101 só altera as linhas vindas da PPU, sem recapturar o banco.
		return bank;
	}

	void Mapper005::resetScanlineState()
	{
		inFrame = false;
		scanlineCounter = 0;
		irqPending = false;
		lastPpuAddr = 0xFFFF;
		sameAddrReadCount = 0;
	}

	void Mapper005::clockScanline()
	{
		if (!inFrame)
		{
			inFrame = true;
			scanlineCounter = 0;
			irqPending = false; // A primeira linha reconhece uma IRQ do quadro anterior.
		}
		else if (++scanlineCounter == 240)
			resetScanlineState(); // 241ª linha detectada.
		else if (irqTargetLine && scanlineCounter == irqTargetLine)
			irqPending = true;
	}

	void Mapper005::onPpuRead(uint16_t addr)
	{
		addr &= 0x3FFF;
		ppuReadSinceTick = 1;
		// Três leituras iguais armam o detector; somente a leitura seguinte conta.
		// Isso segue o texto temporal do NesDev (dot 4), não seu pseudocódigo abreviado.
		if (sameAddrReadCount == 3)
		{
			clockScanline();
			sameAddrReadCount = 0;
		}
		if (addr >= 0x2000 && addr <= 0x2FFF)
			sameAddrReadCount = (addr == lastPpuAddr && sameAddrReadCount) ? sameAddrReadCount + 1 : 1;
		else
			sameAddrReadCount = 0;
		lastPpuAddr = addr;
	}

	bool Mapper005::useChrSetA(PpuReadContext context) const
	{
		if (!(observedPpuCtrl & 0x20))
			return true; // Sprites 8x8 ignoram o conjunto B.
		// Fora dos fetches com substituições habilitadas, o último conjunto escrito
		// define a janela comum, inclusive para leituras/escritas por PPUDATA.
		if (context == PpuReadContext::Cpu || !(observedPpuMask & 0x18))
			return !lastChrSetB;
		return context == PpuReadContext::Sprite;
	}

	void Mapper005::onPpuBackgroundTile(int16_t line, uint8_t column)
	{
		// O contexto vem do pipeline, não do coarse X/Y rolado da nametable.
		if (line < 0 || line >= 240)
		{
			splitLine = -1;
			splitY = splitScroll;
			splitTileActive = 0;
			return;
		}
		if (splitLine < 0 || line < splitLine)
		{
			splitY = splitScroll;
			// Inicialização em meio de quadro mantém a origem alinhada à tela.
			for (int16_t i = 0; i < line; ++i)
				splitY = splitY == 239 || splitY == 255 ? 0 : splitY + 1;
		}
		else
			for (int16_t i = splitLine; i < line; ++i)
				splitY = splitY == 239 || splitY == 255 ? 0 : splitY + 1;
		splitLine = line;
		splitColumn = column;
		splitTileOffset = (splitY >> 3) * 32 + (splitColumn & 31);
		const uint8_t threshold = splitControl & 31;
		splitTileActive = (splitControl & 0x40) ? splitColumn >= threshold : splitColumn < threshold;
	}

	bool Mapper005::ppuMapRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
	{
		return mapPpuRead(addr, mapped_addr, data, ppuReadContext);
	}

	bool Mapper005::ppuMapPeek(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, uint32_t systemClockCounter)
	{
		// O visualizador consulta a janela de PPUDATA sem capturar atributos de tiles.
		return mapPpuRead(addr, mapped_addr, data, PpuReadContext::Cpu);
	}

	bool Mapper005::mapPpuRead(uint16_t addr, uint32_t &mapped_addr, uint8_t &data, PpuReadContext context)
	{
		const bool background = context == PpuReadContext::Background || context == PpuReadContext::BackgroundTile;
		const bool split = background && (observedPpuMask & 0x18) &&
						   exRamMode <= 1 && (splitControl & 0x80) && splitTileActive;
		if (split && addr < 0x2000)
		{
			// Ligação CL comercial: CHR A0-A2 continuam ligados ao fine Y da PPU.
			const uint32_t capacity = (nCHRBanks ? nCHRBanks : 1) * 8192u;
			mapped_addr = (splitBank * 4096u + (addr & 0xFFF)) % capacity;
			return true;
		}
		if (split && addr >= 0x2000 && addr <= 0x3EFF)
		{
			mapped_addr = 0xFFFFFFFF;
			if (context == PpuReadContext::BackgroundTile || (addr & 0x3FF) < 0x3C0)
				data = vExRAM[splitTileOffset];
			else
			{
				const uint16_t attr = 0x3C0 + ((splitTileOffset >> 4) & 0x38) + ((splitTileOffset >> 2) & 7);
				const uint8_t shift = ((splitTileOffset >> 4) & 4) | (splitTileOffset & 2);
				data = ((vExRAM[attr] >> shift) & 3) * 0x55;
			}
			return true;
		}
		const bool extended = exRamMode == 1 && (observedPpuMask & 0x18) &&
							  background;
		if (addr >= 0x0000 && addr <= 0x1FFF)
		{
			uint32_t totalChr1k = (nCHRBanks == 0) ? 8 : (nCHRBanks * 8);

			if (extended)
			{
				// ExRAM seleciona 4 KiB por tile; $5130 permanece em tempo real aqui.
				uint32_t bank = (lastExRamByte & 0x3F) | (static_cast<uint32_t>(chrHigh & 0x03) << 6);
				mapped_addr = (bank * 4096 + (addr & 0x0FFF)) % (totalChr1k * 1024);
				return true;
			}

			uint8_t page = (addr >> 10) & 7;
			uint32_t bank = getChrBankIndex(page, useChrSetA(context));
			mapped_addr = (bank % totalChr1k) * 1024 + (addr & 0x03FF);
			return true;
		}

		if (addr >= 0x2000 && addr <= 0x3EFF)
		{
			uint16_t normAddr = 0x2000 + (addr & 0x0FFF);

			uint8_t ntIndex = (normAddr >> 10) & 3;
			uint8_t ntSource = (nametableTileMode >> (ntIndex * 2)) & 0x03;
			uint16_t offset = normAddr & 0x03FF;

			mapped_addr = 0xFFFFFFFF;
			if (extended)
			{
				if (context == PpuReadContext::BackgroundTile || offset < 0x03C0)
				{
					// O índice vem da posição do tile, independentemente da fonte da NT.
					lastNtAddr = offset;
					lastExRamByte = vExRAM[offset];
				}
				else
				{
					// A leitura de atributos usa o tile anterior, não seu próprio offset.
					// Repetir a paleta atende qualquer quadrante extraído pela PPU.
					data = (lastExRamByte >> 6) * 0x55;
					return true;
				}
			}

			switch (ntSource)
			{
			case 0:
				data = vCIRAM.readRaw(offset);
				return true;
			case 1:
				data = vCIRAM.readRaw(1024 + offset);
				return true;
			case 2:
				if (exRamMode == 0 || exRamMode == 1)
				{
					data = vExRAM[offset];
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
			uint32_t bank = getChrBankIndex(page, useChrSetA(PpuReadContext::Cpu));
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
				vCIRAM.writeRaw(offset, data);
			else if (ntSource == 1)
				vCIRAM.writeRaw(1024 + offset, data);
			else if (ntSource == 2 && exRamMode <= 1)
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
		// Cada banco ocupa dois bytes, em ordem little-endian, inclusive os bits altos.
		for (uint16_t bank : chrSetA)
		{
			os.put(bank & 0xFF);
			os.put(bank >> 8);
		}
		for (uint16_t bank : chrSetB)
		{
			os.put(bank & 0xFF);
			os.put(bank >> 8);
		}
		os.write(reinterpret_cast<const char *>(&chrHigh), sizeof(chrHigh));

		os.write(reinterpret_cast<const char *>(&multA), sizeof(multA));
		os.write(reinterpret_cast<const char *>(&multB), sizeof(multB));

		os.write(reinterpret_cast<const char *>(&irqTargetLine), sizeof(irqTargetLine));
		os.put(irqEnable);
		os.put(irqPending);
		os.put(inFrame);
		os.write(reinterpret_cast<const char *>(&scanlineCounter), sizeof(scanlineCounter));

		os.write(reinterpret_cast<const char *>(audioRegs), sizeof(audioRegs));
		os.put(static_cast<uint8_t>(configuration.revision));
		os.put(expansionControl);
		os.put(expansionLatch);
		os.put(expansionInputs);
		writeWord(os, timerCounter);
		os.put(timerRunning);
		os.put(timerPending);
		// O tamanho vem da placa, nunca de um arquivo de estado não confiável.
		writeWord(os, nPRGBanks);
		writeWord(os, nCHRBanks);
		writeDword(os, configuration.prgRamSize);
		writeDword(os, configuration.prgNvRamSize);
		os.put(configuration.batteryBackedExRam);
		if (!vPRGRAM.empty())
			os.write(reinterpret_cast<const char *>(vPRGRAM.data()), vPRGRAM.size());
		os.write(reinterpret_cast<const char *>(vExRAM), sizeof(vExRAM));
		vCIRAM.saveState(os);

		writeWord(os, lastPpuAddr);
		os.write(reinterpret_cast<const char *>(&sameAddrReadCount), sizeof(sameAddrReadCount));
		os.write(reinterpret_cast<const char *>(&idleCycles), sizeof(idleCycles));
		writeWord(os, lastNtAddr);
		os.write(reinterpret_cast<const char *>(&lastExRamByte), sizeof(lastExRamByte));
		os.write(reinterpret_cast<const char *>(&observedPpuCtrl), sizeof(observedPpuCtrl));
		os.write(reinterpret_cast<const char *>(&observedPpuMask), sizeof(observedPpuMask));
		os.write(reinterpret_cast<const char *>(&lastChrSetB), sizeof(lastChrSetB));
		os.put(ppuReadSinceTick);
		os.put(splitControl);
		os.put(splitScroll);
		os.put(splitBank);
		writeWord(os, splitLine < 0 ? 0xFFFF : uint16_t(splitLine));
		os.put(splitY);
		os.put(splitColumn);
		os.put(splitTileActive);
		writeWord(os, splitTileOffset);
	}

	void Mapper005::loadState(std::istream &is)
	{
		// O console também faz rollback; aqui protegemos o uso isolado do mapper.
		auto next = std::make_unique<Mapper005>(*this);
		next->loadStateData(is);
		if (is)
			*this = *next;
	}

	void Mapper005::loadStateData(std::istream &is)
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
		const auto readBank = [&is](uint16_t &bank)
		{
			uint8_t bytes[2] = {};
			is.read(reinterpret_cast<char *>(bytes), sizeof(bytes));
			bank = bytes[0] | (uint16_t(bytes[1]) << 8);
		};
		for (auto &bank : chrSetA)
			readBank(bank);
		for (auto &bank : chrSetB)
			readBank(bank);
		is.read(reinterpret_cast<char *>(&chrHigh), sizeof(chrHigh));

		is.read(reinterpret_cast<char *>(&multA), sizeof(multA));
		is.read(reinterpret_cast<char *>(&multB), sizeof(multB));

		is.read(reinterpret_cast<char *>(&irqTargetLine), sizeof(irqTargetLine));
		const auto readFlag = [&is](bool &flag)
		{
			const int value = is.get();
			if (value != 0 && value != 1)
				is.setstate(std::ios::failbit);
			else
				flag = value != 0;
		};
		readFlag(irqEnable);
		readFlag(irqPending);
		readFlag(inFrame);
		is.read(reinterpret_cast<char *>(&scanlineCounter), sizeof(scanlineCounter));

		is.read(reinterpret_cast<char *>(audioRegs), sizeof(audioRegs));
		const int savedRevision = is.get();
		expansionControl = uint8_t(is.get());
		expansionLatch = uint8_t(is.get());
		expansionInputs = uint8_t(is.get());
		timerCounter = readWord(is);
		readFlag(timerRunning);
		readFlag(timerPending);
		const uint16_t savedPrg = readWord(is), savedChr = readWord(is);
		const uint32_t savedRam = readDword(is), savedNvRam = readDword(is);
		const int savedBatteryExRam = is.get();
		if (!is || savedRevision != static_cast<uint8_t>(configuration.revision) ||
			savedPrg != nPRGBanks || savedChr != nCHRBanks ||
			savedRam != configuration.prgRamSize || savedNvRam != configuration.prgNvRamSize ||
			savedBatteryExRam != int(configuration.batteryBackedExRam))
		{
			is.setstate(std::ios::failbit);
			return;
		}
		if (!vPRGRAM.empty())
			is.read(reinterpret_cast<char *>(vPRGRAM.data()), vPRGRAM.size());
		is.read(reinterpret_cast<char *>(vExRAM), sizeof(vExRAM));
		vCIRAM.loadState(is);

		lastPpuAddr = readWord(is);
		is.read(reinterpret_cast<char *>(&sameAddrReadCount), sizeof(sameAddrReadCount));
		is.read(reinterpret_cast<char *>(&idleCycles), sizeof(idleCycles));
		lastNtAddr = readWord(is);
		is.read(reinterpret_cast<char *>(&lastExRamByte), sizeof(lastExRamByte));
		is.read(reinterpret_cast<char *>(&observedPpuCtrl), sizeof(observedPpuCtrl));
		is.read(reinterpret_cast<char *>(&observedPpuMask), sizeof(observedPpuMask));
		is.read(reinterpret_cast<char *>(&lastChrSetB), sizeof(lastChrSetB));
		is.read(reinterpret_cast<char *>(&ppuReadSinceTick), sizeof(ppuReadSinceTick));
		is.read(reinterpret_cast<char *>(&splitControl), sizeof(splitControl));
		is.read(reinterpret_cast<char *>(&splitScroll), sizeof(splitScroll));
		is.read(reinterpret_cast<char *>(&splitBank), sizeof(splitBank));
		const uint16_t savedLine = readWord(is);
		if (savedLine == 0xFFFF)
			splitLine = -1;
		else if (savedLine < 240)
			splitLine = savedLine;
		else
			is.setstate(std::ios::failbit);
		is.read(reinterpret_cast<char *>(&splitY), sizeof(splitY));
		is.read(reinterpret_cast<char *>(&splitColumn), sizeof(splitColumn));
		is.read(reinterpret_cast<char *>(&splitTileActive), sizeof(splitTileActive));
		splitTileOffset = readWord(is);
		ppuReadContext = PpuReadContext::Cpu;

		// Impede deslocamentos inválidos e bancos fora dos dez bits do hardware.
		if ((expansionControl & ~0xC3) || (expansionLatch & ~0xC0) || (expansionInputs & ~0xC0))
			is.setstate(std::ios::failbit);
		if (configuration.revision == Revision::MMC5 &&
			(expansionControl != 0xC0 || expansionLatch || timerCounter || timerRunning || timerPending))
			is.setstate(std::ios::failbit);
		if (prgMode > 3 || chrMode > 3 || chrHigh > 3 || prgRamProtect1 > 3 ||
			prgRamProtect2 > 3 || exRamMode > 3 || fillColor > 3 || lastNtAddr > 0x3FF)
			is.setstate(std::ios::failbit);
		for (uint16_t bank : chrSetA)
			if (bank > 0x3FF)
				is.setstate(std::ios::failbit);
		for (uint16_t bank : chrSetB)
			if (bank > 0x3FF)
				is.setstate(std::ios::failbit);
		if ((observedPpuCtrl & ~0x20) || (observedPpuMask & ~0x18) || lastChrSetB > 1)
			is.setstate(std::ios::failbit);
		if (idleCycles > 3 || ppuReadSinceTick > 1 || sameAddrReadCount > 3 || scanlineCounter >= 240 ||
			(lastPpuAddr != 0xFFFF && lastPpuAddr > 0x3FFF))
			is.setstate(std::ios::failbit);
		if (sameAddrReadCount && (lastPpuAddr < 0x2000 || lastPpuAddr > 0x2FFF))
			is.setstate(std::ios::failbit);
		if (idleCycles == 3 && !ppuReadSinceTick && (inFrame || sameAddrReadCount || lastPpuAddr != 0xFFFF))
			is.setstate(std::ios::failbit);
		if ((splitControl & 0x20) || splitLine < -1 || splitLine >= 240 || splitColumn > 33 ||
			splitTileActive > 1 || splitTileOffset > 1023 ||
			(splitLine >= 0 && splitTileOffset != (splitY >> 3) * 32 + (splitColumn & 31)) ||
			(splitLine < 0 && splitTileActive))
			is.setstate(std::ios::failbit);
	}
}
