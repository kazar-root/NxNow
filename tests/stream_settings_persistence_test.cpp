#include "app_paths.hpp"
#include "stream_settings.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <unistd.h>

namespace
{

std::string home;
int failures = 0;

void Check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

void Write(const std::string& suffix, const std::string& body)
{
    std::ofstream file(home + "/stream_settings.json" + suffix);
    file << body;
    file.close();
    assert(file.good());
}

}

namespace nxnow
{

const std::string& AppHomePath()
{
    return home;
}

void PrepareAppStorage()
{
    std::filesystem::create_directories(home);
}

}

int main()
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "nxnow-settings-XXXXXX").string();
    char* directory = mkdtemp(pattern.data());
    assert(directory);
    home = directory;

    Check(nxnow::LoadStreamSettings().fps == 60, "missing settings use balanced defaults");
    Check(nxnow::LoadStreamSettings().interface_language == "auto", "missing settings follow the console language");
    Check(nxnow::LoadStreamSettings().game_language == "Auto", "game language follows the app by default");
    Write("", R"({"interface_language":"fr","game_language":"en_US","fps":30,"bitrate_kbps":25000})");
    Check(nxnow::LoadStreamSettings().interface_language == "auto", "legacy language migrates once to console detection");
    Check(nxnow::LoadStreamSettings().game_language == "Auto" && nxnow::LoadStreamSettings().fps == 30 &&
              nxnow::LoadStreamSettings().bitrate_kbps == 25000,
          "legacy English default follows launcher language without resetting video");
    Write("", R"({"interface_language":"fr","game_language":"en_US","game_language_version":1})");
    Check(nxnow::LoadStreamSettings().game_language == "en_US", "explicit English remains an override");
    Write("", R"({"interface_language":"fr","game_language":"en_GB"})");
    Check(nxnow::LoadStreamSettings().game_language == "en_GB", "other explicit language/country preferences survive migration");
    Check(nxnow::LoadStreamSettings().controller_layout == "Switch" &&
              nxnow::LoadStreamSettings().stream_mode == "Optimal",
          "NXNow defaults to Switch labels and Optimal");
    Write("", R"({"controller_layout":"Xbox","fps":30})");
    auto migrated = nxnow::LoadStreamSettings();
    Check(migrated.controller_layout == "Switch" && migrated.fps == 30,
          "legacy Xbox default migrates without changing video settings");
    migrated.controller_layout = "Xbox";
    migrated.stream_mode = "Reinforced";
    Check(nxnow::SaveStreamSettings(migrated), "new settings save");
    Check(nxnow::LoadStreamSettings().controller_layout == "Xbox" &&
              nxnow::LoadStreamSettings().stream_mode == "Reinforced",
          "explicit Xbox and Renforcé survive restart");
    Write("", R"({"controller_layout":"Switch","controller_layout_version":1,"stream_mode":"Pushed"})");
    Check(nxnow::LoadStreamSettings().stream_mode == "Reinforced",
          "the former reliability profile migrates to Renforcé");
    Write("", R"({"stream_mode":"Optimisation"})");
    Check(nxnow::LoadStreamSettings().stream_mode == "Optimal",
          "the former low-latency profile migrates to Optimal");
    Write("", R"({"controller_layout":"bad","controller_layout_version":1,"stream_mode":"bad"})");
    Check(nxnow::LoadStreamSettings().controller_layout == "Switch" &&
              nxnow::LoadStreamSettings().stream_mode == "Optimal",
          "invalid modes use NXNow defaults");
    std::filesystem::remove(home + "/stream_settings.json");
    Write(".bak", R"({"fps":30,"interface_language":"ru","interface_language_version":1,"audio_enabled":false})");
    auto settings = nxnow::LoadStreamSettings();
    Check(settings.fps == 30 && settings.interface_language == "ru" && !settings.audio_enabled,
          "missing primary recovers the backup");
    for (const std::string body : {"", "{", "[]", "null"})
    {
        Write("", body);
        Check(nxnow::LoadStreamSettings().fps == 30,
              "unreadable primary recovers the backup object");
    }

    Write("", R"({"fps":60})");
    Check(nxnow::LoadStreamSettings().fps == 60, "valid primary wins over backup");
    Write(".bak", "{");
    Write("", "{");
    Check(nxnow::LoadStreamSettings().fps == 60, "invalid primary and backup use defaults");

    Write("", R"({"width":0,"fps":30,"bitrate_kbps":20000,"interface_language":"ru","interface_language_version":1,"controller_layout":"Switch","audio_enabled":false,"stats_overlay_enabled":true})");
    settings = nxnow::LoadStreamSettings();
    Check(settings.width == 1280 && settings.fps == 30 && settings.bitrate_kbps == 20000,
          "invalid width preserves valid video fields");
    Check(settings.interface_language == "ru" && settings.controller_layout == "Switch" &&
              !settings.audio_enabled && settings.stats_overlay_enabled,
          "invalid video preserves unrelated preferences");

    Write("", R"({"width":4294969216,"height":4294968376,"fps":4294967326,"bitrate_kbps":4294975296,"audio_volume":4294968896,"audio_gain_version":4})");
    settings = nxnow::LoadStreamSettings();
    Check(settings.width == 1280 && settings.height == 720 && settings.fps == 60 &&
              settings.bitrate_kbps == 12000 && settings.audio_volume == 1200,
          "out-of-range JSON integers do not wrap into valid settings");

    Write("", R"({"fps":-30,"height":-720,"bitrate_kbps":0,"audio_gain_version":4,"audio_volume":1600})");
    settings = nxnow::LoadStreamSettings();
    Check(settings.fps == 60 && settings.height == 720 && settings.bitrate_kbps == 12000 &&
              settings.audio_volume == 1600,
          "nonpositive video fields use individual defaults");

    for (const auto& [body, volume] : {
             std::pair{R"({"audio_volume":150})", 1200},
             std::pair{R"({"audio_volume":600,"audio_gain_version":3})", 1200},
             std::pair{R"({"audio_volume":-2147483648,"audio_gain_version":3})", 800},
             std::pair{R"({"audio_volume":800,"audio_gain_version":4})", 800},
             std::pair{R"({"audio_enabled":false})", 1200}})
    {
        Write("", body);
        Check(nxnow::LoadStreamSettings().audio_volume == volume,
              "legacy audio gain migration remains compatible");
    }

    settings.interface_language = "ru";
    settings.controller_layout = "Switch";
    settings.audio_enabled = false;
    settings.width = 0;
    Check(nxnow::SaveStreamSettings(settings), "settings save succeeds");
    settings = nxnow::LoadStreamSettings();
    Check(settings.width == 1280 && settings.interface_language == "ru" &&
              settings.controller_layout == "Switch" && !settings.audio_enabled,
          "save normalization preserves unrelated preferences");
    Check(!std::filesystem::exists(home + "/stream_settings.json.tmp"),
          "successful save leaves no temporary file");

    for (const auto& preset : nxnow::StreamPresets())
    {
        Check(nxnow::SaveStreamSettings(preset), "each existing preset saves");
        const auto loaded = nxnow::LoadStreamSettings();
        Check(loaded.preset_id == preset.preset_id && loaded.label == preset.label &&
                  loaded.width == preset.width && loaded.height == preset.height &&
                  loaded.fps == preset.fps && loaded.bitrate_kbps == preset.bitrate_kbps,
              "existing preset video parameters round trip unchanged");
    }

    Write("", R"({"fps":"30","audio_volume":true,"audio_enabled":0,"audio_gain_version":4})");
    settings = nxnow::LoadStreamSettings();
    Check(settings.fps == 60 && settings.audio_volume == 1200 && settings.audio_enabled,
          "wrong JSON types keep defaults");

    settings = nxnow::StreamSettings {};
    settings.mouse_speed_percent = 175;
    assert(nxnow::SaveStreamSettings(settings));
    assert(nxnow::LoadStreamSettings().mouse_speed_percent == 175);
    settings.mouse_speed_percent = 999;
    assert(nxnow::SaveStreamSettings(settings));
    assert(nxnow::LoadStreamSettings().mouse_speed_percent == 300);
    Write("", R"({"mouse_speed_percent":-50})");
    assert(nxnow::LoadStreamSettings().mouse_speed_percent == 25);
    Write("", R"({"mouse_speed_percent":"fast"})");
    assert(nxnow::LoadStreamSettings().mouse_speed_percent == 100);

    std::filesystem::remove_all(home);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
