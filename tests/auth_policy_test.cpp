#include "auth_policy.hpp"

#include <cassert>

static_assert(nxnow::auth::ShouldRefresh(0, 1000, 600000));
static_assert(nxnow::auth::ShouldRefresh(500000, 1000, 600000));
static_assert(!nxnow::auth::ShouldRefresh(700000, 1000, 600000));
static_assert(nxnow::auth::IsExpired(1000, 1000));
static_assert(!nxnow::auth::IsExpired(1001, 1000));
static_assert(nxnow::auth::IsTemporaryHttpStatus(0));
static_assert(nxnow::auth::IsTemporaryHttpStatus(408));
static_assert(nxnow::auth::IsTemporaryHttpStatus(429));
static_assert(nxnow::auth::IsTemporaryHttpStatus(503));
static_assert(!nxnow::auth::IsTemporaryHttpStatus(401));
static_assert(nxnow::auth::RefreshRetryDelayMs(1) == 500);
static_assert(nxnow::auth::RefreshRetryDelayMs(2) == 1500);

int main()
{
    using namespace nxnow::auth;
    assert(ShouldRefresh(61000, 1000, 60001));
    assert(!ShouldRefresh(61001, 1000, 60000));
    return 0;
}
