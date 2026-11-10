#include "cover_image_cache.hpp"

#include "app_paths.hpp"
#include "atomic_file_replace.hpp"
#include "gfn_client.hpp"
#include "http_client.hpp"
#include "cover_image_worker.hpp"
#include "ui_image_lifecycle.hpp"
#include "artwork_image_policy.hpp"
#include "catalog_image_policy.hpp"

#include <borealis/core/cache_helper.hpp>

#include <sys/stat.h>

#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <optional>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace nxnow
{
namespace
{

constexpr const char* kFallbackCoverRes = "icon/icon.jpg";
std::mutex cache_mutex;
std::atomic<std::uint64_t> cache_generation {0};
std::unique_ptr<CoverImageWorker> image_worker;
constexpr std::size_t kMaxImageBytes = 8 * 1024 * 1024;

std::string LoadImageData(const std::string& image_url, const std::atomic_bool* cancelled,
    std::optional<std::uint64_t> requested_generation, std::size_t max_bytes = kMaxImageBytes);

std::string ImageCachePath()
{
    return AppHomePath() + "/cache/images";
}

void EnsureImageCacheDirectory()
{
#ifdef __SWITCH__
    mkdir("sdmc:/switch", 0777);
#endif
    const std::string app_home = AppHomePath();
    const std::string cache_path = app_home + "/cache";
    mkdir(app_home.c_str(), 0777);
    mkdir(cache_path.c_str(), 0777);
    mkdir(ImageCachePath().c_str(), 0777);
}

std::string HashUrl(const std::string& url)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : url)
    {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }

    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

std::string CachePathForUrl(const std::string& url)
{
    return ImageCachePath() + "/" + HashUrl(url) + ".img";
}

bool ReadCachedImage(const std::string& path, std::string& data, std::size_t max_bytes)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
        return false;

    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(max_bytes))
        return false;
    stream.seekg(0);

    data.assign(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
    return !data.empty();
}

void WriteCachedImage(const std::string& path, const std::string& data)
{
    EnsureImageCacheDirectory();

    const std::string temporary_path = path + ".tmp";
    std::ofstream stream(temporary_path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
        return;

    stream.write(data.data(), static_cast<std::streamsize>(data.size()));
    stream.close();
    if (!stream)
    {
        std::remove(temporary_path.c_str());
        return;
    }
    storage::ReplaceWithTemporaryFile(temporary_path, path);
}

} // namespace

CachedImage::~CachedImage()
{
    CancelPending();
#ifdef __SWITCH__
    if (getTexture())
        ui::WaitForImageGpu();
#endif
}

void CachedImage::CancelPending()
{
    if (cancelled_)
        cancelled_->store(true);
    pending_url_.clear();
}

void CachedImage::SetUrl(const std::string& image_url, bool fallback)
{
    BeginRequest(image_url, fallback, true);
}

void CachedImage::SetArtwork(const std::string& image_url)
{
    BeginRequest(image_url, false, false);
}

void CachedImage::SetThumbnail(const std::string& image_url)
{
    BeginRequest(catalog::ThumbnailUrl(image_url), true, true, true);
}

void CachedImage::BeginRequest(const std::string& image_url, bool fallback, bool placeholder, bool thumbnail)
{
    thumbnail_ = thumbnail;
    ready_ = false;
    request_finished_ = image_url.empty();
    placeholder_ = placeholder;
    full_cover_ = fallback;
    if (full_cover_)
    {
        setScalingType(brls::ImageScalingType::FIT);
        setImageAlign(brls::ImageAlignment::CENTER);
        setClipsToBounds(false);
    }
    if (cancelled_)
        cancelled_->store(true);
    pending_url_ = image_url;
    ResetImage();
    if (placeholder_ && (fallback || !image_url.empty()))
        setImageFromRes(kFallbackCoverRes);
    if (image_url.empty())
        return;

    if (!image_worker)
        image_worker = std::make_unique<CoverImageWorker>(2);
    cancelled_ = std::make_shared<std::atomic_bool>(false);
    // UI selection must not wait behind disk reads or cache maintenance.
    requested_generation_ = cache_generation.load();
    TrySubmit();
}

void CachedImage::ResetImage()
{
#ifdef __SWITCH__
    if (getTexture())
        ui::WaitForImageGpu();
#endif
    if (!getFreeTexture() && getTexture() != 0)
        brls::TextureCache::instance().removeCache(getTexture());
    clear();
    setFreeTexture(true);
}

void CachedImage::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx)
{
    Poll();
    if (!full_cover_)
    {
        brls::Image::draw(vg, x, y, width, height, style, ctx);
        return;
    }
    const float original_width = getOriginalImageWidth();
    const float original_height = getOriginalImageHeight();
    if (!getTexture() || width <= 0 || height <= 0 || original_width <= 0 || original_height <= 0 ||
        !std::isfinite(original_width) || !std::isfinite(original_height))
        return;
    const float scale = std::min(width / original_width, height / original_height);
    const float image_width = original_width * scale;
    const float image_height = original_height * scale;
    const float left = x + (width - image_width) * 0.5f;
    const float top = y + (height - image_height) * 0.5f;
    nvgSave(vg);
    nvgBeginPath(vg);
    nvgRect(vg, left, top, image_width, image_height);
    nvgFillPaint(vg, a(nvgImagePattern(vg, left, top, image_width, image_height, 0, getTexture(), 1)));
    nvgFill(vg);
    nvgRestore(vg);
}

void CachedImage::Poll()
{
    if (!pending_url_.empty() && std::chrono::steady_clock::now() >= next_submission_)
        TrySubmit();
}

void CachedImage::TrySubmit()
{
    next_submission_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    const auto cancelled = cancelled_;
    const auto generation = requested_generation_;
    const bool accepted = image_worker->TrySubmit(cancelled,
        [this, cancelled, image_url = pending_url_, generation, artwork = !placeholder_, thumbnail = thumbnail_] {
            std::string data;
            try
            {
                data = LoadImageData(image_url, cancelled.get(), generation,
                    thumbnail ? ui::kThumbnailMaxEncodedBytes : kMaxImageBytes);
            }
            catch (...)
            {
            }
            if (cancelled->load())
                return;
            if ((thumbnail && !ui::SafeThumbnailData(data)) || (artwork && !ui::SafeArtworkData(data)))
                data.clear();
            brls::sync([this, cancelled, data = std::move(data)] {
                if (cancelled->load())
                    return;
                request_finished_ = true;
                if (data.empty())
                    return;
                ResetImage();
                setImageFromMem(reinterpret_cast<const unsigned char*>(data.data()),
                    static_cast<int>(data.size()));
                ready_ = getTexture() != 0;
                if (!ready_ && placeholder_)
                    setImageFromRes(kFallbackCoverRes);
            });
        });
    if (accepted)
        pending_url_.clear();
}

void ShutdownCoverImageWorker()
{
    if (image_worker)
        image_worker->Stop();
}

std::string LoadCachedImageData(const std::string& image_url, const std::atomic_bool* cancelled)
{
    return LoadImageData(image_url, cancelled, std::nullopt);
}

namespace
{
std::string LoadImageData(const std::string& image_url, const std::atomic_bool* cancelled,
    std::optional<std::uint64_t> requested_generation, std::size_t max_bytes)
{
    if (image_url.empty() || (cancelled && cancelled->load()))
        return {};

    const std::string cache_path = CachePathForUrl(image_url);
    std::string cached;
    std::uint64_t generation;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        generation = cache_generation;
        if (requested_generation && *requested_generation != generation)
            return {};
        if (ReadCachedImage(cache_path, cached, max_bytes))
            return cached;
    }

    HttpClient http_client;
    const HttpResponse response = http_client.Get(
        image_url,
        GfnClient::kUserAgent,
        {"Accept: image/jpeg,image/png,image/*,*/*;q=0.8"}, {},
        {.cancelled = cancelled, .max_body_bytes = max_bytes});
    if (response.status_code != 200 || response.body.empty() || response.body.size() > max_bytes)
        return {};

    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (generation == cache_generation && !(cancelled && cancelled->load()))
            WriteCachedImage(cache_path, response.body);
    }
    return response.body;
}
}

CoverImageCacheStats InspectCoverImageCache()
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    CoverImageCacheStats stats;
    const std::string cache_path = ImageCachePath();
    DIR* dir = opendir(cache_path.c_str());
    if (!dir)
        return stats;

    while (dirent* entry = readdir(dir))
    {
        const std::string name = entry->d_name;
        if (name.empty() || name == "." || name == "..")
            continue;

        const std::string path = cache_path + "/" + name;
        struct stat st {};
        if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        ++stats.files;
        stats.bytes += static_cast<std::uint64_t>(st.st_size);
    }

    closedir(dir);
    return stats;
}

std::size_t ClearCoverImageCache()
{
    std::lock_guard<std::mutex> lock(cache_mutex);
    ++cache_generation;
    std::size_t removed = 0;
    const std::string cache_path = ImageCachePath();
    DIR* dir = opendir(cache_path.c_str());
    if (!dir)
        return removed;

    while (dirent* entry = readdir(dir))
    {
        const std::string name = entry->d_name;
        if (name.empty() || name == "." || name == "..")
            continue;

        const std::string path = cache_path + "/" + name;
        struct stat st {};
        if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        if (std::remove(path.c_str()) == 0)
            ++removed;
    }

    closedir(dir);
    return removed;
}

void SetCachedCoverImage(CachedImage* image, const std::string& image_url)
{
    if (image)
        image->SetUrl(image_url, true);
}

void SetCachedAvatarImage(CachedImage* image, const std::string& image_url)
{
    if (image)
        image->SetUrl(image_url, false);
}

} // namespace nxnow
