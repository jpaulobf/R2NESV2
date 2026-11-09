#include "Core/NES.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

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
        NES nes("");
        auto &bus = nes.getBus();
        auto &pad = nes.getJoysticks().controller1;
        pad.setButton(IO::BUTTON_A, true);
        pad.setButton(IO::BUTTON_B, false);
        bus.cpuWrite(0x4016, 1);
        bus.clockControllerStrobe();
        bus.cpuWrite(0x4016, 0);
        bus.clockControllerStrobe();
        bus.cpuWrite(0x0002, 0xE0);
        check(bus.cpuRead(0x4016, true) == 0xE1, "controller peek must preserve upper bus bits");
        check(bus.cpuDataBus == 0xE0, "peek changed bus");
        check(bus.cpuRead(0x4016) == 0xE1, "controller bit and open bus");
        check(bus.cpuRead(0x4016) == 0xE0, "controller peek advanced shift register");

        auto &pad2 = nes.getJoysticks().controller2;
        pad2.setButton(IO::BUTTON_A, true);
        // Um pulso inteiro entre amostragens não deve carregar nenhum controle.
        for (int i = 0; i < 8; ++i)
        {
            bus.cpuRead(0x4016);
            bus.cpuRead(0x4017);
        }
        bus.cpuWrite(0x4016, 1);
        bus.cpuWrite(0x4016, 0);
        bus.clockControllerStrobe();
        check((bus.cpuRead(0x4016) & 1) == 1 && (bus.cpuRead(0x4017) & 1) == 1,
              "Unsampled strobe reloaded controller");
        bus.cpuWrite(0x4016, 1);
        bus.clockControllerStrobe();
        bus.cpuWrite(0x4016, 0);
        bus.clockControllerStrobe();
        check((bus.cpuRead(0x4016) & 1) == 1 && (bus.cpuRead(0x4017) & 1) == 1,
              "Sampled strobe did not reload both controllers");
        check((bus.cpuRead(0x4016) & 1) == 0 && (bus.cpuRead(0x4017) & 1) == 0,
              "Controller sequence did not advance");

        bus.cpuWrite(0x0002, 0xA0);
        check(bus.cpuRead(0x4017) == 0xA0, "port 2 upper bits must remain open");
        nes.getJoysticks().port2Device = IO::DeviceType::Zapper;
        bus.setZapperTrigger(true);
        bus.cpuWrite(0x0002, 0xE0);
        check((bus.cpuRead(0x4017) & 0xF0) == 0xF0, "zapper trigger and upper bus bits");

        bus.cpuWrite(0x4015, 0);
        bus.cpuWrite(0x0002, 0xA5);
        check((bus.cpuRead(0x4015, true) & 0x20) == 0x20, "APU status bit 5 peek");
        check(bus.cpuDataBus == 0xA5, "APU status peek changed external bus");
        check((bus.cpuRead(0x4015) & 0x20) == 0x20, "APU status bit 5");
        check(bus.cpuDataBus == 0xA5 && bus.cpuRead(0x4000) == 0xA5,
              "APU status read changed external open bus");
        bus.cpuWrite(0x0002, 0x40);
        check((bus.cpuRead(0x4015) & 0x20) == 0, "APU status bit 5 did not clear");

        // LDA $3FFF,X: a leitura descartada de $3F16 alimenta o barramento
        // antes da leitura final de $4016, mesmo quando cruza uma página.
        auto &cpu = nes.getCpu();
        while (!cpu.complete())
            cpu.clock();
        bus.ram->write(0, 0xBD);
        bus.ram->write(1, 0xFF);
        bus.ram->write(2, 0x3F);
        bus.cpuWrite(0x2002, 0xE0);
        cpu.pc = 0;
        cpu.x = 0x17;
        do
        {
            cpu.clock();
        } while (!cpu.complete());
        check((cpu.a & 0xE0) == 0xE0, "indexed dummy read did not reach controller open bus");

        // Escritas não oficiais usam a página original + 1; ao cruzar a
        // página, o valor escrito também substitui o byte alto do endereço.
        for (const uint8_t opcode : {uint8_t{0x93}, uint8_t{0x9F}, uint8_t{0x9B}, uint8_t{0x9C}, uint8_t{0x9E}})
        {
            for (const bool cross : {false, true})
            {
                bus.ram->write(0x300, opcode);
                bus.ram->write(0x301, opcode == 0x93 ? 0x20 : (cross ? 0xF0 : 0x10));
                bus.ram->write(0x302, 0x1E);
                bus.ram->write(0x20, cross ? 0xF0 : 0x10);
                bus.ram->write(0x21, 0x1E);
                bus.ram->write(0x600, 0xCC);
                bus.ram->write(0x700, 0xCC);
                bus.ram->write(0x610, 0xCC);
                cpu.pc = 0x300;
                cpu.a = 0xFF;
                cpu.x = opcode == 0x9C ? (cross ? 0x10 : 0) : 0x06;
                cpu.y = opcode == 0x9C ? 0x06 : (cross ? 0x10 : 0);
                do
                {
                    cpu.clock();
                } while (!cpu.complete());
                check(bus.ram->read(cross ? 0x600 : 0x610) == 0x06,
                      "unstable store value or address incorrect");
                if (cross)
                    check(bus.ram->read(0x700) == 0xCC, "unstable store used indexed address");
                if (opcode == 0x9B)
                    check(cpu.stkp == 0x06, "TAS did not update stack pointer");
            }
        }

        // A parada DMC na leitura indexada imediatamente antes da escrita
        // retira a máscara do dado, mas não da corrupção do endereço.
        for (const uint8_t opcode : {uint8_t{0x93}, uint8_t{0x9F}, uint8_t{0x9B}, uint8_t{0x9C}, uint8_t{0x9E}})
        {
            for (const bool cross : {false, true})
            {
                // 0: sem DMA, 1: parada um ciclo antes, 2: parada na leitura indexada.
                for (int scenario = 0; scenario < 3; ++scenario)
                {
                    NES storeNes("");
                    auto &storeCpu = storeNes.getCpu();
                    auto &storeBus = storeNes.getBus();
                    auto &storeApu = storeNes.getApu();
                    while (!storeCpu.complete())
                        storeNes.step(false);
                    storeBus.ram->write(0x300, opcode);
                    storeBus.ram->write(0x301, opcode == 0x93 ? 0x20 : (cross ? 0xF0 : 0x00));
                    storeBus.ram->write(0x302, cross ? 0x01 : 0x05);
                    storeBus.ram->write(0x20, cross ? 0xF0 : 0x00);
                    storeBus.ram->write(0x21, cross ? 0x01 : 0x05);
                    storeBus.ram->write(0x000, 0xCC);
                    storeBus.ram->write(0x200, 0xCC);
                    storeBus.ram->write(0x500, 0xCC);
                    storeCpu.pc = 0x300;
                    storeCpu.a = 0xFF;
                    storeCpu.x = opcode == 0x9C ? (cross ? 0x10 : 0) : 0xA5;
                    storeCpu.y = opcode == 0x9C ? 0xA5 : (cross ? 0x10 : 0);
                    if (scenario != 0)
                    {
                        storeApu.cpuWrite(0x4015, 0x10);
                        for (int i = 0; i < 5 && !storeApu.isDmcDmaReady(); ++i)
                        {
                            ++storeBus.systemClockCounter;
                            storeApu.step();
                        }
                        check(storeApu.isDmcDmaReady(), "DMC store test request missing");
                        const int beforeDummy = opcode == 0x93 ? 4 : 3;
                        for (int i = 0; i < beforeDummy - (scenario == 1 ? 1 : 0); ++i)
                            storeCpu.clock();
                        storeNes.step(false); // Início da parada, antes da leitura da CPU.
                        std::ostringstream saved;
                        storeCpu.saveState(saved);
                        std::istringstream restore(saved.str());
                        storeCpu.loadState(restore);
                        check(bool(restore), "CPU state during DMC store rejected");
                        for (int i = 0; i < 5 && storeApu.hasDmcDmaRequest(); ++i)
                            storeNes.step(false);
                        check(!storeApu.hasDmcDmaRequest(), "DMC store test fetch missing");
                    }
                    do
                    {
                        storeCpu.clock();
                    } while (!storeCpu.complete());
                    const uint8_t expected = scenario == 2 ? 0xA5 : (cross ? 0x00 : 0x04);
                    check(storeBus.ram->read(cross ? 0x000 : 0x500) == expected,
                          "DMC unstable store value or address incorrect");
                    if (cross)
                        check(storeBus.ram->read(0x200) == 0xCC, "DMC changed crossed store address");
                    if (opcode == 0x9B)
                        check(storeCpu.stkp == 0xA5, "TAS stack pointer incorrect after DMA");
                }
            }
        }

        // INC $4014 deve concluir ambas as escritas antes da parada de OAM.
        for (const bool pal : {false, true})
        {
            NES oamNes("");
            oamNes.setPAL(pal);
            auto &oamCpu = oamNes.getCpu();
            auto &oamBus = oamNes.getBus();
            while (!oamCpu.complete())
                oamNes.step(false);
            oamBus.ram->write(0, 0xEE);
            oamBus.ram->write(1, 0x14);
            oamBus.ram->write(2, 0x40);
            oamCpu.pc = 0;
            for (int i = 0; i < 5; ++i)
                oamNes.step(false);
            check(oamBus.dma_transfer && oamCpu.nextCycleIsWrite(), "INC $4014 first write missing");
            const uint8_t firstPage = oamBus.dma_page;
            oamNes.step(false);
            check(oamCpu.complete() && oamBus.dma_page == uint8_t(firstPage + 1),
                  "OAM DMA interrupted second INC $4014 write");
            for (int i = 0; i < 520 && oamBus.dma_transfer; ++i)
                oamNes.step(false);
            check(!oamBus.dma_transfer, "OAM DMA did not complete");
            for (int i = 0; i < 256; ++i)
                check(oamNes.getPpu().getOamMemory()[i] == uint8_t(firstPage + 1),
                      "OAM DMA copied the first RMW page");
        }

        // O DMC deve aguardar as duas escritas consecutivas de INC $0010.
        NES dmaNes("");
        auto &dmaCpu = dmaNes.getCpu();
        auto &dmaBus = dmaNes.getBus();
        while (!dmaCpu.complete())
            dmaNes.step();
        dmaBus.ram->write(0, 0xEE);
        dmaBus.ram->write(1, 0x10);
        dmaBus.ram->write(2, 0x00);
        dmaBus.ram->write(0x10, 0x34);
        dmaCpu.pc = 0;
        for (int i = 0; i < 4; ++i)
            dmaNes.step();
        check(dmaCpu.nextCycleIsWrite(), "INC did not reach first write");
        dmaNes.getApu().cpuWrite(0x4015, 0x10);
        check(dmaNes.getApu().hasDmcDmaRequest(), "DMC request missing");
        dmaNes.step();
        check(dmaNes.getApu().hasDmcDmaRequest() && dmaCpu.nextCycleIsWrite(),
              "DMC interrupted first INC write");
        dmaNes.step();
        check(dmaNes.getApu().hasDmcDmaRequest() && dmaBus.ram->read(0x10) == 0x35,
              "DMC interrupted second INC write");
        dmaNes.step();
        check(dmaNes.getApu().hasDmcDmaRequest(), "DMC fetched before dummy cycle");
        for (int i = 0; i < 4 && dmaNes.getApu().hasDmcDmaRequest(); ++i)
            dmaNes.step();
        check(!dmaNes.getApu().hasDmcDmaRequest(), "DMC fetch did not complete");

        // A carga inicial tenta parar a CPU no get três ou quatro ciclos
        // depois da escrita em $4015, conforme a fase dessa escrita.
        for (unsigned phase = 0; phase < 2; ++phase)
        {
            NES loadNes("");
            auto &loadCpu = loadNes.getCpu();
            auto &loadBus = loadNes.getBus();
            auto &loadApu = loadNes.getApu();
            while (!loadCpu.complete())
                loadNes.step(false);
            loadBus.ram->write(0, 0xA9); // LDA #$10
            loadBus.ram->write(1, 0x10);
            loadBus.ram->write(2, 0x8D); // STA $4015
            loadBus.ram->write(3, 0x15);
            loadBus.ram->write(4, 0x40);
            loadBus.ram->write(5, 0xEA);
            loadBus.ram->write(6, 0xEA);
            loadCpu.pc = 0;
            loadBus.systemClockCounter = (loadBus.systemClockCounter & ~1u) | phase;
            for (int i = 0; i < 10 && !loadApu.hasDmcDmaRequest(); ++i)
                loadNes.step(false);
            check(loadApu.hasDmcDmaRequest(), "initial DMC request missing");
            const uint32_t writeCycle = loadBus.systemClockCounter - 1;
            const uint32_t haltCycle = writeCycle + ((writeCycle & 1) ? 3 : 4);
            check(!loadApu.isDmcDmaReady(), "initial DMC request started immediately");
            while (loadBus.systemClockCounter < haltCycle)
            {
                check(!loadApu.isDmcDmaReady(), "initial DMC halt too early");
                loadNes.step(false);
            }
            check(loadApu.isDmcDmaReady(), "initial DMC halt too late");
            std::ostringstream beforeLoad;
            loadCpu.saveState(beforeLoad);
            for (int i = 0; i < 3; ++i)
            {
                loadNes.step(false);
                std::ostringstream duringLoad;
                loadCpu.saveState(duringLoad);
                check(duringLoad.str() == beforeLoad.str(), "initial DMC load did not halt for three cycles");
            }
            check(!loadApu.hasDmcDmaRequest(), "initial DMC get missing");
            loadNes.step(false);
            std::ostringstream afterLoad;
            loadCpu.saveState(afterLoad);
            check(afterLoad.str() != beforeLoad.str(), "CPU did not resume after initial DMC load");
        }

        // Quando a CPU está parada em $4000-$401F, o get DMC com endereço
        // terminado em $16 também aciona a porta 1 e deixa seu bit no bus.
        for (const bool conflict : {false, true})
        {
            NES conflictNes("");
            auto &conflictCpu = conflictNes.getCpu();
            auto &conflictBus = conflictNes.getBus();
            auto &conflictApu = conflictNes.getApu();
            while (!conflictCpu.complete())
                conflictNes.step(false);
            conflictApu.setAudioOutputSuppressed(true);
            conflictApu.cpuWrite(0x4010, 0x0F);
            conflictApu.cpuWrite(0x4012, 0);
            conflictApu.cpuWrite(0x4013, 3);
            conflictApu.cpuWrite(0x4015, 0x10);
            for (int byte = 0; byte < 22; ++byte)
            {
                check(conflictApu.hasDmcDmaRequest(), "DMC sample request missing");
                conflictApu.completeDmcDma(0, 0);
                for (int tick = 0; tick < 5000 && !conflictApu.hasDmcDmaRequest(); ++tick)
                {
                    ++conflictBus.systemClockCounter;
                    conflictApu.step();
                }
            }
            check(conflictApu.isDmcDmaReady() && conflictApu.getDmcDmaAddress() == 0xC016,
                  "DMC conflict sample not reached");
            conflictNes.getJoysticks().controller1.setButton(IO::BUTTON_A, true);
            conflictNes.getJoysticks().controller1.setButton(IO::BUTTON_B, false);
            conflictBus.cpuWrite(0x4016, 1);
            conflictBus.clockControllerStrobe();
            conflictBus.cpuWrite(0x4016, 0);
            conflictBus.clockControllerStrobe();
            conflictBus.ram->write(0x300, 0xAD); // LDA $4000 ou LDA $0010
            conflictBus.ram->write(0x301, conflict ? 0x00 : 0x10);
            conflictBus.ram->write(0x302, conflict ? 0x40 : 0x00);
            conflictBus.ram->write(0x10, 0x40);
            conflictCpu.pc = 0x300;
            for (int i = 0; i < 3; ++i)
                conflictCpu.clock();
            check(conflictCpu.nextReadAddress() == (conflict ? 0x4000 : 0x0010),
                  "CPU did not reach DMC conflict read");
            for (int i = 0; i < 5 && conflictApu.hasDmcDmaRequest(); ++i)
                conflictNes.step(false);
            check(!conflictApu.hasDmcDmaRequest(), "DMC conflict fetch did not finish");
            conflictNes.step(false);
            check(conflictCpu.a == (conflict ? 0x41 : 0x40), "DMC/controller bus conflict value incorrect");
            check((conflictBus.cpuRead(0x4016) & 1) == (conflict ? 0 : 1),
                  "DMC/controller bus conflict shifted at the wrong time");
        }

        // O timer inicia em 427 e o buffer é consumido no oitavo bit.
        // Recargas seguintes repetem a cada oito períodos,
        // sempre solicitando a parada em put, nas duas fases de inicialização.
        constexpr unsigned periods[] = {428, 380, 340, 320, 286, 254, 226, 214,
                                        190, 160, 142, 128, 106, 84, 72, 54};
        for (unsigned seed = 0; seed < 2; ++seed)
        {
            for (unsigned rate = 0; rate < 16; ++rate)
            {
                NES reloadNes("");
                auto &reloadBus = reloadNes.getBus();
                auto &reloadApu = reloadNes.getApu();
                reloadApu.setAudioOutputSuppressed(true);
                reloadApu.cpuWrite(0x4010, 0x40 | rate);
                reloadApu.cpuWrite(0x4015, 0x10);
                reloadApu.completeDmcDma(0, 0);
                unsigned expected = 427 + 7 * periods[rate];
                if (((seed + expected + 1) & 1) == 0)
                    ++expected;
                unsigned requests = 0;
                for (unsigned tick = 0; tick <= expected + 16 * periods[rate]; ++tick)
                {
                    reloadBus.systemClockCounter = seed + tick + 1;
                    reloadApu.step();
                    if (reloadApu.hasDmcDmaRequest())
                    {
                        check(tick == expected + requests * 8 * periods[rate],
                              "DMC reload cadence or halt phase incorrect");
                        check((reloadBus.systemClockCounter & 1) != 0, "DMC reload requested on get");
                        ++requests;
                        reloadApu.completeDmcDma(0, 0);
                    }
                }
                check(requests == 3, "DMC reload request missing");
            }
        }

        for (unsigned seed = 0; seed < 2; ++seed)
        {
            NES reloadNes("");
            auto &reloadCpu = reloadNes.getCpu();
            auto &reloadBus = reloadNes.getBus();
            auto &reloadApu = reloadNes.getApu();
            while (!reloadCpu.complete())
                reloadNes.step(false);
            reloadBus.ram->write(0, 0x4C); // JMP $0000: somente leituras.
            reloadBus.ram->write(1, 0);
            reloadBus.ram->write(2, 0);
            reloadCpu.pc = 0;
            if (seed)
                reloadNes.step(false);
            reloadApu.cpuWrite(0x4010, 0x4F);
            reloadApu.cpuWrite(0x4015, 0x10);
            reloadApu.completeDmcDma(0, 0);
            for (int i = 0; i < 4000 && !reloadApu.hasDmcDmaRequest(); ++i)
                reloadNes.step(false);
            check(reloadApu.hasDmcDmaRequest(), "DMC reload did not start");
            std::ostringstream before;
            reloadCpu.saveState(before);
            for (int i = 0; i < 4; ++i)
            {
                reloadNes.step(false);
                std::ostringstream during;
                reloadCpu.saveState(during);
                check(during.str() == before.str(), "DMC reload did not halt CPU for four cycles");
            }
            check(!reloadApu.hasDmcDmaRequest(), "DMC reload get missing");
            reloadNes.step(false);
            std::ostringstream after;
            reloadCpu.saveState(after);
            check(after.str() != before.str(), "CPU did not resume after DMC reload");
        }
        std::cout << "CPU bus tests passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}