#pragma once

#include <cstddef>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

struct NVGcontext {};
struct NVGpaint { float x, y, width, height; int texture; };
inline NVGpaint last_image_paint {};
inline void nvgSave(NVGcontext*) {}
inline void nvgRestore(NVGcontext*) {}
inline void nvgBeginPath(NVGcontext*) {}
inline void nvgRect(NVGcontext*, float, float, float, float) {}
inline NVGpaint nvgImagePattern(NVGcontext*, float x, float y, float w, float h, float, int texture, float)
{ return {x, y, w, h, texture}; }
inline void nvgFillPaint(NVGcontext*, NVGpaint paint) { last_image_paint = paint; }
inline void nvgFill(NVGcontext*) {}

// Shared thumbnail textures are created and freed straight through NanoVG.
inline int next_nvg_image = 100;
inline int nvg_images_deleted = 0;
inline int nvgCreateImageMem(NVGcontext*, int, unsigned char*, int);
inline void nvgImageSize(NVGcontext*, int, int* width, int* height)
{
    *width = 320;
    *height = 180;
}
inline void nvgDeleteImage(NVGcontext*, int) { ++nvg_images_deleted; }

namespace brls
{
enum class ImageScalingType { FIT, FILL };
enum class ImageAlignment { CENTER };
struct Style {};
struct FrameContext {};
inline std::mutex sync_mutex;
inline std::condition_variable sync_ready;
inline std::vector<std::function<void()>> sync_tasks;
inline int image_uploads = 0;

class Application
{
  public:
    static NVGcontext* getNVGContext()
    {
        static NVGcontext context;
        return &context;
    }
};

inline void sync(const std::function<void()>& task)
{
    std::lock_guard<std::mutex> lock(sync_mutex);
    sync_tasks.push_back(task);
    sync_ready.notify_one();
}

inline void WaitForSync()
{
    std::unique_lock<std::mutex> lock(sync_mutex);
    sync_ready.wait(lock, [] { return !sync_tasks.empty(); });
}

inline void DrainSync()
{
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(sync_mutex);
        tasks.swap(sync_tasks);
    }
    for (const auto& task : tasks)
        task();
}

class Image
{
  public:
    virtual ~Image() = default;
    void setImageFromRes(const std::string& resource)
    {
        free_texture_ = false;
        texture_ = 1;
        resource_path = resource;
    }
    void setImageFromMem(const unsigned char*, int)
    {
        ++image_uploads;
        texture_ = 2;
    }
    bool getFreeTexture() const { return free_texture_; }
    void setFreeTexture(bool value) { free_texture_ = value; }
    int getTexture() const { return texture_; }
    float getOriginalImageWidth() const { return original_width; }
    float getOriginalImageHeight() const { return original_height; }
    void setScalingType(ImageScalingType type) { scaling_type = type; }
    void setImageAlign(ImageAlignment) {}
    void setClipsToBounds(bool value) { clips = value; }
    NVGpaint a(NVGpaint paint) { return paint; }
    void clear() { texture_ = 0; }
    void innerSetImage(int texture) { texture_ = texture; }
    virtual void draw(NVGcontext*, float, float, float, float, Style, FrameContext*) {}
    std::string resource_path;
    float original_width = 320;
    float original_height = 180;
    bool clips = false;
    ImageScalingType scaling_type = ImageScalingType::FIT;

  private:
    bool free_texture_ = true;
    int texture_ = 0;
};
}

inline int nvgCreateImageMem(NVGcontext*, int, unsigned char*, int)
{
    ++brls::image_uploads;
    return next_nvg_image++;
}
