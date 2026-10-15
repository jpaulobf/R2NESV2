#include "Core/Cartridge/Mappers/Mapper009.h"
#include "Core/NES.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace R2NES::Core;
static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static std::string state(Mapper &m) { std::stringstream s; m.saveState(s); return s.str(); }
static void write(Mapper009 &m, uint16_t addr, uint8_t data)
{
    uint32_t mapped = 0;
    check(m.cpuMapWrite(addr, mapped, data, 0) && mapped == 0xFFFFFFFF, "Register not handled internally");
}
static uint32_t chr(Mapper009 &m, uint16_t addr, bool peek = false)
{
    uint32_t mapped = 0; uint8_t data = 0;
    check(peek ? m.ppuMapPeek(addr, mapped, data, 0) : m.ppuMapRead(addr, mapped, data, 0), "CHR read rejected");
    return mapped;
}
static void configure(Mapper009 &m)
{
    write(m, 0xB000, 1); write(m, 0xC000, 2); write(m, 0xD000, 3); write(m, 0xE000, 4);
}
static void banking()
{
    for (uint16_t prg : {2, 4, 8})
    for (uint16_t count : {1, 2, 4, 8, 16})
    {
        Mapper009 m(prg, count, MirrorMode::HORIZONTAL);
        uint32_t mapped = 0; uint8_t data = 0;
        for (uint16_t reg : {0xA000, 0xAFFF})
        for (unsigned value = 0; value < 256; ++value)
        {
            write(m, reg, value);
            for (uint16_t addr : {0x8000,0x9FFF,0xA000,0xBFFF,0xC000,0xDFFF,0xE000,0xFFFF})
            {
                const unsigned slot = (addr - 0x8000) / 8192;
                const unsigned bank = slot ? prg * 2 - 4 + slot : value & (prg * 2 - 1);
                check(m.cpuMapRead(addr, mapped, data) && mapped == bank * 8192 + (addr & 8191), "PRG bank/window mismatch");
            }
        }
        for (uint16_t reg : {0xB000, 0xBFFF, 0xC000, 0xCFFF, 0xD000, 0xDFFF, 0xE000, 0xEFFF})
        for (unsigned value = 0; value < 256; ++value)
        {
            const bool upper = reg >= 0xD000;
            const bool fd = (reg & 0xF000) == 0xB000 || (reg & 0xF000) == 0xD000;
            chr(m, upper ? (fd ? 0x1FD8 : 0x1FE8) : (fd ? 0x0FD8 : 0x0FE8));
            write(m, reg, value);
            for (unsigned offset : {0u, 0xFFFu})
                check(chr(m, (upper ? 0x1000 : 0) + offset, true) == (value & (count * 2 - 1)) * 4096 + offset,
                      "CHR register/mask mismatch");
        }
        for (uint16_t reg : {0xF000, 0xFFFF})
        for (unsigned value = 0; value < 256; ++value)
        {
            write(m, reg, value);
            check(m.getMirrorMode() == ((value & 1) ? MirrorMode::HORIZONTAL : MirrorMode::VERTICAL), "Mirroring bit mismatch");
        }
        const auto before = state(m);
        for (uint16_t addr : {0x0000, 0x5FFF, 0x6000, 0x7FFF, 0x8000, 0x9FFF})
        {
            if (addr < 0x8000) check(!m.cpuMapRead(addr, mapped, data), "Phantom PRG RAM");
            check(!m.cpuMapWrite(addr, mapped, 0xFF, 0) && state(m) == before, "Write to unmapped register");
        }
        check(!m.ppuMapRead(0x2000, mapped, data, 0) && !m.ppuMapPeek(0x2000, mapped, data, 0), "CHR outside pattern space");
        check(!m.getIrqFlag(), "MMC2 generated IRQ");
    }
}
static void latches()
{
    Mapper009 m(8, 16, MirrorMode::VERTICAL);
    for (bool initialFD : {false, true})
    for (unsigned addr = 0; addr < 8192; ++addr)
    {
        m.reset(); configure(m);
        if (initialFD) { chr(m, 0x0FD8); chr(m, 0x1FD8); }
        const unsigned oldLow = initialFD ? 1 : 2, oldHigh = initialFD ? 3 : 4;
        check(chr(m, addr) == (addr < 4096 ? oldLow : oldHigh) * 4096 + (addr & 4095), "Trigger read used new bank");
        const unsigned low = addr == 0x0FD8 ? 1 : addr == 0x0FE8 ? 2 : oldLow;
        const unsigned high = addr >= 0x1FD8 && addr <= 0x1FDF ? 3 : addr >= 0x1FE8 && addr <= 0x1FEF ? 4 : oldHigh;
        check(chr(m, 0, true) == low * 4096 && chr(m, 0x1000, true) == high * 4096, "Incorrect latch trigger or cross-window effect");
        const auto before = state(m);
        chr(m, addr, true);
        uint32_t mapped = 0;
        check(!m.ppuMapWrite(addr, mapped, 0xFF, 0) && state(m) == before, "Peek/write changed latch or ROM");
    }
    m.reset(); configure(m);
    write(m, 0xB000, 15); // Banco inativo só fica visível após o disparo.
    check(chr(m, 0, true) == 2 * 4096, "Inactive CHR register became visible");
    chr(m, 0x0FD8);
    check(chr(m, 0, true) == 15 * 4096, "Inactive CHR register was lost");
}
static void states()
{
    Mapper009 m(8, 16, MirrorMode::VERTICAL);
    configure(m); write(m, 0xA000, 15); write(m, 0xF000, 1); chr(m, 0x0FD8);
    const auto before = state(m);
    check(before.size() == 8207 && before.substr(0, 15) == std::string("\x0F\x01\x02\x03\x04\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00", 15), "Legacy mapper layout changed");
    m.reset(); std::stringstream saved(before); m.loadState(saved);
    check(saved.good() && state(m) == before && chr(m, 0, true) == 4096, "Mapper restore failed");
    for (size_t length = 0; length < before.size(); ++length)
    {
        auto bytes = before.substr(0, length); if (!bytes.empty()) bytes[0] = 0;
        std::stringstream broken(bytes); m.loadState(broken);
        check(broken.fail() && state(m) == before, "Truncated state partially committed");
    }
    for (unsigned field = 0; field < 15; ++field)
    {
        auto bytes = before; bytes[field] = char(0xFF);
        std::stringstream broken(bytes); m.loadState(broken);
        check(broken.fail() && state(m) == before, "Corrupt register/latch/mirror accepted");
    }
    auto legacy = before; std::fill(legacy.begin() + 15, legacy.end(), char(0xA5));
    std::stringstream old(legacy); m.loadState(old);
    check(old.good() && state(m) == before, "Legacy phantom RAM payload not consumed");
    for (uint16_t count : {1, 2, 4, 8})
    {
        Mapper009 small(2, count, MirrorMode::VERTICAL);
        std::stringstream input(before); small.loadState(input);
        check(input.good() && chr(small, 0, true) == 4096, "Raw register bits rejected for smaller physical ROM");
    }
}
static std::vector<uint8_t> rom(unsigned prg = 8, unsigned chrCount = 16, bool nes2 = true)
{
    std::vector<uint8_t> bytes(16 + prg * 16384 + chrCount * 8192, 0);
    bytes[0]='N'; bytes[1]='E'; bytes[2]='S'; bytes[3]=0x1A;
    bytes[4]=prg; bytes[5]=chrCount; bytes[6]=0x90; bytes[7]=nes2 ? 8 : 0;
    for (unsigned bank=0; bank < prg*2; ++bank)
        std::fill(bytes.begin()+16+bank*8192, bytes.begin()+16+(bank+1)*8192, bank);
    for (unsigned bank=0; bank < chrCount*2; ++bank)
        std::fill(bytes.begin()+16+prg*16384+bank*4096, bytes.begin()+16+prg*16384+(bank+1)*4096, 0x40+bank);
    return bytes;
}
static std::filesystem::path saveRom(const std::filesystem::path &dir, const std::vector<uint8_t> &bytes)
{
    const auto path=dir/"mapper009-fixture.nes";
    std::ofstream output(path, std::ios::binary); output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    check(output.good(), "Fixture write failed"); return path;
}
static void images(const std::filesystem::path &dir)
{
    for (bool nes2 : {false,true})
    for (unsigned prg : {2,4,8})
    for (unsigned count : {1,2,4,8,16})
    for (bool exponent : {false,true})
    {
        auto bytes=rom(prg,count,nes2);
        if (exponent)
        {
            if (!nes2) continue;
            unsigned p=14,c=13;
            for (unsigned n=prg;n>1;n>>=1) ++p;
            for (unsigned n=count;n>1;n>>=1) ++c;
            bytes[4]=p<<2; bytes[5]=c<<2; bytes[9]=0xFF;
        }
        Cartridge cart(saveRom(dir,bytes).string()); check(cart.isValid(), "Valid MMC2 image rejected");
        uint8_t data=0;
        check(cart.cpuWrite(0xAFFF,15,0) && cart.cpuRead(0x8000,data) && data==prg*2-1, "PRG register write/conflict mismatch");
        check(cart.cpuRead(0xA000,data) && data==prg*2-3, "Register mutated PRG ROM");
        check(cart.cpuWrite(0xC000,31,0) && cart.ppuRead(0,data,0) && data==0x40+count*2-1, "Cartridge CHR mapping mismatch");
        check(!cart.ppuWrite(0,0,0) && cart.ppuRead(0,data,0) && data==0x40+count*2-1, "CHR ROM writable");
    }
    auto reject=[&](const std::vector<uint8_t> &bytes) { Cartridge c(saveRom(dir,bytes).string()); check(!c.isValid()&&!c.getMapper(), "Invalid MMC2 image accepted"); };
    for (bool nes2 : {false,true})
    {
        for (unsigned prg : {0,1,3,16}) reject(rom(prg,16,nes2));
        for (unsigned count : {0,3,32}) reject(rom(8,count,nes2));
        auto bytes=rom(8,16,nes2); bytes.resize(16+65536); reject(bytes);
        bytes=rom(8,16,nes2); bytes.pop_back(); reject(bytes);
        for (uint8_t flag : {2,4,8}) { bytes=rom(8,16,nes2); bytes[6]|=flag; if(flag==4) bytes.insert(bytes.begin()+16,512,0); reject(bytes); }
    }
    for (uint8_t sub=1;sub<16;++sub) { auto bytes=rom(); bytes[8]=sub<<4; reject(bytes); }
    for (unsigned field : {10,11,14}) { auto bytes=rom(); bytes[field]=7; reject(bytes); }
    auto bytes=rom(); bytes[8]=1; reject(bytes); // Mapper 265 não pode virar 9.
    bytes=rom(); bytes[9]=0x0F; bytes[4]=(14<<2)|1; reject(bytes); // 48 KiB.
    bytes[4]=0xFF; reject(bytes);
    for (const auto config : {std::pair<unsigned,unsigned>{0,16},{1,16},{3,16},{16,16},{8,0},{8,3},{8,32},{256,16}})
    {
        bool failed=false; try { Mapper009 m(config.first,config.second,MirrorMode::HORIZONTAL); } catch(const std::invalid_argument &) { failed=true; }
        check(failed,"Invalid direct configuration accepted");
    }
    check(!Mapper009::supports(8,16,MirrorMode::FOUR_SCREEN), "Four-screen accepted");
}
static std::string consoleState(NES &nes) { std::stringstream s; check(nes.saveState(s),"Console save failed"); return s.str(); }
static void console(const std::filesystem::path &dir)
{
    NES nes; nes.insertCartridge(saveRom(dir,rom()).string()); check(nes.isCartridgeLoaded(),"Console insertion failed");
    auto &bus=nes.getBus(); auto &ppu=nes.getPpu();
    auto &mapper=*static_cast<Mapper009 *>(bus.cart->getMapper().get());
    configure(mapper); write(mapper,0xA000,7); chr(mapper,0x0FD8); chr(mapper,0x1FD8);
    const auto beforeViewer=state(mapper);
    check(ppu.getPatternTablePixels(0,0).size()==16384 && ppu.getPatternTablePixels(1,0).size()==16384,"Viewer image missing");
    check(state(mapper)==beforeViewer,"Pattern table viewer changed MMC2 latches");
    uint8_t data=0; check(bus.ppuReadSprite(0x0FE8,data) && data==0x41 && chr(mapper,0,true)==8192,"Sprite latch ordering mismatch");
    bus.cpuWrite(0x6000,0xA5); check(bus.cpuRead(0x6000)==0xA5,"Missing PRG RAM not open bus");
    bus.cpuRead(0x8000); check(bus.cpuRead(0x6000)==7,"Phantom RAM retained a write");
    for (unsigned i=0;i<90000;++i) ppu.clock(); // Ultrapassa o bloqueio inicial de PPUADDR.
    bus.cpuWrite(0x2006,0x0F); bus.cpuWrite(0x2006,0xD8);
    const auto beforeDebug=state(mapper); bus.cpuRead(0x2007,true);
    check(state(mapper)==beforeDebug,"Debugger PPUDATA read changed latch");
    bus.cpuRead(0x2007); check(chr(mapper,0,true)==4096,"Real PPUDATA read did not trigger latch");
    for (bool horizontal : {false,true})
    {
        write(mapper,0xF000,horizontal); ppu.ppuWrite(0x2000,0x31); ppu.ppuWrite(0x2C00,0x52);
        check(ppu.ppuRead(horizontal?0x2400:0x2800)==0x31 && ppu.ppuRead(horizontal?0x2800:0x2400)==0x52,"PPU H/V mirroring mismatch");
    }
    const auto before=consoleState(nes); const size_t start=before.size()-8207;
    for (size_t length : {start,start+7,start+15,before.size()-1})
    {
        std::stringstream broken(before.substr(0,length));
        check(!nes.loadState(broken)&&consoleState(nes)==before,"Truncated console load did not roll back");
    }
    auto damaged=before; damaged[start+5]=2;
    std::stringstream broken(damaged); check(!nes.loadState(broken)&&consoleState(nes)==before,"Corrupt latch did not roll back console");
    nes.reset(); std::stringstream restore(before);
    check(nes.loadState(restore)&&consoleState(nes)==before,"Console mapper/CIRAM restore failed");
}
int main(int argc,char **argv)
{
    try
    {
        check(argc==2,"Pass a directory for generated test ROMs"); std::filesystem::create_directories(argv[1]);
        banking(); latches(); states(); images(argv[1]); console(argv[1]);
        std::cout<<"Mapper 9 tests passed (PRG/CHR, latches, ROM protection, open bus, viewer, states, invalid images)\n";
        return 0;
    }
    catch(const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
