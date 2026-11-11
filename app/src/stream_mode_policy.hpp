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
    // Both modes share the bounded decode queue. Clearing it forces IDR
    // recovery, so a one/two-frame bound turned every Wi-Fi burst or slow
    // decode into a visible freeze. The modes now differ only in how long UDP
    // reordering may hold a gap before it is treated as loss.
    if (mode == "Reinforced" || mode == "Pushed")
        return {30, MaximumQueuedAccessUnits(fps), MaximumDecodeQueueDelayMs(fps)};

    return {8, MaximumQueuedAccessUnits(fps), MaximumDecodeQueueDelayMs(fps)};
}

inline const char* StreamModeLabel(std::string_view mode)
{
    return mode == "Reinforced" || mode == "Pushed" ? "Renforcé" : "Optimal";
}

} // namespace nxnow::video
