#include "artwork_image_policy.hpp"
#include <borealis/extern/nanovg/stb_image.h>
#include <limits>

namespace nxnow::ui
{
bool SafeThumbnailData(const std::string& encoded)
{
    if (encoded.empty() || encoded.size() > kThumbnailMaxEncodedBytes) return false;
    int width = 0, height = 0, channels = 0;
    return stbi_info_from_memory(reinterpret_cast<const unsigned char*>(encoded.data()),
        static_cast<int>(encoded.size()), &width, &height, &channels) != 0 &&
        SafeThumbnailDimensions(width, height);
}
bool SafeArtworkData(const std::string& encoded)
{
    if (encoded.empty() || encoded.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        return false;
    int width = 0, height = 0, channels = 0;
    return stbi_info_from_memory(reinterpret_cast<const unsigned char*>(encoded.data()),
        static_cast<int>(encoded.size()), &width, &height, &channels) != 0 &&
        SafeArtworkDimensions(width, height);
}
}
