#include "artwork_image_policy.hpp"
#include <cassert>
#include <limits>

int main()
{
    using nxnow::ui::SafeThumbnailDimensions;
    assert(SafeThumbnailDimensions(208, 208));
    assert(SafeThumbnailDimensions(208, 312));
    assert(SafeThumbnailDimensions(292, 136));
    assert(!SafeThumbnailDimensions(600, 900));
    assert(!SafeThumbnailDimensions(1920, 1080));
    assert(!SafeThumbnailDimensions(200, 600));
    assert(!SafeThumbnailDimensions(320, 480));
    assert(!SafeThumbnailDimensions(0, 114));
    using nxnow::ui::SafeArtworkDimensions;
    assert(SafeArtworkDimensions(400, 600));
    assert(SafeArtworkDimensions(1280, 720));
    assert(SafeArtworkDimensions(1920, 1080));
    assert(SafeArtworkDimensions(2048, 1024));
    assert(!SafeArtworkDimensions(2048, 2048));
    assert(!SafeArtworkDimensions(4096, 1080));
    assert(!SafeArtworkDimensions(0, 600));
    assert(!SafeArtworkDimensions(-1, 600));
    assert(!SafeArtworkDimensions(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
}
