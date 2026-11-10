#include "settings_tab.hpp"

#include "app_state.hpp"
#include "localization.hpp"
#include "stream_settings.hpp"
#include "stream_settings_policy.hpp"
#include "stream_diagnostics.hpp"
#include "ui_action_guard.hpp"
#include "ui_helpers.hpp"

#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

namespace nxnow
{

brls::Label* SettingsTab::MakeParagraph(
    const std::string& text, float bottom_margin)
{
    auto* label = ui::MakeLabel(Tr(text));
    label->setSingleLine(false);
    label->setMarginBottom(bottom_margin);
    return label;
}

std::string SettingsTab::FormatBytes(std::uint64_t bytes)
{
    if (bytes < 1024)
        return std::to_string(bytes) + " B";

    const double kib = static_cast<double>(bytes) / 1024.0;
    if (kib < 1024.0)
        return std::to_string(static_cast<int>(kib)) + " KiB";

    const double mib = kib / 1024.0;
    std::ostringstream formatted;
    formatted << std::fixed << std::setprecision(1) << mib << " MiB";
    return formatted.str();
}

std::string SettingsTab::BitrateValue(int kbps)
{
    std::ostringstream value;
    value << std::fixed << std::setprecision(3) << static_cast<double>(kbps) / 1000.0;
    std::string text = value.str();
    while (!text.empty() && text.back() == '0')
        text.pop_back();
    if (!text.empty() && text.back() == '.')
        text.pop_back();
    return text + " Mbps";
}

namespace
{

std::string OptionId(const std::string& title)
{
    std::string id = "settings/option/";
    for (unsigned char character : title)
        id += std::isalnum(character) ? static_cast<char>(std::tolower(character)) : '-';
    return id;
}

} // namespace

SettingsTab::SettingsTab()
    : brls::Box(brls::Axis::COLUMN)
{
    setId("settings");
    setPadding(20, 64, 16, 64);
    setBackgroundColor(ui::Ground());
    EnsureSessionLoaded();
    saved_settings_ = LoadStreamSettings();
    draft_settings_ = saved_settings_;
    settings_loaded_ = true;

    auto* header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setMarginBottom(6);
    page_title_ = ui::MakeLabel(Tr("Settings"), 30, ui::Text(), ui::FontRole::Heading);
    page_title_->setGrow(1);
    header->addView(page_title_);
    save_status_ = ui::MakeLabel(Tr("Saved automatically"), 15, ui::Accent());
    save_status_->setId("settings/save-status");
    header->addView(save_status_);
    addView(header);
    page_subtitle_ = MakeParagraph("Only the essentials. Changes apply to your next game.", 12);
    page_subtitle_->setFontSize(16);
    page_subtitle_->setTextColor(ui::Muted());
    addView(page_subtitle_);

    scrolling_frame_ = new brls::ScrollingFrame();
    scrolling_frame_->setId("settings/options");
    scrolling_frame_->setGrow(1);
    scrolling_frame_->setMinHeight(0);
    scrolling_frame_->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scrolling_frame_->setScrollingIndicatorVisible(false);
    content_container_ = new brls::Box(brls::Axis::COLUMN);
    content_container_->setPadding(4, 8, 12, 8);
    scrolling_frame_->setContentView(content_container_);
    addView(scrolling_frame_);
    help_description_ = ui::MakeLabel({}, 15, ui::Muted());
    help_description_->setId("settings/help/description");
    help_description_->setHeight(42);
    help_description_->setMarginTop(8);
    help_description_->setSingleLine(false);
    addView(help_description_);
    RebuildSettings();
}

SettingsTab::~SettingsTab()
{
    alive_->store(false);
}

brls::Box* SettingsTab::MakeSection(const std::string& title, const std::string& subtitle)
{
    auto* section = new brls::Box(brls::Axis::COLUMN);
    section->setMarginBottom(8);

    if (!title.empty())
    {
        auto* label = ui::MakeLabel(Tr(title), 14, ui::Muted(), ui::FontRole::Medium);
        label->setSingleLine(true);
        label->setMarginBottom(4);
        section->addView(label);
    }
    (void)subtitle;
    return section;
}

brls::Box* SettingsTab::MakeOptionRow(
    const std::string& title,
    const std::string& description,
    std::function<std::string()> value,
    std::function<bool(brls::View*)> action)
{
    auto* row = new ui::ActionRow(Tr(title), Tr(value()),
        [this, action = std::move(action)](brls::View* view) {
        const bool handled = action ? action(view) : true;
        UpdateOptionValues();
        return handled;
    }, ui::ActionTone::Flat);
    row->setHeight(50);
    row->setId(OptionId(title));
    row->updateActionHint(brls::BUTTON_A, Tr("Change"));
    row->setMarginBottom(6);
    row->SetFocusHandler([this, description] { ShowOptionHelp(description); });
    option_values_.push_back({row, std::move(value)});
    return row;
}

brls::Box* SettingsTab::MakeActionRow(
    const std::string& title,
    const std::string& description,
    const std::string& button_text,
    std::function<bool(brls::View*)> action,
    bool destructive)
{
    auto* row = new ui::ActionRow(Tr(title), Tr(button_text), std::move(action),
        destructive ? ui::ActionTone::Destructive : ui::ActionTone::Flat);
    row->setHeight(50);
    row->setId(OptionId(title));
    row->setMarginBottom(6);
    row->SetFocusHandler([this, description] { ShowOptionHelp(description); });
    return row;
}

brls::Label* SettingsTab::AddInfoLine(
    brls::Box* parent, const std::string& label, const std::string& value)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setMinHeight(40);
    row->setPadding(5, 8, 5, 8);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setMarginBottom(2);
    row->setCornerRadius(7);
    auto* key = MakeParagraph(label, 0.0f);
    key->setWidthPercentage(40);
    key->setMarginRight(12);
    key->setShrink(0);
    key->setSingleLine(true);
    key->setFontSize(15);
    key->setTextColor(ui::Muted());
    row->addView(key);
    auto* text = MakeParagraph(value, 0.0f);
    text->setGrow(1);
    text->setShrink(1);
    text->setSingleLine(true);
    text->setFontSize(15);
    text->setTextColor(ui::Text());
    row->addView(text);
    parent->addView(row);
    return text;
}

void SettingsTab::RebuildSettings()
{
    const auto stack = brls::Application::getActivitiesStack();
    if (!stack.empty() && !IsViewInside(stack.back()->getContentView(), this))
    {
        rebuild_pending_ = true;
        return;
    }
    rebuild_pending_ = false;
    const bool restore_focus = IsViewInside(content_container_, brls::Application::getCurrentFocus());
    MoveFocusBeforeDestroy(content_container_, nullptr);
    setLastFocusedView(nullptr);
    option_values_.clear();
    cover_cache_files_ = nullptr;
    cover_cache_bytes_ = nullptr;
    content_container_->clearViews();
    BuildAccountPage();
    BuildStreamPage();
    BuildAppPage();
    auto* advanced = MakeActionRow("Advanced options", "Game language, server, controls and audio.",
        advanced_visible_ ? "Hide" : "Open", [this](brls::View*) {
            advanced_visible_ = !advanced_visible_;
            focus_advanced_after_rebuild_ = true;
            rebuild_pending_ = true;
            return true;
        });
    advanced->setId("settings/advanced");
    content_container_->addView(advanced);
    if (advanced_visible_)
    {
        BuildAdvancedPage();
        BuildPreferencesPage();
    }
    scrolling_frame_->setContentOffsetY(0, false);
    page_title_->setText(Tr("Settings"));
    page_subtitle_->setText(Tr("Only the essentials. Changes apply to your next game."));
    RefreshSummary();
    UpdateOptionValues();
    auto* first = focus_advanced_after_rebuild_ ? advanced : content_container_->getDefaultFocus();
    focus_advanced_after_rebuild_ = false;
    setLastFocusedView(first);
    if (restore_focus)
        brls::Application::giveFocus(first);
}

void SettingsTab::draw(NVGcontext* vg, float x, float y, float width, float height,
                       brls::Style style, brls::FrameContext* ctx)
{
    const auto stack = brls::Application::getActivitiesStack();
    if (rebuild_pending_ && !stack.empty() && IsViewInside(stack.back()->getContentView(), this))
    {
        rebuild_pending_ = false;
        brls::sync([this, alive = alive_] {
            if (alive->load())
                RebuildSettings();
        });
    }
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void SettingsTab::UpdateOptionValues()
{
    for (auto& [row, value] : option_values_)
    {
        if (row && value)
            row->SetValue(Tr(value()));
    }
    UpdateOptionHelp();
}

void SettingsTab::ShowOptionHelp(const std::string& description)
{
    help_description_text_ = description;
    UpdateOptionHelp();
}

void SettingsTab::UpdateOptionHelp()
{
    if (help_description_)
        help_description_->setText(Tr(help_description_text_));
}

void SettingsTab::MarkDirty()
{
    dirty_ = draft_settings_ != saved_settings_;
    RefreshSummary();
    if (dirty_)
        SaveChanges(nullptr);
}

bool SettingsTab::SaveChanges(brls::View* view)
{
    (void)view;
    if (!settings_loaded_ || !dirty_)
    {
        brls::Application::notify(Tr("Settings are already up to date"));
        return true;
    }

    const bool interface_language_changed =
        draft_settings_.interface_language != saved_settings_.interface_language;
    if (!SaveStreamSettings(draft_settings_))
    {
        ShowError("Settings Save Failed", "Could not safely write stream_settings.json to the SD card.");
        return true;
    }

    saved_settings_ = LoadStreamSettings();
    draft_settings_ = saved_settings_;
    SetInterfaceLanguage(saved_settings_.interface_language);
    SetStreamDiagnosticsEnabled(saved_settings_.debug_diagnostics);
    dirty_ = false;
    RefreshSummary();
    UpdateOptionValues();
    // Rebuild only after the dropdown releases focus, outside its callback.
    rebuild_pending_ = rebuild_pending_ || interface_language_changed;
    return true;
}

void SettingsTab::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    EnsureSessionLoaded();
    if (!settings_loaded_ || !dirty_)
    {
        saved_settings_ = LoadStreamSettings();
        draft_settings_ = saved_settings_;
        settings_loaded_ = true;
    }
    RefreshSummary();
    RebuildSettings();
}

void SettingsTab::EnsureSessionLoaded()
{
    auto& state = AppState::Instance();
    if (state.IsSessionLoaded())
        return;

    AuthSession session;
    if (client_.LoadSavedSession(session))
        state.SetSession(std::move(session));
    else
        state.MarkSessionLoaded();
}

void SettingsTab::RefreshSummary()
{
    if (!save_status_)
        return;
    save_status_->setText(Tr(dirty_ ? "Changes not saved" : "Saved automatically"));
    save_status_->setTextColor(dirty_ ? ui::Danger() : ui::Accent());
    UpdateOptionHelp();
}

} // namespace nxnow
