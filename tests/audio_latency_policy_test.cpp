#include "stream/audio/AudioLatencyPolicy.hpp"

#include <cassert>

static_assert(nxnow::audio::ClampTargetBufferMs(5) == 30);
static_assert(nxnow::audio::ClampTargetBufferMs(140) == 100);
static_assert(nxnow::audio::PrimePacketCount(40) == 4);
static_assert(nxnow::audio::InitialJitterHoldUs(40) == 40000);
static_assert(nxnow::audio::ResyncJitterHoldUs(40) == 30000);
static_assert(nxnow::audio::PacketReorderGraceUs() == 4000);
static_assert(nxnow::audio::EstimatePlayedSamples(480, 100000, 105000, 960, 48000) == 720);
static_assert(nxnow::audio::EstimatePlayedSamples(480, 100000, 120000, 700, 48000) == 700);

int main()
{
    assert(nxnow::audio::InitialJitterHoldUs(30) <
           nxnow::audio::InitialJitterHoldUs(100));
    assert(nxnow::audio::ResyncJitterHoldUs(100) == 50000);
    assert(nxnow::audio::EstimatePlayedSamples(100, 0, 200000, 90, 48000) == 90);
    return 0;
}
