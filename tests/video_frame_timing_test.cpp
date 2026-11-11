#include "../app/src/stream/ffmpeg/VideoFrameTiming.hpp"

#include <cassert>

int main()
{
    using namespace nxnow::video;

    assert(DecideFrame(90000, false, 0, true, 90000) == FrameDecision::Present);
    assert(DecideFrame(91500, false, 0, true, 90000) == FrameDecision::HoldPrevious);
    assert(DecideFrame(90000, true, 90600, true, 90000) == FrameDecision::DropSuperseded);
    assert(DecideFrame(90000, true, 91500, true, 90000) == FrameDecision::Present);
    assert(DecideFrame(91500, false, 0, false, 90000) == FrameDecision::Present);

    // Normal tolerance stays near 10% of the source period.
    assert(AdaptiveEarlyToleranceTicks(0) == 180);
    assert(AdaptiveEarlyToleranceTicks(600) == 90);
    assert(AdaptiveEarlyToleranceTicks(1500) == 150);
    assert(AdaptiveEarlyToleranceTicks(3000) == 300);
    assert(AdaptiveEarlyToleranceTicks(6000) == 300);

    // Recovery tolerance after a repeated tick is bounded to 300..600 ticks.
    assert(RecoveryEarlyToleranceTicks(0) == 500);
    assert(RecoveryEarlyToleranceTicks(600) == 300);
    assert(RecoveryEarlyToleranceTicks(1500) == 500);
    assert(RecoveryEarlyToleranceTicks(3000) == 600);

    // RTP wraparound must retain signed timestamp ordering.
    assert(RtpDelta(0x00000100u, 0xfffffff0u) > 0);
    assert(RtpDelta(0xfffffff0u, 0x00000100u) < 0);
    return 0;
}
