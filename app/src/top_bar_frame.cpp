#include "top_bar_frame.hpp"
#include "app_state.hpp"
#include "app_version.hpp"
#include "controls_help_view.hpp"
#include "library_tab.hpp"
#include "catalog_tab.hpp"
#include "localization.hpp"
#include "membership_label.hpp"
#include "network_utils.hpp"
#include "settings_tab.hpp"
#include "qr_login_dialog.hpp"
#include "ui_helpers.hpp"
#include <borealis/core/touch/tap_gesture.hpp>
#include <algorithm>
#include <ctime>

namespace nxnow
{
namespace
{
class SystemStatusView final : public brls::View
{
  public:
    SystemStatusView() { setId("shell-system-status"); setWidth(402); setHeight(50); setShrink(0); }
    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) override
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - sampled_ >= std::chrono::seconds(1))
        {
            sampled_ = now;
            const auto time = std::time(nullptr); std::tm local {};
#ifdef _WIN32
            localtime_s(&local, &time);
#else
            localtime_r(&time, &local);
#endif
            char value[8] {}; std::strftime(value, sizeof(value), "%H:%M", &local); clock_ = value;
            auto* platform = brls::Application::getPlatform();
            battery_ = platform->canShowBatteryLevel() ? platform->getBatteryLevel() : -1;
            charging_ = battery_ >= 0 && platform->isBatteryCharging();
            network_ = NetworkUtils::GetConnectionInfo();
        }
        nvgSave(vg); nvgGlobalAlpha(vg, a(ui::Text()).a);
        nvgBeginPath(vg); nvgRoundedRect(vg, x, y, w, h, h / 2);
        nvgFillColor(vg, ui::Opacity(ui::Raised(), 0.84f)); nvgFill(vg);
        nvgStrokeWidth(vg, 1); nvgStrokeColor(vg, ui::Opacity(ui::Text(), 0.16f)); nvgStroke(vg);
        nvgFontFaceId(vg, ui::Font(ui::FontRole::Medium)); nvgFontSize(vg, 15);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE); nvgFillColor(vg, ui::Text());
        nvgText(vg, x + 18, y + h / 2, clock_.c_str(), nullptr);
        if (battery_ >= 0 && battery_ <= 100)
        {
            nvgBeginPath(vg); nvgRoundedRect(vg, x + 83, y + 19, 22, 12, 2);
            nvgStrokeWidth(vg, 1.2f); nvgStrokeColor(vg, ui::Text()); nvgStroke(vg);
            nvgBeginPath(vg); nvgRect(vg, x + 106, y + 22, 2, 6); nvgFill(vg);
            nvgBeginPath(vg); nvgRect(vg, x + 86, y + 22, 16.0f * battery_ / 100, 6); nvgFill(vg);
            const auto text = std::to_string(battery_) + "%" + (charging_ ? "+" : "");
            nvgText(vg, x + 114, y + h / 2, text.c_str(), nullptr);
        }
        std::string network = "—";
        if (!network_.connected) network = Tr("Offline");
        else if (network_.type == NetworkConnectionType::Ethernet) network = "LAN";
        else if (network_.type == NetworkConnectionType::Wifi) network = "Wi-Fi";
        for (int bar = 0; bar < 3; ++bar)
        {
            const bool lit = network_.connected && (network_.type == NetworkConnectionType::Ethernet ||
                (network_.type == NetworkConnectionType::Wifi && network_.wifi_strength >= bar + 1));
            nvgBeginPath(vg); nvgRoundedRect(vg, x + 179 + bar * 5, y + 29 - bar * 4, 3, 4 + bar * 4, 1);
            nvgFillColor(vg, ui::Opacity(ui::Text(), lit ? 1 : 0.25f)); nvgFill(vg);
        }
        nvgFillColor(vg, ui::Text()); nvgText(vg, x + 204, y + h / 2, network.c_str(), nullptr);
        const auto& session = AppState::Instance().session();
        if (session)
        {
            const auto membership = session->user.membership_tier_verified
                ? Tr(session->user.membership_tier) : Tr("Unknown");
            nvgBeginPath(vg); nvgMoveTo(vg, x + 277, y + 14); nvgLineTo(vg, x + 277, y + h - 14);
            nvgStrokeColor(vg, ui::Opacity(ui::Text(), 0.2f)); nvgStrokeWidth(vg, 1); nvgStroke(vg);
            nvgScissor(vg, x + 289, y + 8, w - 303, h - 16);
            nvgFillColor(vg, ui::Accent()); nvgFontSize(vg, 14);
            nvgText(vg, x + 289, y + h / 2, membership.c_str(), nullptr);
        }
        nvgRestore(vg);
    }
  private:
    std::chrono::steady_clock::time_point sampled_ {};
    std::string clock_;
    int battery_ = -1;
    bool charging_ = false;
    NetworkConnectionInfo network_;
};
}
TopBarFrame::TopBarFrame() : brls::Box(brls::Axis::COLUMN), backdrop_(std::make_unique<ui::DynamicBackdrop>())
{
    setGrow(1); setBackgroundColor(ui::Ground());
    header_container_ = new brls::Box(brls::Axis::ROW);
    header_container_->setId("shell-header"); header_container_->setHeight(100); header_container_->setShrink(0);
    header_container_->setPadding(10, 32, 10, 32); header_container_->setAlignItems(brls::AlignItems::CENTER);
    auto* brand = new brls::Box(brls::Axis::ROW); brand->setWidth(92); brand->setShrink(0);
    brand->setAlignItems(brls::AlignItems::CENTER);
    auto* logo = new ui::BlendedLogo(); logo->setId("shell-logo"); logo->setWidth(80); logo->setHeight(80); logo->setShrink(0);
    logo->setMarginRight(10); brand->addView(logo);
    header_container_->addView(brand);
    menu_ = new ui::ConsoleButton(ui::ConsoleIcon::Menu, Tr("Main menu"), [this] { ShowMainMenu(); });
    menu_->setId("shell-menu"); menu_->setMarginRight(10); header_container_->addView(menu_);
    favorites_ = new ui::ConsoleButton(ui::ConsoleIcon::Favorite, Tr("Favorites"), [this] {
        auto* library = Library(); library->SetFavoritesOnly(!library->FavoritesOnly());
        UpdateStatusBar(true);
    });
    favorites_->setId("shell-favorites"); favorites_->setMarginRight(10); header_container_->addView(favorites_);
    tabs_container_ = new brls::Box(brls::Axis::ROW); tabs_container_->setId("shell-tabs");
    tabs_container_->setAlignItems(brls::AlignItems::CENTER); header_container_->addView(tabs_container_);
    auto* space = new brls::Box(); space->setGrow(1); header_container_->addView(space);
    queue_chip_ = new brls::Box(brls::Axis::ROW); queue_chip_->setHeight(44); queue_chip_->setCornerRadius(22);
    queue_chip_->setMarginLeft(10); queue_chip_->setPadding(0, 12, 0, 12); queue_chip_->setBackgroundColor(ui::Raised());
    queue_chip_->setAlignItems(brls::AlignItems::CENTER); queue_chip_->setFocusable(true);
    queue_label_ = ui::MakeLabel({}, 14, ui::Text()); queue_chip_->addView(queue_label_);
    queue_chip_->registerClickAction([](brls::View*) { if (IsQueueMinimized()) RestoreMinimizedQueueDialog(); return true; });
    queue_chip_->addGestureRecognizer(new brls::TapGestureRecognizer(queue_chip_));
    queue_chip_->setVisibility(brls::Visibility::GONE); header_container_->addView(queue_chip_);
    auto* system_status = new SystemStatusView(); system_status->setMarginLeft(12);
    header_container_->addView(system_status);
    account_ = new ui::ConsoleButton(ui::ConsoleIcon::Account, Tr("Account"), [this] { ShowAccount(); });
    account_->setId("shell-account"); account_->setMarginLeft(8); header_container_->addView(account_);
    right_container_ = new brls::Box(brls::Axis::ROW); right_container_->setAlignItems(brls::AlignItems::CENTER);
    right_container_->setMarginLeft(10); header_container_->addView(right_container_); addView(header_container_);
    content_container_ = new brls::Box(brls::Axis::COLUMN); content_container_->setId("shell-content");
    content_container_->setGrow(1); content_container_->setMinHeight(0); addView(content_container_);
    auto* footer = new brls::Box(brls::Axis::ROW); footer->setId("shell-footer");
    footer->setHeight(48); footer->setShrink(0); footer->setPadding(0, 36, 0, 36);
    footer->setAlignItems(brls::AlignItems::CENTER);
    auto* version = ui::MakeLabel(Tr("NxNow  ·  ") + std::string(kAppVersion), 12, ui::Muted());
    version->setWidth(144); version->setShrink(0); footer->addView(version);
    auto* hints = new brls::Hints(); hints->setId("shell-hints"); hints->setGrow(1); hints->setAllowAButtonTouch(true);
    hints->setJustifyContent(brls::JustifyContent::CENTER);
    footer->addView(hints);
    auto* footer_balance = new brls::Box(); footer_balance->setWidth(144); footer_balance->setShrink(0);
    footer->addView(footer_balance); addView(footer);
    registerAction({}, brls::BUTTON_LT, [this](brls::View*) { if (!tabs_.empty()) SelectTab((active_tab_index_ + static_cast<int>(tabs_.size()) - 1) % tabs_.size()); return true; }, true);
    registerAction({}, brls::BUTTON_RT, [this](brls::View*) { if (!tabs_.empty()) SelectTab((active_tab_index_ + 1) % tabs_.size()); return true; }, true);
    registerAction(Tr("Settings"), brls::BUTTON_X, [this](brls::View*) { focusTab(kSettingsTab); return true; });
    registerAction(Tr("Main menu"), brls::BUTTON_START, [this](brls::View*) { ShowMainMenu(); return true; });
    registerAction(Tr("Back"), brls::BUTTON_B, [this](brls::View*) {
        if (active_tab_index_ != kLibraryTab) { focusTab(kLibraryTab); return true; }
        if (auto* library = dynamic_cast<LibraryTab*>(active_content_); library && library->FavoritesOnly())
        { library->SetFavoritesOnly(false); favorites_->SetActive(false); return true; }
        return false;
    });
}
TopBarFrame::~TopBarFrame()
{
    alive_->store(false);
    if (active_content_) content_container_->removeView(active_content_, false);
    for (auto& tab : tabs_) delete tab.content;
}
void TopBarFrame::addTab(const std::string& label, TabViewCreator creator)
{
    const int index = static_cast<int>(tabs_.size());
    const auto icon = label == "Settings" ? ui::ConsoleIcon::Settings :
        label == "Catalog" ? ui::ConsoleIcon::Catalog : ui::ConsoleIcon::Library;
    auto* button = new ui::ConsoleButton(icon,
        Tr(label), [this, index] {
            SelectTab(index);
            if (auto* library = dynamic_cast<LibraryTab*>(active_content_)) library->SetFavoritesOnly(false);
            UpdateStatusBar(true);
        });
    button->setId(label == "Settings" ? "shell-settings" : label == "Catalog" ? "shell-catalog" : "shell-library");
    (label == "Settings" ? right_container_ : tabs_container_)->addView(button);
    tabs_.push_back({label, std::move(creator), button, nullptr});
    WireHeaderNavigation();
    if (tabs_.size() == 1) SelectTab(0);
}
void TopBarFrame::WireHeaderNavigation()
{
    header_navigation_ = {menu_, favorites_};
    for (auto& tab : tabs_) if (tab.label != "Settings") header_navigation_.push_back(tab.button);
    header_navigation_.push_back(account_);
    for (auto& tab : tabs_) if (tab.label == "Settings") header_navigation_.push_back(tab.button);
    for (size_t i = 0; i < header_navigation_.size(); ++i)
    {
        auto* button = header_navigation_[i];
        button->setCustomNavigationRoute(brls::FocusDirection::LEFT, header_navigation_[i ? i - 1 : i]);
        button->setCustomNavigationRoute(brls::FocusDirection::RIGHT, header_navigation_[std::min(i + 1, header_navigation_.size() - 1)]);
        if (active_content_) button->setCustomNavigationRoute(brls::FocusDirection::DOWN, active_content_);
    }
    if (auto* library = dynamic_cast<LibraryTab*>(active_content_)) library->SetHeaderNavigation(tabs_[kLibraryTab].button);
}
void TopBarFrame::focusTab(int position) { SelectTab(position); }
brls::View* TopBarFrame::getDefaultFocus() { return active_content_ ? active_content_->getDefaultFocus() : menu_; }
void TopBarFrame::onFocusGained() { brls::Box::onFocusGained(); if (active_content_) brls::Application::giveFocus(active_content_); }
void TopBarFrame::SelectTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;
    if (active_tab_index_ == index && active_content_) { brls::Application::giveFocus(active_content_); return; }
    if (active_content_)
    {
        brls::Application::giveFocus(tabs_[index].button);
        if (auto* library = dynamic_cast<LibraryTab*>(active_content_)) library->SuspendArtwork();
        if (auto* catalog = dynamic_cast<CatalogTab*>(active_content_)) catalog->SuspendArtwork();
        content_container_->removeView(active_content_, false);
    }
    active_tab_index_ = index;
    auto& tab = tabs_[index];
    if (!tab.content) tab.content = tab.creator();
    active_content_ = tab.content;
    if (auto* library = dynamic_cast<LibraryTab*>(active_content_))
    {
        library->SetAccountHandler([this] { ShowAccount(); });
        library->SetBackdropResetHandler([this] { backdrop_->Clear(); });
        library->SetBackdropHandler([this](const std::string& url) { backdrop_->Select(url); });
    }
    else if (dynamic_cast<CatalogTab*>(active_content_)) backdrop_->Clear();
    else backdrop_->Select({});
    active_content_->setGrow(1); content_container_->addView(active_content_);
    WireHeaderNavigation(); UpdateStatusBar(true); brls::Application::giveFocus(active_content_);
}
LibraryTab* TopBarFrame::Library() { SelectTab(kLibraryTab); return dynamic_cast<LibraryTab*>(active_content_); }
void TopBarFrame::RefreshAccountLibrary()
{
    if (active_tab_index_ != kLibraryTab || !active_content_) SelectTab(kLibraryTab);
    if (auto* library = dynamic_cast<LibraryTab*>(active_content_)) library->RefreshAccount();
    UpdateStatusBar(true);
    brls::Application::giveFocus(active_content_);
}
void TopBarFrame::ShowAccount()
{
    try
    {
        const auto& session = AppState::Instance().session();
        if (session && !session->reauthentication_required)
        {
            const auto alive = alive_;
            const auto generation = AppState::Instance().session_generation();
            auto* dialog = new brls::Dialog(Tr("Account connected"));
            dialog->addButton(Tr("Close"), [] {});
            dialog->addButton(Tr("Sign out"), [this, alive, generation] {
                if (!alive->load() || !AppState::Instance().IsCurrentSession(generation)) return;
                DisconnectCurrentAccount(account_client_);
                RefreshAccountLibrary();
            });
            dialog->setCancelable(true);
            dialog->open();
            return;
        }
        const auto alive = alive_;
        auto* dialog = new QrLoginDialog(GfnClient::NvidiaLoginProvider(), account_client_, [this, alive] {
            if (!alive->load()) return;
            RefreshAccountLibrary();
        });
        brls::Application::pushActivity(new brls::Activity(dialog));
    }
    catch (const std::exception& error)
    {
        ShowError("GeForce NOW Login Failed", error.what());
    }
}
void TopBarFrame::ShowMainMenu()
{
    auto* dropdown = new brls::Dropdown(Tr("Main menu"),
        {Tr("Library"), Tr("Favorites"), Tr("Search"), Tr("Library options"), Tr("Settings"), Tr("Controls & help"), Tr("GeForce NOW catalog"), Tr("Free-to-play games")},
        [](int) {}, 0, [this, alive = alive_](int item) {
            if (!alive->load() || item < 0) return;
            switch (item) {
                case 0: Library()->SetFavoritesOnly(false); break;
                case 1: Library()->SetFavoritesOnly(true); break;
                case 2: Library()->BeginSearch(); break;
                case 3: Library()->ShowBrowseOptions(); break;
                case 4: SelectTab(kSettingsTab); break;
                case 5: brls::Application::pushActivity(new brls::Activity(new ControlsHelpView())); break;
                case 6: SelectTab(kCatalogTab); dynamic_cast<CatalogTab*>(active_content_)->SetBrowseMode(0); break;
                case 7: SelectTab(kCatalogTab); dynamic_cast<CatalogTab*>(active_content_)->SetBrowseMode(1); break;
                default: break;
            }
            UpdateStatusBar(true);
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}
void TopBarFrame::UpdateStatusBar(bool force)
{
    if (displayed_language_ != GetInterfaceLanguage())
    { displayed_language_ = GetInterfaceLanguage(); force = true; }
    const auto now = std::chrono::steady_clock::now();
    if (!force && now - last_status_update_ < std::chrono::seconds(1)) return;
    last_status_update_ = now;
    const auto& session = AppState::Instance().session();
    account_->SetVerified(session && !session->reauthentication_required);
    menu_->SetTitle(Tr("Main menu")); favorites_->SetTitle(Tr("Favorites")); account_->SetTitle(Tr(session && !session->reauthentication_required ? "Account" : "Sign in with QR code"));
    updateActionHint(brls::BUTTON_START, Tr("Main menu"));
    updateActionHint(brls::BUTTON_X, Tr("Settings"));
    updateActionHint(brls::BUTTON_B, Tr("Back"));
    for (auto& tab : tabs_) tab.button->SetTitle(Tr(tab.label));
    auto* library = dynamic_cast<LibraryTab*>(active_content_);
    favorites_->SetActive(library && library->FavoritesOnly());
    for (size_t i = 0; i < tabs_.size(); ++i) tabs_[i].button->SetActive(static_cast<int>(i) == active_tab_index_ && !(library && library->FavoritesOnly()));
    const bool queued = IsQueueMinimized();
    queue_chip_->setVisibility(queued ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    const int position = GetCurrentQueuePosition();
    queue_label_->setText(Tr("Queue") + " " + (position < 0 ? "…" : std::to_string(position)));
}
void TopBarFrame::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style style, brls::FrameContext* ctx)
{
    UpdateStatusBar();
    nvgSave(vg); nvgGlobalAlpha(vg, a(ui::Text()).a);
    backdrop_->draw(vg, x, y, w, h, style, ctx); nvgRestore(vg);
    brls::ControllerState state {};
    brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&state);
    const bool chord = state.buttons[brls::BUTTON_BACK] && state.buttons[brls::BUTTON_START];
    if (chord && !queue_chord_down_ && IsQueueMinimized()) brls::sync([] { RestoreMinimizedQueueDialog(); });
    queue_chord_down_ = chord;
    brls::Box::draw(vg, x, y, w, h, style, ctx);
}
}
