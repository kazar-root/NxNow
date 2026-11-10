#include "ui_image_lifecycle.hpp"
#include "cover_image_cache.hpp"

#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)
#include <borealis/platforms/switch/switch_video.hpp>
#endif

namespace nxnow::ui
{
void WaitForImageGpu()
{
#if defined(__SWITCH__) && defined(BOREALIS_USE_DEKO3D)
    auto* context = dynamic_cast<brls::SwitchVideoContext*>(
        brls::Application::getPlatform()->getVideoContext());
    if (context)
        context->getQueue().waitIdle();
#endif
}

void RetireArtwork(std::unique_ptr<CachedImage> image)
{
    if (!image)
        return;
    image->CancelPending();
    // NanoVG records image IDs until nvgEndFrame. Deletion inside draw() would
    // invalidate that batch. The sync queue runs after endFrame; the GPU must
    // also finish the submitted batch before the texture memory is returned.
    brls::sync([retired = std::shared_ptr<CachedImage>(std::move(image))]() mutable {
        WaitForImageGpu();
        retired.reset();
    });
}
}
