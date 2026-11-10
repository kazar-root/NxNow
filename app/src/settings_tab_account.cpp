#include "settings_tab.hpp"
#include "app_state.hpp"
#include "qr_login_dialog.hpp"
#include "ui_helpers.hpp"
#include "localization.hpp"
#include "top_bar_frame.hpp"

namespace nxnow
{
bool SettingsTab::OpenAccountMenu()
{
    if (!AppState::Instance().HasSession())
        return BeginLogin(nullptr);
    if (auto* sign_out = getView("settings/option/sign-out"))
        brls::Application::giveFocus(sign_out);
    return true;
}

bool SettingsTab::BeginLogin(brls::View*)
{
    try
    {
        auto* dialog = new QrLoginDialog(GfnClient::NvidiaLoginProvider(), client_, [this, alive = alive_] {
            if (!alive->load()) return;
            RefreshSummary();
            RebuildSettings();
            for (auto* parent = getParent(); parent; parent = parent->getParent())
                if (auto* frame = dynamic_cast<TopBarFrame*>(parent))
                {
                    frame->RefreshAccountLibrary();
                    break;
                }
        });
        brls::Application::pushActivity(new brls::Activity(dialog));
    }
    catch (const std::exception& ex)
    {
        ShowError("GeForce NOW Login Failed", ex.what());
    }
    return true;
}

bool SettingsTab::ClearSavedLogin(brls::View*)
{
    // Signing out must not silently reactivate another old saved account.
    DisconnectCurrentAccount(client_);
    RefreshSummary();
    brls::sync([this, alive = alive_] {
        if (alive->load()) RebuildSettings();
    });
    return true;
}
}
