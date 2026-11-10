#include "localization.hpp"
#include "StreamView.hpp"
#include "ui_theme.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

void StreamView::DrawDebugOverlay(NVGcontext* vg, float x, float y, float width) {
    if (!session_)
        return;

    std::string debug_text = session_->get_debug_info();
    nvgFontSize(vg, 24.0f);
    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Body));
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBox(vg, x + 50.0f, y + 50.0f, width - 100.0f, debug_text.c_str(), nullptr);
}

void StreamView::DrawPreparingStream(
    NVGcontext*, float, float, float, float)
{
    const auto health = session_ ? session_->get_transport_health() : StreamTransportHealth {};
    const auto counters = session_ ? session_->get_video_performance() : VideoPerformanceCounters {};
    // Keep the same artwork and progress view until a decoded frame is presented.
    nxnow::QueueDisplayState state;
    state.game_title = game_title_;
    state.stage = 3;
    state.status = "Connecting to your cloud rig...";
    state.detail = nxnow::Tr("Opening the secure signaling channel");
    if (health.signaling_connected)
    {
        state.status = "Negotiating the stream...";
        state.detail = nxnow::Tr("Selecting the fastest network path");
    }
    if (health.peer_completed)
    {
        state.stage = 4;
        state.status = "Starting the video stream";
        state.detail = nxnow::Tr("Waiting for the first video packets");
    }
    if (counters.access_units > 0)
    {
        state.stage = 4;
        state.status = "Decoding the video stream...";
        state.detail = counters.decoded_frames > 0
            ? "The game image will appear automatically" : "Preparing the first clean frame...";
    }
    preparing_view_->Update(state);
}

void StreamView::UpdatePerformanceCounter() {
    const auto now = std::chrono::steady_clock::now();
    if (fps_window_started_.time_since_epoch().count() == 0) {
        fps_window_started_ = now;
        previous_video_counters_ = session_
            ? session_->get_video_performance() : VideoPerformanceCounters {};
        return;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - fps_window_started_);
    if (elapsed >= std::chrono::milliseconds(750)) {
        const VideoPerformanceCounters counters = session_
            ? session_->get_video_performance() : VideoPerformanceCounters {};
        const float scale = elapsed.count() > 0 ? 1000.0f / static_cast<float>(elapsed.count()) : 0.0f;
        const auto delta = [](uint64_t current, uint64_t previous) {
            return current >= previous ? current - previous : uint64_t {0};
        };
        incoming_fps_ = static_cast<float>(
            delta(counters.access_units, previous_video_counters_.access_units)) * scale;
        decoded_fps_ = static_cast<float>(
            delta(counters.decoded_frames, previous_video_counters_.decoded_frames)) * scale;
        presented_fps_ = static_cast<float>(
            delta(counters.presented_frames, previous_video_counters_.presented_frames)) * scale;
        stream_bitrate_mbps_ = static_cast<float>(
            delta(counters.access_unit_bytes, previous_video_counters_.access_unit_bytes)) *
            scale * 8.0f / 1000000.0f;
        network_rtt_ms_ = session_ ? session_->get_network_rtt_ms() : -1;
        network_counters_ =
            session_ ? session_->get_network_counters() : StreamNetworkCounters {};
        previous_video_counters_ = counters;
        fps_window_started_ = now;
    }
}

int StreamView::CurrentBatteryLevel()
{
    const auto now = std::chrono::steady_clock::now();
    if (battery_sampled_at_.time_since_epoch().count() == 0 ||
        now - battery_sampled_at_ >= std::chrono::seconds(1))
    {
        battery_sampled_at_ = now;
        auto* platform = brls::Application::getPlatform();
        battery_percent_ = platform->canShowBatteryLevel() ? platform->getBatteryLevel() : -1;
        if (battery_percent_ < 0 || battery_percent_ > 100) battery_percent_ = -1;
    }
    return battery_percent_;
}

void StreamView::DrawPerformanceOverlay(NVGcontext* vg, float x, float y) {
    char text[128];
    if (network_rtt_ms_ >= 0) {
        std::snprintf(
            text, sizeof(text), nxnow::Tr("FPS  %.0f     BITRATE  %.1f Mbps     PING  %d ms").c_str(),
            presented_fps_, stream_bitrate_mbps_, network_rtt_ms_);
    } else {
        std::snprintf(
            text, sizeof(text), nxnow::Tr("FPS  %.0f     BITRATE  %.1f Mbps     PING  -- ms").c_str(),
            presented_fps_, stream_bitrate_mbps_);
    }

    const int battery = CurrentBatteryLevel();
    const std::string battery_text = nxnow::Tr("Battery") + " " +
        (battery >= 0 && battery <= 100 ? std::to_string(battery) + "%" : "—");
    const std::string line = std::string(text) + "     " + battery_text + "     " +
        nxnow::Tr(nxnow::input::InputModeLabel(launcher_mouse_.mouse_mode()));
    const float box_width = nxnow::ui::TextWidth(line, 18, nxnow::ui::FontRole::Body) + 40;
    const float box_x = x + 16.0f;
    const float box_y = y + 16.0f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, box_width, 46.0f, 8.0f);
    nvgFillColor(vg, nvgRGBA(8, 12, 14, 205));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x + 6.0f, box_y + 7.0f, 4.0f, 32.0f, 2.0f);
    nvgFillColor(vg, nxnow::ui::Accent());
    nvgFill(vg);

    nvgFontSize(vg, 18.0f);
    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Body));
    nvgFillColor(vg, nvgRGB(245, 250, 247));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, box_x + 20.0f, box_y + 23.0f, line.c_str(), nullptr);
}

void StreamView::SetStreamOverlayVisible(bool visible)
{
    if (stream_overlay_visible_ == visible)
        return;

    stream_overlay_visible_ = visible;
    ReleasePhysicalInputs();
    stream_overlay_b_was_down_ = false;
    if (visible)
        UpdateGameplayInputCapture(true, std::chrono::steady_clock::now());
    if (session_)
    {
        session_->record_ui_event(
            visible ? "stream overlay opened by Minus+Plus"
                    : "stream overlay closed");
    }

    if (!visible)
        keyboard_release_guard_ = true;
}

void StreamView::DrawControllerNotice(
    NVGcontext* vg, float x, float y, float width,
    std::chrono::steady_clock::time_point now)
{
    UpdateControllerNotice(now);
    if (controller_notice_text_.empty())
        return;

    const float box_width = std::min(360.0f, width - 36.0f);
    const float box_height = 54.0f;
    const float box_x = x + (width - box_width) * 0.5f;
    const float box_y = y + 18.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, box_width, box_height, 13.0f);
    nvgFillColor(vg, nvgRGBA(7, 11, 14, 232));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, 6.0f, box_height, 3.0f);
    nvgFillColor(vg, nxnow::ui::Accent());
    nvgFill(vg);

    nvgFontSize(vg, 21.0f);
    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Body));
    nvgFillColor(vg, nvgRGB(248, 252, 249));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(
        vg, box_x + box_width * 0.5f, box_y + box_height * 0.5f,
        controller_notice_text_.c_str(), nullptr);
}

void StreamView::RefreshNetworkInfo(std::chrono::steady_clock::time_point now)
{
    const bool was_2_4_ghz =
        nxnow::network::ShouldWarnForStreaming(network_info_.wifi_band);
    const nxnow::NetworkMonitorSnapshot snapshot = network_monitor_.snapshot();
    network_info_ = snapshot.connection_info;
    internet_connected_ = snapshot.internet_connected;

    const bool is_2_4_ghz =
        nxnow::network::ShouldWarnForStreaming(network_info_.wifi_band);
    if (is_2_4_ghz && !was_2_4_ghz) {
        network_warning_visible_until_ = now + std::chrono::seconds(12);
        if (session_)
            session_->record_ui_event("network_warning wifiBand=2.4GHz");
    }
}

void StreamView::DrawNetworkWarning(
    NVGcontext* vg, float x, float y, float width, float height,
    std::chrono::steady_clock::time_point now)
{
    if (stream_overlay_visible_ || now >= network_warning_visible_until_ ||
        !nxnow::network::ShouldWarnForStreaming(network_info_.wifi_band))
        return;

    const float box_width = std::min(650.0f, width - 36.0f);
    const float box_height = 72.0f;
    const float box_x = x + (width - box_width) * 0.5f;
    const float box_y = y + height - box_height - 76.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, box_width, box_height, 13.0f);
    nvgFillColor(vg, nvgRGBA(13, 16, 18, 238));
    nvgFill(vg);
    nvgStrokeWidth(vg, 1.0f);
    nvgStrokeColor(vg, nvgRGBA(255, 184, 58, 105));
    nvgStroke(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, 6.0f, box_height, 3.0f);
    nvgFillColor(vg, nvgRGB(255, 184, 58));
    nvgFill(vg);

    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Body));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, 18.0f);
    nvgFillColor(vg, nvgRGB(252, 244, 224));
    nvgText(vg, box_x + 22.0f, box_y + 25.0f,
            nxnow::Tr("2.4 GHz Wi-Fi may cause poor streaming performance").c_str(), nullptr);
    nvgFontSize(vg, 14.0f);
    nvgFillColor(vg, nvgRGB(190, 194, 196));
    nvgText(vg, box_x + 22.0f, box_y + 49.0f,
            nxnow::Tr("For lower latency and fewer frame drops, use 5 GHz Wi-Fi or Ethernet.").c_str(), nullptr);
}

void StreamView::DrawStreamOverlay(
    NVGcontext* vg, float x, float y, float width, float height,
    std::chrono::steady_clock::time_point now)
{
    if (!stream_overlay_visible_)
        return;
    if (!stream_overlay_view_)
        stream_overlay_view_ = std::make_unique<nxnow::StreamOverlayView>();
    if (now >= stream_overlay_next_sample_)
    {
        const auto health = session_ ? session_->get_transport_health() : StreamTransportHealth {};
        const auto& counters = previous_video_counters_;
        const auto& network = network_counters_;
        nxnow::StreamOverlaySample sample;
        sample.game_title = game_title_;
        sample.provider = auth_.provider.display_name;
        sample.codec = stream_codec_;
        sample.location = nxnow::ui::ConfiguredLocation(stream_region_);
        if (!network_info_.connected)
            sample.network = "Disconnected";
        else if (network_info_.type == nxnow::NetworkConnectionType::Ethernet)
            sample.network = "Ethernet";
        else if (network_info_.type == nxnow::NetworkConnectionType::Wifi)
            sample.network = nxnow::network::WifiBandLabel(network_info_.wifi_band);
        sample.incoming_fps = incoming_fps_;
        sample.decoded_fps = decoded_fps_;
        sample.presented_fps = presented_fps_;
        sample.bitrate_mbps = stream_bitrate_mbps_;
        sample.rtt_ms = network_rtt_ms_;
        sample.battery_percent = CurrentBatteryLevel();
        sample.keyboard_mouse_mode = launcher_mouse_.mouse_mode();
        sample.width = session_ ? session_->stream_width() : 0;
        sample.height = session_ ? session_->stream_height() : 0;
        sample.decode_us_p95 = counters.decode_us_p95;
        sample.render_us_p95 = counters.render_us_p95;
        sample.queue_size = counters.decode_queue_size;
        sample.queue_high_water = counters.decode_queue_high_water;
        sample.packets_received = network.packets_received;
        sample.sequence_gaps = network.sequence_gaps;
        sample.late_packets_dropped = network.late_packets_dropped;
        sample.dropped_frames = network.access_units_dropped;
        sample.nack_requests = network.nack_requests;
        sample.peer_connected = health.peer_completed;
        sample.signaling_connected = health.signaling_connected;
        sample.wifi_warning = nxnow::network::ShouldWarnForStreaming(network_info_.wifi_band);
        sample.nte_session = is_nte_session_;
        stream_overlay_view_->Update(sample);
        stream_overlay_next_sample_ = now + std::chrono::milliseconds(750);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stream_started_at_).count();
    if (elapsed != stream_overlay_elapsed_seconds_)
    {
        stream_overlay_elapsed_seconds_ = elapsed;
        stream_overlay_view_->UpdateElapsed(static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed)));
    }
    stream_overlay_view_->draw(vg, x, y, width, height,
                               brls::Application::getStyle(), nullptr);
}

void StreamView::DrawNteAutoLoginStatus(
    NVGcontext* vg, float x, float y, float width, float height,
    std::chrono::steady_clock::time_point now) {
    if (!is_nte_session_ || now >= nte_status_until_)
        return;

    std::string text = nxnow::Tr(nte_status_);
    if (text.empty()) {
        text = nte_credentials_.valid()
            ? nxnow::Tr("NTE Auto-login ready: press L + X")
            : nxnow::Tr("Configure NTE Auto-login on the game page");
    }
    if (nte_stage_ != NteAutoLoginStage::Idle)
        text += nxnow::Tr("  |  B Cancel");

    const float box_width = std::min(560.0f, width - 36.0f);
    const float box_height = 46.0f;
    const float box_x = x + 18.0f;
    const float box_y = y + height - box_height - 18.0f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, box_width, box_height, 11.0f);
    nvgFillColor(vg, nvgRGBA(7, 12, 14, 220));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, 6.0f, box_height, 3.0f);
    nvgFillColor(vg, nxnow::ui::Accent());
    nvgFill(vg);
    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Body));
    nvgFontSize(vg, 19.0f);
    nvgFillColor(vg, nvgRGB(246, 250, 248));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, box_x + 20.0f, box_y + box_height * 0.5f, text.c_str(), nullptr);
}

void StreamView::UpdateSessionLimitNotice(std::chrono::steady_clock::time_point now) {
    if (!free_tier_session_ || stream_started_at_.time_since_epoch().count() == 0)
        return;

    constexpr auto kFreeSessionLimit = std::chrono::hours(1);
    const auto elapsed = now - stream_started_at_;
    const auto remaining = elapsed < kFreeSessionLimit
        ? std::chrono::duration_cast<std::chrono::seconds>(kFreeSessionLimit - elapsed)
        : std::chrono::seconds(0);

    if (elapsed >= kFreeSessionLimit ||
        (session_ && session_->is_terminal() && elapsed >= std::chrono::minutes(59))) {
        BeginStreamEnd(nxnow::StreamEndReason::FreeSessionEnded, now);
        return;
    }

    if (!one_minute_warning_shown_ && remaining <= std::chrono::seconds(60)) {
        one_minute_warning_shown_ = true;
        five_minute_warning_shown_ = true;
        session_limit_notice_ = SessionLimitNotice::OneMinute;
        session_notice_visible_until_ = now + std::chrono::seconds(10);
        if (session_)
            session_->record_ui_event("free_session_warning remainingSeconds=60");
    } else if (!five_minute_warning_shown_ && remaining <= std::chrono::minutes(5)) {
        five_minute_warning_shown_ = true;
        session_limit_notice_ = SessionLimitNotice::FiveMinutes;
        session_notice_visible_until_ = now + std::chrono::seconds(8);
        if (session_)
            session_->record_ui_event("free_session_warning remainingSeconds=300");
    }
}

void StreamView::BeginStreamEnd(
    nxnow::StreamEndReason reason,
    std::chrono::steady_clock::time_point now) {
    if (reason == nxnow::StreamEndReason::None ||
        stream_end_reason_ != nxnow::StreamEndReason::None)
        return;

    ReleaseRemoteMouseButton();
    ReleasePhysicalInputs();
    stream_end_reason_ = reason;
    RecordLifecycleEvent("end");
    network_monitor_.request_stop();
    stream_end_started_at_ = now;
    stream_auto_exit_at_ = now + std::chrono::seconds(15);
    if (reason == nxnow::StreamEndReason::FreeSessionEnded) {
        session_limit_ended_ = true;
        session_limit_notice_ = SessionLimitNotice::Ended;
    }

    std::string event = "stream_end reason=";
    switch (reason) {
        case nxnow::StreamEndReason::FreeSessionEnded:
            event += "free_session_limit";
            break;
        case nxnow::StreamEndReason::NetworkLost:
            event += "network_lost";
            break;
        case nxnow::StreamEndReason::ServerDisconnected:
            event += "server_disconnected";
            break;
        case nxnow::StreamEndReason::ConnectionFailed:
            event += "connection_failed";
            break;
        case nxnow::StreamEndReason::StreamEnded:
            event += "server_ended_stream";
            break;
        case nxnow::StreamEndReason::VideoTimedOut:
            event += "video_timeout";
            break;
        case nxnow::StreamEndReason::None:
            return;
    }
    event += " autoExitSeconds=15";
    if (session_) {
        session_->record_ui_event(event);
        // Let transport and decoder workers finish during the 15-second
        // notice. Destruction then cannot leave Borealis input blocked.
        session_->request_stop();
    }
}

void StreamView::UpdateStreamEndState(std::chrono::steady_clock::time_point now) {
    if (!session_ || stream_end_reason_ != nxnow::StreamEndReason::None)
        return;

    RefreshNetworkInfo(now);

    const StreamTransportHealth health = session_->get_transport_health();
    nxnow::StreamEndSignals signals;
    signals.free_tier = free_tier_session_;
    signals.session_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - stream_started_at_);
    signals.internet_connected = nxnow::HasStreamNetworkConnection(
        internet_connected_, health.video_started, health.video_idle);
    signals.peer_completed = health.peer_completed;
    signals.peer_terminal = health.peer_terminal;
    signals.signaling_connected = health.signaling_connected;
    signals.video_started = health.video_started;
    signals.video_idle = health.video_idle;
    BeginStreamEnd(nxnow::DetectStreamEnd(signals), now);
}

void StreamView::DrawStreamEndNotice(
    NVGcontext* vg, float x, float y, float width,
    std::chrono::steady_clock::time_point now) {
    if (stream_end_reason_ == nxnow::StreamEndReason::None)
        return;

    std::string title;
    std::string detail;
    switch (stream_end_reason_) {
        case nxnow::StreamEndReason::FreeSessionEnded:
            title = nxnow::Tr("Free session ended");
            detail = nxnow::Tr("The one-hour GeForce NOW limit was reached");
            break;
        case nxnow::StreamEndReason::NetworkLost:
            title = nxnow::Tr("Network connection lost");
            detail = nxnow::Tr("Check the Switch internet connection");
            break;
        case nxnow::StreamEndReason::ServerDisconnected:
            title = nxnow::Tr("Streaming server disconnected");
            detail = nxnow::Tr("The connection to the GeForce NOW rig was lost");
            break;
        case nxnow::StreamEndReason::ConnectionFailed:
            title = nxnow::Tr("Stream connection failed");
            detail = nxnow::Tr("The GeForce NOW rig could not be reached");
            break;
        case nxnow::StreamEndReason::StreamEnded:
            title = nxnow::Tr("Stream ended by server");
            detail = nxnow::Tr("GeForce NOW closed this streaming session");
            break;
        case nxnow::StreamEndReason::VideoTimedOut:
            title = nxnow::Tr("Video stream timed out");
            detail = nxnow::Tr("No video packets were received for 15 seconds");
            break;
        case nxnow::StreamEndReason::None:
            return;
    }

    const auto remaining_ms = std::max<std::int64_t>(0,
        std::chrono::duration_cast<std::chrono::milliseconds>(stream_auto_exit_at_ - now).count());
    const int remaining_seconds = static_cast<int>((remaining_ms + 999) / 1000);
    detail += "  |  " + nxnow::TrFormat("Returning in {0}s", {std::to_string(remaining_seconds)});

    const float box_width = std::min(560.0f, width - 36.0f);
    const float box_height = 82.0f;
    const float box_x = x + width - box_width - 18.0f;
    const float box_y = y + 16.0f;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, box_width, box_height, 13.0f);
    nvgFillColor(vg, nvgRGBA(8, 11, 14, 232));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, 7.0f, box_height, 3.5f);
    nvgFillColor(vg, nvgRGB(255, 91, 91));
    nvgFill(vg);
    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Body));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFontSize(vg, 23.0f);
    nvgFillColor(vg, nvgRGB(252, 246, 246));
    nvgText(vg, box_x + 22.0f, box_y + 29.0f, title.c_str(), nullptr);
    nvgFontSize(vg, 16.0f);
    nvgFillColor(vg, nvgRGB(188, 195, 204));
    nvgText(vg, box_x + 22.0f, box_y + 59.0f, detail.c_str(), nullptr);
}

void StreamView::DrawSessionLimitNotice(
    NVGcontext* vg, float x, float y, float width,
    std::chrono::steady_clock::time_point now) {
    if (!free_tier_session_ || session_limit_notice_ == SessionLimitNotice::None)
        return;

    constexpr auto kFreeSessionLimit = std::chrono::hours(1);
    const auto elapsed = now - stream_started_at_;
    const int remaining_seconds = static_cast<int>(std::max<int64_t>(0,
        std::chrono::duration_cast<std::chrono::seconds>(kFreeSessionLimit - elapsed).count()));
    const bool prominent = session_limit_notice_ == SessionLimitNotice::Ended ||
                           now < session_notice_visible_until_;
    if (session_limit_notice_ == SessionLimitNotice::FiveMinutes && !prominent)
        return;

    std::string text;
    NVGcolor accent = nvgRGB(255, 184, 58);
    if (session_limit_notice_ == SessionLimitNotice::Ended) {
        text = nxnow::Tr("Free session ended");
        accent = nvgRGB(255, 91, 91);
    } else if (session_limit_notice_ == SessionLimitNotice::FiveMinutes) {
        text = nxnow::Tr("Free session: 5 minutes remaining");
    } else {
        text = prominent
            ? nxnow::Tr("Free session ends in 60 seconds")
            : "Session 0:" + (remaining_seconds < 10 ? std::string("0") : std::string()) +
                std::to_string(remaining_seconds);
        accent = nvgRGB(255, 112, 82);
    }

    const float box_width = prominent ? std::min(390.0f, width - 36.0f) : 190.0f;
    const float box_height = prominent ? 50.0f : 40.0f;
    const float box_x = x + width - box_width - 18.0f;
    const float box_y = y + 16.0f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, box_width, box_height, prominent ? 12.0f : 9.0f);
    nvgFillColor(vg, nvgRGBA(7, 10, 12, prominent ? 224 : 198));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, box_x, box_y, 6.0f, box_height, 3.0f);
    nvgFillColor(vg, accent);
    nvgFill(vg);

    nvgFontSize(vg, prominent ? 22.0f : 21.0f);
    nvgFontFaceId(vg, nxnow::ui::Font(nxnow::ui::FontRole::Body));
    nvgFillColor(vg, nvgRGB(250, 252, 250));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, box_x + box_width * 0.5f, box_y + box_height * 0.5f, text.c_str(), nullptr);
}
