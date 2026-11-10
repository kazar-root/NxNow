#include <borealis.hpp>
#include <borealis/core/font.hpp>
#include <cassert>
#include <cstring>
#include <map>
#include <set>
#include <thread>

namespace startup_fixture
{
struct Texture { int type, w, h; };
std::map<int, Texture> textures;
std::set<int> sampled_images;
int next_texture = 1;
int visible_draws = 0;
int unsafe_deletions = 0;
brls::Rect qr_background;
brls::Rect QrBackground() { return qr_background; }
int FrameDraws() { return visible_draws; }
int UnsafeDeletions() { return unsafe_deletions; }

int CreateTexture(void*, int type, int w, int h, int, const unsigned char*)
{ const int id = next_texture++; textures.emplace(id, Texture{type, w, h}); return id; }
int DeleteTexture(void*, int id)
{
    if (sampled_images.contains(id) && textures.at(id).type == NVG_TEXTURE_RGBA)
        ++unsafe_deletions;
    textures.erase(id); return 1;
}
int TextureSize(void*, int id, int* w, int* h)
{
    const auto found = textures.find(id);
    if (found == textures.end()) return 0;
    *w = found->second.w; *h = found->second.h; return 1;
}
void Record(NVGpaint* paint)
{
    if (paint->image) { assert(textures.contains(paint->image)); sampled_images.insert(paint->image); }
    if (paint->innerColor.a > 0.01f || paint->outerColor.a > 0.01f) ++visible_draws;
}
class Video final : public VideoContext
{
    NVGcontext* context_;
  public:
    Video()
    {
        NVGparams p {};
        p.renderCreate = [](void*) { return 1; };
        p.renderCreateTexture = CreateTexture;
        p.renderDeleteTexture = DeleteTexture;
        p.renderGetTextureSize = TextureSize;
        p.renderUpdateTexture = [](void*, int, int, int, int, int, const unsigned char*) { return 1; };
        p.renderViewport = [](void*, float, float, float) {};
        p.renderCancel = [](void*) { sampled_images.clear(); };
        p.renderFlush = [](void*) { sampled_images.clear(); };
        p.renderFill = [](void*, NVGpaint* paint, NVGcompositeOperationState, NVGscissor*, float, const float*, const NVGpath* paths, int count) {
            Record(paint);
            if (!paint->image && paint->innerColor.r > .99f && paint->innerColor.g > .99f &&
                paint->innerColor.b > .99f && count == 1 && paths[0].nfill == 4)
            {
                float left = paths[0].fill[0].x, right = left, top = paths[0].fill[0].y, bottom = top;
                for (int i = 1; i < 4; ++i)
                {
                    left = std::min(left, paths[0].fill[i].x); right = std::max(right, paths[0].fill[i].x);
                    top = std::min(top, paths[0].fill[i].y); bottom = std::max(bottom, paths[0].fill[i].y);
                }
                if (right - left > 200 && bottom - top > 200) qr_background = brls::Rect(left, top, right - left, bottom - top);
            }
        };
        p.renderStroke = [](void*, NVGpaint* paint, NVGcompositeOperationState, NVGscissor*, float, float, const NVGpath*, int) { Record(paint); };
        p.renderTriangles = [](void*, NVGpaint* paint, NVGcompositeOperationState, NVGscissor*, const NVGvertex*, int, float) { Record(paint); };
        p.renderDelete = [](void*) {};
        context_ = nvgCreateInternal(&p); assert(context_);
    }
    ~Video() override { nvgDeleteInternal(context_); }
    void clear(NVGcolor) override {}
    void beginFrame() override { visible_draws = 0; }
    void endFrame() override {}
    void resetState() override {}
    double getScaleFactor() override { return 1; }
    NVGcontext* getNVGContext() override { return context_; }
    void setSwapInterval(int) override {}
};
class Input final : public brls::InputManager
{
  public:
    short getControllersConnectedCount() override { return 1; }
    void updateUnifiedControllerState(brls::ControllerState* state) override { std::memset(state, 0, sizeof(*state)); }
    void updateControllerState(brls::ControllerState* state, int) override { updateUnifiedControllerState(state); }
    bool getKeyboardKeyState(brls::BrlsKeyboardScancode) override { return false; }
    void updateTouchStates(std::vector<brls::RawTouchState>* states) override { states->clear(); }
    void updateMouseStates(brls::RawMouseState* state) override { *state = {}; }
    void sendRumble(unsigned short, unsigned short, unsigned short) override {}
    void sendRumble(unsigned short, unsigned short, unsigned short, unsigned short, unsigned short) override {}
};
class Fonts final : public brls::FontLoader
{
  public:
    void loadFonts() override
    {
        using namespace brls;
        assert(Application::loadFontFromFile(FONT_REGULAR, BRLS_ASSET("font/Nunito-Medium.ttf")));
        assert(Application::loadFontFromFile(FONT_SWITCH_ICONS, BRLS_ASSET("font/switch_icons.ttf")));
        assert(Application::loadFontFromFile(FONT_MATERIAL_ICONS, BRLS_ASSET("material/MaterialIcons-Regular.ttf")));
    }
};
class Ime final : public brls::ImeManager
{
  public:
    bool openForText(std::function<void(std::string)>, std::string, std::string, int, std::string, int) override { return false; }
    bool openForNumber(std::function<void(long)>, std::string, std::string, int, std::string, std::string, std::string, int) override { return false; }
};
class Platform final : public brls::Platform
{
    Video video_; Input input_; Fonts fonts_; Ime ime_; brls::NullAudioPlayer audio_;
    brls::ThemeVariant theme_ = brls::ThemeVariant::DARK;
  public:
    void createWindow(std::string, uint32_t w, uint32_t h, float, float) override { brls::Application::setWindowSize(w, h); }
    std::string getName() override { return "Native headless startup test (no GPU)"; }
    int getWirelessLevel() override { return 0; }
    std::string getIpAddress() override { return {}; }
    std::string getDnsServer() override { return {}; }
    bool canShowBatteryLevel() override { return false; }
    int getBatteryLevel() override { return -1; }
    bool isBatteryCharging() override { return false; }
    void disableScreenDimming(bool, const std::string&, const std::string&) override {}
    bool isScreenDimmingDisabled() override { return false; }
    void setBacklightBrightness(float) override {}
    float getBacklightBrightness() override { return 1; }
    bool canSetBacklightBrightness() override { return false; }
    bool mainLoopIteration() override { return true; }
    brls::ThemeVariant getThemeVariant() override { return theme_; }
    void setThemeVariant(brls::ThemeVariant theme) override { theme_ = theme; }
    std::string getLocale() override { return "en-US"; }
    brls::AudioPlayer* getAudioPlayer() override { return &audio_; }
    VideoContext* getVideoContext() override { return &video_; }
    brls::InputManager* getInputManager() override { return &input_; }
    brls::ImeManager* getImeManager() override { return &ime_; }
    brls::FontLoader* getFontLoader() override { return &fonts_; }
    bool isApplicationMode() override { return true; }
    void exitToHomeMode(bool) override {}
    void forceEnableGamePlayRecording() override {}
    void openBrowser(std::string) override {}
    std::string getHomeDirectory(std::string) override { return "/tmp/"; }
};
}
namespace brls { Platform* Platform::createPlatform() { return new startup_fixture::Platform(); } }

// The external-boundary fixture also supports GNU --wrap in the Linux visual
// matrix. Resolve its unused observation hooks for this macOS/native target.
extern "C" float __real_nvgText(NVGcontext* vg, float x, float y, const char* text, const char* end) { return nvgText(vg, x, y, text, end); }
extern "C" float __real_nvgTextWithCursor(NVGcontext* vg, float x, float y, const char* text, const char* end, int cursor) { return nvgTextWithCursor(vg, x, y, text, end, cursor); }
extern "C" void RealNotify(const std::string&) asm("__real__ZN4brls11Application6notifyERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE");
extern "C" void RealNotify(const std::string& text) { brls::Application::notify(text); }
