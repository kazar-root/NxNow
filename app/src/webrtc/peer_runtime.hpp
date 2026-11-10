#pragma once

#include "peer.h"

#include <stdexcept>

namespace nxnow::webrtc
{

class PeerRuntime
{
  public:
    PeerRuntime()
    {
        if (peer_init() != 0)
            throw std::runtime_error("Could not initialize the streaming security runtime");
    }

    ~PeerRuntime()
    {
        peer_deinit();
    }

    PeerRuntime(const PeerRuntime&) = delete;
    PeerRuntime& operator=(const PeerRuntime&) = delete;
};

}
