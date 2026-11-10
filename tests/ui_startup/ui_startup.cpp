#include "app_state.hpp"
#include "main_tabs_view.hpp"
#include "main_activity.hpp"
#include "game_carousel.hpp"
#include "game_detail_view.hpp"
#include "catalog_tab.hpp"
#include "localization.hpp"
#include "queue_view.hpp"
#include "settings_tab.hpp"
#include "stream_overlay_view.hpp"
#include "qr_login_dialog.hpp"
#include "ui_theme.hpp"
#include "ui_layout_fixtures.hpp"
#include <borealis.hpp>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <unistd.h>

namespace startup_fixture { int FrameDraws(); int UnsafeDeletions(); brls::Rect QrBackground(); }

void Pump(int frames = 4)
{
    for (int i = 0; i < frames; ++i)
    {
        assert(brls::Application::mainLoop());
        assert(startup_fixture::UnsafeDeletions() == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
void Activate(brls::View* view)
{
    assert(view && view->isFocusable());
    brls::Application::giveFocus(view);
    brls::Application::onControllerButtonPressed(brls::BUTTON_A, false);
    Pump();
}
template <class Predicate> void Until(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!predicate()) { assert(std::chrono::steady_clock::now() < deadline); Pump(1); }
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    auto path = (std::filesystem::temp_directory_path() / "nxnow-startup-XXXXXX").string();
    assert(mkdtemp(path.data()));
    const std::string scenario = argc > 1 ? argv[1] : "library";
    const bool guest = scenario == "guest" || scenario == "profile-fr" || scenario == "settings-login-fr";
    const bool stress = scenario == "stress";
    const bool detail_test = scenario == "detail-fr" || scenario == "direct-fr";
    const bool french = detail_test || scenario.find("-fr") != std::string::npos;
    const bool loading_test = scenario == "loading-fr";
    ui_fixture::Reset(path, scenario == "guest" || scenario == "empty");
    if (loading_test) ui_fixture::HoldArtwork();
    setenv("LC_ALL", french ? "fr_FR.UTF-8" : "en_US.UTF-8", 1);
    nxnow::SetInterfaceLanguage(scenario == "auto-fr" ? "auto" : french ? "fr" : "en");
    if (scenario == "qr-zh") nxnow::SetInterfaceLanguage("zh-CN");
    brls::Logger::setLogLevel(brls::LogLevel::LOG_WARNING);
    assert(brls::Application::init());
    brls::Application::createWindow("Startup regression");
    brls::Application::setGlobalQuit(false);
    nxnow::ui::InitializeThemeAndFonts();
    nxnow::AuthSession session; session.user.user_id = "native-fixture-user";
    session.user.display_name = "Test player";
    if (guest) nxnow::AppState::Instance().MarkSessionLoaded();
    else nxnow::AppState::Instance().SetSession(session);
    nxnow::AppState::Instance().SetLibraryGames(guest || stress || loading_test || scenario == "cold" || scenario == "cold-fr"
        ? std::vector<nxnow::GameInfo> {} : ui_fixture::Library());
    std::puts("STEP constructing production MainActivity and startup gate");
    auto* activity = new nxnow::MainActivity();
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
    auto* gate = dynamic_cast<brls::Box*>(activity->getContentView());
    assert(gate && gate->getChildren().size() == 1);
    auto* root = dynamic_cast<nxnow::MainTabsView*>(gate->getChildren().front());
    assert(root);
    std::puts("STEP attached production shell through its real startup gate");
    auto* carousel = dynamic_cast<nxnow::GameCarouselView*>(root->getView("library-carousel"));
    assert(carousel);
    auto* logo = root->getView("shell-logo");
    assert(logo && logo->getWidth() == 80 && logo->getHeight() == 80);
    nxnow::GfnClient qr_client;
    int qr_completed = 0;
    if (scenario == "qr-fr" || scenario == "qr-zh")
    {
        ui_fixture::EnableQrChallenge();
        auto* qr = new nxnow::QrLoginDialog(nxnow::GfnClient::NvidiaLoginProvider(), qr_client,
            [&] { ++qr_completed; });
        brls::Application::pushActivity(new brls::Activity(qr), brls::TransitionAnimation::NONE);
        auto* code = dynamic_cast<brls::Label*>(qr->getView("login/code"));
        assert(code);
        Until([&] { return code->getFullText().find("WXYZ-1234") != std::string::npos; });
        Pump();
        const auto qr_frame = qr->getView("login/qr")->getFrame();
        assert(qr_frame.getWidth() == 344 && qr_frame.getHeight() == 344);
        assert(qr_frame.inscribed(qr->getView("login/qr-card")->getFrame()));
        const auto white = startup_fixture::QrBackground();
        assert(white.getWidth() > 250 && white.getHeight() == white.getWidth());
        assert(white.inscribed(qr_frame));
        const auto details = qr->getView("login/details")->getFrame();
        assert(qr->getView("login/retry")->getFrame().inscribed(details));
        assert(qr->getView("login/cancel")->getFrame().inscribed(details));
        assert(!qr_frame.collideWith(qr->getView("login/actions")->getFrame()));
        assert(qr->getView("login/logo")->getWidth() == 80);
        brls::Application::giveFocus(qr->getView("login/retry"));
        brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_RIGHT, false);
        assert(brls::Application::getCurrentFocus() == qr->getView("login/cancel"));
        brls::Application::onControllerButtonPressed(brls::BUTTON_B, false);
        auto* status = dynamic_cast<brls::Label*>(qr->getView("login/status"));
        Until([&] { return status->getFullText() == nxnow::Tr("QR login cancelled"); });
        Activate(qr->getView("login/retry"));
        Until([&] { return code->getFullText().find("WXYZ-1234") != std::string::npos; });
        assert(ui_fixture::Snapshot().qr_providers == std::vector<std::string>({"NVIDIA", "NVIDIA"}));
        ui_fixture::ApproveQrChallenge();
        Until([&] { return qr_completed == 1; });
        Pump(20);
        assert(nxnow::AppState::Instance().session()->user.user_id == "native-qr-user");
        assert(ui_fixture::Snapshot().provider_requests == 0);
    }
    if (scenario == "catalog-fr")
    {
        Pump(20);
        auto owned = nxnow::AppState::Instance().library_games();
        owned.resize(1);
        nxnow::AppState::Instance().SetLibraryGames(owned);
        Activate(root->getView("shell-catalog"));
        Until([&] { return root->getView("catalog-card-1000") != nullptr; });
        assert(carousel->ResidentCount() == 0);
        assert(ui_fixture::ResidentArtworkUrls().size() <= 10);
        assert(root->getView("catalog-card-1009") && !root->getView("catalog-card-1010"));
        Activate(root->getView("catalog-next"));
        Until([&] { return root->getView("catalog-card-1010") != nullptr; });
        assert(root->getView("catalog-card-1010") && !root->getView("catalog-card-1000"));
        assert(root->getView("catalog-card-1019") && !root->getView("catalog-card-1020"));
        assert(ui_fixture::ResidentArtworkUrls().size() <= 10);
        Activate(root->getView("catalog-next"));
        Until([&] { return root->getView("catalog-card-1020") != nullptr; });
        assert(!root->getView("catalog-card-1010") && ui_fixture::ResidentArtworkUrls().size() == 1);
        Activate(root->getView("catalog-previous"));
        assert(root->getView("catalog-card-1010") && !root->getView("catalog-card-1020"));
        assert(ui_fixture::ResidentArtworkUrls().size() == 10);
        auto* catalog = dynamic_cast<nxnow::CatalogTab*>(root->getView("catalog"));
        assert(catalog);
        catalog->SetBrowseMode(1);
        Until([&] { return root->getView("catalog-card-1000") != nullptr; });
        assert(root->getView("catalog-card-1002") && !root->getView("catalog-card-1001"));
        assert(ui_fixture::ResidentArtworkUrls().size() <= 10);
        catalog->SetBrowseMode(2);
        Until([&] { return root->getView("catalog-card-1001") != nullptr; });
        assert(!root->getView("catalog-card-1000"));
        Activate(root->getView("catalog-card-1001"));
        Pump(30);
        auto* detail = brls::Application::getActivitiesStack().back()->getContentView();
        auto* add = dynamic_cast<nxnow::ui::ActionRow*>(detail->getView("detail/add-library"));
        assert(add && dynamic_cast<brls::Label*>(add->getChildren().front())->getFullText() == "Je possède ce jeu");
        Activate(add);
        nxnow::GfnClient client;
        assert(client.LoadManualLibraryGames(*nxnow::AppState::Instance().session()).size() == 1);
        assert(dynamic_cast<brls::Label*>(add->getChildren().front())->getFullText() == "Dans la bibliothèque");
        brls::Application::onControllerButtonPressed(brls::BUTTON_B, false);
        Pump(30);
        assert(!root->getView("catalog-card-1001"));
        Activate(root->getView("shell-library"));
        assert(carousel->ResidentCount() <= 10);
        assert(ui_fixture::ResidentArtworkUrls().size() <= 11); // Covers plus selected backdrop.
        std::puts("PASS catalogue FR, free games, unowned filter, manual add, ten-cover paging");
    }
    if (scenario == "pages-fr")
    {
        Pump(20);
        assert(carousel->ItemCount() == 10 && carousel->ResidentCount() == 10);
        assert(carousel->SelectedIdentity() == "fixture-1000");
        carousel->MoveSelection(9);
        assert(carousel->SelectedIdentity() == "fixture-1009");
        carousel->MoveSelection(1);
        assert(carousel->IsNextPageSelected() && carousel->ResidentCount() == 10);
        Activate(carousel);
        assert(carousel->ItemCount() == 10 && carousel->ResidentCount() == 10);
        assert(carousel->SelectedIdentity() == "fixture-1010");
        for (const auto& url : ui_fixture::ResidentArtworkUrls())
            for (int old = 1000; old < 1010; ++old)
                assert(url != "fixture://cover/" + std::to_string(old));
        Activate(root->getView("library/next"));
        assert(carousel->ItemCount() == 1 && carousel->ResidentCount() == 1);
        assert(carousel->SelectedIdentity() == "fixture-1020");
        carousel->MoveSelection(100);
        assert(!carousel->IsNextPageSelected());
        Activate(root->getView("library/next"));
        assert(carousel->SelectedIdentity() == "fixture-1020");
        carousel->MoveSelection(-1);
        assert(carousel->IsPreviousPageSelected() && carousel->ResidentCount() == 1);
        Activate(carousel);
        assert(carousel->ItemCount() == 10 && carousel->SelectedIdentity() == "fixture-1010");
    }
    nxnow::GameDetailView* detail = nullptr;
    if (detail_test)
    {
        auto data = nxnow::MakeLibraryGameDetail(ui_fixture::Library().front());
        data.description_language = "en";
        data.description = "English description must not leak into the French page.";
        detail = new nxnow::GameDetailView(nxnow::GfnClient {}, std::move(data), scenario != "direct-fr");
        brls::Application::pushActivity(new brls::Activity(detail), brls::TransitionAnimation::NONE);
    }
    const auto began = std::chrono::steady_clock::now();
    bool settings_open = false, menu_open = false;
    int artwork_released_at = -1;
    for (int i = 0; i < (stress ? 4500 : 120); ++i)
    {
        assert(brls::Application::mainLoop());
        assert(startup_fixture::FrameDraws() > 10);
        assert(startup_fixture::UnsafeDeletions() == 0);
        assert(carousel->ResidentCount() <= 10);
        assert(carousel->ItemCount() <= 10);
        if (loading_test && i >= 20 && carousel->ItemCount() == 10 && artwork_released_at < 0)
        {
            auto* loading = root->getView("library/loading");
            assert(loading && loading->getVisibility() == brls::Visibility::VISIBLE);
            assert(carousel->ItemCount() == 10 && carousel->getVisibility() == brls::Visibility::GONE);
            auto* caption = dynamic_cast<brls::Label*>(loading->getView("loading-caption"));
            assert(caption && caption->getFullText() == "Chargement…");
            ui_fixture::ReleaseArtwork();
            artwork_released_at = i;
        }
        if (loading_test && artwork_released_at >= 0 && i == artwork_released_at + 10)
        {
            assert(root->getView("library/loading")->getVisibility() == brls::Visibility::GONE);
            assert(carousel->getVisibility() == brls::Visibility::VISIBLE);
        }
        if (stress && i % 500 == 100)
        { brls::Application::onControllerButtonPressed(brls::BUTTON_X, false); settings_open = true; }
        if (stress && i % 500 == 160)
        { brls::Application::onControllerButtonPressed(brls::BUTTON_B, false); settings_open = false; }
        if (stress && i % 500 == 230)
        { brls::Application::onControllerButtonPressed(brls::BUTTON_START, false); menu_open = true; }
        if (stress && i % 500 == 260)
        { brls::Application::onControllerButtonPressed(brls::BUTTON_B, false); menu_open = false; }
        if (!settings_open && !menu_open && carousel->ItemCount() &&
            (i == 20 || i == 60 || (stress && i % 40 == 0)))
            carousel->MoveSelection(i % 800 < 400 ? 5 : -5);
        if (stress && !settings_open && !menu_open && i % 80 == 0)
            brls::Application::onControllerButtonPressed(i % 480 < 240 ? brls::BUTTON_RB : brls::BUTTON_LB, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    std::printf("RESULT elapsed=%.2fs draws=%d unsafe_texture_deletions=%d shell=%gx%g carousel=%gx%g\n", elapsed,
        startup_fixture::FrameDraws(), startup_fixture::UnsafeDeletions(), root->getWidth(), root->getHeight(), carousel->getWidth(), carousel->getHeight());
    assert(startup_fixture::FrameDraws() > 10);
    assert(root->getWidth() == 1280 && root->getHeight() == 720);
    assert(startup_fixture::UnsafeDeletions() == 0);
    assert(!root->getView("library-info"));
    if (!guest && scenario != "empty")
    {
        const auto group_left = root->getView("library-play")->getFrame().getMinX();
        const auto group_right = (root->getView("library/pager")->getVisibility() == brls::Visibility::VISIBLE
            ? root->getView("library/pager") : root->getView("library-favorite"))->getFrame().getMaxX();
        assert(std::abs((group_left + group_right) / 2 - 640) < 1);
        assert(std::abs(root->getView("shell-hints")->getFrame().getMidX() - 640) < 1);
        assert(!root->getView("shell-account-name") && !root->getView("shell-account-detail"));
        assert(root->getView("shell-system-status")->getFrame().getMaxX() <= root->getView("shell-account")->getFrame().getMinX());
        assert(root->getView("shell-account")->getFrame().inscribed(root->getView("shell-header")->getFrame()));
        assert(root->getView("shell-system-status")->getFrame().inscribed(root->getView("shell-header")->getFrame()));
    }
    if (scenario == "auto-fr")
    {
        assert(nxnow::GetInterfaceLanguage() == "fr");
        assert(nxnow::LoadStreamSettings().interface_language == "auto");
        root->focusTab(nxnow::TopBarFrame::kSettingsTab); Pump();
        Activate(root->getView("settings/option/app-language"));
        brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_DOWN, false);
        brls::Application::onControllerButtonPressed(brls::BUTTON_A, false); Pump(20);
        assert(nxnow::GetInterfaceLanguage() == "en");
        assert(nxnow::LoadStreamSettings().interface_language == "en");
        Activate(root->getView("settings/option/app-language"));
        brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_UP, false);
        brls::Application::onControllerButtonPressed(brls::BUTTON_A, false); Pump(20);
        assert(nxnow::GetInterfaceLanguage() == "fr");
        assert(nxnow::LoadStreamSettings().interface_language == "auto");
        root->focusTab(nxnow::TopBarFrame::kLibraryTab); Pump();
    }
    if (french && !guest)
    {
        const auto calls = ui_fixture::Snapshot();
        assert(!calls.library_languages.empty() && calls.library_languages.front() == "fr");
        if (detail_test)
        {
            auto* frame = dynamic_cast<brls::ScrollingFrame*>(detail->getView("detail/description"));
            assert(frame && frame->getHeight() == 166);
            auto* label = dynamic_cast<brls::Label*>(frame->getChildren().front());
            assert(label && label->getFullText().find("English") == std::string::npos);
            if (scenario == "detail-fr")
            {
                assert(calls.description_languages == std::vector<std::string> {"fr"});
                assert(label->getFullText() == "Description complète en français du jeu sélectionné.");
            }
            else assert(calls.description_languages.empty());
        }
    }
    if (stress) assert(elapsed > 40);
    if (loading_test) assert(artwork_released_at >= 20);
    if (scenario == "input-fr")
    {
        root->focusTab(nxnow::TopBarFrame::kSettingsTab); Pump();
        assert(root->getView("settings/option/streaming-profile"));
        assert(!root->getView("settings/option/quality-profile"));
        Activate(root->getView("settings/advanced")); Pump(12);
        Activate(root->getView("settings/option/mouse-speed"));
        for (int i = 0; i < 5; ++i)
        {
            brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_DOWN, false);
            Pump(8);
        }
        brls::Application::onControllerButtonPressed(brls::BUTTON_A, false); Pump(20);
        assert(nxnow::LoadStreamSettings().mouse_speed_percent == 300);
        Activate(root->getView("settings/option/streaming-profile"));
        brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_DOWN, false);
        brls::Application::onControllerButtonPressed(brls::BUTTON_A, false); Pump(20);
        assert(nxnow::LoadStreamSettings().mouse_speed_percent == 300);
        Activate(root->getView("settings/option/check-for-updates"));
        Until([&] { return brls::Application::getActivitiesStack().back()->getContentView() != gate; });
        Pump(30);
        brls::Application::onControllerButtonPressed(brls::BUTTON_A, false); Pump();
        nxnow::StreamOverlaySample sample; sample.battery_percent = 87;
        assert(nxnow::FormatStreamOverlay(sample).battery == "Batterie 87%");
        assert(nxnow::FormatStreamOverlay(sample).input_mode == "Mode manette");
        sample.keyboard_mouse_mode = true;
        assert(nxnow::FormatStreamOverlay(sample).input_mode == "Mode clavier / souris");
        sample.battery_percent = -1;
        assert(nxnow::FormatStreamOverlay(sample).battery == "Batterie —");
        nxnow::QueueDisplayState display; display.game_title = "Synthetic game";
        auto* queue = new nxnow::QueueView(display, nxnow::StreamSettings {}, [] {});
        brls::Application::pushActivity(new brls::Activity(queue), brls::TransitionAnimation::NONE); Pump();
        assert(brls::Application::getCurrentFocus() != queue->getView("queue/cancel"));
        assert(queue->getView("queue/cover")->getFrame().getMidX() == queue->getView("queue/game")->getFrame().getMidX());
        brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_DOWN, false); Pump();
        assert(brls::Application::getCurrentFocus() == queue->getView("queue/cancel"));
        brls::Application::popActivity(brls::TransitionAnimation::NONE); Pump();
    }
    if (scenario == "profile-fr" || scenario == "settings-login-fr")
    {
        assert(root->getView("library/connect-help")->getVisibility() == brls::Visibility::VISIBLE);
        auto* prompt = dynamic_cast<brls::Label*>(root->getView("library/connect-prompt"));
        assert(prompt && prompt->getFullText() == nxnow::Tr("Click the profile icon to sign in and find your games."));
        assert(root->getView("library/connect-profile")->getFrame().inscribed(root->getFrame()));
        ui_fixture::EnableQrChallenge();
        if (scenario == "settings-login-fr")
        {
            root->focusTab(nxnow::TopBarFrame::kSettingsTab); Pump();
            Activate(root->getView("settings/option/sign-in-to-geforce-now"));
        }
        else Activate(root->getView("shell-account"));
        auto* qr = dynamic_cast<nxnow::QrLoginDialog*>(brls::Application::getActivitiesStack().back()->getContentView());
        assert(qr);
        auto* code = dynamic_cast<brls::Label*>(qr->getView("login/code"));
        Until([&] { return code->getFullText().find("WXYZ-1234") != std::string::npos; });
        std::puts("STEP profile QR approved; waiting for the library");
        ui_fixture::ApproveQrChallenge();
        Until([&] { return nxnow::AppState::Instance().HasSession() && root->getView("library") &&
            carousel->ItemCount() == 10; });
        Pump(20);
        assert(root->getView("library")->getParent());
        assert(ui_fixture::Snapshot().library_requests > 0);
        assert(ui_fixture::Snapshot().library_languages.front() == "fr");
        assert(carousel->ResidentCount() <= 10);
        assert(!root->getView("settings"));
        assert(root->getView("library/connect-help")->getVisibility() == brls::Visibility::GONE);
        Activate(root->getView("shell-account")); Pump(30);
        assert(nxnow::AppState::Instance().HasSession());
        assert(ui_fixture::Snapshot().sign_outs == 0);
        brls::Application::onControllerButtonPressed(brls::BUTTON_B, false); Pump(30);
        assert(nxnow::AppState::Instance().HasSession());
        Activate(root->getView("shell-account")); Pump(30);
        brls::Application::onControllerButtonPressed(brls::BUTTON_NAV_RIGHT, false); Pump(4);
        brls::Application::onControllerButtonPressed(brls::BUTTON_A, false); Pump(30);
        assert(!nxnow::AppState::Instance().HasSession());
        assert(ui_fixture::Snapshot().sign_outs == 1);
        assert(root->getView("library/connect-help")->getVisibility() == brls::Visibility::VISIBLE);
        assert(!root->getView("settings"));
        assert(ui_fixture::Snapshot().provider_requests == 0);
        std::puts("PASS direct QR/login-to-library/profile-logout flow");
    }
    if (scenario == "account-fr")
    {
        root->focusTab(nxnow::TopBarFrame::kSettingsTab); Pump();
        assert(!root->getView("settings/option/stream-mode"));
        assert(!root->getView("settings/option/switch-account"));
        Activate(root->getView("settings/option/sign-out"));
        assert(!nxnow::AppState::Instance().HasSession());
        assert(ui_fixture::Snapshot().sign_outs == 1);
        auto* settings = dynamic_cast<nxnow::SettingsTab*>(root->getView("settings"));
        assert(settings && settings->OpenAccountMenu()); Pump(20);
        const auto& activities = brls::Application::getActivitiesStack();
        assert(dynamic_cast<nxnow::QrLoginDialog*>(activities.back()->getContentView()));
        assert(ui_fixture::Snapshot().provider_requests == 0);
        assert(ui_fixture::Snapshot().qr_providers == std::vector<std::string> {"NVIDIA"});
    }
    brls::Application::quit();
    while (brls::Application::mainLoop()) {}
    std::filesystem::remove_all(path);
}
