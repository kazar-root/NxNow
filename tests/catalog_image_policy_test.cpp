#include "catalog_image_policy.hpp"
#include <cassert>

int main()
{
    using namespace nxnow::catalog;
    const std::string base = "https://img.nvidiagrid.net/apps/game/ZZ/KEY_ART.jpg";
    const auto thumbnail = base + ";w=208;q=65";
    assert(ThumbnailUrl(base) == thumbnail);
    assert(ThumbnailUrl(base + ";w=4096;h=2160;q=100") == thumbnail);
    assert(ThumbnailUrl(thumbnail) == thumbnail);
    assert(ThumbnailUrl(base + ";w=256?revision=2#cover") == thumbnail + "?revision=2#cover");
    assert(ImageUrl(base + ";w=1920", 224) == base + ";w=224");
    const std::string steam = "https://cdn.cloudflare.steamstatic.com/steam/apps/730/header_292x136.jpg";
    assert(ThumbnailUrl("https://cdn.cloudflare.steamstatic.com/steam/apps/730/library_600x900.jpg") == steam);
    assert(ThumbnailUrl("https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/730/hash/header.jpg?t=24") == steam);
    assert(ThumbnailUrl(steam) == steam);
    assert(ThumbnailUrl("").empty());
    for (const auto& url : {"fixture://cover/1000", "https://other.example/image.jpg",
         "https://img.nvidiagrid.net.other.example/art.jpg",
         "https://other.example/art?source=img.nvidiagrid.net",
         "https://cdn.cloudflare.steamstatic.com/steam/apps/not-a-number/library.jpg",
         "https://cdn.cloudflare.steamstatic.com/image?path=/steam/apps/730/library.jpg"})
        assert(ThumbnailUrl(url) == url);
}
