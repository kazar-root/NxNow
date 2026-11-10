#include "app_state.hpp"
#include "catalog_tab.hpp"
#include "cover_image_cache.hpp"
#include "game_card_view.hpp"
#include "game_detail_view.hpp"
#include "library_tab.hpp"
#include "game_carousel.hpp"
#include "library_favorites.hpp"
#include "launch_settings_policy.hpp"
#include "library_row_view.hpp"
#include "localization.hpp"
#include "queue_view.hpp"
#include "settings_tab.hpp"
#include "stream_overlay_view.hpp"
#include "stream_settings.hpp"
#include "stream_settings_policy.hpp"
#include "top_bar_frame.hpp"
#include "ui_layout_fixtures.hpp"
#include "ui_theme.hpp"
#include "ui_helpers.hpp"

#include <borealis.hpp>
#include <borealis/extern/glad/glad.h>
#include <borealis/extern/nanovg/stb_truetype.h>
#include <SDL2/SDL.h>
#include <yoga/Yoga.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
int failures = 0;
std::string capture_path;
void Screenshot(const std::string& path);
void Check(bool passed, const std::string& message)
{
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", message.c_str());
    failures += !passed;
}
void Pump(int frames = 24)
{
    for (int index = 0; index < frames; ++index)
        if (!brls::Application::mainLoop())
            throw std::runtime_error("Native event loop exited before assertions completed");
}
void DrainToasts()
{
    auto style = brls::Application::getStyle();
    const auto duration = static_cast<int>(style.getMetric("brls/animations/notification_timeout") +
        2 * style.getMetric("brls/animations/notification_show") + 100);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration);
    while (std::chrono::steady_clock::now() < deadline) Pump(1);
}
void Until(const std::function<bool()>& predicate, const std::string& message, int seconds = 5)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) Pump(1);
    Check(predicate(), message);
    Pump();
}
brls::View* CurrentRoot()
{
    const auto stack = brls::Application::getActivitiesStack();
    return stack.empty() ? nullptr : stack.back()->getContentView();
}
void Walk(brls::View* root, const std::function<void(brls::View*)>& visit)
{
    if (!root || root->getVisibility() == brls::Visibility::GONE) return;
    visit(root);
    if (auto* box = dynamic_cast<brls::Box*>(root))
        for (auto* child : box->getChildren()) Walk(child, visit);
}
std::string Text(brls::View* root)
{
    std::string text;
    Walk(root, [&](brls::View* view) {
        if (auto* label = dynamic_cast<brls::Label*>(view)) text += label->getFullText() + "\n";
    });
    return text;
}
brls::View* Required(brls::View* root, const std::string& id)
{
    auto* view = root ? root->getView(id) : nullptr;
    Check(view != nullptr, "semantic view exists: " + id);
    if (!view) throw std::runtime_error("Missing production view ID: " + id);
    return view;
}
brls::View* Action(brls::View* root, const std::string& title)
{
    brls::View* result = nullptr;
    Walk(root, [&](brls::View* view) {
        if (view->isFocusable() && Text(view).find(nxnow::Tr(title)) != std::string::npos) result = view;
    });
    Check(result != nullptr, "production action exists: " + title);
    if (!result) throw std::runtime_error("Missing production action: " + title);
    return result;
}
void Key(brls::ControllerButton button)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (brls::Application::isInputBlocks() && std::chrono::steady_clock::now() < deadline) Pump(1);
    Check(!brls::Application::isInputBlocks(), "controller dispatch is not blocked by a transition");
    std::printf("INPUT button=%d focus=%s\n", static_cast<int>(button),
        brls::Application::getCurrentFocus() ? brls::Application::getCurrentFocus()->describe().c_str() : "none");
    brls::Application::onControllerButtonPressed(button, false);
    Pump();
}
void Activate(brls::View* view)
{
    brls::Application::giveFocus(view);
    Pump();
    Check(brls::Application::getCurrentFocus() == view->getDefaultFocus(), "action owns native focus before A");
    Key(brls::BUTTON_A);
}
void Tap(brls::View* target)
{
    const auto frame = target->getFrame();
    brls::Point point(frame.getMinX() + frame.getWidth() * 0.5f, frame.getMinY() + frame.getHeight() * 0.5f);
    auto* responder = CurrentRoot()->hitTest(point);
    Check(responder != nullptr, "native hitTest finds touch responder");
    if (!responder) throw std::runtime_error("Touch target is outside the production activity");
    auto* ancestor = responder;
    while (ancestor && ancestor != target) ancestor = ancestor->hasParent() ? ancestor->getParent() : nullptr;
    Check(ancestor == target, "touch responder belongs to the painted action");
    brls::TouchState touch;
    touch.position = point;
    touch.view = responder;
    touch.phase = brls::TouchPhase::START;
    responder->gestureRecognizerRequest(touch, {}, responder);
    touch.phase = brls::TouchPhase::END;
    responder->gestureRecognizerRequest(touch, {}, responder);
    Pump();
}
void Search(brls::View* root, const std::string& query)
{
    const std::string id = root->getView("catalog") ? "catalog-search" : "library-search";
    brls::Application::giveFocus(Required(root, id));
    Pump();
    const auto count = brls::Application::getActivitiesStack().size();
    Key(brls::BUTTON_A);
    Check(brls::Application::getActivitiesStack().size() == count + 1, "Search action opens actual native SDL IME dialog");
    auto* input = dynamic_cast<brls::Label*>(Required(CurrentRoot(), "brls/dialog/label"));
    auto* count_label = dynamic_cast<brls::Label*>(Required(CurrentRoot(), "brls/dialog/count"));
    if (!input || !count_label) throw std::runtime_error("Native IME text/count label missing");
    size_t deletes = 0;
    while (!count_label->getFullText().starts_with("0/") && deletes++ < 64)
        brls::Application::onControllerButtonPressed(brls::BUTTON_BACK, false);
    Check(count_label->getFullText().starts_with("0/"), "native IME deletion clears previous query");
    if (!query.empty())
    {
        SDL_Event event {};
        event.type = SDL_TEXTINPUT;
        std::snprintf(event.text.text, sizeof(event.text.text), "%s", query.c_str());
        Check(SDL_PushEvent(&event) == 1, "SDL accepts actual text-input event");
        Pump();
        Check(input->getFullText() == query, "native IME receives SDL text-input query");
    }
    Key(brls::BUTTON_A);
    Check(brls::Application::getActivitiesStack().size() == count, "native IME submit returns to production page");
}
void CheckChildren(brls::Box* box, const std::string& path)
{
    const auto bounds = box->getFrame();
    float previous_right = bounds.getMinX();
    size_t index = 0;
    for (auto* child : box->getChildren())
    {
        if (child->getVisibility() == brls::Visibility::GONE) continue;
        const auto rect = child->getFrame();
        const auto name = path + "/" + std::to_string(index++);
        std::printf("RECT %s %.1f %.1f %.1f %.1f\n", name.c_str(), rect.getMinX(), rect.getMinY(), rect.getWidth(), rect.getHeight());
        Check(rect.getMinX() >= bounds.getMinX() - 2 && rect.getMaxX() <= bounds.getMaxX() + 2, name + " horizontal containment");
        if (!dynamic_cast<brls::ScrollingFrame*>(box))
            Check(rect.getMinY() >= bounds.getMinY() - 2 && rect.getMaxY() <= bounds.getMaxY() + 2, name + " vertical containment");
        const bool flow = YGNodeStyleGetPositionType(child->getYGNode()) != YGPositionTypeAbsolute;
        if (box->getAxis() == brls::Axis::ROW && !dynamic_cast<brls::ScrollingFrame*>(box) && flow)
            Check(rect.getMinX() >= previous_right - 2, name + " no flow sibling overlap");
        if (flow) previous_right = rect.getMaxX();
        if (auto* nested = dynamic_cast<brls::Box*>(child)) CheckChildren(nested, name);
    }
}
struct FontFile
{
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info {};
    explicit FontFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(file), {});
        if (bytes.empty() || !stbtt_InitFont(&info, bytes.data(), stbtt_GetFontOffsetForIndex(bytes.data(), 0)))
            throw std::runtime_error("Cannot inspect bundled font: " + path);
    }
};
std::vector<int> Codepoints(const std::string& text)
{
    std::vector<int> result;
    for (size_t offset = 0; offset < text.size();)
    {
        const unsigned char first = text[offset++];
        int count = first < 0x80 ? 0 : (first & 0xe0) == 0xc0 ? 1 : (first & 0xf0) == 0xe0 ? 2 : 3;
        int code = first & (count == 0 ? 0x7f : count == 1 ? 0x1f : count == 2 ? 0x0f : 0x07);
        while (count-- && offset < text.size()) code = (code << 6) | (static_cast<unsigned char>(text[offset++]) & 0x3f);
        result.push_back(code);
    }
    return result;
}
void CheckFonts(brls::View* root)
{
    const std::string dir = std::string(NXNOW_UI_SOURCE_ROOT) + "/resources/";
    const std::array<const char*, 5> paths {"font/Nunito-Medium.ttf", "font/Nunito-SemiBold.ttf", "font/Nunito-Bold.ttf", "font/Nunito-ExtraBold.ttf", "font/IBMPlexMono-Medium.ttf"};
    const std::array<const char*, 5> names {"regular", "nxnow-semibold", "nxnow-bold", "nxnow-extrabold", "nxnow-mono"};
    std::vector<FontFile> bases;
    std::vector<int> handles;
    for (size_t index = 0; index < paths.size(); ++index)
    {
        handles.push_back(nxnow::ui::Font(static_cast<nxnow::ui::FontRole>(index)));
        Check(handles.back() != brls::FONT_INVALID && brls::Application::getFont(names[index]) == handles.back(), "real named NXNow font role registered: " + std::to_string(index));
        bases.emplace_back(dir + paths[index]);
    }
    Check(brls::Application::getDefaultFont() == handles.front(), "stock label default uses initialized Nunito 500");
    std::vector<FontFile> fallbacks;
    for (const char* path : {"font/NXNow-CJK.ttf", "font/switch_font.ttf", "font/switch_icons.ttf", "material/MaterialIcons-Regular.ttf"}) fallbacks.emplace_back(dir + path);
    Check(brls::Application::getFont("nxnow-cjk") != brls::FONT_INVALID, "bundled Chinese fallback registered");
    Walk(root, [&](brls::View* view) {
        auto* label = dynamic_cast<brls::Label*>(view);
        if (!label) return;
        const auto found = std::find(handles.begin(), handles.end(), label->getFont());
        Check(found != handles.end(), "production label uses an initialized NXNow font");
        if (found == handles.end()) return;
        const size_t index = found - handles.begin();
        for (int code : Codepoints(label->getFullText()))
        {
            if (code <= 32) continue;
            bool present = stbtt_FindGlyphIndex(&bases[index].info, code) != 0;
            for (auto& fallback : fallbacks) present = present || stbtt_FindGlyphIndex(&fallback.info, code) != 0;
            if (!present) Check(false, "actual bundled font chain lacks codepoint " + std::to_string(code));
        }
    });
    auto* vg = brls::Application::getNVGContext();
    for (int handle : handles)
    {
        nvgFontFaceId(vg, handle);
        nvgFontSize(vg, 18);
        float bounds[4] {};
        const float measured = nvgTextBounds(vg, 0, 0, "NXNow Жї 玩家设置", nullptr, bounds);
        Check(std::isfinite(measured) && measured > 100 && bounds[3] > bounds[1], "actual font chain measures mixed Latin/Cyrillic/Chinese text");
    }
}
struct GlyphMetrics { float width, left_bearing, right_overhang; };
GlyphMetrics MeasureGlyphs(brls::Label* label, const std::string& text, const ui_fixture::TextDraw* draw = nullptr)
{
    auto* vg = brls::Application::getNVGContext();
    nvgSave(vg);
    nvgFontFaceId(vg, label->getFont());
    nvgFontSize(vg, label->getFontSize());
    nvgFontQuality(vg, label->getFontQuality());
    nvgTextLineHeight(vg, label->getLineHeight());
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    if (draw)
    {
        nvgResetTransform(vg);
        const auto& transform = draw->transform;
        nvgTransform(vg, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5]);
    }
    const float x = draw ? draw->raw_x : 0, y = draw ? draw->raw_y : 0;
    float bounds[4] {};
    const float advance = nvgTextBounds(vg, x, y, text.c_str(), nullptr, bounds);
    nvgRestore(vg);
    return {bounds[2] - bounds[0], std::min(0.0f, bounds[0] - x), std::max(0.0f, bounds[2] - x - advance)};
}
float GlyphWidth(brls::Label* label) { return MeasureGlyphs(label, label->getFullText()).width; }
std::vector<ui_fixture::TextDraw> CaptureText()
{
    ui_fixture::BeginTextCapture();
    Pump(1);
    return ui_fixture::EndTextCapture();
}
void CheckPaintedLabel(brls::Label* label, const std::vector<ui_fixture::TextDraw>& draws,
                       const std::string& name, bool require_ellipsis = false,
                       const brls::Rect* row_bounds = nullptr, const brls::Rect* scissor_bounds = nullptr)
{
    const auto full = label->getFullText();
    if (full.empty()) return;
    const auto frame = label->getFrame();
    bool found = false;
    for (const auto& draw : draws)
    {
        const bool ellipsis = draw.text.ends_with("…");
        const bool identity = draw.text == full ||
            (ellipsis && full.starts_with(draw.text.substr(0, draw.text.size() - std::string("…").size())));
        if (!identity || std::abs(draw.y - frame.getMidY()) > 1 ||
            draw.bounds[2] < frame.getMinX() || draw.bounds[0] > frame.getMaxX()) continue;
        found = true;
        const auto glyphs = MeasureGlyphs(label, draw.text, &draw);
        std::printf("GLYPHS %s text=[%s] ink=%.2f,%.2f,%.2f,%.2f frame=%.2f,%.2f,%.2f,%.2f scrolling=%d\n",
            name.c_str(), draw.text.c_str(), draw.bounds[0], draw.bounds[1], draw.bounds[2], draw.bounds[3],
            frame.getMinX(), frame.getMinY(), frame.getMaxX(), frame.getMaxY(), draw.scrolling_path);
        Check(!draw.scrolling_path, name + " paints static text, not a scissored marquee");
        Check(std::abs(draw.x - frame.getMinX()) <= 0.5f || std::abs(draw.x - frame.getMidX()) <= 0.5f,
            name + " native text anchor remains at allocated label origin or centre");
        const float raster_pixel = 1.0f / brls::Application::windowScale;
        Check(draw.bounds[0] >= frame.getMinX() + glyphs.left_bearing - raster_pixel &&
            draw.bounds[2] <= frame.getMaxX() + glyphs.right_overhang + raster_pixel,
            name + " painted glyphs fit label width with actual measured bearings");
        std::printf("BEARINGS %s anchor=%.2f expected=%.2f left=%.2f right=%.2f raster_pixel=%.5f\n",
            name.c_str(), draw.x, frame.getMinX(), glyphs.left_bearing, glyphs.right_overhang, raster_pixel);
        for (const auto* bounds : {row_bounds, scissor_bounds})
            if (bounds) Check(draw.bounds[0] >= bounds->getMinX() - 0.5f && draw.bounds[2] <= bounds->getMaxX() + 0.5f &&
                draw.bounds[1] >= bounds->getMinY() - 0.5f && draw.bounds[3] <= bounds->getMaxY() + 0.5f,
                name + " actual glyph ink remains inside row and scissor bounds");
        Check(draw.bounds[1] >= frame.getMinY() - 1 && draw.bounds[3] <= frame.getMaxY() + 1,
            name + " actual painted glyphs fit allocated label height");
        if (require_ellipsis) Check(ellipsis, name + " overflowing game identity has a complete static ellipsis");
    }
    Check(found, name + " observed actual NanoVG text draw");
}
void CheckActionRow(nxnow::ui::ActionRow* row, const std::vector<ui_fixture::TextDraw>& draws,
                    const std::string& name, bool full_caption = false)
{
    const auto frame = row->getFrame();
    const float left = frame.getMinX() + YGNodeLayoutGetPadding(row->getYGNode(), YGEdgeLeft);
    const float right = frame.getMaxX() - YGNodeLayoutGetPadding(row->getYGNode(), YGEdgeRight);
    float previous_right = left;
    brls::Rect scissor;
    bool has_scissor = false;
    auto* parent = row->hasParent() ? row->getParent() : nullptr;
    while (parent)
    {
        if (auto* viewport = dynamic_cast<brls::ScrollingFrame*>(parent))
        {
            scissor = viewport->getFrame();
            has_scissor = true;
            break;
        }
        parent = parent->hasParent() ? parent->getParent() : nullptr;
    }
    for (auto* child : row->getChildren())
    {
        auto* label = dynamic_cast<brls::Label*>(child);
        if (!label || label->getVisibility() != brls::Visibility::VISIBLE || label->getFullText().empty()) continue;
        const auto label_frame = label->getFrame();
        Check(label_frame.getMinX() >= left - 0.5f && label_frame.getMaxX() <= right + 0.5f,
            name + " title/value label stays inside row content padding");
        Check(label_frame.getMinX() >= previous_right - 0.5f, name + " title and value do not overlap");
        previous_right = label_frame.getMaxX();
        const float required = GlyphWidth(label);
        std::printf("CAPTION %s text=[%s] font=%d size=%.1f glyph_width=%.2f available=%.2f\n",
            name.c_str(), label->getFullText().c_str(), label->getFont(), label->getFontSize(), required, label_frame.getWidth());
        if (full_caption) Check(required <= label_frame.getWidth() + 0.5f, name + " complete fixed caption fits measured content width");
        CheckPaintedLabel(label, draws, name, false, &frame, has_scissor ? &scissor : nullptr);
    }
}
void CheckVisibleActionRows(brls::View* root, const std::string& name)
{
    const auto draws = CaptureText();
    Walk(root, [&](brls::View* view) {
        auto* row = dynamic_cast<nxnow::ui::ActionRow*>(view);
        if (!row) return;
        const auto frame = row->getFrame();
        auto* parent = row->hasParent() ? row->getParent() : nullptr;
        while (parent)
        {
            if (auto* viewport = dynamic_cast<brls::ScrollingFrame*>(parent))
            {
                const auto bounds = viewport->getFrame();
                if (frame.getMinY() < bounds.getMinY() || frame.getMaxY() > bounds.getMaxY()) return;
            }
            parent = parent->hasParent() ? parent->getParent() : nullptr;
        }
        auto title = Text(row);
        std::replace(title.begin(), title.end(), '\n', ' ');
        CheckActionRow(row, draws, name + "/" + title);
    });
}
void CheckLibraryToolbar(brls::View* root)
{
    Check(!root->getView("library-more"), "Library has no duplicate toolbar refresh button");
    for (const char* id : {"shell-menu", "shell-favorites", "shell-library", "shell-account", "shell-settings", "library-search"})
    {
        auto* button = Required(root, id);
        Check(dynamic_cast<nxnow::ui::ConsoleButton*>(button) && button->isFocusable(), "floating toolbar has a native focusable icon: " + std::string(id));
    }
    Check(Required(root, "shell-system-status")->getWidth() == 402, "system status has bounded native geometry");
}
void CheckStaticGameTitle(brls::View* owner, brls::Label* title, const std::string& name)
{
    Check(title != nullptr, name + " native title label exists");
    if (!title) throw std::runtime_error("Missing production game title label");
    brls::Application::giveFocus(owner); Pump(); DrainToasts();
    const bool overflow = GlyphWidth(title) > title->getWidth() + 1;
    Check(overflow, name + " fixture genuinely exceeds native title width");
    CheckPaintedLabel(title, CaptureText(), name + "/focused", overflow);
    auto read_title_pixels = [&] {
        const auto frame = title->getFrame();
        const float scale = brls::Application::windowScale;
        const int x = std::lround(frame.getMinX() * scale), width = std::lround(frame.getWidth() * scale);
        const int y = brls::Application::windowHeight - std::lround(frame.getMaxY() * scale), height = std::lround(frame.getHeight() * scale);
        std::vector<unsigned char> pixels(width * height * 3);
        glReadBuffer(GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto first_pixels = read_title_pixels();
    const std::string title_name = name.starts_with("library") ? "library" : "store";
    Screenshot(capture_path + "." + title_name + "-title-focused.ppm");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2200);
    while (std::chrono::steady_clock::now() < deadline) Pump(1);
    CheckPaintedLabel(title, CaptureText(), name + "/after-scroll-timer", overflow);
    Check(first_pixels == read_title_pixels(), name + " title framebuffer pixels remain unchanged after native scroll timer");
    Screenshot(capture_path + "." + title_name + "-title-after-scroll-timer.ppm");
}
float CheckScrollDecorations(brls::ScrollingFrame* viewport, const std::string& name)
{
    float reserved_right = 0;
    for (auto* child : viewport->getChildren())
    {
        if (!child->isDetached() || child->getWidth() != 4 || child->getAlpha() <= 0.001f) continue;
        reserved_right = std::max(reserved_right, viewport->getFrame().getMaxX() - child->getFrame().getMinX());
    }
    Check(reserved_right == 0, name + " has no visible scrollbar over art, labels or focus border");
    return reserved_right;
}
void CheckFocusedScrollInsets(brls::View* focused, const std::string& name)
{
    auto* parent = focused->hasParent() ? focused->getParent() : nullptr;
    while (parent && !dynamic_cast<brls::ScrollingFrame*>(parent)) parent = parent->hasParent() ? parent->getParent() : nullptr;
    auto* viewport = dynamic_cast<brls::ScrollingFrame*>(parent);
    Check(viewport != nullptr, name + " is inside a native scrolling viewport");
    if (!viewport) throw std::runtime_error("Missing native scrolling ancestor");
    const auto bounds = viewport->getFrame(), frame = focused->getFrame();
    const float reserved = CheckScrollDecorations(viewport, name);
    Check(frame.getMinX() - 4 >= bounds.getMinX() - 0.5f && frame.getMaxX() + 4 <= bounds.getMaxX() - reserved + 0.5f,
        name + " complete focus border fits horizontally inside scrolling viewport");
}
void CheckSettingsActionRows(brls::View* root, const std::string& category)
{
    auto* viewport = dynamic_cast<brls::ScrollingFrame*>(Required(root, "settings/options"));
    if (!viewport) throw std::runtime_error("Missing production settings scissor viewport");
    std::vector<nxnow::ui::ActionRow*> rows;
    Walk(viewport, [&](brls::View* view) { if (auto* row = dynamic_cast<nxnow::ui::ActionRow*>(view)) rows.push_back(row); });
    Check(!rows.empty(), "Settings category contains production action rows: " + category);
    auto* original_focus = brls::Application::getCurrentFocus();
    const auto bounds = viewport->getFrame();
    for (auto* row : rows)
    {
        brls::Application::giveFocus(row); Pump(16);
        const auto frame = row->getFrame();
        auto title = Text(row);
        std::replace(title.begin(), title.end(), '\n', ' ');
        const auto name = "settings/" + category + "/" + title;
        const float scrollbar_reservation = CheckScrollDecorations(viewport, name);
        Check(brls::Application::getCurrentFocus() == row, name + " actual row owns focus");
        Check(frame.getMinX() - 4 >= bounds.getMinX() - 0.5f, name + " focus border has left scissor inset");
        Check(frame.getMaxX() + 4 <= bounds.getMaxX() - scrollbar_reservation + 0.5f,
            name + " focus border clears actual right scissor and scrollbar reservation");
        Check(frame.getMinY() - 4 >= bounds.getMinY() - 0.5f && frame.getMaxY() + 4 <= bounds.getMaxY() + 0.5f,
            name + " focused row and border fit vertical scissor viewport");
        CheckActionRow(row, CaptureText(), name);
    }
    if (original_focus) { brls::Application::giveFocus(original_focus); Pump(); }
}
void CheckTabLanguage(brls::View* root)
{
    for (const auto& [id, label] : std::vector<std::pair<std::string, std::string>>{{"shell-library", "Library"}, {"shell-settings", "Settings"}})
    {
        const auto& actions = Required(root, id)->getActions();
        Check(std::any_of(actions.begin(), actions.end(), [&](const auto& action) {
            return action->getButton() == brls::BUTTON_A && action->getHintText() == nxnow::Tr(label);
        }), "toolbar icons expose translated accessible action names");
    }
}
void SaveLanguageThroughSettings(brls::View* root, const std::string& language)
{
    auto* frame = dynamic_cast<nxnow::TopBarFrame*>(root);
    if (!frame) throw std::runtime_error("Language regression requires production shell");
    frame->focusTab(nxnow::TopBarFrame::kSettingsTab); Pump();
    Activate(Action(root, "App language"));
    Check(dynamic_cast<brls::Dropdown*>(CurrentRoot()) != nullptr, "actual App language opens native selector");
    const auto& options = nxnow::InterfaceLanguageOptions();
    const auto current = std::find_if(options.begin(), options.end(), [](const auto& option) {
        return option.code == nxnow::GetInterfaceLanguage();
    });
    const auto target = std::find_if(options.begin(), options.end(), [&](const auto& option) { return option.code == language; });
    if (current == options.end() || target == options.end()) throw std::runtime_error("Unsupported language regression fixture");
    const auto stored = nxnow::LoadStreamSettings().interface_language;
    const auto current_index = stored == "auto" ? 0 : current - options.begin() + 1;
    const auto difference = target - options.begin() + 1 - current_index;
    for (int index = 0; index < std::abs(difference); ++index)
        Key(difference > 0 ? brls::BUTTON_NAV_DOWN : brls::BUTTON_NAV_UP);
    Key(brls::BUTTON_A);
    Check(nxnow::LoadStreamSettings().interface_language == language && nxnow::GetInterfaceLanguage() == language,
        "actual Settings selector automatically persists and applies interface language");
    CheckTabLanguage(root);
    frame->focusTab(nxnow::TopBarFrame::kLibraryTab); Pump();
    Check(Text(root).find(nxnow::Tr("My Library")) != std::string::npos &&
        dynamic_cast<nxnow::ui::ConsoleButton*>(Required(root, "library-search")) != nullptr,
        "retained library headings refresh after automatic language save");

}
void Screenshot(const std::string& path)
{
    const int width = brls::Application::windowWidth, height = brls::Application::windowHeight;
    std::vector<unsigned char> pixels(width * height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    auto lit = [&](int x, int y) {
        const size_t index = (static_cast<size_t>(y) * width + x) * 3;
        return pixels[index] != 0 || pixels[index + 1] != 0 || pixels[index + 2] != 0;
    };
    bool right_edge = false, footer = false;
    for (int y = 0; y < height && !right_edge; ++y) right_edge = lit(width - 1, y);
    const int footer_rows = static_cast<int>(48 * brls::Application::windowScale);
    for (int y = 0; y < footer_rows && !footer; ++y)
        for (int x = 0; x < width && !footer; ++x) footer = lit(x, y);
    Check(right_edge && footer, "native frame covers right edge and bottom footer of the framebuffer");
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = height - 1; y >= 0; --y) file.write(reinterpret_cast<const char*>(pixels.data() + y * width * 3), width * 3);
    Check(file.good(), "native framebuffer capture written");
}

void StoreChecks(brls::View* root, bool empty, bool guest)
{
    Until([] { return ui_fixture::Snapshot().catalog_requests + ui_fixture::Snapshot().public_requests > 0; }, "production Store fetch callback reached external fixture");
    std::vector<nxnow::GameCardView*> cards;
    Walk(root, [&](brls::View* view) { if (auto* card = dynamic_cast<nxnow::GameCardView*>(view)) cards.push_back(card); });
    if (empty) { Check(cards.empty(), "empty real Store feed creates no game cards"); return; }
    Check(cards.size() == 10, "production Store page displays ten covers");
    CheckStaticGameTitle(cards.front(), dynamic_cast<brls::Label*>(cards.front()->getView("catalog-card-title")), "store/game-title");
    CheckFocusedScrollInsets(cards.front(), "store/first-column");
    for (size_t index = 0; index < cards.size(); ++index)
    {
        const auto rect = cards[index]->getFrame();
        Check(std::abs(rect.getWidth() - 220) < 0.01 && std::abs(rect.getHeight() - 194) < 0.01, "Store card has exact 220x194 logical geometry");
        if (index % 5 != 0) Check(std::abs(rect.getMinX() - cards[index - 1]->getFrame().getMinX() - 245) < 0.01, "Store card origins have exact 245 spacing");
        const auto row = cards[index]->getParent()->getFrame();
        Check(std::abs(row.getWidth() - 1200) < 0.01, "Store row spans 1200 logical pixels");
        Check(rect.getMinX() >= row.getMinX() && rect.getMaxX() <= row.getMaxX() + 0.01, "Store card remains within the 1200-wide row");
    }
    brls::Application::giveFocus(cards[4]); Pump(); Key(brls::BUTTON_NAV_DOWN);
    Check(brls::Application::getCurrentFocus() == cards[9], "real Store down navigation preserves fifth column");
    Key(brls::BUTTON_NAV_UP);
    Check(brls::Application::getCurrentFocus() == cards[4], "real Store up navigation returns to fifth column");
    CheckFocusedScrollInsets(cards[4], "store/fifth-column");
    Check(Required(root, "catalog-filter")->getVisibility() == brls::Visibility::VISIBLE &&
          Required(root, "catalog-sort")->getVisibility() == brls::Visibility::GONE,
          "catalog exposes search and browse filters while keeping legacy sorting hidden");
    Search(root, "Fixture game 1");
    if (guest) Check(root->getView("catalog-card-1000") == nullptr && ui_fixture::Snapshot().catalog_requests == 0, "guest Store search filters actual public feed without authenticated requests");
    else Until([] { const auto calls = ui_fixture::Snapshot(); return !calls.search_queries.empty() && calls.search_queries.back() == "Fixture game 1"; }, "Y search passes actual query to catalog service");
    Search(root, "");
    if (!guest) Until([] { return ui_fixture::Snapshot().catalog_requests >= 3; }, "clearing catalog search reloads feed");
    Activate(Required(root, "catalog-next"));
    if (guest) Check(root->getView("catalog-card-1010") != nullptr && ui_fixture::Snapshot().public_requests == 1, "guest Store next page uses already-loaded public data without another fetch");
    else
    {
        Until([&] { return root->getView("catalog-card-1010") != nullptr; }, "next ten-game page arrives");
        Check(root->getView("catalog-card-1010") != nullptr && root->getView("catalog-card-1000") == nullptr,
            "next page replaces the previous ten covers with remaining loaded titles");
        Until([] { const auto calls = ui_fixture::Snapshot(); return std::find(calls.cursors.begin(), calls.cursors.end(), "fixture-page-2") != calls.cursors.end(); },
            "each ten-game page consumes the next catalog cursor");
    }
    Activate(Required(root, "catalog-previous"));
    brls::Application::giveFocus(Required(root, "catalog-card-1000")); Pump();
    const size_t size = brls::Application::getActivitiesStack().size(); Key(brls::BUTTON_A);
    Check(brls::Application::getActivitiesStack().size() == size + 1 && dynamic_cast<nxnow::GameDetailView*>(CurrentRoot()), "Store A opens actual production detail activity");
    Key(brls::BUTTON_B);
    Check(brls::Application::getActivitiesStack().size() == size, "detail B returns to real Store");
}
void LibraryChecks(brls::View* root, bool guest, bool empty)
{
    CheckLibraryToolbar(root);
    auto* carousel = dynamic_cast<nxnow::GameCarouselView*>(Required(root, "library-carousel"));
    if (!carousel) throw std::runtime_error("Library does not use the production carousel");
    if (guest || empty)
    {
        Check(carousel->ItemCount() == 0 && carousel->ResidentCount() == 0, "guest and empty libraries do not fabricate games or artwork");
        if (guest)
            Check(Required(root, "library/connect-help")->getVisibility() == brls::Visibility::VISIBLE &&
                  Required(root, "library-empty")->getVisibility() == brls::Visibility::GONE,
                  "guest library offers QR sign-in instead of an empty-library message");
        else
            Check(Required(root, "library-empty")->getVisibility() == brls::Visibility::VISIBLE, "empty state remains readable and actionable through the toolbar");
        return;
    }
    Until([] { return ui_fixture::Snapshot().library_requests > 0; }, "Library reaches the fixture");
    Check(carousel->ItemCount() == 10 && carousel->ResidentCount() <= 10, "the carousel contains only the first page of ten games");
    const auto first = carousel->SelectedIdentity();
    brls::Application::giveFocus(carousel); Pump(); Key(brls::BUTTON_NAV_RIGHT);
    Check(carousel->SelectedIdentity() != first && brls::Application::getCurrentFocus() == carousel,
        "native Right selects the next cover without leaving the carousel");
    Key(brls::BUTTON_NAV_LEFT);
    Check(carousel->SelectedIdentity() == first, "native Left restores selected game identity");
    const int fetches = ui_fixture::Snapshot().library_requests;
    Key(brls::BUTTON_RB); Key(brls::BUTTON_LB);
    Check(carousel->SelectedIdentity() == first && ui_fixture::Snapshot().library_requests == fetches,
        "L/R switch bounded pages without extra metadata requests");
    for (int i = 0; i < 30; ++i) Key(brls::BUTTON_NAV_RIGHT);
    Check(carousel->ResidentCount() <= 10, "repeated navigation never grows the texture window");
    for (int i = 0; i < 30; ++i) Key(brls::BUTTON_NAV_LEFT);
    Key(brls::BUTTON_NAV_DOWN);
    Check(brls::Application::getCurrentFocus() == Required(root, "library-play"), "Down reaches the white Play button");
    Activate(Required(root, "library-favorite"));
    Check(nxnow::library::LoadFavorites("native-fixture-user").contains(first), "favorite action persists the real selected game for this account");
    Activate(Required(root, "shell-favorites"));
    Check(carousel->ItemCount() == 1 && carousel->SelectedIdentity() == first, "toolbar Favorites filters the actual library");
    Key(brls::BUTTON_B);
    Check(carousel->ItemCount() == 10, "Back leaves Favorites and restores the first page");
    brls::Application::giveFocus(carousel); Pump(); Key(brls::BUTTON_NAV_UP);
    Check(brls::Application::getCurrentFocus() == Required(root, "shell-library"), "Up reaches the floating Library icon");
    Key(brls::BUTTON_NAV_DOWN);
    Check(brls::Application::getCurrentFocus() == carousel, "Down from toolbar returns to the real selected game");
    Search(root, "Fixture game 1");
    Check(carousel->ItemCount() < 21 && carousel->ItemCount() > 0, "native IME filters the actual library");
    Search(root, "");
    brls::Application::giveFocus(carousel); Key(brls::BUTTON_A);
    Check(dynamic_cast<nxnow::GameDetailView*>(CurrentRoot()), "the selected cover opens the game page with profile and shortcut actions");
    Key(brls::BUTTON_B); Key(brls::BUTTON_X);
    Check(root->getView("settings/options"), "X opens existing settings");
    Key(brls::BUTTON_B); Check(root->getView("library-carousel"), "B returns to the carousel");
    const std::string original = nxnow::GetInterfaceLanguage();
    for (const std::string& language : {"fr", "en"}) SaveLanguageThroughSettings(root, language);
    SaveLanguageThroughSettings(root, original);
}
void SettingsChecks(brls::View* root, const std::string& capture)
{
    Check(!root->getView("settings/category/app") && !root->getView("settings/option/decoder") &&
          !root->getView("settings/option/bitrate"), "settings contain one essential list without technical submenus");
    const auto original = nxnow::LoadStreamSettings();
    Check(!root->getView("settings/option/button-mapping") && !root->getView("settings/option/game-language") &&
          !root->getView("settings/option/server-location"), "advanced settings start collapsed");
    Activate(Action(root, "Streaming profile"));
    Check(dynamic_cast<brls::Dropdown*>(CurrentRoot()), "quality opens one native selector");
    Key(brls::BUTTON_NAV_DOWN); Key(brls::BUTTON_A);
    auto changed = nxnow::LoadStreamSettings();
    Check(changed.preset_id == "quality" && changed.height == 1080 && changed.fps == 60 && changed.bitrate_kbps == 20000,
          "one quality selection saves all video parameters without X Save");
    Check(changed.controller_layout == original.controller_layout && changed.audio_volume == original.audio_volume &&
          changed.stream_mode == original.stream_mode, "quality selection preserves control/audio/stream choices");
    Check(!root->getView("settings/option/stream-mode"), "redundant stream mode is absent from Settings");
    Activate(Required(root, "settings/advanced"));
    Until([root] { return root->getView("settings/option/stream-stats-overlay") != nullptr; },
          "advanced options open outside the activation callback");
    Check(brls::Application::getCurrentFocus() == root->getView("settings/advanced"),
          "expanding advanced options retains focus on the toggle");
    Activate(Action(root, "Stream stats overlay"));
    Check(nxnow::LoadStreamSettings().stats_overlay_enabled != original.stats_overlay_enabled, "overlay switch saves immediately");
    Activate(Action(root, "Streaming profile"));
    Key(brls::BUTTON_NAV_DOWN); Key(brls::BUTTON_A);
    Until([root] { return root->getView("settings/option/maximum-bitrate") != nullptr; },
          "Custom exposes individual video controls after the selector closes");
    const auto custom = nxnow::LoadStreamSettings();
    Activate(Action(root, "Frame rate"));
    Activate(Action(root, "Maximum bitrate"));
    const auto edited = nxnow::LoadStreamSettings();
    Check(edited.preset_id == "custom" && edited.fps != custom.fps && edited.bitrate_kbps != custom.bitrate_kbps,
          "custom FPS and bitrate save immediately without reapplying a preset");
    CheckSettingsActionRows(root, "essential");
    CheckChildren(dynamic_cast<brls::Box*>(root), "settings-essential");
    CheckFonts(root); DrainToasts(); Screenshot(capture + ".settings-essential.ppm");
    Check(ui_fixture::Snapshot().region_requests == 0 && ui_fixture::Snapshot().latency_measurements == 0,
          "simplified settings do not start region probes");
    Activate(Required(root, "settings/advanced"));
    Until([root] { return root->getView("settings/option/button-mapping") == nullptr; }, "advanced options collapse again");
    Check(nxnow::LoadStreamSettings() == edited, "collapsing advanced options retains all saved values");
    auto external = edited;
    nxnow::settings::ApplyVideoPreset(external, nxnow::StreamPresets().front());
    Check(nxnow::SaveStreamSettings(external), "fixture applies a preset outside the retained Settings view");
    Required(root, "settings")->willAppear(false); Pump();
    Check(Text(Action(root, "Streaming profile")).find(nxnow::Tr(external.label)) != std::string::npos,
          "retained Settings reloads the externally selected quality profile on return");
    Activate(Required(root, "settings/advanced"));
    Until([root] { return root->getView("settings/option/stream-stats-overlay") != nullptr; }, "advanced options reopen");
    Activate(Action(root, "Stream stats overlay"));
    Check(nxnow::LoadStreamSettings().preset_id == external.preset_id,
          "changing another setting retains the profile chosen from a game page");
}
void DetailChecks(nxnow::GameDetailView* root)
{
    const auto original_settings = nxnow::LoadStreamSettings();
    auto* summary = Required(root, "detail/next-stream");
    brls::Application::giveFocus(Required(root, "detail/play")); Pump(); Key(brls::BUTTON_NAV_RIGHT);
    Check(brls::Application::getCurrentFocus() == summary, "controller Right reaches the next-stream profile button");
    Activate(summary); Key(brls::BUTTON_B);
    Check(nxnow::LoadStreamSettings() == original_settings, "canceling the profile selector retains saved settings");
    for (size_t target = 0; target < nxnow::StreamPresets().size(); ++target)
    {
        const auto before = nxnow::LoadStreamSettings();
        int current = 0;
        for (size_t i = 0; i < nxnow::StreamPresets().size(); ++i)
            if (nxnow::StreamPresets()[i].preset_id == before.preset_id) current = static_cast<int>(i);
        Activate(summary);
        Check(dynamic_cast<brls::Dropdown*>(CurrentRoot()) != nullptr, "next-stream button opens the native quality selector");
        for (const auto& preset : nxnow::StreamPresets())
            Check(Text(CurrentRoot()).find(nxnow::Tr(preset.label)) != std::string::npos, "quality selector offers existing preset " + preset.preset_id);
        const int difference = static_cast<int>(target) - current;
        for (int step = 0; step < std::abs(difference); ++step)
            Key(difference > 0 ? brls::BUTTON_NAV_DOWN : brls::BUTTON_NAV_UP);
        Key(brls::BUTTON_A);
        auto expected = before;
        nxnow::settings::ApplyVideoPreset(expected, nxnow::StreamPresets()[target]);
        Check(nxnow::LoadStreamSettings() == expected, "detail preset persists video settings while retaining other preferences");
        Check(Text(summary).find(nxnow::ui::StreamSummary(expected)) != std::string::npos, "next-stream summary immediately reflects selection");
    }
    Check(nxnow::SaveStreamSettings(original_settings), "restore detail fixture settings after profile selection");
    dynamic_cast<nxnow::ui::NextStreamSummaryView*>(summary)->Update(original_settings);
    CheckVisibleActionRows(root, "detail/actions");
    auto* description = dynamic_cast<brls::ScrollingFrame*>(Required(root, "detail/description"));
    Check(description && description->getChildren().front()->getHeight() > description->getHeight(), "actual production description overflows bounded scroll viewport");
    brls::Application::giveFocus(Required(root, "detail/play")); Pump(); Key(brls::BUTTON_NAV_UP);
    Check(brls::Application::getCurrentFocus() == description, "detail Up routes from Play to real description scroller");
    SDL_Event scroll {};
    scroll.type = SDL_KEYDOWN; scroll.key.keysym.scancode = SDL_SCANCODE_DOWN; scroll.key.keysym.sym = SDLK_DOWN;
    SDL_PushEvent(&scroll); Pump(16); scroll.type = SDL_KEYUP; SDL_PushEvent(&scroll); Pump();
    Check(description->getContentOffsetY() > 0, "held native SDL Down scrolls actual overflow description");
    Activate(Action(root, "Play on GeForce NOW"));
    Check(ui_fixture::Snapshot().launches.size() == 1, "actual detail Play callback reaches launch boundary once");
    const auto call = ui_fixture::Snapshot().launches.front();
    Check(call.app_id == "1000" && call.store == "Steam" && call.user_id == "native-fixture-user" && call.title == ui_fixture::Library().front().title, "actual Play passes selected launch ID, Steam, current account identity and internal title");
    Until([] { return !ui_fixture::Snapshot().handed_off_sessions.empty(); }, "real queue worker reaches recording stream handoff");
    Check(ui_fixture::Snapshot().played_ids == std::vector<std::string>{"fixture-1000"}, "real queue worker records selected history identity");
    Check(ui_fixture::Snapshot().start_settings == ui_fixture::Snapshot().handoff_settings, "actual Play forwards complete request settings to stream handoff unchanged");
    Check(root->getView("detail/store") == nullptr, "game details do not offer a Store action");
    Activate(Action(root, "Create Switch shortcut"));
    Until([] { return !ui_fixture::Snapshot().shortcuts.empty(); }, "actual shortcut worker reaches external shortcut boundary");
    const auto request = ui_fixture::Snapshot().shortcuts.front();
    Check(request.launch_app_id == "1000" && request.store == "Steam" && request.game_id == "fixture-1000", "actual shortcut request preserves launch/store/game IDs");
    if (CurrentRoot() != root) Key(brls::BUTTON_B);
    auto data = nxnow::MakeLibraryGameDetail(ui_fixture::Library().front());
    data.game_id = "fixture-multi";
    data.variants.push_back({"2000", "Epic", "OWNED", {}, "AVAILABLE", true});
    ui_fixture::ClearLauncherPreference();
    auto* multi = new nxnow::GameDetailView(nxnow::GfnClient(), data);
    brls::Application::pushActivity(new brls::Activity(multi)); Pump();
    const size_t starts = ui_fixture::Snapshot().launches.size();
    Activate(Required(multi, "detail/play"));
    Check(dynamic_cast<brls::Dropdown*>(CurrentRoot()) != nullptr, "multi-variant Play opens actual native launcher selector");
    Key(brls::BUTTON_NAV_DOWN); Key(brls::BUTTON_A);
    Until([starts] { return ui_fixture::Snapshot().launches.size() > starts; }, "native selector dismissal launches selected variant");
    Check(ui_fixture::Snapshot().launches.back().app_id == "2000" && ui_fixture::Snapshot().launches.back().store == "Epic", "actual selector launches Epic variant, not default Steam variant");
    Check(ui_fixture::Snapshot().saved_variants.back() == "2000", "actual selector persists selected launcher preference");
    Until([starts] { return ui_fixture::Snapshot().handed_off_sessions.size() > starts; }, "selected variant completes real queue handoff");
    Key(brls::BUTTON_B);
    ui_fixture::ClearLauncherPreference();
    auto* guarded = new nxnow::GameDetailView(nxnow::GfnClient(), data);
    brls::Application::pushActivity(new brls::Activity(guarded)); Pump();
    Activate(Required(guarded, "detail/play")); Key(brls::BUTTON_NAV_DOWN);
    const size_t guarded_starts = ui_fixture::Snapshot().launches.size();
    const auto auth = *nxnow::AppState::Instance().session();
    brls::Application::onControllerButtonPressed(brls::BUTTON_A, false);
    nxnow::AppState::Instance().ActivateSession(auth); Pump(48);
    Check(ui_fixture::Snapshot().launches.size() == guarded_starts, "account change between selection and deferred dismiss prevents stale-account launch");
    Key(brls::BUTTON_B);
    description->setContentOffsetY(0, false); Pump();
}

void QueueChecks(const nxnow::AuthSession& session, const std::string& capture)
{
    nxnow::QueueDisplayState presentation {"Queue presentation fixture", "Checking your NVIDIA account", "Waiting for authorization", 123, 0, false, "fixture://cover/1000"};
    int cancels = 0;
    auto* presentation_view = new nxnow::QueueView(presentation, nxnow::LoadStreamSettings(), [&cancels] { ++cancels; });
    brls::Application::pushActivity(new brls::Activity(presentation_view)); Pump();
    auto* cancel = Required(presentation_view, "queue/cancel");
    Check(brls::Application::getCurrentFocus() != cancel, "launch cancellation is not initially selected");
    const auto cancel_caption = Text(cancel);
    Check(cancel_caption.starts_with(nxnow::Tr("Cancel launch")) && cancel_caption.find("\nB\n") == std::string::npos,
          "cancel button has an explicit title without a B suffix");
    Key(brls::BUTTON_A);
    Check(cancels == 0, "confirming on the initial launch screen does not cancel");
    Key(brls::BUTTON_NAV_DOWN);
    Check(brls::Application::getCurrentFocus() == cancel, "controller direction selects the cancel button");
    Key(brls::BUTTON_A);
    Check(cancels == 1, "selected cancel button cancels with A");
    Key(brls::BUTTON_NAV_UP); Key(brls::BUTTON_B);
    Check(cancels == 2, "B retains its direct cancellation shortcut");
    for (int stage : {0, 1, 2, 3, 4})
    {
        presentation.stage = stage;
        presentation.status = stage == 0 ? "Checking your NVIDIA account" : stage == 1 ? "Waiting in queue..." : stage == 2 ? "Preparing your cloud rig" : "Starting the video stream";
        presentation_view->Update(presentation); Pump();
        const auto expected = stage == 1 ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
        Check(Required(presentation_view, "queue/position")->getParent()->getVisibility() == expected,
            "queue number cards only occupy space during the actual queue phase");
        Check(Required(presentation_view, "queue/position-heading")->getParent()->getVisibility() == expected,
            "position-in-queue heading is only visible with the actual number cards");
        CheckChildren(presentation_view, "queue-presentation-" + std::to_string(stage));
        Screenshot(capture + ".phase-" + std::to_string(stage) + ".ppm");
    }
    presentation.stage = 1; presentation.position = -1;
    presentation_view->Update(presentation); Pump();
    Check(Text(Required(presentation_view, "queue/position")) == "—\n" &&
          Text(Required(presentation_view, "queue/position-heading")).find(nxnow::Tr("Waiting for your queue position")) != std::string::npos,
        "unknown queue position is explicit and never fabricated");
    presentation.position = 123; presentation.failed = true;
    presentation_view->Update(presentation); Pump();
    Check(Required(presentation_view, "queue/position")->getParent()->getVisibility() == brls::Visibility::GONE,
        "failed session hides stale queue number cards");
    brls::Application::popActivity(brls::TransitionAnimation::NONE); Pump();
    ui_fixture::SetSessionMode(ui_fixture::SessionMode::Waiting);
    nxnow::LaunchSessionDialog(nxnow::GfnClient(), session, "1000", "Native queue fixture", "Steam", "Native queue fixture", "fixture-1000", "fixture://cover/1000");
    Until([] { return nxnow::GetCurrentQueuePosition() == 123; }, "real queue worker publishes actual three-digit position");
    Check(Text(Required(CurrentRoot(), "queue/position")).find("123") != std::string::npos, "native queue displays all three position digits");
    const auto covers = ui_fixture::Snapshot().cover_requests;
    auto* hero = dynamic_cast<nxnow::CachedImage*>(Required(CurrentRoot(), "queue/cover"));
    Check(hero && hero->getScalingType() == brls::ImageScalingType::FIT && !hero->getClipsToBounds(),
          "launch artwork fits its full image without filling or cropping the frame");
    Check(std::any_of(covers.begin(), covers.end(), [hero](const auto& request) {
        return request.image == hero && request.url == "fixture://cover/1000";
    }), "the launch hero requests the selected game's real artwork URL");
    CheckChildren(dynamic_cast<brls::Box*>(CurrentRoot()), "queue"); CheckFonts(CurrentRoot());
    CheckVisibleActionRows(CurrentRoot(), "queue/actions"); Screenshot(capture + ".queue.ppm");
    Check(!CurrentRoot()->getView("queue/minimize"), "launch offers cancellation without a minimize action");
    brls::Application::popActivity(brls::TransitionAnimation::NONE); Pump();
    Check(nxnow::IsQueueMinimized(), "unexpected queue activity removal preserves background session recovery");
    const auto original = nxnow::LoadStreamSettings();
    auto changed = original;
    changed.bitrate_kbps = 25000; changed.queue_notify_threshold = 5; changed.region = "https://changed-region.fixture.invalid/";
    Check(nxnow::SaveStreamSettings(changed), "fixture saves new settings while actual queue is minimized");
    const size_t starts_before_second = ui_fixture::Snapshot().launches.size();
    nxnow::LaunchSessionDialog(nxnow::GfnClient(), session, "9999", "Blocked second launch", "Steam"); Pump();
    Check(ui_fixture::Snapshot().launches.size() == starts_before_second, "second launch is blocked while actual queue exists");
    Check(Required(CurrentRoot(), "queue/cover"), "restored queue retains the large game cover");
    brls::Application::popActivity(brls::TransitionAnimation::NONE); Pump();
    const int polls = ui_fixture::Snapshot().polls;
    Until([polls] { return ui_fixture::Snapshot().polls >= polls + 2; }, "real minimized queue keeps polling without another worker", 12);
    const auto notifications = ui_fixture::Snapshot().notifications;
    const auto reminder = nxnow::Tr("Queue almost ready. Tap the Queue chip to return.");
    Check(std::count(notifications.begin(), notifications.end(), reminder) == 1, "actual minimized queue reminds once using original threshold despite changed saved settings");
    nxnow::RestoreMinimizedQueueDialog(); Pump();
    Check(!nxnow::IsQueueMinimized() && CurrentRoot()->getView("queue/position"), "actual restore reattaches native queue activity");
    Activate(Required(CurrentRoot(), "queue/cancel"));
    Until([] { return !ui_fixture::Snapshot().stopped_sessions.empty(); }, "actual queue cancellation stops adopted remote session");
    Check(nxnow::GetCurrentQueuePosition() == -1 && !nxnow::IsQueueMinimized(), "cancel clears active queue ownership");
    ui_fixture::SetSessionMode(ui_fixture::SessionMode::BlockStart);
    const size_t before = ui_fixture::Snapshot().launches.size(), stops = ui_fixture::Snapshot().stopped_sessions.size();
    nxnow::LaunchSessionDialog(nxnow::GfnClient(), session, "1001", "Cancel-before-adopt fixture", "Epic");
    Until([before] { return ui_fixture::Snapshot().launches.size() > before; }, "actual start worker reaches controlled external gate");
    Activate(Required(CurrentRoot(), "queue/cancel")); ui_fixture::ReleaseStart();
    Until([stops] { return ui_fixture::Snapshot().stopped_sessions.size() > stops; }, "session returned after cancel is stopped instead of adopted");
    Check(ui_fixture::Snapshot().handed_off_sessions.empty(), "canceled queue never hands off a stream");
    nxnow::SaveStreamSettings(original);
    for (const auto mode : {ui_fixture::SessionMode::Unknown, ui_fixture::SessionMode::Patching, ui_fixture::SessionMode::Confirmation, ui_fixture::SessionMode::Failure})
    {
        ui_fixture::SetSessionMode(mode);
        nxnow::LaunchSessionDialog(nxnow::GfnClient(), session, "1000", "Actual phase fixture", "Steam"); Pump(48);
        if (mode == ui_fixture::SessionMode::Unknown)
            Check(Text(Required(CurrentRoot(), "queue/position")).find("—") != std::string::npos, "real queue with unavailable position stays unknown");
        else if (mode == ui_fixture::SessionMode::Patching)
            Check(Text(Required(CurrentRoot(), "queue/status")).find(nxnow::Tr("Updating the game")) != std::string::npos, "actual patching state takes priority over positive queue position");
        else if (mode == ui_fixture::SessionMode::Confirmation)
            Check(Text(CurrentRoot()).find(nxnow::Tr("Waiting for NVIDIA confirmation")) != std::string::npos, "actual confirmation state takes priority over positive queue position");
        else { CheckChildren(dynamic_cast<brls::Box*>(CurrentRoot()), "queue-failure"); Check(nxnow::GetCurrentQueuePosition() == -1, "actual failed queue relinquishes running ownership"); }
        Check(Required(CurrentRoot(), "queue/position")->getParent()->getVisibility() ==
              (mode == ui_fixture::SessionMode::Unknown ? brls::Visibility::VISIBLE : brls::Visibility::GONE),
            "unknown is explained; patching, confirmation and failure hide stale queue positions");
        Activate(Required(CurrentRoot(), "queue/cancel"));
    }
    ui_fixture::SetSessionMode(ui_fixture::SessionMode::BlockStart);
    const size_t epoch_starts = ui_fixture::Snapshot().launches.size(), epoch_stops = ui_fixture::Snapshot().stopped_sessions.size();
    nxnow::LaunchSessionDialog(nxnow::GfnClient(), session, "1002", "Account guard fixture", "Steam");
    Until([epoch_starts] { return ui_fixture::Snapshot().launches.size() > epoch_starts; }, "account-guarded worker reaches controlled start boundary");
    auto replacement = session; replacement.user.user_id = "replacement-native-fixture-user";
    nxnow::AppState::Instance().ActivateSession(replacement); ui_fixture::ReleaseStart();
    Until([epoch_stops] { return ui_fixture::Snapshot().stopped_sessions.size() > epoch_stops; }, "actual account-generation guard cancels and cleans up prior account session");
    Check(ui_fixture::Snapshot().handed_off_sessions.empty(), "prior-account queue never hands off a stream");
    nxnow::AppState::Instance().ActivateSession(session); nxnow::SaveStreamSettings(original);
    ui_fixture::SetSessionMode(ui_fixture::SessionMode::BlockStart);
    const size_t frozen_starts = ui_fixture::Snapshot().launches.size(), frozen_handoffs = ui_fixture::Snapshot().handed_off_sessions.size();
    nxnow::LaunchSessionDialog(nxnow::GfnClient(), session, "1003", "Frozen settings handoff fixture", "BNET", "Frozen settings handoff fixture", "fixture-1003", "fixture://cover/1003");
    Until([frozen_starts] { return ui_fixture::Snapshot().launches.size() > frozen_starts; }, "gated actual request captures explicit stream settings");
    brls::Application::popActivity(brls::TransitionAnimation::NONE); Pump();
    Check(nxnow::SaveStreamSettings(changed), "saved settings change while real start request is gated and minimized");
    nxnow::RestoreMinimizedQueueDialog(); Pump();
    Check(Required(CurrentRoot(), "queue/cover"), "restored gated queue displays its game artwork");
    Screenshot(capture + ".restored.ppm"); ui_fixture::ReleaseStart();
    Until([frozen_handoffs] { return ui_fixture::Snapshot().handed_off_sessions.size() > frozen_handoffs; }, "gated real worker completes actual stream handoff");
    const auto resolved = nxnow::settings::ResolveLaunchSettings(original);
    Check(ui_fixture::Snapshot().start_settings.back() == resolved && ui_fixture::Snapshot().handoff_settings.back() == resolved, "StartSession and PresentCloudStream receive the same resolved settings after saved-settings change");
    Check(nxnow::LoadStreamSettings() == changed, "handoff does not roll back independently saved next-stream settings");
    Check(ui_fixture::Snapshot().handoff_images.back() == "fixture://cover/1003",
          "the launched game artwork is carried through the actual handoff into video startup");
    Check(ui_fixture::Snapshot().handoff_stores.back() == "BNET",
          "the selected launch store reaches video startup for Battle.net mouse mode");
    ui_fixture::SetSessionMode(ui_fixture::SessionMode::BlockStart);
    const size_t covered_starts = ui_fixture::Snapshot().launches.size(), covered_handoffs = ui_fixture::Snapshot().handed_off_sessions.size();
    const size_t original_stack = brls::Application::getActivitiesStack().size();
    nxnow::LaunchSessionDialog(nxnow::GfnClient(), session, "1004", "Covered queue handoff fixture", "Steam");
    Until([covered_starts] { return ui_fixture::Snapshot().launches.size() > covered_starts; }, "covered queue worker reaches external gate");
    auto* dialog = new brls::Dialog("Native dialog covering the production queue");
    dialog->addButton("Close", [] {}); dialog->setCancelable(true); dialog->open(); Pump(); ui_fixture::ReleaseStart();
    Until([covered_handoffs] { return ui_fixture::Snapshot().handed_off_sessions.size() > covered_handoffs; }, "covered real queue hands off exactly when ready");
    Check(CurrentRoot() == dialog && brls::Application::getActivitiesStack().size() == original_stack + 2, "covered queue readiness does not pop unrelated top dialog");
    Check(ui_fixture::Snapshot().handed_off_sessions.size() == covered_handoffs + 1, "covered queue invokes actual handoff callback exactly once");
    Key(brls::BUTTON_B);
    Until([original_stack] { return brls::Application::getActivitiesStack().size() == original_stack; }, "closing cover dialog removes stale queue on resume");
}
void OverlayChecks()
{
    nxnow::StreamOverlaySample sample;
    const auto unknown = nxnow::FormatStreamOverlay(sample);
    Check(unknown.metrics[2] == nxnow::Tr("Unknown") && unknown.metrics[3] == nxnow::Tr("Unknown"), "real overlay formatter keeps unavailable RTT and zero-packet loss unknown");
    sample.game_title = "A long live game title Жї 玩家"; sample.provider = "GeForce NOW";
    sample.presented_fps = 60; sample.incoming_fps = 60; sample.decoded_fps = 59; sample.bitrate_mbps = 15.8;
    sample.rtt_ms = 18; sample.width = 1280; sample.height = 720; sample.decode_us_p95 = 6100; sample.render_us_p95 = 2400;
    sample.queue_size = 1; sample.queue_high_water = 4; sample.packets_received = 9998; sample.sequence_gaps = 2;
    sample.codec = "H.264"; sample.location = "Auto"; sample.network = "Wi-Fi 5 GHz"; sample.peer_connected = true;
    auto* view = new nxnow::StreamOverlayView(); view->Update(sample); view->UpdateElapsed(2538);
    std::printf("OVERLAY loss=%s decode=%s render=%s queue=%s elapsed=%s\n", view->Display().metrics[3].c_str(), view->Display().details[2].c_str(), view->Display().details[3].c_str(), view->Display().details[4].c_str(), view->Display().elapsed.c_str());
    Check(view->Display().metrics[3] == "0.02", "actual overlay loss estimate uses received plus sequence gaps denominator");
    Check(view->Display().details[2] == "6.1 ms" && view->Display().details[3] == "2.4 ms", "actual overlay preserves measured p95 fractions");
    Check(view->Display().details[4] == "1 / 4" && view->Display().elapsed == "42:18", "actual overlay formats queue high water and elapsed time");
    view->setDimensions(1280, 720); brls::Application::pushActivity(new brls::Activity(view)); Pump();
}
}

int main(int argc, char** argv)
{
    try
    {
        const std::string language = argc > 1 ? argv[1] : "en", account = argc > 2 ? argv[2] : "Player";
        const std::string capture = argc > 3 ? argv[3] : "native.ppm", scenario = argc > 5 ? argv[5] : "library";
        capture_path = capture;
        const int physical_width = argc > 4 ? std::stoi(argv[4]) : 1280;
        const bool guest = account == "guest", empty = scenario == "empty" || scenario == "empty-store";
        ui_fixture::Reset(capture + ".storage", empty);
        nxnow::StreamSettings initial; initial.interface_language = language;
        Check(nxnow::SaveStreamSettings(initial), "real settings fixture initializes isolated scratch persistence");
        nxnow::SetInterfaceLanguage(language); brls::Logger::setLogLevel(brls::LogLevel::LOG_ERROR);
        if (!brls::Application::init()) return 2;
        brls::Application::createWindow("NXNow production native UI tests"); nxnow::ui::InitializeThemeAndFonts();
        brls::Application::setGlobalQuit(false);
        if (physical_width == 1920)
        {
            SDL_SetWindowSize(SDL_GL_GetCurrentWindow(), 1920, 1080);
            int width, height; SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height);
            glViewport(0, 0, width, height); brls::Application::setWindowSize(1920, 1080);
        }
        nxnow::AuthSession session;
        session.user.user_id = "native-fixture-user"; session.user.display_name = account;
        session.user.membership_tier = "Ultimate"; session.user.membership_tier_verified = true;
        session.subscription.available = true; session.subscription.remaining_hours = 100.5;
        session.subscription.has_storage = true; session.subscription.storage_size_gb = 1000;
        auto& state = nxnow::AppState::Instance();
        if (!guest) state.SetSession(session); else state.MarkSessionLoaded();
        state.SetLibraryGames(ui_fixture::Library());
        auto* frame = new nxnow::TopBarFrame();
        frame->addTab("Library", [] { return new nxnow::LibraryTab(); });
        frame->addTab("Settings", [] { return new nxnow::SettingsTab(); });
        frame->focusTab(scenario == "settings" ? nxnow::TopBarFrame::kSettingsTab : nxnow::TopBarFrame::kLibraryTab);
        brls::Application::pushActivity(new brls::Activity(frame)); Pump();
        int width, height, viewport[4] {}; SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &width, &height); glGetIntegerv(GL_VIEWPORT, viewport);
        Check(width == physical_width && height == physical_width * 9 / 16, "native drawable reaches requested resolution");
        Check(viewport[2] == width && viewport[3] == height, "OpenGL viewport covers drawable");
        Check(brls::Application::windowWidth == static_cast<unsigned>(width) && brls::Application::windowHeight == static_cast<unsigned>(height), "physical dimensions match drawable");
        Check(brls::Application::contentWidth == 1280 && brls::Application::contentHeight == 720, "layout remains logical 1280x720");
        CheckChildren(frame, "production");
        const auto header = Required(frame, "shell-header")->getFrame(), footer = Required(frame, "shell-footer")->getFrame();
        Check(header.getHeight() == 100 && footer.getHeight() == 48 && footer.getMinY() == 672, "production shell retains floating toolbar and single compact footer at y672");
        Walk(Required(frame, "shell-tabs"), [](brls::View* view) {
            if (auto* label = dynamic_cast<brls::Label*>(view)) Check(label->isSingleLine() && label->getHeight() <= 20, "production tab label stays on one line");
        });
        CheckFonts(frame);
        CheckVisibleActionRows(frame, "initial-actions");
        nxnow::StreamSettings fractional; fractional.bitrate_kbps = 12345;
        Check(nxnow::ui::StreamSummary(fractional).find("12.345") != std::string::npos, "configured stream summary preserves 12345 kbps");
        fractional.bitrate_kbps = 12500;
        Check(nxnow::ui::StreamSummary(fractional).find("12.5") != std::string::npos, "configured stream summary preserves 12500 kbps");
        fractional.bitrate_kbps = 12000;
        Check(nxnow::ui::StreamSummary(fractional).find("12 Mbps") != std::string::npos, "configured stream summary preserves integer 12000 kbps");
        if (scenario == "store" || scenario == "empty-store")
        {
            // Retain legacy catalog component coverage outside the application menu.
            auto* catalog = new nxnow::CatalogTab();
            brls::Application::pushActivity(new brls::Activity(catalog)); Pump();
            StoreChecks(catalog, empty, guest);
        }
        else if (scenario == "settings") SettingsChecks(frame, capture);
        else if (scenario == "queue") { DrainToasts(); QueueChecks(session, capture); }
        else if (scenario == "overlay") OverlayChecks();
        else if (scenario == "detail")
        {
            auto data = nxnow::MakeLibraryGameDetail(ui_fixture::Library().front());
            auto* detail = new nxnow::GameDetailView(nxnow::GfnClient(), std::move(data));
            brls::Application::pushActivity(new brls::Activity(detail)); Pump();
            CheckChildren(detail, "detail"); CheckFonts(detail); DetailChecks(detail);
        }
        else LibraryChecks(frame, guest, empty);
        DrainToasts(); Screenshot(capture);
        Check(dynamic_cast<brls::Dialog*>(CurrentRoot()) == nullptr && dynamic_cast<brls::Dropdown*>(CurrentRoot()) == nullptr, "production callbacks leave no unexpected modal activity");
        brls::Application::quit(); while (brls::Application::mainLoop()) {}
    }
    catch (const std::exception& error)
    {
        Check(false, std::string("native harness exception: ") + error.what());
        if (argc > 3 && CurrentRoot()) Screenshot(std::string(argv[3]) + ".failure.ppm");
    }
    std::printf("RESULT failures=%d\n", failures);
    return failures ? 1 : 0;
}
