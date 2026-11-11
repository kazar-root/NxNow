#pragma once

#include <borealis.hpp>

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>

namespace nxnow
{

struct CoverImageCacheStats
{
    std::uint64_t bytes = 0;
    std::size_t files = 0;
};

// Synchronously returns cached remote image bytes, downloading once when
// necessary. Call from a worker thread.
std::string LoadCachedImageData(const std::string& image_url,
    const std::atomic_bool* cancelled = nullptr);
CoverImageCacheStats InspectCoverImageCache();
std::size_t ClearCoverImageCache();
void ShutdownCoverImageWorker();

class CachedImage : public brls::Image
{
  public:
    ~CachedImage() override;
    void SetUrl(const std::string& image_url, bool fallback);
    // Small catalogue cards use CDN thumbnails and bounded transfers/textures.
    void SetThumbnail(const std::string& image_url);
    // Artwork never substitutes the product logo for a missing game image.
    void SetArtwork(const std::string& image_url);
    bool IsReady() const { return ready_; }
    bool HasFinished() const { return request_finished_; }
    void Poll();
    void CancelPending();
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

  private:
    bool full_cover_ = false;
    bool placeholder_ = true;
    bool thumbnail_ = false;
    bool ready_ = false;
    bool request_finished_ = true;
    void BeginRequest(const std::string& image_url, bool fallback, bool placeholder, bool thumbnail = false);
    void ResetImage();
    void TrySubmit();
    void SetThumbnailTexture(const std::string& image_url, const std::string& data);
    std::shared_ptr<std::atomic_bool> cancelled_;
    std::string pending_url_;
    std::uint64_t requested_generation_ = 0;
    int shared_thumbnail_ = 0;
    std::chrono::steady_clock::time_point next_submission_ {};
};

void SetCachedCoverImage(CachedImage* image, const std::string& image_url);
void SetCachedAvatarImage(CachedImage* image, const std::string& image_url);

} // namespace nxnow
