#include "carousel_policy.hpp"
#include <cassert>

int main()
{
    using namespace nxnow::library;
    assert(CarouselTarget(0, -1, 0) == 0);
    assert(CarouselWindow(100, 0).second == 0);
    assert(CarouselPageCount(0) == 0 && CarouselPageCount(10) == 1 && CarouselPageCount(21) == 3);
    std::size_t covered = 0;
    for (std::size_t page = 0; page < CarouselPageCount(10003); ++page)
    {
        const auto [first, last] = CarouselPageRange(page, 10003);
        assert(first == covered && last > first && last - first <= 10);
        covered = last;
    }
    assert(covered == 10003);
    for (std::size_t count : {1, 3, 9, 21, 10000})
    {
        std::size_t selected = 0;
        for (int step = 0; step < 12000; ++step)
        {
            selected = CarouselTarget(selected, step % 3 ? 1 : 5, count);
            const auto [first, last] = CarouselWindow(selected, count);
            assert(first <= selected && selected < last && last <= count);
            assert(last - first <= kCarouselTextureLimit);
        }
        assert(selected == count - 1);
        for (int step = 0; step < 12000; ++step) selected = CarouselTarget(selected, -5, count);
        assert(selected == 0);
    }
}
