#include "libretro.h"
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
    unsigned frames = 0, polls = 0, audioFrames = 0, inputReads = 0;
    bool env(unsigned command, void *data)
    {
        return command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT &&
               *static_cast<retro_pixel_format *>(data) == RETRO_PIXEL_FORMAT_XRGB8888;
    }
    void video(const void *data, unsigned w, unsigned h, size_t pitch)
    {
        if (data && w == 256 && h == 240 && pitch == 1024)
            ++frames;
    }
    size_t audio(const int16_t *, size_t count)
    {
        audioFrames += static_cast<unsigned>(count);
        return count;
    }
    void poll() { ++polls; }
    int16_t input(unsigned, unsigned, unsigned, unsigned)
    {
        ++inputReads;
        return 0;
    }
}
int main()
{
    retro_set_environment(env);
    retro_set_video_refresh(video);
    retro_set_audio_sample_batch(audio);
    retro_set_input_poll(poll);
    retro_set_input_state(input);
    retro_init();
    std::vector<uint8_t> rom(16 + 16384 + 8192, 0);
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1a;
    rom[4] = 1;
    rom[5] = 1;
    rom[16] = 0x4c;
    rom[17] = 0;
    rom[18] = 0x80; // JMP $8000
    rom[16 + 0x3ffc] = 0;
    rom[16 + 0x3ffd] = 0x80;
    retro_game_info game{};
    game.data = rom.data();
    game.size = rom.size();
    if (!retro_load_game(&game))
        return 1;
    for (int i = 0; i < 4; ++i)
        retro_run();
    if (frames != 4 || polls != 4 || inputReads != 64 || audioFrames < 2000)
        return 2;
    retro_reset();
    retro_run();
    if (frames != 5)
        return 3;
    retro_unload_game();
    game.size = 1;
    if (retro_load_game(&game))
        return 4;
    retro_deinit();
    std::puts("libretro smoke test passed");
}
