#include "stream/ffmpeg/AVFrameHolder.hpp"

#include <cassert>

static void push(AVFrameQueue& queue, int64_t pts)
{
    AVFrame* frame = av_frame_alloc();
    assert(frame);
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = 16;
    frame->height = 16;
    assert(av_frame_get_buffer(frame, 0) == 0);
    frame->pts = pts;
    queue.push(frame);
    av_frame_free(&frame);
}

int main()
{
    AVFrameQueue queue;
    bool reused = true;
    uint64_t generation = 99;

    // Startup holds presentation until two pictures are ready.
    assert(queue.pop(reused, generation) == nullptr);
    assert(!reused && generation == 0 && queue.getStartupHoldStat() == 1);
    push(queue, 90000);
    assert(queue.pop(reused, generation) == nullptr);
    assert(queue.getStartupHoldStat() == 2);

    // Without a media clock, pictures are presented in order.
    push(queue, 91500);
    assert(queue.pop(reused, generation)->pts == 90000);
    assert(!reused && generation == 1 && queue.size() == 1);
    assert(queue.pop(reused, generation)->pts == 91500);
    assert(!reused && generation == 2 && queue.size() == 0);

    // An empty queue reuses the last picture, either after a bounded wait or
    // immediately when the next picture is not expected before the deadline.
    assert(queue.pop(reused, generation)->pts == 91500);
    assert(reused && generation == 2);
    assert(queue.getQueueEmptyReuseStat() == 1);
    assert(queue.getBriefWaitTimeoutStat() + queue.getPredictiveWaitSkippedStat() >= 1);
    assert(queue.getBriefWaitMaximumUs() <= 6500 + 2000);
    assert(queue.getCurrentConsecutiveReuse() == 1);

    // A new picture ends the reuse streak once startup has completed.
    push(queue, 93000);
    assert(queue.pop(reused, generation)->pts == 93000);
    assert(!reused && generation == 3 && queue.getCurrentConsecutiveReuse() == 0);

    // Overflow keeps only the newest pictures.
    queue.cleanup();
    for (int i = 0; i < 10; ++i)
        push(queue, AV_NOPTS_VALUE);
    assert(queue.size() == 3 && queue.getOverflowDropStat() == 7);
    assert(queue.pop(reused, generation));
    assert(!reused && generation == 8 && queue.size() == 2);

    // Without a media clock, pictures stay in order through short bursts; a
    // backlog that lasts 6 display ticks loses exactly ONE oldest picture.
    queue.cleanup();
    push(queue, 90000);
    push(queue, 91500);
    assert(queue.pop(reused, generation)->pts == 90000); // startup reserve
    int64_t next_pts = 93000;
    int64_t expected = 91500;
    for (int tick = 1; tick <= 5; ++tick) {
        push(queue, next_pts); next_pts += 1500;
        assert(queue.pop(reused, generation)->pts == expected); // in order
        expected += 1500;
    }
    assert(queue.getCatchUpDropStat() == 0);
    push(queue, next_pts); next_pts += 1500;
    // Tick 6: one picture skipped, the newer of the two is shown.
    assert(queue.pop(reused, generation)->pts == expected + 1500);
    assert(queue.size() == 0 && queue.getCatchUpDropStat() == 1);
    // A burst of three is not dropped at once: still shown in order.
    push(queue, 200000);
    push(queue, 201500);
    push(queue, 203000);
    assert(queue.pop(reused, generation)->pts == 200000);
    assert(queue.getCatchUpDropStat() == 1 && queue.size() == 2);

    // With a media clock, tolerance follows the observed 1500-tick cadence.
    queue.cleanup();
    push(queue, 90000);
    push(queue, 91500);
    push(queue, 93000);
    assert(queue.getObservedProducerRtpTicks() == 1500);
    assert(queue.pop(reused, generation, 90000)->pts == 90000);
    assert(!reused && generation == 1 && queue.size() == 2);
    assert(queue.getPacingEarlyToleranceTicks() == 150);
    assert(queue.pop(reused, generation, 90000)->pts == 90000);
    assert(reused && generation == 1 && queue.size() == 2);
    assert(queue.pop(reused, generation, 93000)->pts == 93000);
    assert(!reused && generation == 3 && queue.size() == 0);
    assert(queue.getPacingEarlyToleranceTicks() == 500);
    assert(queue.getTimingDropStat() == 1);
    assert(queue.getTimingHoldStat() == 1);
    assert(queue.getMaximumConsecutiveReuse() == 1);

    queue.cleanup();
    assert(queue.pop(reused, generation) == nullptr);
    assert(!reused && generation == 0);
    return 0;
}
