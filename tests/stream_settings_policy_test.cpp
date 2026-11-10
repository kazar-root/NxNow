#include "stream_settings_policy.hpp"

#include <cassert>

int main()
{
    nxnow::StreamSettings value;
    value.video_backend = "NVDEC";
    value.audio_volume = 1400;
    value.game_language = "ru_RU";
    value.persist_game_settings = false;
    value.controller_layout = "Switch";
    value.image_quality_mode = "Adaptive";
    value.stats_overlay_enabled = true;
    value.community_proxy_enabled = true;
    value.community_proxy_url =
        "http://client:secret@nxnow-proxy-tcp.zortos.me:3128";

    nxnow::settings::CycleResolution(value);
    assert(value.width == 1920 && value.height == 1080);
    assert(value.preset_id == "custom" && value.label == "Custom");
    assert(value.video_backend == "NVDEC" && value.audio_volume == 1400);
    assert(value.game_language == "ru_RU" && !value.persist_game_settings);
    assert(value.controller_layout == "Switch");
    assert(value.image_quality_mode == "Adaptive");
    value.stream_mode = "Reinforced";
    value.controller_layout = "Xbox";
    const auto previous = value;
    nxnow::StreamSettings preset;
    preset.preset_id = "quality";
    preset.label = "Quality";
    preset.width = 1920;
    preset.height = 1080;
    preset.bitrate_kbps = 20000;
    nxnow::settings::ApplyVideoPreset(value, preset);
    assert(value.width == 1920 && value.height == 1080 && value.fps == 60);
    assert(value.bitrate_kbps == 20000 && value.preset_id == "quality");
    auto expected = previous;
    expected.preset_id = preset.preset_id;
    expected.label = preset.label;
    expected.width = preset.width;
    expected.height = preset.height;
    expected.fps = preset.fps;
    expected.bitrate_kbps = preset.bitrate_kbps;
    expected.codec = preset.codec;
    assert(value == expected); // Layout, stream profile, audio and language survive.
    value = previous;
    assert(value.stats_overlay_enabled);
    assert(value.community_proxy_enabled);
    assert(!value.community_proxy_url.empty());

    nxnow::settings::CycleResolution(value);
    assert(value.width == 1280 && value.height == 720);

    nxnow::settings::CycleFrameRate(value);
    assert(value.fps == 30);
    nxnow::settings::CycleFrameRate(value);
    assert(value.fps == 60);

    value.bitrate_kbps = 12000;
    nxnow::settings::CycleBitrate(value);
    assert(value.bitrate_kbps == 16000);
    value.bitrate_kbps = 25000;
    nxnow::settings::CycleBitrate(value);
    assert(value.bitrate_kbps == 8000);

    nxnow::settings::CycleVideoBackend(value);
    assert(value.video_backend == "Software");
    nxnow::settings::CycleVideoBackend(value);
    assert(value.video_backend == "Auto");
    nxnow::settings::CycleVideoBackend(value);
    assert(value.video_backend == "NVDEC");

    nxnow::settings::CycleImageQuality(value);
    assert(value.image_quality_mode == "Clarity");
    nxnow::settings::CycleImageQuality(value);
    assert(value.image_quality_mode == "Original");
    nxnow::settings::CycleImageQuality(value);
    assert(value.image_quality_mode == "Adaptive");
    return 0;
}
