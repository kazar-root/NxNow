#include "../app/src/http_client.cpp"

#include <cassert>

int main()
{
    nxnow::ResponseBody body {{}, 4};
    char bytes[] = "12345";
    assert(nxnow::WriteBody(bytes, 1, 4, &body) == 4);
    assert(body.data == "1234");
    assert(nxnow::WriteBody(bytes, 1, 1, &body) == 0);
    assert(body.data == "1234");
    assert(nxnow::WriteBody(bytes, std::numeric_limits<size_t>::max(), 2, &body) == 0);
    assert(body.data == "1234");
    nxnow::ResponseBody unlimited {{}, 0};
    assert(nxnow::WriteBody(bytes, 1, 5, &unlimited) == 5);

    std::atomic_bool cancelled = false;
    assert(nxnow::CheckCancelled(&cancelled, 0, 0, 0, 0) == 0);
    cancelled.store(true);
    assert(nxnow::CheckCancelled(&cancelled, 0, 0, 0, 0) == 1);
    bool rejected = false;
    try
    {
        nxnow::HttpClient().Get("https://example.test", "test", {}, {},
            {.cancelled = &cancelled, .max_body_bytes = 4});
    }
    catch (const std::runtime_error& error)
    {
        rejected = true;
        assert(std::string(error.what()) == "HTTP request cancelled");
    }
    assert(rejected);
}
