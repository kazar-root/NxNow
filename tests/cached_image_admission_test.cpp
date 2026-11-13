#include "cover_image_cache.hpp"
#include "http_client.hpp"
#include "artwork_image_policy.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <future>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
#include <array>
#include <cmath>

namespace
{
std::atomic<int> requests = 0;
std::atomic<int> thumbnail_requests = 0;
std::string last_thumbnail_url;
std::promise<void> first_started;
std::promise<void> first_release;
const auto first_signal = first_release.get_future().share();

void Draw(nxnow::CachedImage& image)
{
    image.draw(nullptr, 0, 0, 100, 100, {}, nullptr);
}
}

namespace nxnow
{
const std::string& AppHomePath()
{
    static const std::string path =
        (std::filesystem::temp_directory_path() /
         ("nxnow-admission-test-" + std::to_string(getpid()))).string();
    return path;
}

namespace ui
{
void WaitForImageGpu() {}
}

HttpResponse HttpClient::Get(
    const std::string& url, const std::string&, const std::vector<std::string>&,
    const std::string&, HttpTransferControl control) const
{
    ++requests;
    if (url.starts_with("https://img.nvidiagrid.net/thumbnail-"))
    {
        assert(control.max_body_bytes == ui::kThumbnailMaxEncodedBytes);
        assert(url.ends_with(";w=208;q=65"));
        ++thumbnail_requests;
        last_thumbnail_url = url;
        std::ifstream file("resources/img/nxnow-logo-mark-small.png", std::ios::binary);
        std::string png((std::istreambuf_iterator<char>(file)), {});
        assert(!png.empty());
        if (url.find("-huge") != std::string::npos)
        {
            png[16] = 0x00; png[17] = 0x00; png[18] = 0x10; png[19] = 0x00;
        }
        if (url.find("-oversized") != std::string::npos)
            png.append(ui::kThumbnailMaxEncodedBytes, 'x');
        return {.status_code = 200, .body = png};
    }
    if (url == "https://example.test/0")
    {
        first_started.set_value();
        first_signal.wait();
    }
    if (url == "https://example.test/failed")
        throw std::runtime_error("transfer failed");
    if (url == "https://example.test/unavailable")
        return {.status_code = 503, .body = {}};
    if (url.starts_with("https://example.test/artwork-"))
    {
        std::ifstream file("resources/img/nxnow-logo-mark-small.png", std::ios::binary);
        std::string png((std::istreambuf_iterator<char>(file)), {});
        assert(png.size() > 24);
        if (url.ends_with("huge"))
        {
            // PNG IHDR: a tiny encoded file can request an enormous texture.
            png[16] = 0x7f; png[17] = png[18] = png[19] = static_cast<char>(0xff);
        }
        return {.status_code = 200, .body = png};
    }
    return {.status_code = 200, .body = "artwork"};
}
}

int main()
{
    using namespace nxnow;
    std::filesystem::remove_all(AppHomePath());
    std::vector<std::unique_ptr<CachedImage>> images;
    for (int index = 0; index < 61; ++index)
    {
        auto image = std::make_unique<CachedImage>();
        image->SetUrl("https://example.test/" + std::to_string(index), true);
        images.push_back(std::move(image));
        if (index == 0)
            first_started.get_future().wait();
    }
    assert(requests >= 1); // A second bounded worker can proceed around the blocked first image.
    first_release.set_value();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    bool complete = false;
    while (!complete && std::chrono::steady_clock::now() < deadline)
    {
        complete = true;
        for (auto& image : images)
        {
            Draw(*image);
            complete = complete && image->getTexture() == 2;
        }
        brls::DrainSync();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(images[1]->getTexture() == 2);
    assert(complete);
    assert(requests == 61);

    for (const auto& url : {"https://example.test/failed", "https://example.test/unavailable"})
    {
        CachedImage failed;
        failed.SetUrl(url, true);
        brls::WaitForSync();
        brls::DrainSync();
        assert(failed.getTexture() == 1);
        assert(failed.resource_path == "icon/icon.jpg");
        assert(std::filesystem::is_regular_file("resources/" + failed.resource_path));
        assert(!failed.getFreeTexture());
        const int completed_requests = requests;
        for (int frame = 0; frame < 35; ++frame)
        {
            Draw(failed);
            brls::DrainSync();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        assert(requests == completed_requests);
        failed.SetUrl("https://example.test/1", true);
        brls::WaitForSync();
        brls::DrainSync();
        assert(failed.getTexture() == 2);
        assert(failed.getFreeTexture());
    }
    // Exercise the actual CachedImage draw path: portrait, square and wide
    // textures must stay fully inside cards, rows, detail and launch frames.
    for (const auto& size : {std::array{600.0f, 900.0f}, std::array{1254.0f, 1254.0f},
                            std::array{1920.0f, 1080.0f}, std::array{2000.0f, 500.0f}})
    {
        images[1]->original_width = size[0];
        images[1]->original_height = size[1];
        for (const auto& box : {std::array{204.0f, 114.0f}, std::array{112.0f, 56.0f},
                              std::array{576.0f, 300.0f}, std::array{684.0f, 350.0f}})
        {
            images[1]->draw(nullptr, 17, 23, box[0], box[1], {}, nullptr);
            const auto& paint = last_image_paint;
            assert(paint.texture == images[1]->getTexture());
            assert(paint.x >= 17 - .001f && paint.y >= 23 - .001f);
            assert(paint.x + paint.width <= 17 + box[0] + .001f);
            assert(paint.y + paint.height <= 23 + box[1] + .001f);
            assert(std::abs(paint.x + paint.width / 2 - (17 + box[0] / 2)) < .001f);
            assert(std::abs(paint.y + paint.height / 2 - (23 + box[1] / 2)) < .001f);
            assert(std::abs(paint.width / paint.height - size[0] / size[1]) < .001f);
        }
    }
    {
        CachedImage artwork;
        artwork.SetArtwork("");
        assert(artwork.getTexture() == 0 && !artwork.IsReady());
        assert(artwork.HasFinished());
        artwork.SetArtwork("https://example.test/artwork-valid");
        assert(artwork.getTexture() == 0 && !artwork.IsReady());
        assert(!artwork.HasFinished());
        brls::WaitForSync(); brls::DrainSync();
        assert(artwork.getTexture() == 2 && artwork.IsReady());
        assert(artwork.HasFinished());
        artwork.SetArtwork("https://example.test/artwork-other");
        brls::WaitForSync();
        artwork.SetArtwork(""); brls::DrainSync();
        assert(artwork.getTexture() == 0 && !artwork.IsReady());
        artwork.SetArtwork("https://example.test/failed");
        brls::WaitForSync(); brls::DrainSync();
        assert(artwork.getTexture() == 0 && !artwork.IsReady());
        assert(artwork.HasFinished());
        const int uploads = brls::image_uploads;
        artwork.SetArtwork("https://example.test/artwork-huge");
        brls::WaitForSync(); brls::DrainSync();
        assert(artwork.getTexture() == 0 && !artwork.IsReady());
        assert(brls::image_uploads == uploads);
    }
    {
        CachedImage thumbnail;
        thumbnail.SetThumbnail("https://img.nvidiagrid.net/thumbnail-valid.jpg;w=4096;h=2160");
        brls::WaitForSync(); brls::DrainSync();
        assert(thumbnail.IsReady() && thumbnail.HasFinished());
        assert(last_thumbnail_url == "https://img.nvidiagrid.net/thumbnail-valid.jpg;w=208;q=65");
        assert(thumbnail_requests == 1);
        const auto uploads_before_reuse = brls::image_uploads;
        thumbnail.SetThumbnail("https://img.nvidiagrid.net/thumbnail-valid.jpg;w=4096;h=2160");
        // The decoded thumbnail is still in memory: it is reused at once,
        // without a new download or texture upload.
        assert(thumbnail.IsReady() && thumbnail_requests == 1);
        assert(brls::image_uploads == uploads_before_reuse);
        const auto uploads = brls::image_uploads;
        thumbnail.SetThumbnail("https://img.nvidiagrid.net/thumbnail-huge.jpg");
        brls::WaitForSync(); brls::DrainSync();
        assert(!thumbnail.IsReady() && thumbnail.HasFinished());
        assert(brls::image_uploads == uploads); // Reject oversized textures before upload.
        const auto cached_files = InspectCoverImageCache().files;
        thumbnail.SetThumbnail("https://img.nvidiagrid.net/thumbnail-oversized.jpg");
        brls::WaitForSync(); brls::DrainSync();
        assert(!thumbnail.IsReady() && thumbnail.HasFinished());
        assert(brls::image_uploads == uploads && InspectCoverImageCache().files == cached_files);
    }
    ShutdownCoverImageWorker();
    images.clear();
    brls::DrainSync();
    std::filesystem::remove_all(AppHomePath());
}
