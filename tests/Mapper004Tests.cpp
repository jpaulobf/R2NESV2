#include "Core/NES.h"
#include "Core/Cartridge/Mappers/Mapper004.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace R2NES::Core;
static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static std::string state(Mapper &m) { std::stringstream s; m.saveState(s); return s.str(); }
static void write(Mapper &m, uint16_t addr, uint8_t value)
{
    uint32_t mapped = 0;
    check(m.cpuMapWrite(addr, mapped, value, 0) && mapped == 0xFFFFFFFF, "Register write not handled internally");
}
static uint32_t prg(Mapper &m, uint16_t addr)
{
    uint32_t mapped = 0; uint8_t data = 0; check(m.cpuMapRead(addr, mapped, data), "PRG mapping rejected"); return mapped;
}
static uint32_t chr(Mapper &m, uint16_t addr)
{
    uint32_t mapped = 0; uint8_t data = 0; check(m.ppuMapRead(addr, mapped, data, 0), "CHR mapping rejected"); return mapped;
}
static void edge(Mapper &m, unsigned ticks = 3)
{
    m.onPpuAddress(0, 0); for (unsigned i = 0; i < ticks; ++i) m.tick(); m.onPpuAddress(0x1000, 0);
}
static std::vector<uint8_t> rom(unsigned prgBanks = 2, unsigned chrBanks = 1, bool nes2 = true,
                                unsigned sub = 0, unsigned ram = 7, bool four = false)
{
    std::vector<uint8_t> b(16 + prgBanks * 0x4000 + chrBanks * 0x2000, 0xEA);
    std::fill(b.begin(), b.begin() + 16, 0);
    b[0]='N'; b[1]='E'; b[2]='S'; b[3]=0x1A; b[4]=prgBanks; b[5]=chrBanks;
    b[6]=0x40 | (four ? 8 : 0) | ((ram & 0xF0) ? 2 : 0);
    b[7]=nes2 ? 8 : 0; b[8]=nes2 ? sub << 4 : 0;
    b[10]=nes2 ? ram : 0; b[11]=nes2 && !chrBanks ? 7 : 0;
    if (prgBanks)
    {
        const auto end = 16 + prgBanks * 0x4000;
        b[end-0x2000]=0x4C; b[end-0x1FFF]=0; b[end-0x1FFE]=0xE0;
        b[end-6]=0; b[end-5]=0xE0; b[end-4]=0; b[end-3]=0xE0; b[end-2]=0; b[end-1]=0xE0;
    }
    return b;
}
static std::filesystem::path saveRom(const std::filesystem::path &dir, const std::vector<uint8_t> &b)
{
    const auto path = dir / "mapper004-fixture.nes";
    std::ofstream f(path, std::ios::binary); f.write(reinterpret_cast<const char *>(b.data()), b.size());
    check(f.good(), "ROM fixture write failed"); return path;
}
static uint8_t readRam(Mapper &m, uint16_t addr, bool handled = true, uint8_t open = 0xAB)
{
    uint32_t mapped = 0; uint8_t data = open;
    check(m.cpuMapRead(addr, mapped, data) == handled, "RAM/open bus mapping incorrect");
    check(!handled || mapped == 0xFFFFFFFF, "RAM mapped to ROM");
    return data;
}
static void banking()
{
    for (unsigned p : {1, 2, 4, 8, 16, 32})
    for (unsigned c : {0, 1, 2, 4, 8, 16, 32})
    {
        Mapper004 m(p, c, MirrorMode::HORIZONTAL);
        const unsigned chrPages = c ? c * 8 : 8;
        for (unsigned mode : {0, 0x40, 0x80, 0xC0})
        for (unsigned value = 0; value < 256; ++value)
        {
            for (unsigned reg = 0; reg < 8; ++reg) { write(m, 0x9FFE, mode | reg); write(m, 0x9FFF, value); }
            for (uint16_t addr : {0x8000,0x9FFF,0xA000,0xBFFF,0xC000,0xDFFF,0xE000,0xFFFF})
            {
                const unsigned slot = (addr - 0x8000) / 0x2000;
                const unsigned bank = slot == 3 ? p * 2 - 1 :
                    slot == ((mode & 0x40) ? 0 : 2) ? p * 2 - 2 : value & (p * 2 - 1);
                check(prg(m, addr) == bank * 0x2000 + (addr & 0x1FFF), "PRG mode/bank/alias/boundary incorrect");
            }
            for (unsigned slot = 0; slot < 8; ++slot)
            {
                const unsigned logical = slot ^ ((mode & 0x80) ? 4 : 0);
                const unsigned bank = logical < 4 ? ((value & 0xFE) + (logical & 1)) % chrPages : value % chrPages;
                for (unsigned offset : {0, 0x3FF})
                {
                    uint32_t mapped = 0;
                    const uint16_t addr = slot * 0x400 + offset;
                    check(chr(m, addr) == bank * 0x400 + offset, "CHR inversion/alignment/boundary incorrect");
                    check(m.ppuMapWrite(addr, mapped, 1, 0) == (c == 0), "CHR ROM writable / RAM rejected");
                    if (!c) check(mapped == bank * 0x400 + offset, "CHR RAM read/write disagree");
                }
            }
        }
        write(m, 0xBFFE, 0); check(m.getMirrorMode() == MirrorMode::VERTICAL, "Vertical mirroring failed");
        write(m, 0xBFFE, 1); check(m.getMirrorMode() == MirrorMode::HORIZONTAL, "Horizontal mirroring failed");
    }
    // Distinct register values ensure R0..R7 are routed independently.
    Mapper004 m(32, 32, MirrorMode::VERTICAL);
    const unsigned values[8] = {10,20,30,40,50,60,7,9};
    for (unsigned i=0;i<8;++i) { write(m,0x8000,i); write(m,0x8001,values[i]); }
    const unsigned expected[8] = {10,11,20,21,30,40,50,60};
    for(unsigned i=0;i<8;++i) check(chr(m,i*0x400)==expected[i]*0x400, "CHR register routing incorrect");
    check(prg(m,0x8000)==7*0x2000 && prg(m,0xA000)==9*0x2000, "R6/R7 routing incorrect");
}
static void irq()
{
    for (unsigned sub : {0,1,2,4})
    {
        Mapper004::Configuration config; config.submapper=sub; if(sub==1)config.prgRamSize=1024;
        Mapper004 m(2,1,MirrorMode::HORIZONTAL,config);
        write(m,0xC000,1); write(m,0xE001,0);
        edge(m,2); check(!m.getIrqFlag(),"Short A12 interval clocked counter");
        edge(m); check(!m.getIrqFlag(),"Reload immediately triggered nonzero IRQ");
        m.onPpuAddress(0x1001,0); check(!m.getIrqFlag(),"Repeated high A12 clocked counter");
        edge(m); check(m.getIrqFlag(),"Decrement to zero did not assert IRQ");
        m.clearIrqFlag(); check(m.getIrqFlag(),"CPU acknowledgment cleared IRQ source");
        write(m,0xE000,0); check(!m.getIrqFlag(),"E000 did not acknowledge IRQ");
        write(m,0xC000,0); write(m,0xC001,0); check(!m.getIrqFlag(),"C001 asserted IRQ immediately");
        write(m,0xE001,0); edge(m); check(m.getIrqFlag(),"Explicit zero reload should assert IRQ");
        write(m,0xE000,0); write(m,0xE001,0); edge(m);
        check(m.getIrqFlag()==(sub!=1&&sub!=4),"Sharp/NEC/MMC6 natural zero reload distinction incorrect");
        m.reset(); write(m,0xC000,1); edge(m); edge(m); write(m,0xE001,0);
        check(!m.getIrqFlag(),"Enable asserted IRQ without a clock"); edge(m); check(!m.getIrqFlag(),"Counter stopped while disabled");
        edge(m); check(m.getIrqFlag(),"Disabled counter did not continue");
    }
    Mapper004::Configuration config; config.submapper=3;
    Mapper004 acc(2,1,MirrorMode::HORIZONTAL,config);
    write(acc,0xC000,1); write(acc,0xC001,0); write(acc,0xE001,0);
    acc.onPpuAddress(0x1000,0);
    for(unsigned i=0;i<9;++i) { acc.onPpuAddress(0,0); check(acc.getIrqFlag()==(i==8),"MC-ACC falling edge / divide-by-eight incorrect"); acc.onPpuAddress(0x1000,0); }
}
static void ram()
{
    Mapper004 m(2,1,MirrorMode::HORIZONTAL);
    for(unsigned addr=0x6000;addr<0x8000;++addr) write(m,addr,addr^(addr>>8));
    write(m,0xA001,0xC0); write(m,0x6000,0);
    check(readRam(m,0x6000)==0x60,"Protected RAM changed");
    write(m,0xA001,0); check(readRam(m,0x6000,false)==0xAB,"Disabled RAM drove open bus");
    write(m,0xA001,0x80); m.reset(); check(readRam(m,0x6000)==0x60,"Reset cleared PRG RAM");
    Mapper004::Configuration config; config.prgRamSize=0;
    Mapper004 noRam(2,1,MirrorMode::HORIZONTAL,config);
    write(noRam,0xA001,0x80); check(readRam(noRam,0x6000,false)==0xAB,"Absent RAM invented");
    config.submapper=1; config.prgNvRamSize=1024;
    Mapper004 mmc6(2,1,MirrorMode::HORIZONTAL,config);
    write(mmc6,0xA001,0xF0); write(mmc6,0x8000,0x20);
    check(readRam(mmc6,0x7000,false)==0xAB,"A001 accepted while MMC6 RAM disabled");
    write(mmc6,0xA001,0xF0);
    for(unsigned i=0;i<1024;++i) write(mmc6,0x7000+i,i^(i>>8));
    for(unsigned i=0;i<4096;++i) check(readRam(mmc6,0x7000+i)==uint8_t((i&1023)^((i&1023)>>8)),"MMC6 mirror incorrect");
    check(readRam(mmc6,0x6000,false)==0xAB,"MMC6 exposed RAM below $7000");
    for(unsigned bits=0;bits<16;++bits)
    {
        write(mmc6,0xA001,bits<<4);
        const bool lowRead=bits&2,highRead=bits&8;
        const auto lowBefore=readRam(mmc6,0x7000,lowRead||highRead);
        const auto highBefore=readRam(mmc6,0x7200,lowRead||highRead);
        write(mmc6,0x7000,0xA5); write(mmc6,0x7200,0x5A);
        const uint8_t low=readRam(mmc6,0x7000,lowRead||highRead),high=readRam(mmc6,0x7200,lowRead||highRead);
        check(low==(lowRead ? (bits&1 ? 0xA5 : lowBefore) : (highRead ? 0 : 0xAB)),"MMC6 lower half permissions incorrect");
        check(high==(highRead ? (bits&4 ? 0x5A : highBefore) : (lowRead ? 0 : 0xAB)),"MMC6 upper half permissions incorrect");
    }
    write(mmc6,0x8000,0); write(mmc6,0x8000,0x20);
    check(readRam(mmc6,0x7000,false)==0xAB,"MMC6 disable did not clear permissions");
}
static void states()
{
    for(unsigned sub=0;sub<=5;++sub)
    for(bool four : {false,true})
    {
        Mapper004::Configuration config;config.submapper=sub;if(sub==1)config.prgRamSize=1024;
        Mapper004 m(2,1,four?MirrorMode::FOUR_SCREEN:MirrorMode::HORIZONTAL,config);
        write(m,0x8000,0x26);write(m,0x8001,3);write(m,0xA001,sub==1?0xF0:0x80);write(m,0x7000,0xA5);
        uint32_t mapped=0;m.ppuMapWrite(0x2800,mapped,0x55,0);edge(m,2);
        const auto valid=state(m);
        check(valid.size()==8219+(sub==1?2:sub==3||sub==5?1:0)+(four?2048:0),"Legacy payload length changed");
        m.reset();std::stringstream saved(valid);m.loadState(saved);
        check(saved.good()&&state(m)==valid,"State round trip failed");
        for(size_t size : {size_t(0),size_t(1),size_t(26),size_t(100),valid.size()-1})
        {std::stringstream partial(valid.substr(0,size));m.loadState(partial);check(partial.fail()&&state(m)==valid,"Partial mapper state committed");}
        for(size_t field : {size_t(0),size_t(1),size_t(2),size_t(3),size_t(7),size_t(15),size_t(19),size_t(21),size_t(24),size_t(26)})
        {
            auto bad=valid;bad[field]=char(255);std::stringstream damaged(bad);m.loadState(damaged);
            check(damaged.fail()&&state(m)==valid,"Invalid mapper state accepted or changed state");
        }
    }
}
static void scrambling()
{
    Mapper004::Configuration config; config.submapper=5;
    Mapper004 m(32,32,MirrorMode::HORIZONTAL,config);
    write(m,0x8000,6);write(m,0x8001,2);
    check(prg(m,0x8000)==16*0x2000,"T9552 power-on PRG pattern incorrect");
    write(m,0x8000,2);write(m,0x8001,4);
    check(chr(m,0x1000)==8*0x400,"T9552 power-on CHR pattern incorrect");
    write(m,0x5000,2);
    check(prg(m,0x8000)==2*0x2000&&chr(m,0x1000)==4*0x400,"T9552 pattern 2 must be unscrambled");
    // Known address-line permutations from the NesDev table, independent of
    // the implementation's loop and table representation.
    const unsigned expectedPrg[4]={16,8,2,4}; // Input PRG A14.
    const unsigned expectedChr[8]={8,32,4,16,64,8,64,4}; // Input CHR A12.
    for(unsigned pattern=0;pattern<8;++pattern)
    {
        write(m,0x5FFF,pattern);
        check(prg(m,0x8000)==expectedPrg[pattern&3]*0x2000,"T9552 PRG permutation incorrect");
        check(chr(m,0x1000)==expectedChr[pattern]*0x400,"T9552 CHR permutation incorrect");
        const auto saved=state(m);write(m,0x5000,2);std::stringstream restore(saved);m.loadState(restore);
        check(restore.good()&&state(m)==saved,"T9552 pattern not restored");
    }
}
static void cpuIrq(const std::filesystem::path &dir)
{
    const auto prepare=[&](uint8_t opcode) {
        auto bytes=rom();const unsigned base=16+0x6000;
        bytes[base]=opcode;bytes[base+1]=0xEA;bytes[base+2]=0xEA;
        bytes[16+0x7FFE]=0;bytes[16+0x7FFF]=0xE1;
        return saveRom(dir,bytes);
    };
    const auto arm=[](NES &n) {
        auto &m=*n.getBus().cart->getMapper();write(m,0xC000,0);write(m,0xC001,0);write(m,0xE001,0);edge(m);
    };
    for(bool replay : {false,true})
    {
        NES n;n.insertCartridge(prepare(0x58).string());n.reset();for(unsigned i=0;i<7;++i)n.step();
        arm(n);n.step();n.step();check(n.getCpu().complete()&&n.getCpu().pc==0xE001,"CLI failed");
        n.step();n.step();check(n.getCpu().complete()&&n.getCpu().pc==0xE002,"IRQ did not allow instruction after CLI");
        if(replay){std::stringstream saved;check(n.saveState(saved),"IRQ snapshot failed");check(n.loadState(saved),"Pending IRQ restore failed");}
        write(*n.getBus().cart->getMapper(),0xE000,0); // A sampled request survives late deassertion.
        for(unsigned i=0;i<7;++i)n.step();check(n.getCpu().pc==0xE100,"Sampled IRQ lost or vector incorrect");
    }
    {NES n;n.insertCartridge(prepare(0x78).string());n.reset();for(unsigned i=0;i<7;++i)n.step();
     n.getCpu().SetFlag(CPU::I,false);arm(n);n.step();n.step();check(n.getCpu().GetFlag(CPU::I)==1,"SEI failed");
     for(unsigned i=0;i<7;++i)n.step();check(n.getCpu().pc==0xE100,"SEI incorrectly canceled sampled IRQ");}
    for(bool wasMasked : {false,true})
    {
        NES n;n.insertCartridge(prepare(0x28).string());n.reset();for(unsigned i=0;i<7;++i)n.step();
        n.getCpu().stkp=0xFC;n.getCpu().SetFlag(CPU::I,wasMasked);
        n.getBus().cpuWrite(0x1FD,wasMasked?0x20:0x24);arm(n);
        for(unsigned i=0;i<4;++i)n.step();
        if(wasMasked){n.step();n.step();check(n.getCpu().pc==0xE002&&n.getCpu().complete(),"PLP clearing I skipped next instruction");}
        for(unsigned i=0;i<7;++i)n.step();check(n.getCpu().pc==0xE100,"PLP IRQ polling used final I instead of prior I");
    }
    {NES n;n.insertCartridge(prepare(0x40).string());n.reset();for(unsigned i=0;i<7;++i)n.step();
     n.getCpu().stkp=0xFA;n.getBus().cpuWrite(0x1FB,0x20);n.getBus().cpuWrite(0x1FC,0x10);n.getBus().cpuWrite(0x1FD,0xE0);arm(n);
     for(unsigned i=0;i<6;++i)n.step();check(n.getCpu().pc==0xE010,"RTI return incorrect");
     for(unsigned i=0;i<7;++i)n.step();check(n.getCpu().pc==0xE100,"RTI did not use restored I for polling");}
    // An IRQ that arrives after STA's polling point must wait through a NOP.
    {auto bytes=rom();const uint8_t program[]={0x8D,0,0,0xEA,0xEA};std::copy(std::begin(program),std::end(program),bytes.begin()+16+0x6000);
     bytes[16+0x7FFE]=0;bytes[16+0x7FFF]=0xE1;NES n;n.insertCartridge(saveRom(dir,bytes).string());n.reset();
     for(unsigned i=0;i<7;++i)n.step();n.getCpu().SetFlag(CPU::I,false);for(unsigned i=0;i<3;++i)n.step();arm(n);n.step();
     n.step();n.step();check(n.getCpu().pc==0xE004&&n.getCpu().complete(),"Late IRQ skipped following instruction");
     for(unsigned i=0;i<7;++i)n.step();check(n.getCpu().pc==0xE100,"Late IRQ never accepted");}
    // A branch samples before displacement fetch, not on opcode fetch or
    // on the extra taken cycle. Crossing a page adds a PCH-fixup polling point.
    for(unsigned scenario=0;scenario<4;++scenario)
    {
        auto bytes=rom();const unsigned start=scenario==3?0xE0FD:0xE000;
        bytes[16+start-0x8000]=0xD0;bytes[16+start-0x8000+1]=2;
        bytes[16+0x7FFC]=start&255;bytes[16+0x7FFD]=start>>8;
        bytes[16+0x7FFE]=0;bytes[16+0x7FFF]=0xE2;
        NES n;n.insertCartridge(saveRom(dir,bytes).string());n.reset();for(unsigned i=0;i<7;++i)n.step();
        n.getCpu().SetFlag(CPU::I,false);n.getCpu().SetFlag(CPU::Z,scenario==0);
        if(scenario==1)arm(n);n.step(); // Pulse during opcode fetch only.
        if(scenario==1)write(*n.getBus().cart->getMapper(),0xE000,0);
        if(scenario==0)arm(n);n.step();
        if(scenario>=2)arm(n); // Arrives after displacement polling.
        if(scenario!=0)n.step();
        if(scenario==3)n.step();
        if(scenario==1)
        {
            n.step();n.step();check(n.getCpu().complete()&&n.getCpu().pc==0xE005,"Branch incorrectly polled on opcode fetch");
            continue;
        }
        if(scenario==2){n.step();n.step();check(n.getCpu().pc==0xE005&&n.getCpu().complete(),"Taken branch polled again without page cross");}
        for(unsigned i=0;i<7;++i)n.step();check(n.getCpu().pc==0xE200,"Branch displacement/PCH IRQ polling failed");
    }
}
static void integration(const std::filesystem::path &dir)
{
    NES n;n.insertCartridge(saveRom(dir,rom(2,1,true,0,7,true)).string());n.reset();auto& bus=n.getBus();
    auto& ppu=n.getPpu();
    for(unsigned table=0;table<4;++table) ppu.ppuWrite(0x2000+table*0x400,table+1);
    for(unsigned table=0;table<4;++table) check(ppu.ppuRead(0x2000+table*0x400)==table+1,"Four-screen tables overlap");
    check(ppu.ppuRead(0x3800)==3,"Four-screen $3000 mirror incorrect");
    bus.cpuWrite(0xA000,0);check(bus.cart->getMirrorMode()==MirrorMode::FOUR_SCREEN,"A000 changed four-screen");
    std::stringstream saved;check(n.saveState(saved),"Console save failed");const auto valid=saved.str();
    ppu.ppuWrite(0x2800,0);std::stringstream restore(valid);check(n.loadState(restore)&&ppu.ppuRead(0x2800)==3,"Four-screen state restore failed");
    std::stringstream current;check(n.saveState(current),"Console save failed");
    const size_t mapperOffset=valid.size()-state(*bus.cart->getMapper()).size();
    auto bad=valid;bad[mapperOffset]=char(255);std::stringstream damaged(bad);
    check(!n.loadState(damaged),"Console accepted bad mapper state");
    std::stringstream after;check(n.saveState(after)&&after.str()==current.str(),"Console rollback failed");
    bus.cpuWrite(0xA001,0);bus.cpuWrite(0,0xAB);check(bus.cpuRead(0x6000)==0xAB,"CPU open bus not preserved");
    // Header RAM declarations, fixed mirroring, trainer, CHR protection.
    for(unsigned sub : {0,1,2,3,4,5})
    {
        auto bytes=rom(2,1,true,sub,sub==1?4:7);bytes[6]|=4;bytes.insert(bytes.begin()+16,512,0x55);
        Cartridge cart(saveRom(dir,bytes).string());check(cart.isValid(),"Supported submapper/trainer rejected");
        if(sub==1){cart.cpuWrite(0x8000,0x20,0);cart.cpuWrite(0xA001,0xF0,0);}
        uint8_t data=0;check(cart.cpuRead(0x7000,data)&&data==0x55,"Trainer not installed");
        cart.cpuWrite(0xA000,0,0);check(cart.getMirrorMode()==(sub==2?MirrorMode::HORIZONTAL:MirrorMode::VERTICAL),"Fixed/dynamic mirroring incorrect");
        const auto before=chr(*cart.getMapper(),0);check(!cart.ppuWrite(0,0,0)&&chr(*cart.getMapper(),0)==before,"CHR ROM writable");
    }
}
static void invalid(const std::filesystem::path &dir)
{
    const auto reject=[&](const std::vector<uint8_t>& bytes){Cartridge c(saveRom(dir,bytes).string());check(!c.isValid()&&!c.getMapper(),"Unsupported/truncated ROM accepted");};
    for(unsigned p : {0,3,64})reject(rom(p));for(unsigned c : {3,64})reject(rom(2,c));
    reject(rom(2,1,true,6));reject(rom(2,1,true,15));reject(rom(2,1,true,1,7));
    for(unsigned ram : {1,4,6,8,0x77})reject(rom(2,1,true,0,ram));
    for(bool nes2 : {false,true}) {auto b=rom(2,2,nes2);b.pop_back();reject(b);b=rom(2,2,nes2);b.resize(17);reject(b);}
    auto b=rom(2,0);b[11]=8;reject(b);b=rom(2,1);b[11]=7;reject(b);
    b=rom(2,1,true,0,0);b[6]|=4;b.insert(b.begin()+16,512,0);reject(b);
    b=rom(2,1);b[14]=1;reject(b);b=rom(2,1);b[8]|=1;reject(b);
    for(unsigned mapper : {187,189})
    {
        b=rom(64,1,false);b[6]=(mapper&15)<<4;b[7]=mapper&0xF0;reject(b);
        b=rom(4,1,false);b[6]=(mapper&15)<<4;b[7]=mapper&0xF0;b.resize(16+3*0x4000);reject(b);
    }
    Mapper004::Configuration config;
    check(!Mapper004::supports(0,1,MirrorMode::HORIZONTAL,config),"Zero PRG supported");
    bool failed=false;try{Mapper004 m(0,1,MirrorMode::HORIZONTAL);}catch(const std::invalid_argument&){failed=true;}
    check(failed,"Direct invalid mapper accepted");
}
static int testRom(const std::filesystem::path &path, const std::filesystem::path &directory)
{
    auto loadPath=path;
    if(path.filename()=="6-MMC3_alt.nes" || path.filename()=="6-MMC6.nes")
    {
        std::ifstream input(path,std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        check(bytes.size()>=16,"Variant test ROM truncated");
        const bool mmc6=path.filename()=="6-MMC6.nes";
        // These distributed ROMs have ambiguous iNES headers. Select the
        // hardware they test in a generated copy; never change source ROMs.
        bytes[7]=(bytes[7]&0xF0)|8;bytes[8]=(mmc6?1:4)<<4;
        bytes[9]=0;bytes[10]=mmc6?4:7;bytes[11]=bytes[5]?0:7;
        bytes[6]&=~2;bytes[12]=0;bytes[13]=0;bytes[14]=0;bytes[15]=0;
        loadPath=saveRom(directory,bytes);
    }
    NES n;n.insertCartridge(loadPath.string());n.reset();check(n.isCartridgeLoaded(),"Test ROM not loaded");
    for(unsigned ticks=0;ticks<30000000;++ticks)
    {
        n.step();
        if((ticks&1023)==0 && n.cpuRead(0x6001)==0xDE&&n.cpuRead(0x6002)==0xB0&&n.cpuRead(0x6003)==0x61&&n.cpuRead(0x6000)<0x80)
        {
            const int result=n.cpuRead(0x6000);std::cout<<path.filename().string()<<" status="<<result<<" ticks="<<ticks<<"\n";
            for(unsigned addr=0x6004;addr<0x6200;++addr){const char c=n.cpuRead(addr);if(!c)break;std::cout<<c;}
            std::cout<<"\n";return result;
        }
        if((ticks&65535)==0)
        {
            std::string text;
            for(unsigned i=0;i<960;++i){const uint8_t c=n.getPpu().getVram().readRaw(i);text+=c>=32&&c<127?char(c):' ';}
            if(text.find("Passed")!=std::string::npos){std::cout<<path.filename().string()<<" Passed (screen)\n";return 0;}
        }
    }
    std::string text;
    for(unsigned i=0;i<960;++i){const uint8_t c=n.getPpu().getVram().readRaw(i);text+=c>=32&&c<127?char(c):' ';}
    std::cout<<path.filename().string()<<" screen: "<<text<<"\n";
    return text.find("Passed")!=std::string::npos?0:255;
}
int main(int argc,char **argv)
{
    try
    {
        check(argc>=2,"Pass a directory for generated test ROMs");std::filesystem::create_directories(argv[1]);
        if(argc>2){int result=0;for(int i=2;i<argc;++i)result|=testRom(argv[i],argv[1]);return result;}
        banking();irq();ram();states();scrambling();cpuIrq(argv[1]);integration(argv[1]);invalid(argv[1]);
        std::cout<<"Mapper 4 synthetic tests passed\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}