#include "stream_mode_policy.hpp"
#include "stream_settings_policy.hpp"

#include <cassert>
#include <string_view>

int main()
{
    using namespace nxnow::video;
    nxnow::StreamSettings settings;
    assert(settings.controller_layout == "Switch");
    assert(settings.stream_mode == "Optimal");
    const auto optimised = ResolveStreamMode(settings.stream_mode, 60);
    assert(optimised.reorder_hold_ms == 8);
    // A one/two-frame bound cleared the queue on ordinary bursts and froze
    // the picture until the next IDR; keep the shared ~80 ms bound instead.
    assert(optimised.maximum_queued_units == 5);
    assert(optimised.maximum_queue_delay_ms == 83);
    assert(ResolveStreamMode("bad", 60).reorder_hold_ms == 8);
    for (int fps : {0, 1, 30, 60, 120})
    {
        const auto optimal = ResolveStreamMode("Optimal", fps);
        assert(optimal.reorder_hold_ms == 8);
        assert(optimal.maximum_queue_delay_ms == MaximumDecodeQueueDelayMs(fps));
        assert(optimal.maximum_queued_units == MaximumQueuedAccessUnits(fps));
        const auto pushed = ResolveStreamMode("Reinforced", fps);
        assert(pushed.reorder_hold_ms == 30);
        assert(pushed.maximum_queue_delay_ms == MaximumDecodeQueueDelayMs(fps));
        assert(pushed.maximum_queued_units == MaximumQueuedAccessUnits(fps));
    }
    assert(std::string_view(StreamModeLabel("Reinforced")) == "Renforcé");
    settings.stream_mode = "Reinforced";
    nxnow::settings::CycleResolution(settings);
    nxnow::settings::CycleFrameRate(settings);
    nxnow::settings::CycleBitrate(settings);
    assert(settings.stream_mode == "Reinforced");
}
