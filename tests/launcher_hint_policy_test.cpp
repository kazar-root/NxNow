#include "launcher_hint_policy.hpp"
#include <cassert>
using namespace nxnow::input;
int main()
{
    using namespace std::chrono;
    LauncherHintWindow hint; const auto now = LauncherHintWindow::Clock::time_point{} + seconds(100);
    assert(!hint.Visible(now)); hint.FirstFrame(now);
    assert(hint.Visible(now + milliseconds(4999))); assert(!hint.Visible(now + seconds(5)));
    hint.FirstFrame(now + seconds(10)); assert(!hint.Visible(now + seconds(10)));
    hint.ModeChanged(now + seconds(20)); assert(hint.Visible(now + milliseconds(21999)));
    assert(!hint.Visible(now + seconds(22)));
    hint.ModeChanged(now + seconds(30)); assert(!hint.Visible(now + seconds(32)));
}
