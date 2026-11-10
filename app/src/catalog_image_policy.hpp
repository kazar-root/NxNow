#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace nxnow::catalog
{
inline constexpr int kThumbnailWidth = 256;
inline constexpr int kPortraitWidth = 224;
inline constexpr int kBackdropWidth = 640;
inline constexpr int kCatalogThumbnailWidth = 208;
inline std::string ImageHost(const std::string& url)
{
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return {};
    const auto start = scheme + 3;
    auto host = url.substr(start, url.find_first_of("/?#", start) - start);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return host;
}
inline std::string ImageUrl(const std::string& url, int width)
{
    if (ImageHost(url) != "img.nvidiagrid.net") return url;
    const auto suffix = url.find_first_of("?#");
    auto image = url.substr(0, suffix);
    // Replace an existing CDN size instead of stacking conflicting widths.
    image.resize(image.find(';') == std::string::npos ? image.size() : image.find(';'));
    return image + ";w=" + std::to_string(width) +
        (suffix == std::string::npos ? "" : url.substr(suffix));
}
inline std::string ThumbnailUrl(const std::string& url)
{
    const auto host = ImageHost(url);
    if (host == "img.nvidiagrid.net")
    {
        auto image = ImageUrl(url, kCatalogThumbnailWidth);
        const auto suffix = image.find_first_of("?#");
        image.insert(suffix == std::string::npos ? image.size() : suffix, ";q=65");
        return image;
    }
    if (host == "steamstatic.com" || host.ends_with(".steamstatic.com") ||
        host == "steamcdn-a.akamaihd.net")
    {
        const auto marker = url.find("/steam/apps/");
        const auto suffix = url.find_first_of("?#");
        if (marker == std::string::npos || (suffix != std::string::npos && marker > suffix)) return url;
        const auto start = marker + std::string("/steam/apps/").size();
        const auto end = url.find('/', start);
        const auto id = url.substr(start, end - start);
        if (end == std::string::npos || id.empty() ||
            !std::all_of(id.begin(), id.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; }))
            return url;
        return "https://cdn.cloudflare.steamstatic.com/steam/apps/" + id + "/header_292x136.jpg";
    }
    return url;
}
}
