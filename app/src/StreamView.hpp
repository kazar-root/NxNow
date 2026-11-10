#pragma once
#include <borealis.hpp>
#include "controller_assignment_policy.hpp"
#include "gfn_client.hpp"
#include "controller_delivery_policy.hpp"
#include "keyboard_input_policy.hpp"
#include "launcher_mouse_policy.hpp"
#include "launcher_hint_policy.hpp"
#include "physical_input_policy.hpp"
#include "bnet_intro_policy.hpp"
#include "wow_chat_chord.hpp"
#include "deferred_keyboard_input.hpp"
#include "virtual_keyboard_capture.hpp"
#include "network_monitor.hpp"
#include "nte_credentials.hpp"
#include "stream_end_policy.hpp"
#include "stream_overlay_policy.hpp"
#include "stream_overlay_view.hpp"
#include "stream_settings.hpp"
#include "queue_view.hpp"
#include "webrtc_session.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#endif

class StreamView : public brls::Box {
public:
    StreamView(
        const std::string& signaling_url,
        const std::string& jwt_token,
        const std::string& session_id,
        const std::string& media_ip,
        int media_port,
        const std::vector<nxnow::IceServerInfo>& ice_servers,
        const nxnow::GfnClient& client,
        const nxnow::AuthSession& auth,
        const std::string& game_title,
        const nxnow::StreamSettings& settings, const std::string& image_url = {},
        const std::string& launch_store = {},
        std::shared_ptr<nxnow::input::DeferredKeyboardInput> pending_keyboard = {});
    ~StreamView() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    void onFocusGained() override;
    void onFocusLost() override;

    static brls::View* create(
        const std::string& signaling_url,
        const std::string& jwt_token,
        const std::string& session_id,
        const std::string& media_ip,
        int media_port,
        const std::vector<nxnow::IceServerInfo>& ice_servers,
        const nxnow::GfnClient& client,
        const nxnow::AuthSession& auth,
        const std::string& game_title,
        const nxnow::StreamSettings& settings, const std::string& image_url = {},
        const std::string& launch_store = {},
        std::shared_ptr<nxnow::input::DeferredKeyboardInput> pending_keyboard = {});

private:
    void ExitStream();
    void StopCloudSessionAsync();
    void RecordLifecycleEvent(const char* event);
    void DrawDebugOverlay(NVGcontext* vg, float x, float y, float width);
    void DrawPreparingStream(NVGcontext* vg, float x, float y, float width, float height);
    void UpdatePerformanceCounter();
    void DrawPerformanceOverlay(NVGcontext* vg, float x, float y);
    int CurrentBatteryLevel();
    void SetStreamOverlayVisible(bool visible);
    void DrawStreamOverlay(NVGcontext* vg, float x, float y, float width, float height,
                           std::chrono::steady_clock::time_point now);
    void RefreshNetworkInfo(std::chrono::steady_clock::time_point now);
    void DrawNetworkWarning(NVGcontext* vg, float x, float y, float width, float height,
                            std::chrono::steady_clock::time_point now);
    void UpdateSessionLimitNotice(std::chrono::steady_clock::time_point now);
    void DrawSessionLimitNotice(NVGcontext* vg, float x, float y, float width,
                                std::chrono::steady_clock::time_point now);
    void UpdateStreamEndState(std::chrono::steady_clock::time_point now);
    void BeginStreamEnd(nxnow::StreamEndReason reason,
                        std::chrono::steady_clock::time_point now);
    void DrawStreamEndNotice(NVGcontext* vg, float x, float y, float width,
                             std::chrono::steady_clock::time_point now);
    void PollControllerStates(std::chrono::steady_clock::time_point now);
    void ObserveControllerSystemButtons(std::chrono::steady_clock::time_point now);
    void SendControllerInputs(std::chrono::steady_clock::time_point now);
    uint16_t SendPendingControllerDisconnects();
    void UpdateGameplayInputCapture(bool captured, std::chrono::steady_clock::time_point now);
    void QueueControllerConnectedNotice(
        std::size_t controller, std::chrono::steady_clock::time_point now);
    void UpdateControllerNotice(std::chrono::steady_clock::time_point now);
    void DrawControllerNotice(NVGcontext* vg, float x, float y, float width,
                              std::chrono::steady_clock::time_point now);
#ifdef __SWITCH__
    bool ReadSwitchControllerSource(std::size_t source, brls::ControllerState& state);
#endif
    void OpenInlineKeyboard(const brls::ControllerState& state);
    void OpenWowChatKeyboard(const brls::ControllerState& state);
    nxnow::input::LauncherMouseDecision UpdateLauncherMouse(
        bool enabled, std::chrono::steady_clock::time_point now);
    void SyncRemoteMouseButton(bool desired);
    void ReleaseRemoteMouseButton();
    void UpdatePhysicalInputs(bool enabled, bool escape_pulse, std::chrono::steady_clock::time_point now);
    void ReleasePhysicalInputs();
    void DeliverPhysicalKeyboard(bool enabled, bool escape_pulse);
    void SyncPhysicalMouseButtons();
    void DrawLauncherInputHint(NVGcontext* vg, float x, float y, float width, float height,
                               std::chrono::steady_clock::time_point now);
    void HideInlineKeyboard(bool send_enter);
    void UpdateInlineKeyboard();
    void SetKeyboardUiBlocked(bool blocked);
    bool HandleKeyboardText(const char* text);
    void SendKeyboardStroke(nxnow::input::KeyboardStroke stroke);
    void SendKeyboardCharacter(char character);
    void StartNteAutoLogin(std::chrono::steady_clock::time_point now);
    void CancelNteAutoLogin();
    void UpdateNteAutoLogin(std::chrono::steady_clock::time_point now);
    void SendNteClick(float normalized_x, float normalized_y);
    void ReanchorRemotePointer(float target_x, float target_y);
    static int16_t ClampMouseDelta(float value);
    void ClearNteFocusedField();
    void SubmitNteFocusedField(const char* stage);
    void DrawNteAutoLoginStatus(NVGcontext* vg, float x, float y, float width, float height,
                                std::chrono::steady_clock::time_point now);
#ifdef __SWITCH__
    static void KeyboardChangedCallback(const char* text, SwkbdChangedStringArg* arg);
    static void KeyboardEnterCallback(const char* text, SwkbdDecidedEnterArg* arg);
    static void KeyboardCancelCallback();
    static StreamView* active_keyboard_view_;
#endif

    enum class SessionLimitNotice {
        None,
        FiveMinutes,
        OneMinute,
        Ended,
    };

    enum class NteAutoLoginStage {
        Idle,
        ClickEmailProvider,
        WaitEmailPage,
        FocusEmail,
        TypeEmail,
        SubmitEmail,
        WaitPasswordPage,
        FocusPassword,
        TypePassword,
        SubmitLogin,
    };

    std::unique_ptr<WebRtcSession> session_;
    nxnow::GfnClient client_;
    nxnow::AuthSession auth_;
    std::string cloud_session_id_;
    std::string game_title_;
    nxnow::QueueView* preparing_view_ = nullptr;
    bool cloud_stop_requested_ = false;
    bool debug_diagnostics_ = false;
    bool stats_overlay_enabled_ = false;
    bool stream_overlay_visible_ = false;
    std::unique_ptr<nxnow::StreamOverlayView> stream_overlay_view_;
    std::chrono::steady_clock::time_point stream_overlay_next_sample_ {};
    std::int64_t stream_overlay_elapsed_seconds_ = -1;
    bool stream_overlay_b_was_down_ = false;
    nxnow::input::StreamShortcutChord stream_shortcut_chord_;
    std::string stream_codec_ = "H264";
    std::string stream_region_ = "Auto";
    std::string controller_layout_ = "Switch";
    bool show_debug_overlay_ = false;
    bool is_wow_session_ = false;
    nxnow::input::BnetIntroShortcut intro_shortcut_;
    std::chrono::steady_clock::time_point intro_escape_until_ {};
    std::chrono::steady_clock::time_point intro_notice_until_ {};
    std::chrono::steady_clock::time_point intro_start_until_ {};
    std::chrono::steady_clock::time_point wow_r3_until_ {};
    nxnow::input::LauncherMouseController launcher_mouse_;
    nxnow::input::MouseButtonDelivery mouse_button_delivery_;
    bool launcher_mouse_left_desired_ = false;
    bool launcher_mouse_right_desired_ = false;
    int mouse_speed_percent_ = 100;
    nxnow::input::KeyboardLayout keyboard_layout_ = nxnow::input::KeyboardLayout::Us;
    int battery_percent_ = -1;
    std::chrono::steady_clock::time_point battery_sampled_at_ {};
    bool intro_shortcuts_enabled_ = false;
    nxnow::input::MouseEscapeKey mouse_escape_key_;
    std::chrono::steady_clock::time_point mouse_escape_until_ {};
    nxnow::input::PhysicalKeys virtual_keys_;
    std::shared_ptr<nxnow::input::DeferredKeyboardInput> pending_keyboard_;
    bool launcher_pointer_anchored_ = false;
    int launcher_mouse_source_ = -1;
    std::chrono::steady_clock::time_point launcher_mouse_updated_at_ {};
    nxnow::input::LauncherHintWindow launcher_hint_;
    nxnow::input::PhysicalKeys physical_keys_;
    nxnow::input::PhysicalKeyboardCapture physical_keyboard_capture_;
    nxnow::input::PhysicalKeyboardDelivery physical_keyboard_delivery_;
    nxnow::input::PhysicalMouseCapture physical_mouse_capture_;
    nxnow::input::HidSampleSequence physical_keyboard_sequence_;
    nxnow::input::HidSampleSequence physical_mouse_sequence_;
    std::chrono::steady_clock::time_point physical_keyboard_seen_at_ {};
    std::chrono::steady_clock::time_point physical_mouse_seen_at_ {};
    unsigned physical_mouse_held_ = 0;
    unsigned physical_mouse_buttons_ = 0;
    std::array<nxnow::input::MouseButtonDelivery, 4> physical_mouse_delivery_;
    int physical_wheel_pending_ = 0;
    bool is_nte_session_ = false;
    bool nte_combo_was_down_ = false;
    bool nte_cancel_was_down_ = false;
    NteAutoLoginStage nte_stage_ = NteAutoLoginStage::Idle;
    nxnow::NteCredentials nte_credentials_;
    std::string nte_text_buffer_;
    size_t nte_text_index_ = 0;
    std::string nte_status_;
    std::chrono::steady_clock::time_point nte_next_action_ {};
    std::chrono::steady_clock::time_point nte_status_until_ {};
    bool exit_requested_ = false;
    bool input_focused_ = true;
    nxnow::input::ControllerInputCapture controller_input_capture_;
    nxnow::input::GamepadPresence gamepad_presence_;
    bool touch_was_down_ = false;
    bool touch_pointer_initialized_ = false;
    float touch_pointer_x_ = 0.0f;
    float touch_pointer_y_ = 0.0f;
    int touch_pointer_stream_width_ = 0;
    int touch_pointer_stream_height_ = 0;
    nxnow::input::KeyboardOpenChord keyboard_open_chord_;
    nxnow::input::WowChatChord wow_chat_chord_;
    bool keyboard_chat_mode_ = false;
    bool keyboard_ui_blocked_ = false;
    nxnow::input::VirtualKeyboardCapture virtual_keyboard_capture_;
    nxnow::input::GamepadPresence keyboard_neutralization_;
    bool suppress_b_until_release_ = false;
    bool keyboard_release_guard_ = false;
    bool keyboard_visible_ = false;
    bool keyboard_launched_ = false;
    std::string keyboard_text_;
#ifdef __SWITCH__
    SwkbdInline inline_keyboard_ {};
#endif
    nxnow::input::ControllerAssignments controller_assignments_;
    std::array<brls::ControllerState, nxnow::input::kRemoteControllerCount>
        controller_states_ {};
    std::array<bool, nxnow::input::kRemoteControllerCount> controller_connected_ {};
    std::array<nxnow::input::ControllerDeliveryState, nxnow::input::kRemoteControllerCount>
        controller_delivery_ {};
    bool controller_connections_initialized_ = false;
    std::string controller_notice_text_;
    std::deque<std::string> controller_notice_queue_;
    std::chrono::steady_clock::time_point controller_notice_visible_until_ {};
#ifdef __SWITCH__
    std::array<PadState, nxnow::input::kSwitchControllerSourceCount>
        switch_controller_sources_ {};
#endif
    std::chrono::steady_clock::time_point fps_window_started_ {};
    VideoPerformanceCounters previous_video_counters_ {};
    float incoming_fps_ = 0.0f;
    float decoded_fps_ = 0.0f;
    float presented_fps_ = 0.0f;
    float stream_bitrate_mbps_ = 0.0f;
    int network_rtt_ms_ = -1;
    StreamNetworkCounters network_counters_ {};
    bool free_tier_session_ = false;
    bool five_minute_warning_shown_ = false;
    bool one_minute_warning_shown_ = false;
    bool session_limit_ended_ = false;
    SessionLimitNotice session_limit_notice_ = SessionLimitNotice::None;
    std::chrono::steady_clock::time_point stream_started_at_ {};
    std::chrono::steady_clock::time_point session_notice_visible_until_ {};
    nxnow::StreamEndReason stream_end_reason_ = nxnow::StreamEndReason::None;
    std::chrono::steady_clock::time_point stream_end_started_at_ {};
    std::chrono::steady_clock::time_point stream_auto_exit_at_ {};
    nxnow::NetworkMonitor network_monitor_;
    std::chrono::steady_clock::time_point network_warning_visible_until_ {};
    nxnow::NetworkConnectionInfo network_info_ {};
    bool internet_connected_ = true;
};
