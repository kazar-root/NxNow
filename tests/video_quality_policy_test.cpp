#include "video_quality_policy.hpp"

#include <cassert>

int main()
{
    const auto original = nxnow::video::ResolveQualityTuning("Original");
    const auto adaptive = nxnow::video::ResolveQualityTuning("Adaptive");
    const auto clarity = nxnow::video::ResolveQualityTuning("Clarity");

    assert(original.denoise_strength == 0.0f);
    assert(adaptive.fec_repair_percent == 8);
    assert(adaptive.sharpen_strength > 0.0f);
    assert(clarity.preserve_reference_deblocking);
    assert(nxnow::video::NextQualityMode("Adaptive") == "Clarity");
    assert(nxnow::video::NextQualityMode("Clarity") == "Original");
    assert(nxnow::video::NextQualityMode("invalid") == "Adaptive");
    return 0;
}
