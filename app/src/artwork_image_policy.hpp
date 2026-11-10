#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace nxnow::ui
{
inline constexpr std::size_t kThumbnailMaxEncodedBytes = 128 * 1024;
inline constexpr bool SafeThumbnailDimensions(int width, int height)
{
    return width > 0 && height > 0 && width <= 320 && height <= 480 &&
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) <= 320ULL * 320;
}
inline constexpr bool SafeArtworkDimensions(int width, int height)
{
    // At most 8 MiB of decoded RGBA per image; compressed file size alone is
    // not a useful bound for panoramas and other large cloud illustrations.
    return width > 0 && height > 0 && width <= 2048 && height <= 2048 &&
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) <= 2048ULL * 1024;
}
bool SafeArtworkData(const std::string& encoded);
bool SafeThumbnailData(const std::string& encoded);
}
