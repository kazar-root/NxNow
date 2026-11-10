#include "launch_settings_policy.hpp"

#include <cassert>
#include <utility>

int main()
{
    for (const auto& [language, locale] : {
        std::pair{"en", "en_US"}, {"fr", "fr_FR"}, {"es", "es_ES"},
        {"it", "it_IT"}, {"ru", "ru_RU"}, {"uk", "uk_UA"},
        {"pl", "pl_PL"}, {"zh-CN", "zh_CN"}})
    {
        nxnow::StreamSettings saved;
        saved.interface_language = language;
        saved.region = "https://paris.fixture.invalid/";
        saved.width = 1920;
        saved.height = 1080;
        saved.fps = 30;
        saved.bitrate_kbps = 25000;
        saved.persist_game_settings = true;
        auto launch = nxnow::settings::ResolveLaunchSettings(saved);
        assert(launch.game_language == locale);
        assert(saved.game_language == "Auto");
        assert(launch.keyboard_layout != "auto");
        assert(launch.keyboard_layout ==
            nxnow::input::CloudKeyboardLayout(nxnow::input::SystemKeyboardLayout()));
        launch.game_language = "Auto";
        launch.keyboard_layout = "auto";
        assert(launch == saved);
        saved.game_language = "en_GB";
        saved.keyboard_layout = "fr-FR_azerty";
        assert(nxnow::settings::ResolveLaunchSettings(saved) == saved);
    }
}
