#include "Core/NES.h"
#include <stdexcept>
#include <iostream>
#include "TestFixtures.h"

using namespace R2NES::Core;

static void check(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

int main()
{
	try
	{
		FixtureNES nes("");
		auto &bus = nes.getBus();
		auto &apu = nes.getApu();
		auto &pad = nes.getJoysticks().controller1;

		// A página $40 sem CPU parada em $4000-$401F deixa os registradores
		// internos inativos, inclusive o sinal de limpeza da IRQ de quadro.
		bus.cpuWrite(0x4017, 0);
		for (int i = 0; i < 30000 && !apu.getIrqFlag(); ++i)
			apu.step();
		check(apu.getIrqFlag(), "frame IRQ was not raised");
		for (int i = 0; i < 4; ++i)
			apu.step(); // Fora dos três ciclos que podem reafirmar a IRQ.
		bus.cpuWrite(0, 0xA5);
		check(bus.readOamDmaSource(0x4015, 0x8000) == 0xA5,
			  "inactive OAM DMA read activated APU status");
		bus.systemClockCounter &= ~uint64_t{1};
		apu.step();
		check(apu.getIrqFlag(), "inactive OAM DMA read cleared frame IRQ");

		// Com a CPU parada na janela interna, a fonte $50xx também ativa
		// $4015 pelos cinco bits baixos do endereço da DMA.
		check((bus.readOamDmaSource(0x5035, 0x4001) & 0x40) != 0,
			  "mirrored OAM DMA read missed APU status");
		apu.step();
		check(!apu.getIrqFlag(), "active OAM DMA read did not clear frame IRQ");

		// A RAM dirige o barramento externo. O status interno aproveita seu
		// bit 5, enquanto uma porta de controle avança sem substituir o byte.
		bus.ram->write(0x215, 0xFF);
		check(bus.readOamDmaSource(0x0215, 0x4001) == 0x20,
			  "status bit 5 did not come from RAM source");
		check(bus.cpuDataBus == 0xFF, "internal status changed external bus");

		pad.setButton(IO::BUTTON_A, true);
		pad.setButton(IO::BUTTON_B, false);
		bus.cpuWrite(0x4016, 1);
		bus.clockControllerStrobe();
		bus.cpuWrite(0x4016, 0);
		bus.clockControllerStrobe();
		bus.ram->write(0x216, 0xFF);
		check(bus.readOamDmaSource(0x0216, 0x4001) == 0xFF,
			  "controller overrode RAM during OAM DMA");
		check((bus.cpuRead(0x4016) & 1) == 0,
			  "OAM DMA did not clock the controller behind RAM");
		bus.cpuWrite(0x4016, 1);
		bus.clockControllerStrobe();
		bus.cpuWrite(0x4016, 0);
		bus.clockControllerStrobe();
		bus.cpuWrite(0, 0xA0);
		check(bus.readOamDmaSource(0x5016, 0x4001) == 0xA1,
			  "controller did not drive an undriven DMA source");

		// Só cinco bits existem no byte de atributos da OAM primária.
		auto &ppu = nes.getPpu();
		ppu.cpuWrite(0x2003, 2);
		ppu.cpuWrite(0x2004, 0xFF);
		ppu.cpuWrite(0x2003, 2);
		check(ppu.cpuRead(0x2004) == 0xE3,
			  "OAM attribute bits 2-4 must read as zero");
		ppu.cpuWrite(0x2003, 3);
		ppu.cpuWrite(0x2004, 0xFF);
		ppu.cpuWrite(0x2003, 3);
		check(ppu.cpuRead(0x2004) == 0xFF,
			  "ordinary OAM byte lost bits 2-4");

		// Depois do put final, a trava externa contém o dado da OAM DMA.
		FixtureNES transferNes("");
		auto &transferBus = transferNes.getBus();
		transferBus.ram->write(0x2FF, 0xAB);
		transferBus.cpuWrite(0x4014, 0x02);
		for (int i = 0; i < 520 && transferBus.dma_transfer; ++i)
			transferNes.step(false);
		check(!transferBus.dma_transfer, "OAM DMA did not finish");
		check(transferBus.cpuDataBus == 0xAB,
			  "OAM DMA put did not drive the external data bus");

		std::cout << "OAM DMA register activation and OAM attributes passed\n";
		return 0;
	}
	catch (const std::exception &error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}