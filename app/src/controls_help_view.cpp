#include "controls_help_view.hpp"
#include "localization.hpp"
#include "ui_theme.hpp"

namespace nxnow
{
ControlsHelpView::ControlsHelpView() : brls::Box(brls::Axis::COLUMN)
{
    setId("controls-help");
    setBackgroundColor(ui::Ground());
    setPadding(28, 64, 24, 64);
    auto* title = ui::MakeLabel(Tr("Controls & help"), 30, ui::Text(), ui::FontRole::Heading);
    title->setMarginBottom(20);
    title->setShrink(0);
    addView(title);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setId("controls-help/scroll");
    scroll->setGrow(1);
    scroll->setMinHeight(0);
    auto* content = new brls::Box(brls::Axis::COLUMN);
    content->setPadding(4, 24, 16, 4);
    const auto paragraph = [content](const char* command, const char* explanation) {
        auto* row = new brls::Box(brls::Axis::COLUMN);
        row->setFocusable(true);
        row->setShrink(0);
        row->setPadding(10, 16, 10, 16);
        row->setMarginBottom(16);
        row->setCornerRadius(10);
        row->setBackgroundColor(ui::Raised());
        auto* keys = ui::MakeLabel(Tr(command), 21, ui::Accent(), ui::FontRole::Heading);
        keys->setSingleLine(false);
        keys->setMarginBottom(8);
        row->addView(keys);
        auto* detail = ui::MakeLabel(Tr(explanation), 19, ui::Text());
        detail->setSingleLine(false);
        row->addView(detail);
        content->addView(row);
    };
    paragraph("Library: +", "Plus opens the main menu. Left/right select a cover within this page of ten games. Previous/Next or L/R change page. Down reaches Play. Up reaches the toolbar. Search finds titles across your library.");
    paragraph("Stats: + and -", "Press both buttons together, then release to show or hide FPS, bitrate, latency and battery level.");
    paragraph("Home: + and - and ZL and ZR", "Press the four buttons together to stop the session and return to NxNow.");
    paragraph("Keyboard: R3 + ZR", "Press ZR and the right stick click in either order, then release both to open the keyboard in any game or loading screen. Plus sends Enter; B closes.");
    paragraph("WoW chat: ZR + R3", "Press ZR and the right stick click, then release both. The chat and keyboard open. Plus sends your message; B cancels.");
    paragraph("World of Warcraft: A/B and X/Y", "A/B and X/Y are automatically swapped only in World of Warcraft to play with Blizzard's official controller module. Other games keep the mapping selected in Settings.");
    paragraph("Mouse: L3 + R3", "L3 + R3 toggles mouse/gamepad in every game. Left stick moves the pointer; ZR left-clicks, ZL right-clicks. D-pad sends keyboard arrows; Minus sends Escape. Adjust pointer speed in Settings.");
    paragraph("Skip intro: hold + for 1 second", "Send Escape to skip an intro. Confirm inside the game if requested.");
    paragraph("USB keyboard and mouse", "Connect compatible USB devices to the Switch dock or USB adapter. Keyboard and mouse input is available in every game, regardless of its store.");
    scroll->setContentView(content);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    addView(scroll);
    auto* close = new ui::ActionRow(Tr("Press B to close this window"), {}, [](brls::View*) {
        brls::Application::popActivity(); return true;
    });
    close->setId("controls-help/close");
    close->setHeight(48);
    close->setMarginTop(16);
    close->setShrink(0);
    addView(close);
    registerAction(Tr("Close help"), brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity(); return true;
    }, true);
    setLastFocusedView(scroll);
}
}
