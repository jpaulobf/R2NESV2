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
