#pragma once

#include <algorithm>
#include <cstddef>

namespace nxnow::video
{

constexpr std::size_t MaximumDecodeUnitBytes()
{
    return 2 * 1024 * 1024;
}

// Keep roughly 80 ms of complete pictures. The extra frame absorbs short
// retransmission/decode bursts without building a latency-heavy backlog.
// A tighter bound clears the queue on ordinary Wi-Fi bursts and then stalls
// presentation until the next IDR arrives.
constexpr std::size_t MaximumQueuedAccessUnits(int frames_per_second)
{
    const int safe_fps = std::max(1, frames_per_second);
    return static_cast<std::size_t>(std::clamp((safe_fps + 11) / 12, 2, 6));
}

// Age limit matching the unit bound above, so a slow decode is not discarded
// earlier than a burst of the same span.
constexpr int MaximumDecodeQueueDelayMs(int frames_per_second)
{
    const int safe_fps = std::max(1, frames_per_second);
    const int span_ms =
        static_cast<int>(MaximumQueuedAccessUnits(safe_fps)) * 1000 / safe_fps;
    return std::clamp(span_ms, 67, 134);
}

constexpr std::size_t ReadyFrameQueueCapacity()
{
    return 3;
}

constexpr std::size_t StartupReadyFrameDepth()
{
    return 2;
}

// Wait only when the ready queue is empty. This adaptive micro-reserve catches
// a decoder completion racing the UI frame without stalling input/render for a
// large part of the refresh interval. Runtime prediction may shorten or skip it.
constexpr int FrameWaitBudgetMicroseconds(int frames_per_second)
{
    const int safe_fps = std::max(1, frames_per_second);
    return std::clamp((1000000 / safe_fps) * 2 / 5, 2500, 6500);
}

} // namespace nxnow::video
