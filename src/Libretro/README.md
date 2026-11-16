# Experimental libretro core

This first prototype supports NTSC ROMs, video, audio, reset and two joypads.
It does not yet support battery saves, save states, rewind, PAL, Zapper or
cheats. Do not use it for games whose progress needs to be saved. The frontend
handles ZIP extraction; the core accepts NES and UNIF content in memory.

Build Android 32-bit with the Android NDK, CMake and Ninja:

```sh
cmake -S . -B build-libretro-android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-23 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
  -DR2NES_BUILD_GUI=OFF -DR2NES_BUILD_LIBRETRO=ON -DBUILD_TESTING=OFF
cmake --build build-libretro-android --target r2nes_libretro
```

The resulting `r2nes_libretro.so` is intended for a 32-bit Android RetroArch.
Install it using the frontend's core installation function if available. The
optional `r2nes_libretro.info` belongs in RetroArch's core info directory. Some
Android distributions restrict installation of external cores.

To test the host adapter without a device, build with `R2NES_BUILD_LIBRETRO=ON`
and `BUILD_TESTING=ON`, then run `ctest -R libretro_smoke --output-on-failure`.
The bundled `libretro.h` is the official API header, with its MIT license in the
header itself.
