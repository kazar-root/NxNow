#pragma once
#include "console_widgets.hpp"
#include "gfn_client.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <vector>

namespace nxnow
{
class LibraryTab;
class TopBarFrame : public brls::Box
{
  public:
    using TabViewCreator = std::function<brls::View*()>;
    static constexpr int kLibraryTab = 0, kSettingsTab = 1, kCatalogTab = 2;
    TopBarFrame();
    ~TopBarFrame() override;
    void addTab(const std::string& label, TabViewCreator creator);
    void focusTab(int position);
    void RefreshAccountLibrary();
    brls::View* getDefaultFocus() override;
    void onFocusGained() override;
    void draw(NVGcontext*, float, float, float, float, brls::Style, brls::FrameContext*) override;
  private:
    void SelectTab(int index);
    void UpdateStatusBar(bool force = false);
    void ShowMainMenu();
    void ShowAccount();
    GfnClient account_client_;
    LibraryTab* Library();
    void WireHeaderNavigation();
    brls::Box *header_container_, *tabs_container_, *content_container_, *right_container_;
    ui::ConsoleButton *menu_, *favorites_, *account_;
    brls::Label* queue_label_;
    brls::Box* queue_chip_;
    std::unique_ptr<ui::DynamicBackdrop> backdrop_;
    std::chrono::steady_clock::time_point last_status_update_ {};
    std::string displayed_language_;
    std::shared_ptr<std::atomic_bool> alive_ = std::make_shared<std::atomic_bool>(true);
    struct TabInfo {
        std::string label;
        TabViewCreator creator;
        ui::ConsoleButton* button;
        brls::View* content = nullptr;
    };
    std::vector<TabInfo> tabs_;
    std::vector<brls::View*> header_navigation_;
    int active_tab_index_ = -1;
    brls::View* active_content_ = nullptr;
    bool queue_chord_down_ = false;
};
}
