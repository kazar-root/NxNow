#pragma once

#include <string>
#include <vector>

namespace nxnow
{

struct StreamSettings
{
    std::string preset_id = "balanced";
    std::string label     = "Balanced";
    int width             = 1280;
    int height            = 720;
    int fps               = 60;
    int bitrate_kbps      = 12000;
    std::string codec     = "H264";
    std::string region    = "Auto";
    bool audio_enabled    = true;
    int audio_volume      = 1200;
    int audio_buffer_ms   = 60;
    std::string video_backend = "Auto";
    bool debug_diagnostics = false;
    bool stats_overlay_enabled = false;
    std::string game_language = "Auto";
    bool persist_game_settings = true;
    std::string controller_layout = "Switch";
    std::string stream_mode = "Optimal";
    std::string image_quality_mode = "Adaptive";
    std::string interface_language = "auto";
    bool community_proxy_enabled = false;
    std::string community_proxy_url;
    int queue_notify_threshold = 10;
    int mouse_speed_percent = 100;
    // Runtime launch snapshot, always derived from console keyboard settings.
    std::string keyboard_layout = "auto";

    bool operator==(const StreamSettings&) const = default;
};

struct GameLanguageOption
{
    std::string code;
    std::string label;
};

const std::vector<StreamSettings>& StreamPresets();
const std::vector<GameLanguageOption>& GameLanguageOptions();
std::string GameLanguageLabel(const std::string& code);
StreamSettings LoadStreamSettings();
bool SaveStreamSettings(const StreamSettings& settings);
StreamSettings NextStreamPreset(const StreamSettings& current);
std::string FormatStreamSettings(const StreamSettings& settings);

} // namespace nxnow
