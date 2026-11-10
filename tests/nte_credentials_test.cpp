#include "nte_credentials.hpp"

#include <cassert>

int main()
{
    assert(nxnow::IsNevernessToEverness("NTE: Neverness to Everness"));
    assert(nxnow::IsNevernessToEverness("Neverness to Everness"));
    assert(!nxnow::IsNevernessToEverness("Fortnite"));

    auto credentials = nxnow::ParseNteCredentials(
        "email=player@example.com\r\npassword=example value\r\n");
    assert(credentials.valid());
    assert(credentials.email == "player@example.com");
    assert(credentials.password == "example value");

    const auto positional = nxnow::ParseNteCredentials(
        "player@example.com\nexample-password\n");
    assert(positional.valid());
    assert(nxnow::ParseNteCredentials("email=invalid\npassword=value\n").email.empty());
    assert(nxnow::SerializeNteCredentials(credentials).find("email=player@example.com") == 0);
    return 0;
}
