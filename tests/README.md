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
Derived PRG banking is recomputed on load. The console now writes format `R2NI`,
which preserves CPU clock/data-bus state and the MMC5's ten-bit CHR registers
and CHR/IRQ/split controls, MMC5A revision/GPIO/timer and the PPU's pending rendering read.
Previous `R2NB`/`R2NC`/`R2ND`/`R2NE`/`R2NF`/`R2NG`/`R2NH` states are intentionally
rejected, for every mapper; older builds cannot read `R2NI`.
Old NES 2.0 states created with an incorrectly
sized RAM payload are also unsupported.
Any failed console load restores its previous serialized state.

`reset()` is the emulator's deterministic complete initialization. Its MMC1
Control value `0x1E` is not a hardware power-up guarantee. A serial bit-7 reset
preserves CHR mode and mirroring while selecting PRG mode 3. Neither reset
clears PRG RAM. Game compatibility ratings are not inferred from these tests.

# Mapper 5 regression tests — batches 1 through 7

Second-review R1 coverage checks documented initial memory modes for both
MMC5 and MMC5A: read-only CPU ExRAM, blocked nametable access, protected
WRAM, explicit unlock in either order, and memory preservation on reset.
The test fails before the initial-value fix. State layout remains R2NI.

Implements findings 1 through 9 and 11 through 14 in [Mapper005Review.md](Mapper005Review.md),
excluding battery-file persistence as explicitly deferred by the user,
with the hardware uncertainties documented in that review.
Generated ROMs and direct mapper tests cover all four CHR bank sizes, the A/B
register windows, all byte values and upper-bit combinations, independent
capture of `$5130`, changes of CHR mode without rewriting banks, CHR capacity
wrapping, CHR-ROM protection and CHR-RAM read/write agreement.

Background, sprite and CPU reads now have explicit contexts. Tests cover
8x8 versus 8x16 sprites, last-written-set PPUDATA selection, PPUMASK gating,
fully decoded PPUCTRL/PPUMASK addresses and their ignored mirrors. Real
PPUDATA reads verify selection through the PPU, Bus and Cartridge layers.

Extended-attribute tests check per-tile ExRAM latching for all four nametables
and sources (both CIRAM pages, ExRAM and fill), all palettes, live `$5130`,
4 KiB banks independent of CHR mode, PPU address bit 12 being ignored,
capacity wrapping and restoration of normal attributes when substitutions
are disabled. CPU, sprite and peek reads must not replace the tile latch.
The pattern viewer must leave the serialized mapper state unchanged.

ExRAM permission tests exercise every byte of the 1 KiB memory in all four
modes, both in-frame and outside a frame. CPU reads in modes 0/1 preserve open
bus; CPU writes require in-frame activity. Mode 2 permits CPU reads/writes;
mode 3 permits reads only. Nametable reads/writes use ExRAM only in modes 0/1;
modes 2/3 return zero and ignore writes. All four nametables and their `$3000`
mirrors are checked, along with real PPUDATA access during blanking and write
permission after restoring in-frame state and after activity timeout.
Out-of-frame CPU writes in modes 0/1 are deterministically ignored: NesDev
allows ignored/corrupted outcomes, so preservation is an emulator policy,
not a hardware guarantee. Batch 4 preserved R2NF; batch 5 introduces R2NG.

Combined debugger tests compare the entire serialized console before/after
CPU queries through Bus and NES, CHR/nametable peeks and both pattern viewers,
while an IRQ is active, an extended tile is latched and a partial scanline
sequence exists. The sequence must still trigger on its remaining reads.
A synthetic ROM also checks pixels rendered with normal background set B,
extended CHR/palette with CIRAM, and extended attributes with fill mode.

State tests cover ten-bit bank values independently of the pending `$5130`
latch, mode changes after restore, truncated payloads, invalid CHR controls
and registers, console rollback, CHR-RAM suffix alignment and rejection of
old console formats. `R2NH` stores each MMC5 CHR register as two little-endian
bytes. The old trailing fetch-budget byte is replaced by three bytes:
observed PPUCTRL.5, observed PPUMASK.3/.4 and the last-written-set selector.
These controls are validated and restored, including a disabled mask and
last-written B. The extended tile latch is restored between tile and pattern
fetches; the per-call read context is transient and returns to CPU on load.

Batch 5 extends semantic checks to all implemented mapper controls, detector
coherence and split state. Truncations across control/memory/tail boundaries,
invalid modes/protection/fill/IRQ bytes, impossible detector addresses and
incoherent split coordinates must fail without changing the mapper. Console
tests also reject semantic errors and verify full rollback. PRG bank bytes,
memory and other unrestricted register values remain legal. MMC5 16-bit
words now have explicit little-endian encoding throughout the payload.

Vertical split tests cover left/right regions, thresholds 0/1/8/31, 34 fetch
positions, scroll values 0/7/8/239/240/255 and vertical wrapping, palettes,
CHR capacity wrapping and the bank's independence from CHR mode/$5130.
ExRAM modes and PPUMASK gate substitutions; non-split mode-1 tiles retain
extended attributes. Real PPU pixels are checked for fine X 0/3/7, displaced
normal scroll and independent split scrolling. An active split snapshot
must reproduce identical future pixels and serialized console state.

The implemented commercial CL wiring preserves PPU fine Y; alternate SL
wiring and electrical pin-92 control are unsupported. Rendering supplies
target tile coordinates, including next-line prefetch, to the mapper; this
is not a reconstruction of split counters from arbitrary bus waveforms.
Mid-frame rendering enable uses nominal screen position, without claiming
hardware glitch fidelity. A tile-fetch phase distinguishes nametable data
from attributes even with unusual PPU coarse Y. R2NG appends ten bytes to
the mapper payload: split control/scroll/bank, signed line (-1 as FFFF),
Y, column, active-tile selector and 16-bit tile offset. Audio remains deferred.

Batch 6 validates power-of-two PRG (16 KiB..1 MiB) and CHR-ROM (8 KiB..1 MiB),
or the existing 8 KiB CHR-RAM extension. It rejects unsupported submappers,
four-screen, incompatible RAM declarations and truncated ROM/trainer data.
NES 2.0 explicitly controls RAM/NVRAM capacity, including zero; iNES uses
explicit byte-8 sizes or the legacy 64 KiB compatibility superset when absent.
Tests also exercise exponent-encoded ROM sizes and direct-constructor rejection.

WRAM profiles cover zero, single 8/32 KiB, dual 8 KiB, dual 32 KiB and single
128 KiB memory, plus mixed RAM/NVRAM chips of 8/32 KiB. Bit 2 selects the chip
on commercial profiles, with open bus for missing chips; 128 KiB uses all four
bank bits. Tests exhaust register bytes and PRG modes, check low/high-window
aliases, protection and trainer installation without unlocking writes.
RAM/NVRAM classification only affects emulated memory and snapshots: no
battery-file loading or saving is implemented.

Recognized register writes return the internal-access sentinel; holes remain
unhandled. The MMC5 exposes its authoritative CIRAM through PPU::getVram(),
so the raw viewer edits the memory actually used by nametables. Tests cover
both pages, mirroring and full console restore through that accessor.
The generic PPU buffer remains unused for MMC5 nametable service.

R2NH inserts twelve configuration bytes after the 22 audio-register bytes:
16-bit PRG/CHR bank counts and 32-bit RAM/NVRAM sizes, all little-endian.
The WRAM payload is sized to installed memory (0..128 KiB), followed by
ExRAM, CIRAM and the existing detector/split tail. Mismatched cartridge
capacities are rejected before reading RAM. No allocation size is trusted
from the state file. That R2NH mapper payload was 3172 bytes plus installed WRAM.

Batch 7 adds eight bytes before that configuration block in R2NI: revision,
GPIO control/latch/external input levels, a little-endian 16-bit counter,
running flag and pending timer IRQ flag. The current payload is 3180 bytes
plus installed WRAM. Cross-revision states are rejected atomically.
`Configuration::revision` defaults to MMC5; the loader cannot infer MMC5A
from iNES/NES 2.0, so tests explicitly construct/inject the A revision.
There is no GUI revision selector or CRC override.

MMC5A tests cover timer byte writes, exact expiry, zero start, independent
scanline/timer acknowledge, GPIO direction/latch, external pin inputs and
the combinational /M2 function for peripheral addresses. There is no RAM
or external device installed at $5800-$5BFF. `expansionPins()` accepts an
explicit bus phase; no bus callbacks drive an external peripheral.
Input pins default to zero; undocumented status bits return zero. These
are deterministic policies. Forcing an active counter to zero stops it
without a new IRQ; that edge case has not been verified on hardware.
Console tests check timer expiry during DMA, debugger peeks, deterministic
future execution after restore, and cross-revision rollback.

`hardwareReset()` handles an explicitly supplied chip-reset event and
preserves all state whose reset is not documented, including pending video
IRQ, its target/counter/in-frame and memories. `reset()` and the console
reset command keep their previous complete deterministic initialization.
Analog M2-gap detection, pin contention/loads, external devices and audio
reset behavior remain outside this batch. No wall-clock pause triggers a reset.

IRQ tests cover pending versus enabled state, late enable, disable without
acknowledge, status acknowledge without counter reset, target zero, targets
1/119/239/240/255 and reset on the 241st detected scanline. The detector arms
after three equal `$2xxx` reads and clocks on the following read. Three empty
system M2 ticks clear in-frame without clearing pending IRQ; continuing PPU
reads retain activity. Non-read address changes, PPU writes, peeks and the old
scanline callback must not clock it. NMI vectors, exact-address PPUSTATUS,
PPUMASK transitions and OAMDMA exercise their documented reset conditions.
IRQ flags and detector limits are validated before committing mapper state.

Real PPU tests assert scanline-zero detection and IRQ timing at dot 4 for
both sprite sizes and background tables, all rendering mask combinations
and Unlimited Sprites on/off. Restoring between dots 3 and 4 must reproduce
the same console state and IRQ. PPU rendering read pulses are delayed one dot
from the existing logical fetch, and this pending address is serialized in
the PPU. CPU PPUDATA pulses are immediate. Extra sprite reads used only by the
Unlimited Sprites compositor do not emit hardware read pulses.

The mapper payload removes redundant IRQ-active state, stores IRQ booleans
as validated bytes, narrows idle count to a byte (0..3), and appends one byte
tracking PPU activity since the last M2 tick. The pending PPU read adds two
bytes to the PPU payload. CPU debugger reads of `$5204`, PPUSTATUS and the NMI
vectors must leave the mapper unchanged; actual bus reads exercise their
side effects. DMA is still handled by the existing console scheduler.

NesDev caveats are preserved in the review: its prose specifies the fourth
read/dot 4, unlike its simplified pseudocode. OAMDMA reset and immediate
PPUMASK-disable behavior still have hardware-verification qualifications;
these tests validate the chosen software model, not electrical measurements.

```powershell
cmake -S . -B build -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build --target mapper005-tests -j 4
ctest --test-dir build -R '^mapper005$' --output-on-failure
```

Reference: https://www.nesdev.org/wiki/MMC5 (PPU monitoring, CHR banking,
extended attributes, fill mode and `$5130`).
Unknown low bits marked `?` in the NesDev write matrix are initialized to zero;
mode-transition checks assert only documented bits. The documented copy of
CPU D0 into internal bit 0 is preserved even in the larger bank modes.
These tests do not establish full MMC5 conformance or gameplay compatibility.
Manually retest Castlevania III and create new save states before continuing.

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

Optional BB Car regression: append local `.nes` and/or `.zip` paths to
`mapper007-tests <fixture-directory>`. The legacy payload CRC `7F9C1DEC`
is recognized as fixed NROM with CHR-ROM and lower single-screen mirroring,
as described by the [dump author](https://cah4e3.shedevr.org.ru/dumping_2006.php).
Tests check ROM protection, fixed banks/mirroring, boot/video for 201 frames,
and console state restore. An unpacked `.nes` also tests rejection of changed
payload, incompatible flags, NES 2.0 and truncation. No commercial ROM data
is included; ordinary mapper 7 images with CHR-ROM remain rejected.

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

# Mapper 11 regression tests

```powershell
cmake -S . -B build -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build --target mapper011-tests -j 4
ctest --test-dir build -R '^mapper011$' --output-on-failure
```

Direct mapper tests exercise all 256 register values at PRG window boundaries,
all supported PRG/CHR capacities and fixed H/V mirroring. Generated iNES and
NES 2.0 ROMs verify AND conflicts against the current PRG bank at the written
address, including successive writes whose source banks contain different bytes.
Both PRG and CHR selection use the conflicted value. Bits 2/3 are for CIC lockout
defeat and do not change emulated banking or mirroring.

Standard Color Dreams supports up to 128 KiB each of PRG-ROM and CHR-ROM, with
32 KiB PRG and 8 KiB CHR windows and no PRG-RAM. Power-of-two capacities are
required. The pre-existing 16 KiB mirrored PRG and fixed 8 KiB CHR-RAM extensions
are retained. NES 2.0 accepts only submapper zero, no PRG RAM/NVRAM, and either
no CHR RAM with CHR-ROM or exactly 8 KiB volatile CHR-RAM without CHR-ROM.
Battery, trainers, four-screen, declared Misc. ROMs and truncated images fail.
Legacy iNES byte 8 is ignored; NES 2.0 linear and exponent/multiplier sizes
are exercised. No submapper assignments from mappers 2/3/7 are borrowed.

Mapper states retain the old two-byte physical bank layout. Every valid bank
combination restores; truncated or out-of-range states leave the mapper intact.
Console tests check ROM protection, all CHR-RAM bytes, sprite reads, H/V CIRAM
aliases, open bus without PRG-RAM, reset and rollback after failed state loads.

No-conflict variants such as the Free Fall prototype still need a verified
board/dump identification mechanism. Applying standard AND conflicts can break
those images, including ones that previously ran without conflicts. Mapper 144
variants are not implemented here. Complete initialization selects bank zero;
this is not an electrical warm reset or a hardware power-up guarantee.
Synthetic tests do not establish commercial gameplay compatibility.
See the [review in Portuguese](Mapper011Review.md).

References: https://www.nesdev.org/wiki/INES_Mapper_011,
https://www.nesdev.org/wiki/Bus_conflict,
https://www.nesdev.org/wiki/NES_2.0_submappers

# Sachen and UNIF

This suite covers mappers 79, 147 (72-pin latch), and 150, as well as UNIF loading for these boards and CNROM.

The Sachen ZIPs reported as invalid iNES are actually UNIF images named `.nes`.
The loader detects their signature and supports MAPR board names `CNROM`
(mapper 3), `SA-016-1M` (mapper 79), `TC-U01-1.5M` (mapper 147, 72-pin latch),
and `Sachen-74LS374N` (mapper 150).
Optional `UNL-`/`NES-`/`HVC-` prefixes, case and outer whitespace are normalized. Other UNIF
boards are rejected with their board name. Native `.unf`/`.unif` files and these
extensions inside ZIPs are supported as well.

The parser bounds-checks chunks, rejects duplicates/gaps in ROM chip indices,
orders PRG/CHR chips by hexadecimal index, validates MIRR per board and refuses
unsupported versions, RAM/battery configurations and capacities. NAME and other
unrecognized metadata are skipped. Optional checksum chunks are not verified.
PRG/CHR data is normalized in memory to NES 2.0; CRC32 uses only the normalized
PRG+CHR payload, so filenames and UNIF metadata do not affect game identification.
Original files are never rewritten.

Mapper 79 decodes writes with `(address & $E100) == $4100`, using data bit 3
for PRG and bits 0-2 for CHR. No extra CNROM-style register is added at $8000.
The mapper 147 72-pin latch decodes `(address & $4103) == $4102`, using bit 2
for PRG and bits 3-6 for CHR; writes in the ROM mirror apply AND conflicts.
Both support 32/64 KiB PRG-ROM, fixed H/V mirroring, read-only CHR and no PRG-RAM.
CHR capacities are 8-64 KiB for mapper 79 and 8-128 KiB for mapper 147.
The 60-pin mapper 147 JV001 arithmetic/protection circuit and 128 KiB PRG
configuration are explicitly unsupported. Two-byte bank states reuse the
validated mapper 11 implementation and its initialization convention.

Mapper 150 uses an index/data pair decoded by `$C101 == $4100/$4101`. All eight
registers retain/read three bits; read bits 3-7 remain open bus. Tests exercise
every register value, all CPU register-address aliases, PRG/CHR bank masks,
state validation/rollback and all four CIRAM arrangements through the PPU.
The L-shaped arrangement is `[0,0,0,1]`; the existing MirrorMode enum values
were preserved by appending the new mode. The new mapper payload is nine bytes
(index plus eight registers), with mirroring derived from register 7.
Supported PRG capacities are 32/64/128 KiB and CHR capacities are 8-64 KiB.
The implemented solder pad connects CPU D2; the alternate Vcc setting and
mapper 243 are not included. UNIF requires mapper-controlled `MIRR=5` for this
board and still rejects that mode for fixed-mirroring boards.
CNROM UNIF maps to NES 2.0 mapper 3 submapper 2, including AND bus conflicts.

```powershell
cmake -S . -B build -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build build --target sachen-tests -j 4
ctest --test-dir build -R '^sachen$' --output-on-failure
```

To add a boot check for one or more local ROMs, pass their paths after the fixture directory:

```powershell
./bin/sachen-tests.exe build/sachen-test-roms 'C:/ROMs/Challenge of the Dragon (Sachen).zip' 'C:/ROMs/Galactic Crusader (Sachen).zip'
```

Optional ROM/ZIP paths after the fixture directory run a 6,000,000-cycle boot
smoke test, checking visible video and a full console save/load round trip.
The user's seven archives each reached 201 frames and passed state restoration:

| Archive title | MAPR | PRG / CHR | Canonical CRC32 |
| --- | --- | --- | --- |
| Challenge of the Dragon (Sachen) | TC-U01-1.5M | 64 / 128 KiB | `7DD0AFC8` |
| Galactic Crusader (Sachen) | SA-016-1M | 32 / 32 KiB | `3F2450EA` |
| Master Chu & The Drunkard Hu (Sachen) | SA-016-1M | 32 / 32 KiB | `EB4CCA31` |
| 2-in-1 Cosmo Cop + Cyber Monster (Sachen) | Sachen-74LS374N | 64 / 64 KiB | `73FB55AC` |
| 2-in-1 Tough Cop + Super Tough Cop (Sachen) | Sachen-74LS374N | 64 / 64 KiB | `DDCBDA16` |
| 75 Bingo (Sachen-English) | CNROM | 32 / 32 KiB | `D04A40E6` |
| Auto-Upturn (Sachen) | Sachen-74LS374N | 32 / 64 KiB | `47918D84` |

These results verify loading and startup, not complete gameplay. The Color Dreams
releases are different board variants; the Sachen images must not be forced to
mapper 11 or fixed by replacing only the first four header bytes.
The emulator build completed and all 15 registered CTest checks passed after
adding this support; real-archive smoke tests were run separately.

References: https://www.nesdev.org/wiki/UNIF,
https://www.nesdev.org/wiki/UNIF_to_NES_2.0_Mapping,
https://www.nesdev.org/wiki/INES_Mapper_079,
https://www.nesdev.org/wiki/INES_Mapper_147,
https://www.nesdev.org/wiki/INES_Mapper_150

# Mapper 71 regression tests

These tests use generated ROMs to check bank switching, address boundaries,
CHR-RAM, mirroring, reset, save states, cartridge loading, and the absence of
bus conflicts. No game ROM is required.

After building the emulator in `build/`, run from the repository root with
PowerShell and MinGW-w64:

```powershell
$mapperSources = (Get-ChildItem src/Core/Cartridge/Mappers -Filter '*.cpp').FullName
g++ -std=c++17 -Isrc -Ibuild/_deps/zlib-src -Ibuild/_deps/zlib-build tests/Mapper071Tests.cpp src/Core/Cartridge/Cartridge.cpp src/Core/Cartridge/UNIF.cpp src/Core/Memory/VRAM/VRAM.cpp $mapperSources build/_deps/zlib-build/libzlibstatic.a -o build/mapper071-tests.exe
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
