#pragma once

#include <memory>

namespace nxnow { class CachedImage; }

namespace nxnow::ui
{
// Rendering ownership stays behind this boundary, outside launcher views.
void WaitForImageGpu();
void RetireArtwork(std::unique_ptr<CachedImage> image);
}
