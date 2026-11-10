#pragma once

#include "stream/DecodeQueuePolicy.hpp"

#include <algorithm>
#include <string_view>

namespace nxnow::video
{

struct StreamModeTuning
{
    unsigned int reorder_hold_ms;
    std::size_t maximum_queued_units;
    int maximum_queue_delay_ms;
};

inline StreamModeTuning ResolveStreamMode(std::string_view mode, int fps)
{
    if (mode == "Reinforced" || mode == "Pushed")
        return {30, MaximumQueuedAccessUnits(fps), MaximumDecodeQueueDelayMs()};

    // Keep a short opportunity for UDP reordering, then abandon late work.
    // Encoded reference frames must still go through the existing IDR recovery
    // path when dropped; never feed a broken reference chain to the decoder.
    return {8, static_cast<std::size_t>(std::clamp((std::max(1, fps) + 29) / 30, 1, 2)), 33};
}

inline const char* StreamModeLabel(std::string_view mode)
{
    return mode == "Reinforced" || mode == "Pushed" ? "Renforcé" : "Optimal";
}

} // namespace nxnow::video
