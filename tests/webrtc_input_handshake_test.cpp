#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define private public
#include "webrtc_session.hpp"
#undef private
#include "webrtc/internal.hpp"
#include "stream_diagnostics.hpp"

#include <cassert>

struct SentMessage {
    uint16_t sid;
    std::vector<uint8_t> bytes;
};

static std::vector<SentMessage> sent_messages;
static int send_result = 0;
static char channel_label[] = "input_channel_v1";
static int open_attempts = 0;
static std::vector<std::string> input_logs;
static PeerConnectionState peer_state = PEER_CONNECTION_COMPLETED;
static bool diagnostics_enabled = false;
static std::vector<std::string> stream_logs;

extern "C" {
int peer_init() { return 0; }
void peer_deinit() {}
PeerConnectionState peer_connection_get_state(PeerConnection*) {
    return peer_state;
}
const char* peer_connection_state_to_string(PeerConnectionState) { return "completed"; }
char* peer_connection_lookup_sid_label(PeerConnection*, uint16_t) { return channel_label; }
int peer_connection_datachannel_send_binary_sid(PeerConnection*, char* data, size_t len, uint16_t sid) {
    sent_messages.push_back({sid, std::vector<uint8_t>(data, data + len)});
    return send_result < 0 ? send_result : static_cast<int>(len);
}
int peer_connection_create_datachannel_sid(PeerConnection*, DecpChannelType type, uint16_t,
                                           uint32_t, char* label, char*, uint16_t sid) {
    assert(type == DATA_CHANNEL_RELIABLE && sid == 0);
    assert(std::string(label) == "input_channel_v1");
    ++open_attempts;
    return send_result;
}
}

namespace nxnow {
bool StreamDiagnosticsEnabled() { return diagnostics_enabled; }
bool SensitiveInputLoggingSuppressed() { return false; }
}

namespace nxnow::webrtc::internal {
void AppendInputLog(const std::string& line) { input_logs.push_back(line); }
void AppendStreamLog(const std::string& line) { stream_logs.push_back(line); }
std::string HexPreview(const uint8_t*, size_t, size_t) { return {}; }
std::string PreviewText(const std::string&, size_t) { return {}; }
uint64_t NowUs() { return 0x0102030405060708ULL; }
}

WebRtcSession::WebRtcSession(const std::string&, const std::string&, const std::string&,
                             const std::string&, int, const std::vector<nxnow::IceServerInfo>&,
                             const nxnow::StreamSettings&) {}
WebRtcSession::~WebRtcSession() = default;
SignalingClient::~SignalingClient() = default;
WebSocketClient::~WebSocketClient() = default;
FFmpegVideoDecoder::~FFmpegVideoDecoder() = default;
int FFmpegVideoDecoder::setup(int, int, int, int, void*, int) { return 0; }
void FFmpegVideoDecoder::cleanup() {}
int FFmpegVideoDecoder::submit_decode_unit(uint8_t*, int, int64_t) { return 0; }
int FFmpegVideoDecoder::capabilities() const { return 0; }
VideoDecodeStats* FFmpegVideoDecoder::video_decode_stats() { return nullptr; }
struct AudioPipeline::Impl {};
AudioPipeline::~AudioPipeline() = default;

static void RunScenario(const std::string& scenario) {
    sent_messages.clear();
    send_result = 0;
    open_attempts = 0;
    input_logs.clear();
    stream_logs.clear();
    diagnostics_enabled = false;
    peer_state = PEER_CONNECTION_COMPLETED;
    channel_label[0] = 'i';
    WebRtcSession session("", "", "", "", 0, {}, {});
    session.pc_ = reinterpret_cast<PeerConnection*>(&session);
    session.on_datachannel_open();
    session.datachannel_open_requested_ = true;
    session.reliable_input_channel_requested_ = true;
    assert(nxnow::webrtc::internal::InputEncodingSelfTest());
    if (scenario == "echo") {
        const uint8_t handshake[] = {0x0e, 0x02, 0x03, 0x00};
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 0);
        assert(!sent_messages.empty());
        assert(sent_messages[0].sid == 0);
        assert(sent_messages[0].bytes == std::vector<uint8_t>(std::begin(handshake), std::end(handshake)));
        assert(session.input_ready_ && session.input_protocol_version_ == 3);
        assert(session.send_gamepad_input(0, 0x0101, 0x1000, 0, 0, 0, 0, 0, 0));
        assert(sent_messages.back().bytes.size() == 50);
        assert(sent_messages.back().bytes[0] == 0x23);
        assert(sent_messages.back().bytes[9] == 0x21);
        assert(session.ready_for_input());
        assert(session.send_mouse_left_button(true));
        assert(sent_messages.back().bytes.size() == 28);
        assert(sent_messages.back().bytes[9] == 0x22 && sent_messages.back().bytes[10] == 8);
        session.send_mouse_move(10, -5);
        assert(sent_messages.back().bytes.size() == 34);
        assert(sent_messages.back().bytes[9] == 0x21 && sent_messages.back().bytes[12] == 7);
        session.send_keyboard_key(0x41, 0x1e, 0, true);
        assert(sent_messages.back().bytes.size() == 28);
        assert(sent_messages.back().bytes[9] == 0x22 && sent_messages.back().bytes[10] == 3);
    } else if (scenario == "physical-input") {
        for (uint16_t version : {2, 3}) {
            session.input_ready_ = true;
            session.input_protocol_version_ = version;
            const size_t base = version == 3 ? 10 : 0;
            for (uint8_t button : {1, 2, 3, 4, 5}) {
                assert(session.send_mouse_button(button, true));
                assert(sent_messages.back().bytes.size() == base + 18);
                assert(sent_messages.back().bytes[base] == 8 && sent_messages.back().bytes[base + 4] == button);
                assert(session.send_mouse_button(button, false));
                assert(sent_messages.back().bytes[base] == 9 && sent_messages.back().bytes[base + 4] == button);
            }
            const auto before_invalid = sent_messages.size();
            assert(!session.send_mouse_button(0, true) && !session.send_mouse_button(6, true));
            assert(sent_messages.size() == before_invalid);
            assert(session.send_mouse_wheel(120));
            auto wheel = sent_messages.back().bytes;
            assert(wheel.size() == base + 22 && wheel[base] == 10 && wheel[base + 6] == 0 && wheel[base + 7] == 120);
            assert(session.send_mouse_wheel(-120));
            wheel = sent_messages.back().bytes;
            assert(wheel[base + 6] == 0xff && wheel[base + 7] == 0x88);
            diagnostics_enabled = true;
            input_logs.clear(); stream_logs.clear();
            assert(session.send_keyboard_key(0x1b, 1, 0, true));
            auto key = sent_messages.back().bytes;
            assert(key.size() == base + 18 && key[base] == 3 && key[base + 5] == 0x1b && key[base + 9] == 1);
            assert(session.send_keyboard_key(0x1b, 1, 0, false));
            assert(sent_messages.back().bytes[base] == 4);
            assert(input_logs.empty() && stream_logs.empty());
            send_result = -1;
            assert(!session.send_keyboard_key('W', 0, 0, true));
            assert(!session.send_mouse_button(3, true));
            assert(!session.send_mouse_wheel(120));
            send_result = 0;
        }
        session.input_ready_ = false;
        const auto before_blocked = sent_messages.size();
        assert(!session.send_keyboard_key('W', 0, 0, true));
        assert(!session.send_mouse_wheel(120));
        assert(sent_messages.size() == before_blocked);
    } else if (scenario == "mouse-delivery") {
        assert(!session.ready_for_input());
        assert(!session.send_mouse_left_button(true) && sent_messages.empty());
        const uint8_t handshake[] = {0x0e, 0x02, 0x03, 0x00};
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 0);
        assert(session.ready_for_input());
        assert(session.send_mouse_left_button(true));
        const auto down = sent_messages.back().bytes;
        send_result = -1;
        assert(!session.send_mouse_left_button(false));
        send_result = 0;
        assert(session.send_mouse_left_button(false));
        const auto up = sent_messages.back().bytes;
        assert(down.size() == 28 && up.size() == 28 && down[10] == 8 && up[10] == 9);
        const auto before_disconnect = sent_messages.size();
        peer_state = PEER_CONNECTION_DISCONNECTED;
        assert(!session.ready_for_input());
        assert(!session.send_mouse_left_button(true));
        assert(sent_messages.size() == before_disconnect);
        peer_state = PEER_CONNECTION_COMPLETED;
        session.on_datachannel_close();
        assert(!session.ready_for_input() && !session.send_mouse_left_button(false));
    } else if (scenario == "post-handshake") {
        const uint8_t handshake[] = {0x0e, 0x02, 0x03, 0x00};
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 0);
        assert(session.input_ready_ && session.input_protocol_version_ == 3);
        const auto messages_after_handshake = sent_messages.size();
        const uint8_t later_message[] = {0x0e, 0x02, 0x02, 0x00};
        session.on_datachannel_message(reinterpret_cast<const char*>(later_message), sizeof(later_message), 0);
        assert(session.input_ready_ && session.input_protocol_version_ == 3);
        assert(sent_messages.size() == messages_after_handshake);
        const uint8_t legacy_message[] = {0x0e, 0x03, 0x00, 0x00};
        session.on_datachannel_message(reinterpret_cast<const char*>(legacy_message), sizeof(legacy_message), 0);
        assert(session.input_ready_ && session.input_protocol_version_ == 3);
        assert(sent_messages.size() == messages_after_handshake);
        session.on_datachannel_close();
        session.on_datachannel_open();
        session.on_datachannel_message(reinterpret_cast<const char*>(later_message), sizeof(later_message), 0);
        assert(session.input_ready_ && session.input_protocol_version_ == 2);
        assert(sent_messages[messages_after_handshake].bytes ==
               std::vector<uint8_t>(std::begin(later_message), std::end(later_message)));
    } else if (scenario == "retry") {
        const uint8_t handshake[] = {0x0e, 0x02, 0x03, 0x00};
        send_result = -1;
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 0);
        assert(!session.input_ready_);
        assert(!session.send_gamepad_input(0, 0x0101, 0, 0, 0, 0, 0, 0, 0));
        session.input_activation_due_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        session.maybe_activate_input();
        assert(!session.input_ready_);
        send_result = 0;
        session.input_activation_due_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        session.maybe_activate_input();
        assert(session.input_ready_ && session.input_protocol_version_ == 3);
        assert(sent_messages.back().bytes == std::vector<uint8_t>(std::begin(handshake), std::end(handshake)));
    } else if (scenario == "recovery") {
        session.input_ready_ = true;
        session.input_protocol_version_ = 3;
        session.input_handshake_acknowledged_ = true;
        session.note_input_send_result(-1, "gamepad");
        session.note_input_send_result(-1, "gamepad");
        assert(session.input_ready_);
        session.note_input_send_result(-1, "gamepad");
        assert(!session.input_ready_);
        session.maybe_activate_input();
        assert(!session.input_ready_);
        session.input_activation_due_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        session.maybe_activate_input();
        assert(session.input_ready_);
        assert(session.input_protocol_version_ == 3);
        assert(input_logs.back().find("HANDSHAKE reactivate protocol=3") == 0);
        assert(session.send_gamepad_input(0, 0x0101, 0, 0, 0, 0, 0, 0, 0));
        assert(sent_messages.back().bytes.size() == 50);
    } else if (scenario == "reset") {
        session.datachannel_open_attempts_ = 60;
        session.reset_input_channel_for_retry("sctp_close");
        assert(session.datachannel_open_attempts_ == 0);
        assert(!session.input_ready_ && !session.datachannel_open_requested_);
        assert(!session.datachannel_opened_);
        assert(session.input_protocol_version_ == 2);
        session.on_datachannel_open();
        session.maybe_open_datachannel();
        assert(open_attempts == 1 && session.datachannel_open_requested_);
    } else if (scenario == "budget") {
        session.datachannel_open_requested_ = false;
        session.reliable_input_channel_requested_ = false;
        send_result = -1;
        for (int attempt = 0; attempt < 65; ++attempt) {
            session.last_datachannel_open_attempt_ = {};
            session.maybe_open_datachannel();
        }
        assert(open_attempts == 60);
        session.on_datachannel_close();
        session.on_datachannel_open();
        send_result = 0;
        session.maybe_open_datachannel();
        assert(open_attempts == 61 && session.datachannel_open_requested_);
    } else if (scenario == "association-wait") {
        session.reset_input_channel_for_retry("sctp_close");
        send_result = -1;
        for (int attempt = 0; attempt < 65; ++attempt) {
            session.last_datachannel_open_attempt_ = {};
            session.maybe_open_datachannel();
        }
        assert(open_attempts == 0 && session.datachannel_open_attempts_ == 0);
        session.on_datachannel_open();
        send_result = 0;
        session.maybe_open_datachannel();
        assert(open_attempts == 1 && session.datachannel_open_requested_);
    } else if (scenario == "bounds") {
        std::vector<uint8_t> oversized(65, 0);
        oversized[0] = 0x0e;
        oversized[1] = 0x02;
        oversized[2] = 0x03;
        session.on_datachannel_message(reinterpret_cast<const char*>(oversized.data()), oversized.size(), 0);
        assert(!session.input_ready_ && session.pending_input_handshake_.empty());
        assert(sent_messages.empty());
        const uint8_t handshake[] = {0x0e, 0x02, 0x03, 0x00};
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 2);
        assert(!session.input_ready_ && sent_messages.empty());
        channel_label[0] = 'x';
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 0);
        assert(!session.input_ready_ && sent_messages.empty());
    } else if (scenario == "pending-reset") {
        const uint8_t handshake[] = {0x0e, 0x02, 0x03, 0x00};
        send_result = -1;
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 0);
        assert(!session.pending_input_handshake_.empty());
        session.on_datachannel_close();
        assert(session.pending_input_handshake_.empty());
        assert(session.input_protocol_version_ == 2 && !session.input_ready_);
        assert(!session.input_handshake_acknowledged_);
    } else if (scenario == "fallback") {
        session.input_activation_due_ = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        session.maybe_activate_input();
        assert(session.input_ready_ && session.input_protocol_version_ == 2);
        assert(input_logs.back().find("HANDSHAKE fallback protocol=2") == 0);
        assert(session.send_gamepad_input(0, 0x0101, 0, 0, 0, 0, 0, 0, 0));
        assert(sent_messages.back().bytes.size() == 38 && sent_messages.back().bytes[0] == 12);
    } else if (scenario == "legacy") {
        const uint8_t handshake[] = {0x0e, 0x03, 0x00, 0x00};
        session.on_datachannel_message(reinterpret_cast<const char*>(handshake), sizeof(handshake), 0);
        assert(session.input_ready_ && session.input_protocol_version_ == 0x030e);
        assert(sent_messages[0].bytes == std::vector<uint8_t>(std::begin(handshake), std::end(handshake)));
    } else {
        assert(false);
    }
}

int main(int argc, char** argv) {
    if (argc == 2) {
        RunScenario(argv[1]);
        return 0;
    }
    assert(argc == 1);
    for (const auto* scenario : {"echo", "post-handshake", "retry", "recovery", "reset", "budget",
                                 "association-wait", "bounds", "pending-reset", "fallback", "legacy", "mouse-delivery", "physical-input"}) {
        RunScenario(scenario);
        std::printf("PASS webrtc_input_handshake/%s\n", scenario);
    }
}
