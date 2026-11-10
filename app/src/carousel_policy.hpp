#pragma once
#include <algorithm>
#include <cstddef>
#include <utility>

namespace nxnow::library
{
// Each Library page supplies at most ten items to the carousel.
inline constexpr std::size_t kCarouselTextureLimit = 10;
inline std::size_t CarouselPageCount(std::size_t count)
{ return count / kCarouselTextureLimit + (count % kCarouselTextureLimit != 0); }
inline std::pair<std::size_t, std::size_t> CarouselPageRange(std::size_t page, std::size_t count)
{
    if (!count) return {0, 0};
    page = std::min(page, CarouselPageCount(count) - 1);
    const auto first = page * kCarouselTextureLimit;
    return {first, first + std::min(kCarouselTextureLimit, count - first)};
}
inline std::size_t CarouselTarget(std::size_t selected, int delta, std::size_t count)
{
    if (!count) return 0;
    const auto next = static_cast<long long>(selected) + delta;
    return static_cast<std::size_t>(std::clamp(next, 0LL, static_cast<long long>(count - 1)));
}
inline std::pair<std::size_t, std::size_t> CarouselWindow(std::size_t selected, std::size_t count)
{
    if (!count) return {0, 0};
    selected = std::min(selected, count - 1);
    return CarouselPageRange(selected / kCarouselTextureLimit, count);
}
}
