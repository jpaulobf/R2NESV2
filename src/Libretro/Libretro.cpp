#include "libretro.h"
#include "Core/NES.h"
#include "Core/Cartridge/RomFile.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace
{
    retro_environment_t environment = nullptr;
    retro_video_refresh_t video = nullptr;
    retro_audio_sample_t audio = nullptr;
    retro_audio_sample_batch_t audioBatch = nullptr;
    retro_input_poll_t poll = nullptr;
    retro_input_state_t input = nullptr;
    std::unique_ptr<R2NES::Core::NES> nes;
    std::shared_ptr<R2NES::Core::Cartridge> cartridge;
    constexpr double sampleRate = 44100.0;
    constexpr unsigned width = 256, height = 240;
}

extern "C"
{
    void retro_set_environment(retro_environment_t cb) { environment = cb; }
    void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
    void retro_set_audio_sample(retro_audio_sample_t cb) { audio = cb; }
    void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audioBatch = cb; }
    void retro_set_input_poll(retro_input_poll_t cb) { poll = cb; }
    void retro_set_input_state(retro_input_state_t cb) { input = cb; }
    void retro_init() {}
    void retro_deinit()
    {
        nes.reset();
        cartridge.reset();
    }
    unsigned retro_api_version() { return RETRO_API_VERSION; }
    void retro_get_system_info(retro_system_info *info)
    {
        *info = {};
        info->library_name = "R2NES";
        info->library_version = "1.5.0-alpha";
        info->valid_extensions = "nes|unf|unif";
        info->need_fullpath = false;
    }
    void retro_get_system_av_info(retro_system_av_info *info)
    {
        *info = {};
        info->geometry = {width, height, width, height, 4.0f / 3.0f};
        info->timing = {60.0988138974405, sampleRate};
    }
    void retro_set_controller_port_device(unsigned, unsigned) {}
    void retro_reset()
    {
        if (nes)
            nes->reset();
    }
    bool retro_load_game(const retro_game_info *game)
    {
        if (!environment || !game || !game->data || !game->size || game->size > R2NES::Core::MaxRomFileSize)
            return false;
        retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
        if (!environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format))
            return false;
        try
        {
            const auto *bytes = static_cast<const uint8_t *>(game->data);
            auto nextCart = std::make_shared<R2NES::Core::Cartridge>(std::vector<uint8_t>(bytes, bytes + game->size));
            if (!nextCart->isValid())
                return false;
            auto next = std::make_unique<R2NES::Core::NES>();
            if (!next->insertCartridge(nextCart))
                return false;
            next->getApu().setAudioSampleRate(static_cast<float>(sampleRate));
            cartridge = std::move(nextCart);
            nes = std::move(next);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    bool retro_load_game_special(unsigned, const retro_game_info *, size_t) { return false; }
    void retro_unload_game()
    {
        nes.reset();
        cartridge.reset();
    }
    unsigned retro_get_region() { return RETRO_REGION_NTSC; }
    void *retro_get_memory_data(unsigned) { return nullptr; }
    size_t retro_get_memory_size(unsigned) { return 0; }
    size_t retro_serialize_size() { return 0; }
    bool retro_serialize(void *, size_t) { return false; }
    bool retro_unserialize(const void *, size_t) { return false; }
    void retro_cheat_reset() {}
    void retro_cheat_set(unsigned, bool, const char *) {}
    void retro_run()
    {
        if (!nes || !video || !poll || !input)
            return;
        poll();
        using namespace R2NES::Core::IO;
        const struct
        {
            unsigned id;
            NESButtons button;
        } buttons[] = {
            {RETRO_DEVICE_ID_JOYPAD_A, BUTTON_A}, {RETRO_DEVICE_ID_JOYPAD_B, BUTTON_B}, {RETRO_DEVICE_ID_JOYPAD_SELECT, BUTTON_SELECT}, {RETRO_DEVICE_ID_JOYPAD_START, BUTTON_START}, {RETRO_DEVICE_ID_JOYPAD_UP, BUTTON_UP}, {RETRO_DEVICE_ID_JOYPAD_DOWN, BUTTON_DOWN}, {RETRO_DEVICE_ID_JOYPAD_LEFT, BUTTON_LEFT}, {RETRO_DEVICE_ID_JOYPAD_RIGHT, BUTTON_RIGHT}};
        auto &sticks = nes->getJoysticks();
        for (unsigned port = 0; port < 2; ++port)
        {
            auto &controller = port == 0 ? sticks.controller1 : sticks.controller2;
            for (const auto &button : buttons)
                controller.setButton(button.button, input(port, RETRO_DEVICE_JOYPAD, 0, button.id) != 0);
        }
        while (!nes->isFrameComplete())
            nes->step();
        nes->clearFrameComplete();
        video(nes->getPpu().getFrameBuffer(), width, height, width * sizeof(uint32_t));
        std::vector<int16_t> samples;
        auto &apu = nes->getApu();
        while (apu.hasSamples())
        {
            const auto sample = static_cast<int16_t>(std::clamp(apu.getOutputSample(), -1.0f, 1.0f) * 32767.0f);
            samples.push_back(sample);
            samples.push_back(sample);
        }
        if (audioBatch && !samples.empty())
            audioBatch(samples.data(), samples.size() / 2);
        else if (audio)
            for (size_t i = 0; i < samples.size(); i += 2)
                audio(samples[i], samples[i + 1]);
    }
}