# Mapper 1 regression tests

Generated ROMs cover PRG/CHR modes and boundaries, serial bit order and fifth-write
address selection, consecutive writes (including real INC/RRA instructions with
and without CPU overclock), reset bit 7, first writes at cycles 0/1 and counter
wrap, dynamic/fixed mirroring, fixed PRG, CHR-RAM/ROM board variants and isolated
PRG RAM banks. Invalid configurations, truncated images/states and corrupt serial
registers are rejected. Console tests verify rollback after failed loads and the
CPU open-bus latch, including side-effect-free debugger reads.
NMI tests cover edges on all four cycles of STA, a real PPUCTRL-triggered edge
during VBlank, the subsequent RAM shadow store, and pending-NMI state restore.
An edge on the last cycle must wait until the following instruction has run.

```powershell
cmake --build build --target mapper001-tests -j 4
ctest --test-dir build -R '^mapper001$' --output-on-failure
```

Optionally pass a game ROM/ZIP as the second argument to `bin/mapper001-tests.exe`.
This boot smoke test runs 15,000,000 CPU cycles and checks continuing NMI entries
and visible video. Pugsley's Scavenger Hunt (`063E5653`) reached its opening
credits in 503 frames with 484 NMI entries after the CPU polling fix. This does
not establish complete gameplay compatibility or change the game's rating.
A separate 45,000,000-cycle run was visually checked at the title screen.
Timing reference: https://www.nesdev.org/wiki/CPU_interrupts

MMC1 supports power-of-two PRG sizes up to 512 KiB and CHR-ROM up to 128 KiB or
8 KiB CHR-RAM. CHR capacities up to 8 KiB leave the extra bank lines available
for SNROM/SOROM/SUROM/SXROM, including their CHR-ROM variants. SZROM uses CHR.4
for its two RAM banks. NES 2.0 PRG-RAM and PRG-NVRAM declarations are stored
separately, with a supported combined capacity of 0/8/16/32 KiB. No automatic
battery-file persistence is implemented. Ambiguous iNES headers retain the old
8 KiB default; specify RAM sizes explicitly for SOROM/SXROM/SZROM.

Submapper 5 has fixed 32 KiB PRG and submapper 7 uses header H/V mirroring.
Deprecated submappers 1/2/4 require matching memory sizes. Unsupported EEPROM
submapper 6, MMC1A submapper 3 (mapper 155), unknown variants and incompatible
memory capacities are rejected instead of approximated.

The mapper payload keeps its previous length; a reserved bit records whether
there has been a CPU write, including cycle zero. Old payloads remain readable.
Derived PRG banking is recomputed on load. The console writes format `R2NC`,
which adds CPU clock and data-bus state, and still reads `R2NB` using the saved
system clock and zero for its missing bus latch, when memory capacities match.
Old NES 2.0 states created with an incorrectly sized RAM payload are rejected.
Older builds cannot read `R2NC`.
Any failed console load restores its previous serialized state.

`reset()` is the emulator's deterministic complete initialization. Its MMC1
Control value `0x1E` is not a hardware power-up guarantee. A serial bit-7 reset
preserves CHR mode and mirroring while selecting PRG mode 3. Neither reset
clears PRG RAM. Game compatibility ratings are not inferred from these tests.

# Mapper 2 regression tests

Generated ROMs check all register values and PRG window boundaries for 1 through
256 banks, iNES and NES 2.0 submappers 0/1/2, AND conflicts against the current
switchable or fixed ROM bank, CHR-RAM, read-only CHR-ROM, fixed H/V mirroring,
save states and invalid/truncated images. Both linear and exponent/multiplier
NES 2.0 sizes are exercised, including 4 MiB PRG. No game ROM is required.

```powershell
cmake --build build --target mapper002-tests -j 4
ctest --test-dir build -R '^mapper002$' --output-on-failure
```

Mapper 2 accepts power-of-two PRG sizes from 16 KiB through 4 MiB and either
8 KiB CHR-RAM or 8 KiB CHR-ROM. NES 2.0 RAM declarations must match these
capacities; PRG RAM, CHR NVRAM and four-screen mirroring are unsupported.
Other mappers retain their existing eight-bit constructor limits; extended
sizes beyond those limits are rejected rather than silently truncated.

`Mapper002::reset()` selects bank zero for the complete initialization currently
performed by `NES::reset()`. This does not model the console's electrical warm
reset, which would require a separate system reset path. States retain the old
Windows five-byte layout (one bank byte plus four little-endian mirroring bytes).
Loading a truncated state, an out-of-range bank or a different hardware mirroring
mode fails without changing the mapper. Existing game ratings are not retested
by this synthetic suite.

# Mapper 3 regression tests

Generated ROMs exercise every register value at all PRG window boundaries for
16/32 KiB PRG and 8/16/32/64/128 KiB CHR-ROM, or 8 KiB CHR-RAM. Tests cover
iNES and NES 2.0 submappers 0/1/2, conflicts against the written PRG address,
physical CHR bank mirroring, read-only CHR-ROM (including PPUDATA), fixed H/V
mirroring, background/sprite reads, reset, legacy states and invalid images.
The Namcot variant has 2 KiB volatile PRG-RAM mirrored across $6000-$7FFF;
tests check all RAM addresses, trainer installation and state persistence.
Console tests verify RAM/bank restore and rollback after an invalid mapper load.

```powershell
cmake --build build --target mapper003-tests -j 4
ctest --test-dir build -R '^mapper003$' --output-on-failure
```

The default iNES/submapper 0 applies AND bus conflicts; NES 2.0 submapper 1
disables them and submapper 2 enables them. The ordinary latch retains two bits;
CHR capacities above 32 KiB retain four. Only physically connected bank lines
affect reads, so legacy latch values 2/3 remain valid with smaller CHR capacities.
CHR-RAM is supported as an 8 KiB extension with a fixed physical bank.
Reference: https://www.nesdev.org/wiki/CNROM
Submappers: https://www.nesdev.org/wiki/NES_2.0_submappers

NES 2.0 byte 10 must declare zero RAM or exactly 2 KiB volatile PRG-RAM (5).
Byte 11 must declare zero RAM with CHR-ROM, or exactly 8 KiB volatile CHR-RAM (7)
without CHR-ROM. Unsupported memory sizes, submappers, NVRAM, four-screen
mirroring, truncated data and declared Misc. ROMs are rejected. An iNES header
with explicit PRG-RAM size is rejected because its 8 KiB units cannot describe
the Namcot RAM: use NES 2.0. Trainers require declared PRG-RAM and are installed
at $7000. The M50805 speech chip and Mapper 185 protection remain unsupported;
security diode electrical effects are not modeled.

States without PRG-RAM keep the old five-byte Windows layout (latch plus fixed
32-bit little-endian mirroring). New PRG-RAM states append 2048 bytes. Invalid
or truncated loads leave the mapper intact. States from the formerly unsupported
PRG-RAM configuration lack that payload and are rejected. Complete emulator
initialization selects latch zero and preserves cartridge RAM; this does not
model the console's electrical warm reset. Synthetic tests do not retest or
establish commercial game compatibility ratings.

# Mapper 4 regression tests

```powershell
cmake --build build --target mapper004-tests -j 4
ctest --test-dir build -R '^mapper004' --output-on-failure
```

The synthetic suite checks all bank register values and PRG/CHR window boundaries,
CHR inversion and even 2 KiB banks, ROM write protection, PRG-RAM enable/protect,
MMC6's 1 KiB RAM mirrors and every combination of its half-chip permissions,
fixed mirroring, four-screen memory and its state restore, trainers, reset,
invalid configurations and atomic mapper state loading. CPU tests exercise CLI,
SEI, PLP, RTI, branch polling (including page crossing), late IRQs and
pending-request persistence. Console failures roll
back the previously serialized state. Mappers 1/2/3 remain separate regressions.

When present, seven local ROMs from `nestest/blarg` are registered in CTest:
`1-clocking`, `2-details`, `3-A12_clocking`, `4-scanline_timing`, `5-MMC3`,
`6-MMC3_alt`, and `6-MMC6`. The two variant ROMs are copied into test directories
and given NES 2.0 headers selecting NEC/MMC6 respectively; source ROMs are intact.
Results use the $6000 test protocol, or the final nametable text for MMC6, whose
RAM does not occupy $6000. These tests passed; game ratings are not inferred.

Mapper 4 accepts power-of-two PRG sizes from 16 to 512 KiB and CHR-ROM from 8 to
256 KiB, or fixed-capacity 8 KiB CHR-RAM. NES 2.0 selects submappers 0 through 5:
Sharp MMC3, MMC6, hard-wired mirroring, MC-ACC, NEC MMC3, and T9552. Unsupported
submappers, RAM sizes, mixed volatile/nonvolatile PRG memory, CHR-NVRAM, Misc.
ROMs and truncated images are rejected. Regular PRG-RAM/NVRAM capacities are
0/8 KiB; MMC6 requires 1 KiB. Battery-backed memory participates in save states;
automatic .sav file persistence is still a separate, unimplemented feature.
Ambiguous iNES headers retain the MMC3 interpretation and historical 8 KiB RAM
default; use NES 2.0 to identify MMC6/MC-ACC/NEC/T9552 explicitly.

MMC6 uses $8000 bit 5 and the four $A001 permission bits. A disabled half reads
zero only if the other half is readable; with neither half readable, reads are
open bus. Disabling bit 5 clears permissions and prevents writes to $A001.
The zero-reload IRQ distinction follows the alternate behavior exercised by
the MMC6 and NEC tests. MC-ACC counts falling A12 edges through a divide-by-eight
prescaler; $C001 resets that prescaler. T9552 powers on with pattern zero and
implements all PRG/CHR scrambling patterns selected in $5000-$5FFF, using
Mapper 4.5's file order (pattern 2 is unscrambled).

The PPU exposes address changes from $2006/$2007 and separately presents the
rendering fetch address before data consumption, including the aborted CHR
fetch on the visible idle dot. This prevents extra inter-scanline clocks when
background uses $1000. IRQ polling belongs to CPU instruction cycles; accepting
an interrupt does not acknowledge the mapper. The default MMC3 A12 filter still
requires three M2 falling edges while low, including during DMA.

Default mapper payloads retain the legacy 8219-byte Windows layout with explicit
little-endian integers and canonical boolean bytes. The newly supported MMC6,
MC-ACC and T9552 append two, one and one variant bytes respectively. Four-screen
adds 2048 cartridge nametable bytes while keeping the PPU's existing 2 KiB CIRAM
state. Old four-screen states and old states for previously misidentified variants
lack the required data and are rejected. Complete initialization preserves the
cartridge RAM and selects deterministic banks; it is not electrical warm reset.
The consecutive-$C001 silicon reload glitch is not modeled by this revision.

Broader CPU/APU diagnostics `4-jitter`, `6-irq_flag_timing`,
`5-branch_delays_irq`, and `2-nmi_and_brk` still fail. The same ROMs were checked
with the pre-change CPU/NES sources: all four already failed. The first two
report late APU frame IRQ flags; the remaining tests include behavior beyond
this mapper revision. They are not claimed as passing by this suite.

References: https://www.nesdev.org/wiki/MMC3,
https://www.nesdev.org/wiki/MMC6,
https://www.nesdev.org/wiki/NES_2.0_submappers,
https://www.nesdev.org/wiki/T9552,
https://www.nesdev.org/wiki/PPU_rendering,
https://www.nesdev.org/wiki/CPU_interrupts

# Mapper 189 regression tests

Checks 32 KiB PRG switching, both data nibbles, independent MMC3 CHR banking,
mirroring, IRQs, reset, save states, and cartridge loading. Generated mapper 187
fixtures verify that unrelated ROMs keep their declared mapper.

```powershell
$coreSources = (Get-ChildItem src/Core -Filter '*.cpp' -Recurse).FullName
g++ -O2 -std=c++17 -Isrc -Ibuild/_deps/zlib-src -Ibuild/_deps/zlib-build tests/Mapper189Tests.cpp $coreSources build/_deps/zlib-build/libzlibstatic.a -o build/mapper189-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
./build/mapper189-tests.exe build/mapper189-test-data
if ($LASTEXITCODE -ne 0) { throw 'Mapper 189 tests failed' }
```

Optionally pass a game ROM/ZIP as a second argument. It runs 6,000,000 CPU cycles
and checks that video contains more than four colors. This is a boot smoke test,
not a complete gameplay test. The SF II payload CRC `96CE586E` with an old iNES
mapper 187 header is corrected to mapper 189 during loading; the source file is
unchanged.

# Mapper 7 regression tests

```powershell
cmake -S . -B build -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build --target mapper007-tests -j 4
ctest --test-dir build -R '^mapper007$' --output-on-failure
```

Generated ROMs and direct mapper tests cover every register value at PRG window
boundaries, power-of-two PRG capacities from 16 through 512 KiB, both linear and
exponent/multiplier NES 2.0 sizes, ignored bits and the oversized bit-3 extension.
iNES and NES 2.0 submappers 0/1 have no conflicts; submapper 2 ANDs the written
value with the ROM byte at the write address in the bank selected before that
write. Sequential conflicts use different bytes in different banks.

Console tests check all 8 KiB of CHR-RAM, background/sprite reads, both CIRAM
pages and all four single-screen nametable aliases, open bus without PRG-RAM,
state restoration and rollback after invalid/truncated console loads.
Mapper states retain the previous Windows five-byte layout (bank followed by
32-bit little-endian mirroring). Invalid bank/page values and every truncated
mapper payload length fail without modifying the mapper.

Standard AxROM has up to 256 KiB PRG, fixed 8 KiB CHR-RAM and no PRG-RAM.
16 KiB mirrored PRG is retained as a compatibility extension; 512 KiB uses bit 3.
Non-power-of-two sizes and larger PRG, CHR-ROM, four-screen, battery and trainers
are rejected. NES 2.0 additionally requires submapper 0/1/2, no PRG RAM/NVRAM,
exactly 8 KiB volatile CHR-RAM and no Misc. ROMs. Legacy iNES byte 8 is ignored
and does not create PRG-RAM or select a submapper.

Complete emulator initialization selects bank zero and the lower CIRAM page,
preserving cartridge CHR-RAM. This is not an electrical warm reset or a claim
that hardware powers on in a guaranteed bank. No game compatibility ratings
are inferred from the synthetic suite. See the [review in Portuguese](Mapper007Review.md).

References: https://www.nesdev.org/wiki/AxROM,
https://www.nesdev.org/wiki/NES_2.0_submappers,
https://www.nesdev.org/wiki/NES_2.0

# Mapper 9 regression tests

```powershell
cmake -S . -B build -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build --target mapper009-tests -j 4
ctest --test-dir build -R '^mapper009$' --output-on-failure
```

Generated ROMs cover power-of-two PRG capacities of 32/64/128 KiB and CHR-ROM
capacities from 8 through 128 KiB, iNES/NES 2.0 linear and exponent/multiplier
sizes, all register values, ignored bits, physical bank mirroring and the fixed
last three 8 KiB PRG banks. Unsupported sizes, submappers, RAM/NVRAM, four-screen,
trainers, battery flags, Misc. ROMs and truncated images are rejected.
Legacy iNES byte 8 is ignored; NES 2.0 requires zero PRG/CHR RAM declarations.

Latch tests read every CHR address from both FD and FE initial states. The
triggering read uses the old bank; only subsequent reads use the new one.
The low window triggers only at $0FD8/$0FE8; the upper window uses the ranges
$1FD8-$1FDF/$1FE8-$1FEF. Peeks and writes do not change latches. Console tests
also check sprite reads, real versus debugger PPUDATA reads, both nametable
mirroring modes and Pattern Table viewer isolation.

PxROM has no PRG-RAM: $6000-$7FFF remains open bus. States retain the old Windows
8207-byte mapper layout with explicit little-endian mirroring. The old 8192-byte
phantom RAM payload is consumed and ignored; new states write zero padding.
Register/latch/mirror values and every truncated mapper payload length are
validated before committing. Console tests verify restoration and rollback.

Complete initialization retains the existing bank-zero/FE-latch convention,
without claiming an electrical warm reset or guaranteed hardware power-up state.
The synthetic suite does not establish complete Punch-Out!! gameplay
compatibility. See the [review in Portuguese](Mapper009Review.md).

# Mapper 71 regression tests

These tests use generated ROMs to check bank switching, address boundaries,
CHR-RAM, mirroring, reset, save states, cartridge loading, and the absence of
bus conflicts. No game ROM is required.

After building the emulator in `build/`, run from the repository root with
PowerShell and MinGW-w64:

```powershell
$mapperSources = (Get-ChildItem src/Core/Cartridge/Mappers -Filter '*.cpp').FullName
g++ -std=c++17 -Isrc -Ibuild/_deps/zlib-src -Ibuild/_deps/zlib-build tests/Mapper071Tests.cpp src/Core/Cartridge/Cartridge.cpp $mapperSources build/_deps/zlib-build/libzlibstatic.a -o build/mapper071-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
./build/mapper071-tests.exe build/mapper071-test-data
if ($LASTEXITCODE -ne 0) { throw 'Mapper 71 tests failed' }
```

## PPU sprite overflow

This regression checks eight versus nine or more sprites, both sprite sizes,
background-only and sprite-only rendering, disabled rendering, status reads,
and pre-render clearing, with Unlimited Sprites both enabled and disabled.
It does not check cycle accuracy or the hardware's diagonal overflow bug.

```powershell
$coreSources = (Get-ChildItem src/Core -Filter '*.cpp' -Recurse).FullName
g++ -O2 -std=c++17 -Isrc -Ibuild/_deps/zlib-src -Ibuild/_deps/zlib-build tests/PPUSpriteOverflowTests.cpp $coreSources build/_deps/zlib-build/libzlibstatic.a -o build/ppu-sprite-overflow-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
./build/ppu-sprite-overflow-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'PPU sprite overflow tests failed' }
```

## Event waits

This headless SDL test checks timeout behavior, wakeup on pending and newly
arriving events, and event retention for `Window::pollEvents()`. It does not
measure CPU usage or validate the full Engine loop on a graphics driver.

Run from `build/` after building the emulator, reusing its SDL linker settings:

```powershell
g++ -std=c++17 -I_deps/sdl2-src/include ../tests/EventWaitTests.cpp '@CMakeFiles/R2NESV2.dir/linkLibs.rsp' -o event-wait-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
./event-wait-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Event wait tests failed' }
```

## APU output suppression

Compares normal and suppressed output over 60,000 CPU cycles, checking channel
state, DMC requests and stalls, frame and DMC IRQs, audio resumption, pending
sample clearing, and the user's muted sound preference. Also checks that
transition-related queue clearing preserves the APU state and filters.

Run from the repository root:

```powershell
g++ -O2 -std=c++17 -Isrc tests/APUOutputSuppressionTests.cpp src/Core/APU/APU.cpp -o build/apu-output-suppression-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
./build/apu-output-suppression-tests.exe
if ($LASTEXITCODE -ne 0) { throw 'APU output suppression tests failed' }
```
