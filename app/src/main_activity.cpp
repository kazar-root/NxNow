#include "main_activity.hpp"
#include "startup_callback_policy.hpp"

#include "app_state.hpp"
#include "gfn_client.hpp"
#include "home_shortcut_policy.hpp"
#include "main_tabs_view.hpp"
#include "localization.hpp"
#include "ui_helpers.hpp"
#include "ui_theme.hpp"
#include "ui_loading_view.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace nxnow
{
namespace
{

class StartupGateView final : public brls::Box
{
  public:
    explicit StartupGateView(
        std::optional<shortcut::LaunchRequest> launch_request)
        : brls::Box(brls::Axis::COLUMN)
        , launch_request_(std::move(launch_request))
    {
        setGrow(1.0f);
        setAlignItems(brls::AlignItems::CENTER);
        setJustifyContent(brls::JustifyContent::CENTER);
        setBackgroundColor(ui::Ground());

        GfnClient client;
        AuthSession saved;
        if (!client.LoadSavedSession(saved))
        {
            AppState::Instance().MarkSessionLoaded();
            ShowMainTabs();
            return;
        }

        BuildStatusView(saved);
        BeginAuthentication(std::move(client), std::move(saved));
    }

    ~StartupGateView() override
    {
        alive_->store(false);
    }

  private:
    void BuildStatusView(const AuthSession&)
    {
        addView(new ui::LoadingView());
    }

    void BeginAuthentication(GfnClient client, AuthSession saved)
    {
        const auto alive = alive_;
        const auto complete = [this, alive](AuthSession session, std::string notification) {
            brls::sync(GuardStartupCallback(alive,
                [this, session = std::move(session), notification = std::move(notification)]() mutable {
                    if (finished_)
                        return;
                    AppState::Instance().SetSession(std::move(session));
                    ShowMainTabs();
                    if (!notification.empty())
                        brls::Application::notify(Tr(notification));
                }));
        };
        brls::async([alive, complete,
                     client = std::move(client), saved = std::move(saved)]() mutable {
            if (!alive->load())
                return;
            try
            {
                AuthSession verified = client.RecoverSavedSession(saved, true);

                if (!alive->load())
                    return;
                verified.reauthentication_required = false;
                complete(std::move(verified), "GeForce NOW account is ready");
            }
            catch (const ReauthenticationRequired& ex)
            {
                if (!alive->load())
                    return;
                saved.reauthentication_required = true;
                complete(std::move(saved),
                         "The provider requires a new QR code sign-in from Settings");
                brls::Logger::warning("Startup sign-in requires QR reconnection: {}", ex.what());
            }
            catch (const std::exception& ex)
            {
                if (!alive->load())
                    return;
                const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                const bool expired = saved.tokens.expires_at_ms > 0 &&
                    saved.tokens.expires_at_ms <= now_ms;
                saved.reauthentication_required = saved.reauthentication_required || expired;
                complete(std::move(saved),
                         expired
                            ? "Automatic sign-in could not finish; it will retry in NxNow"
                            : "Session check is temporarily unavailable; using the saved account");
                brls::Logger::warning("Startup authentication check failed: {}", ex.what());
            }
        }, false);
    }

    void ShowMainTabs()
    {
        if (finished_)
            return;
        // Build the replacement first: a construction error must not erase
        // the only working screen and leave the startup gate permanently black.
        auto* tabs = new MainTabsView();
        finished_ = true;
        clearViews();
        setAlignItems(brls::AlignItems::STRETCH);
        setJustifyContent(brls::JustifyContent::FLEX_START);
        setPadding(0);
        // The activity is measured in Borealis' logical content coordinates.
        // windowWidth/windowHeight are physical framebuffer dimensions and are
        // 1920x1080 while docked, so assigning them here scales the tab layout
        // twice and places part of the UI outside the visible 1280x720 area.
        // Let flex sizing follow the activity across dock/undock changes.
        tabs->setGrow(1.0f);
        addView(tabs);
        invalidate();
        brls::Application::giveFocus(tabs);
        const auto alive = alive_;
        brls::sync([this, alive] {
            if (alive->load())
                MaybeLaunchShortcut();
        });
    }

    void MaybeLaunchShortcut()
    {
        if (!launch_request_)
            return;

        const shortcut::LaunchRequest request = std::move(*launch_request_);
        launch_request_.reset();
        const auto& session = AppState::Instance().session();
        if (!session)
        {
            ShowError(
                "Shortcut needs sign-in",
                TrFormat("NxNow could not start {0} because no saved GeForce NOW account is connected. Sign in, then open the shortcut again.", {request.title}));
            return;
        }
        if (session->reauthentication_required)
        {
            ShowError(
                "Shortcut needs sign-in",
                TrFormat("Reconnect the saved GeForce NOW account, then open the {0} shortcut again.", {request.title}));
            return;
        }

        GfnClient client;
        LaunchSessionDialog(
            client, *session, request.launch_app_id, request.title,
            request.store, request.title, request.game_id, request.image_url);
    }

    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
    std::optional<shortcut::LaunchRequest> launch_request_;
    bool finished_ = false;
};

} // namespace

MainActivity::MainActivity(
    std::optional<shortcut::LaunchRequest> launch_request)
    : brls::Activity(new StartupGateView(std::move(launch_request)))
{
}

} // namespace nxnow
