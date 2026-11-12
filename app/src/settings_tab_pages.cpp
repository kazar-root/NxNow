#include "settings_tab.hpp"
#include "app_state.hpp"
#include "localization.hpp"
#include "membership_tier_policy.hpp"
#include "controls_help_view.hpp"
#include "ui_helpers.hpp"

namespace nxnow
{
void SettingsTab::BuildAccountPage()
{
    const auto& state = AppState::Instance();
    auto* account = MakeSection({});
    if (state.HasSession())
    {
        const auto& user = state.session()->user;
        AddInfoLine(account, "User", user.display_name);
        AddInfoLine(account, "Membership", user.membership_tier_verified ? Tr(user.membership_tier) : Tr("Unknown"));
        account->addView(MakeActionRow("Sign out", "Disconnect this account from this console.",
            state.session()->user.display_name,
            [this](brls::View* view) { return ClearSavedLogin(view); }, true));
    }
    else
        account->addView(MakeActionRow("Sign in to GeForce NOW", "Sign in securely using your phone.", "Connect",
            [this](brls::View* view) { return BeginLogin(view); }));
    content_container_->addView(account);
}

void SettingsTab::BuildStreamPage()
{
    auto* stream = MakeSection({});
    stream->addView(MakeOptionRow("Streaming profile",
        "A preset sets resolution, FPS and bitrate together. Custom lets you adjust each value.",
        [this] { return draft_settings_.label; },
        [this](brls::View* view) { return ChooseQualityPreset(view); }));
    if (draft_settings_.preset_id == "custom")
    {
        stream->addView(MakeOptionRow("Resolution", "Applied when a new stream starts.",
            [this] { return std::to_string(draft_settings_.width) + " x " + std::to_string(draft_settings_.height); },
            [this](brls::View* view) { return CycleResolution(view); }));
        stream->addView(MakeOptionRow("Frame rate", "Maximum stream frame rate. Applied to the next game launch.",
            [this] { return std::to_string(draft_settings_.fps) + " FPS"; },
            [this](brls::View* view) { return CycleFrameRate(view); }));
        stream->addView(MakeOptionRow("Maximum bitrate", "Maximum network bitrate, not a constant target. Applied to the next game launch.",
            [this] { return BitrateValue(draft_settings_.bitrate_kbps); },
            [this](brls::View* view) { return CycleBitrate(view); }));
    }
    content_container_->addView(stream);
}

void SettingsTab::BuildPreferencesPage()
{
    auto* controls = MakeSection("Controls & audio");
    controls->addView(MakeOptionRow("Button mapping",
        "Current mapping uses Xbox positions (A/B and X/Y swapped). Switch mapping matches the letters: A=A, B=B, X=X, Y=Y. L=LB, R=RB, ZL=LT, ZR=RT in both modes.",
        [this] { return draft_settings_.controller_layout == "Switch" ? "Switch mapping" : "Xbox mapping"; },
        [this](brls::View* view) { return ToggleControllerLayout(view); }));
    controls->addView(MakeOptionRow("Mouse speed",
        "Adjust left-stick mouse speed. L3 + R3 switches between mouse and gamepad in every game.",
        [this] { return std::to_string(draft_settings_.mouse_speed_percent) + " %"; },
        [this](brls::View* view) { return ChooseMouseSpeed(view); }));
    controls->addView(MakeOptionRow("Audio output", "Enable or mute GeForce NOW audio.",
        [this] { return draft_settings_.audio_enabled ? "Enabled" : "Muted"; },
        [this](brls::View* view) { return ToggleAudio(view); }));
    controls->addView(MakeOptionRow("Volume", "Adjust stream audio volume.",
        [this] { return std::to_string(draft_settings_.audio_volume / 100) + "x"; },
        [this](brls::View* view) { return CycleAudioVolume(view); }));
    controls->addView(MakeOptionRow("Stream stats overlay",
        "Show FPS, bitrate, latency and battery level. Press Plus + Minus during play to toggle these stats.",
        [this] { return draft_settings_.stats_overlay_enabled ? "Enabled" : "Disabled"; },
        [this](brls::View* view) { return ToggleStatsOverlay(view); }));
    controls->addView(MakeOptionRow("Diagnostics",
        "Writes detailed stream logs to sdmc:/switch/NXNow/stream_trace.log to investigate lag, freezes or packet loss. Leave disabled for normal play.",
        [this] { return draft_settings_.debug_diagnostics ? "Enabled" : "Disabled"; },
        [this](brls::View* view) { return ToggleDiagnostics(view); }));
    content_container_->addView(controls);
}

void SettingsTab::BuildAppPage()
{
    auto* language = MakeSection({});
    language->addView(MakeOptionRow("App language", "Choose the launcher language.",
        [this] { return InterfaceLanguageLabel(draft_settings_.interface_language); },
        [this](brls::View* view) { return ChooseInterfaceLanguage(view); }));
    content_container_->addView(language);
    auto* about = MakeSection({});
    about->addView(MakeActionRow("Controls & help", "Controller shortcuts and automatic USB keyboard/mouse support in every game.", "Open",
        [](brls::View*) {
            brls::Application::pushActivity(new brls::Activity(new ControlsHelpView()));
            return true;
        }));
    about->addView(MakeActionRow("Check for updates", "Check the latest NxNow release on GitHub.", "GitHub",
        [this](brls::View* view) { return CheckForUpdates(view); }));
    content_container_->addView(about);
}

void SettingsTab::BuildAdvancedPage()
{
    auto* language = MakeSection("Game preferences");
    language->addView(MakeOptionRow("Game language", "Same as NxNow follows the app language. An explicit language/country overrides it for supported games.",
        [this] { return GameLanguageLabel(draft_settings_.game_language); },
        [this](brls::View* view) { return ChooseGameLanguage(view); }));
    language->addView(MakeOptionRow("Remember game graphics",
        "Ask GeForce NOW to keep supported in-game settings between sessions.",
        [this] { return draft_settings_.persist_game_settings ? "Enabled" : "Disabled"; },
        [this](brls::View* view) { return TogglePersistGameSettings(view); }));
    content_container_->addView(language);
    language->addView(MakeOptionRow("Server location",
        "Choose an advertised GeForce NOW server region. This does not change the game's language or your account country.",
        [this] { return ServerLocationValue(); },
        [this](brls::View* view) { return ChooseServerLocation(view); }));
}
}
