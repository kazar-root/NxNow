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
