#include "StreamView.hpp"
#include "input/TouchMapping.hpp"
#include "main_activity.hpp"
#include "main_tabs_view.hpp"
#include "stream/ffmpeg/AVFrameHolder.hpp"
#include "stream_diagnostics.hpp"
#include "stream_settings.hpp"
#include "game_input_policy.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>

namespace
{

bool IsFreeTier(std::string tier)
{
    std::transform(tier.begin(), tier.end(), tier.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return tier == "FREE";
}

void ReturnToHomeActivity()
{
    const auto activities = brls::Application::getActivitiesStack();
    if (activities.empty() || !activities.back())
        return;
    auto* activity = activities.back();
    if (dynamic_cast<nxnow::MainActivity*>(activity))
    {
        auto* content = activity->getContentView();
        if (auto* container = dynamic_cast<brls::Box*>(content))
        {
            for (auto* child : container->getChildren())
            {
                if (auto* tabs = dynamic_cast<nxnow::MainTabsView*>(child))
                {
                    tabs->focusTab(nxnow::TopBarFrame::kLibraryTab);
                    content = tabs;
                    break;
                }
            }
        }
        if (content)
            brls::Application::giveFocus(content);
        return;
    }
    if (activities.size() > 1)
        brls::Application::popActivity(brls::TransitionAnimation::NONE, ReturnToHomeActivity);
}

} // namespace

StreamView::StreamView(
    const std::string& signaling_url,
    const std::string& jwt_token,
    const std::string& session_id,
    const std::string& media_ip,
    int media_port,
    const std::vector<nxnow::IceServerInfo>& ice_servers,
    const nxnow::GfnClient& client,
    const nxnow::AuthSession& auth,
    const std::string& game_title,
    const nxnow::StreamSettings& stream_settings, const std::string& image_url, const std::string& launch_store,
    std::shared_ptr<nxnow::input::DeferredKeyboardInput> pending_keyboard)
    : client_(client)
    , auth_(auth)
    , cloud_session_id_(session_id)
    , game_title_(game_title) {
    pending_keyboard_ = pending_keyboard ? std::move(pending_keyboard) :
        std::make_shared<nxnow::input::DeferredKeyboardInput>();
    mouse_speed_percent_ = stream_settings.mouse_speed_percent;
    keyboard_layout_ = stream_settings.keyboard_layout == "auto" ? nxnow::input::SystemKeyboardLayout() :
        nxnow::input::KeyboardLayoutForCloudCode(stream_settings.keyboard_layout);
    physical_keyboard_delivery_ = nxnow::input::PhysicalKeyboardDelivery(keyboard_layout_);
    debug_diagnostics_ = stream_settings.debug_diagnostics;
    stats_overlay_enabled_ = stream_settings.stats_overlay_enabled;
    stream_codec_ = stream_settings.codec;
    stream_region_ = stream_settings.region;
    is_wow_session_ = nxnow::input::IsWorldOfWarcraft(game_title_);
    controller_layout_ = nxnow::input::ControllerLayoutForGame(game_title_, stream_settings.controller_layout);
    intro_shortcuts_enabled_ = nxnow::input::IsBattleNetStore(launch_store) || is_wow_session_;
    launcher_mouse_ = nxnow::input::LauncherMouseController(true);
    nxnow::SetStreamDiagnosticsEnabled(debug_diagnostics_);
    free_tier_session_ = auth_.user.membership_tier_verified &&
        IsFreeTier(auth_.user.membership_tier);
    stream_started_at_ = std::chrono::steady_clock::now();
    is_nte_session_ = nxnow::IsNevernessToEverness(game_title_);
    if (is_nte_session_) {
        nte_credentials_ = nxnow::LoadNteCredentials();
        nte_status_until_ = stream_started_at_ + std::chrono::seconds(12);
    }
#ifdef __SWITCH__
    hidInitializeKeyboard();
    hidInitializeMouse();
    padInitialize(&switch_controller_sources_[0], HidNpadIdType_Handheld);
    for (std::size_t source = 1; source < switch_controller_sources_.size(); ++source)
    {
        padInitialize(
            &switch_controller_sources_[source],
            static_cast<HidNpadIdType>(source - 1));
    }
#endif
    session_ = std::make_unique<WebRtcSession>(
        signaling_url,
        jwt_token,
        session_id,
        media_ip,
        media_port,
        ice_servers,
        stream_settings);
    session_->start();
    network_monitor_.start();
    RefreshNetworkInfo(stream_started_at_);
    
    // Suggest the view to take all available space
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
    this->setHideHighlight(true);
    this->setHideHighlightBackground(true);
    this->setHideHighlightBorder(true);
    
    this->setFocusable(true);
    preparing_view_ = new nxnow::QueueView(
        {game_title_, "Connecting to your cloud rig...", "Opening the secure signaling channel", -1, 3, false, image_url},
        stream_settings);
    preparing_view_->setGrow(1);
    addView(preparing_view_);
}

StreamView::~StreamView() {
    SetKeyboardUiBlocked(false);
    ReleaseRemoteMouseButton();
    ReleasePhysicalInputs();
    RecordLifecycleEvent("destroy");
    network_monitor_.request_stop();
    nxnow::SetSensitiveInputLoggingSuppressed(false);
    std::fill(nte_credentials_.password.begin(), nte_credentials_.password.end(), '\0');
    std::fill(nte_text_buffer_.begin(), nte_text_buffer_.end(), '\0');
    std::fill(keyboard_text_.begin(), keyboard_text_.end(), '\0');
#ifdef __SWITCH__
    if (active_keyboard_view_ == this)
        active_keyboard_view_ = nullptr;
    if (keyboard_launched_) {
        swkbdInlineClose(&inline_keyboard_);
        keyboard_launched_ = false;
    }
#endif
    brls::Application::getPlatform()->disableScreenDimming(false);
    brls::Application::getPlatform()->getInputManager()->setPointerLock(false);
    StopCloudSessionAsync();
    if (session_) {
        session_->stop();
    }
}

void StreamView::onFocusGained() {
    brls::Box::onFocusGained();
    if (!input_focused_)
    {
        keyboard_release_guard_ = true;
        ReleasePhysicalInputs();
    }
    input_focused_ = true;
    brls::Application::getPlatform()->disableScreenDimming(true);
    brls::Application::getPlatform()->getInputManager()->setPointerLock(true);
}

void StreamView::onFocusLost() {
    ReleaseRemoteMouseButton();
    ReleasePhysicalInputs();
    HideInlineKeyboard(false);
    SetKeyboardUiBlocked(false);
    input_focused_ = false;
    UpdateGameplayInputCapture(true, std::chrono::steady_clock::now());
    brls::Box::onFocusLost();
    brls::Application::getPlatform()->disableScreenDimming(false);
    brls::Application::getPlatform()->getInputManager()->setPointerLock(false);
}

void StreamView::ExitStream() {
    if (exit_requested_)
        return;
    ReleaseRemoteMouseButton();
    ReleasePhysicalInputs();
    RecordLifecycleEvent("exit");
    exit_requested_ = true;
    network_monitor_.request_stop();
    if (session_)
        session_->request_stop();
    StopCloudSessionAsync();
    if (!brls::Application::popActivity(brls::TransitionAnimation::FADE, ReturnToHomeActivity)) {
        exit_requested_ = false;
    }
}

void StreamView::RecordLifecycleEvent(const char* event) {
    if (!debug_diagnostics_ || !session_)
        return;

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - stream_started_at_).count();
    const auto network = network_monitor_.snapshot();
    const auto health = session_->get_transport_health();
    const auto video = session_->get_video_performance();
    session_->record_ui_event(
        std::string("stream_lifecycle event=") + event +
        " reason=" + std::to_string(static_cast<int>(stream_end_reason_)) +
        " elapsedMs=" + std::to_string(elapsed) +
        " internetConnected=" + std::to_string(network.internet_connected) +
        " peerCompleted=" + std::to_string(health.peer_completed) +
        " peerTerminal=" + std::to_string(static_cast<int>(health.peer_terminal)) +
        " signalingConnected=" + std::to_string(health.signaling_connected) +
        " videoStarted=" + std::to_string(health.video_started) +
        " accessUnitIdleMs=" + std::to_string(health.video_idle.count()) +
        " accessUnits=" + std::to_string(video.access_units) +
        " decodedFrames=" + std::to_string(video.decoded_frames) +
        " presentedFrames=" + std::to_string(video.presented_frames));
}

void StreamView::StopCloudSessionAsync() {
    if (cloud_stop_requested_ || cloud_session_id_.empty())
        return;

    cloud_stop_requested_ = true;
    nxnow::GfnClient bg_client = client_;
    nxnow::AuthSession bg_auth = auth_;
    std::string bg_session_id = cloud_session_id_;
    brls::async([bg_client, bg_auth, bg_session_id]() mutable {
        try {
            bg_client.StopSession(bg_auth, bg_session_id);
        } catch (...) {
        }
    });
}

void StreamView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    if (session_) {
        session_->poll();
        UpdateInlineKeyboard();


        const auto stream_now = std::chrono::steady_clock::now();
        UpdateSessionLimitNotice(stream_now);
        UpdateStreamEndState(stream_now);
        if (stream_end_reason_ != nxnow::StreamEndReason::None &&
            stream_now >= stream_auto_exit_at_) {
            ExitStream();
            return;
        }
        
        brls::ControllerState state;
        brls::Application::getPlatform()->getInputManager()->updateUnifiedControllerState(&state);

        const auto input_now = std::chrono::steady_clock::now();
        bool keyboard_button_held = false;
        for (int button = 0; button < brls::_BUTTON_MAX; ++button)
            keyboard_button_held = keyboard_button_held || state.buttons[button];
        const bool native_keyboard_owns_input = virtual_keyboard_capture_.Update(
            keyboard_visible_, keyboard_button_held, pending_keyboard_->size() != 0, input_now);
        if (keyboard_ui_blocked_ && !native_keyboard_owns_input) SetKeyboardUiBlocked(false);
        PollControllerStates(input_now);
        gamepad_presence_.Update(session_->ready_for_input(), controller_connected_,
            [this](uint8_t controller, uint16_t bitmap) {
                return session_->send_gamepad_input(controller, bitmap, 0, 0, 0, 0, 0, 0, 0);
            });
        bool nte_owned_input = false;
        if (input_focused_ && is_nte_session_ && !native_keyboard_owns_input && !stream_overlay_visible_ &&
            stream_end_reason_ == nxnow::StreamEndReason::None) {
            const bool nte_combo = state.buttons[brls::BUTTON_LB] &&
                                   state.buttons[brls::BUTTON_X];
            if (nte_combo && !nte_combo_was_down_)
                StartNteAutoLogin(input_now);
            nte_combo_was_down_ = nte_combo;

            const bool was_active = nte_stage_ != NteAutoLoginStage::Idle;
            const bool cancel_down = state.buttons[brls::BUTTON_B];
            if (was_active && cancel_down && !nte_cancel_was_down_)
                CancelNteAutoLogin();
            else if (was_active)
                UpdateNteAutoLogin(input_now);
            nte_cancel_was_down_ = cancel_down;
            nte_owned_input = was_active || nte_combo;
        }

        const bool minus_down = state.buttons[brls::BUTTON_BACK];
        const bool plus_down_for_overlay = state.buttons[brls::BUTTON_START];
        const auto chord_now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            input_now.time_since_epoch()).count();
        const bool stream_shortcuts_allowed = input_focused_ && !native_keyboard_owns_input && !keyboard_release_guard_;
        const auto shortcut = stream_shortcut_chord_.Update(
            stream_shortcuts_allowed && minus_down, stream_shortcuts_allowed && plus_down_for_overlay,
            stream_shortcuts_allowed && state.buttons[brls::BUTTON_LT], stream_shortcuts_allowed && state.buttons[brls::BUTTON_RT],
            input_focused_ && !keyboard_visible_ && !nte_owned_input &&
                !keyboard_release_guard_ &&
                stream_end_reason_ == nxnow::StreamEndReason::None,
            chord_now_ms);
        if (input_focused_ && shortcut.exit_stream)
        {
            UpdateGameplayInputCapture(true, input_now);
            HideInlineKeyboard(false);
            CancelNteAutoLogin();
            ExitStream();
            return;
        }
        if (shortcut.toggle_stats)
        {
            stats_overlay_enabled_ = !stats_overlay_enabled_;
            if (stream_overlay_visible_)
                SetStreamOverlayVisible(false);
            if (session_)
                session_->record_ui_event(stats_overlay_enabled_
                    ? "stream stats shown" : "stream stats hidden");
        }

        if (input_focused_ && stream_overlay_visible_)
        {
            const bool b_down = state.buttons[brls::BUTTON_B];
            if (b_down && !stream_overlay_b_was_down_)
            {
                suppress_b_until_release_ = true;
                SetStreamOverlayVisible(false);
            }
            stream_overlay_b_was_down_ = b_down;
        }
        else
        {
            stream_overlay_b_was_down_ = false;
        }

        const bool can_open_keyboard = input_focused_ && !native_keyboard_owns_input && !keyboard_visible_ &&
            !stream_overlay_visible_ && !nte_owned_input && !keyboard_release_guard_ &&
            stream_end_reason_ == nxnow::StreamEndReason::None;
        const auto keyboard_open = keyboard_open_chord_.Update(
            minus_down, state.buttons[brls::BUTTON_RT], state.buttons[brls::BUTTON_Y],
            !is_wow_session_ && can_open_keyboard && !plus_down_for_overlay && !state.buttons[brls::BUTTON_LT]);
        bool wow_other_button = false;
        for (int button = 0; button < brls::_BUTTON_MAX; ++button)
            if (button != brls::BUTTON_RSB && button != brls::BUTTON_RT && state.buttons[button])
                wow_other_button = true;
        const auto chat_open = wow_chat_chord_.Update(
            state.buttons[brls::BUTTON_RSB], state.buttons[brls::BUTTON_RT],
            can_open_keyboard && !wow_other_button);
        if (chat_open.tap_r3 && !launcher_mouse_.mouse_mode())
            wow_r3_until_ = input_now + std::chrono::milliseconds(120);
        bool keyboard_owned_input = !input_focused_ || native_keyboard_owns_input || keyboard_visible_ || stream_overlay_visible_ ||
            nte_owned_input || keyboard_open.capture || chat_open.capture ||
            stream_end_reason_ != nxnow::StreamEndReason::None;
        if (keyboard_open.open) {
            OpenInlineKeyboard(state);
            keyboard_owned_input = keyboard_owned_input || keyboard_visible_;
        }
        if (chat_open.open) {
            if (is_wow_session_) OpenWowChatKeyboard(state);
            else OpenInlineKeyboard(state);
            keyboard_owned_input = keyboard_owned_input || keyboard_visible_;
        }
        // Navigation/confirm/cancel belong exclusively to the native applet.
        // Its decided-enter/cancel callbacks close it; game shortcuts stay paused.
        keyboard_owned_input = keyboard_owned_input || virtual_keyboard_capture_.active();

        if (!state.buttons[brls::BUTTON_B])
            suppress_b_until_release_ = false;

        if (keyboard_release_guard_) {
            std::vector<brls::RawTouchState> touches;
            brls::Application::getPlatform()->getInputManager()->updateTouchStates(&touches);
            const bool touch_active = std::any_of(touches.begin(), touches.end(),
                [](const brls::RawTouchState& touch) { return touch.pressed; });
            const bool controller_active =
                state.buttons[brls::BUTTON_A] || state.buttons[brls::BUTTON_B] ||
                state.buttons[brls::BUTTON_X] || state.buttons[brls::BUTTON_Y] ||
                state.buttons[brls::BUTTON_UP] || state.buttons[brls::BUTTON_DOWN] ||
                state.buttons[brls::BUTTON_LEFT] || state.buttons[brls::BUTTON_RIGHT] ||
                state.buttons[brls::BUTTON_LB] || state.buttons[brls::BUTTON_RB] ||
                state.buttons[brls::BUTTON_LT] || state.buttons[brls::BUTTON_RT] ||
                state.buttons[brls::BUTTON_LSB] || state.buttons[brls::BUTTON_RSB] ||
                state.buttons[brls::BUTTON_START] || state.buttons[brls::BUTTON_BACK];
            if (controller_active || touch_active)
                keyboard_owned_input = true;
            else
                keyboard_release_guard_ = false;
        }

        bool intro_other_button = false;
        for (int button = 0; button < brls::_BUTTON_MAX; ++button)
            if (button != brls::BUTTON_START && state.buttons[button])
                intro_other_button = true;
        const bool intro_context = intro_shortcuts_enabled_ && !keyboard_owned_input &&
            !keyboard_release_guard_ && session_->ready_for_input();
        const auto intro = intro_shortcut_.Update(plus_down_for_overlay, intro_other_button,
            intro_context, chord_now_ms);
        if (!intro_context || intro_other_button)
            intro_escape_until_ = {};
        if (intro.fire)
        {
            intro_escape_until_ = input_now + std::chrono::milliseconds(120);
            intro_notice_until_ = input_now + std::chrono::seconds(2);
            session_->record_ui_event("Battle.net intro Escape requested");
        }
        if (intro.tap_start && !launcher_mouse_.mouse_mode())
            intro_start_until_ = input_now + std::chrono::milliseconds(120);
        bool escape_companion = false;
        for (int button = 0; button < brls::_BUTTON_MAX; ++button)
            if (button != brls::BUTTON_BACK && state.buttons[button]) escape_companion = true;
        if (mouse_escape_key_.Update(minus_down, escape_companion,
                launcher_mouse_.mouse_mode() && !keyboard_owned_input && session_->ready_for_input()))
            mouse_escape_until_ = input_now + std::chrono::milliseconds(120);
        if (escape_companion || keyboard_owned_input) mouse_escape_until_ = {};
        const bool escape_pulse = input_now < intro_escape_until_ || input_now < mouse_escape_until_;
        keyboard_owned_input = keyboard_owned_input || shortcut.capture || intro.capture;
        const auto mouse = UpdateLauncherMouse(!keyboard_owned_input, input_now);
        keyboard_owned_input = keyboard_owned_input || keyboard_release_guard_;
        const bool launcher_release_pending = launcher_mouse_.available() && !launcher_mouse_.mouse_mode() &&
            ((mouse_button_delivery_.held() && !touch_was_down_ && !(physical_mouse_buttons_ & 1)) ||
             (physical_mouse_delivery_[1].held() && !(physical_mouse_buttons_ & 2)));
        UpdateGameplayInputCapture(keyboard_owned_input || mouse.capture || launcher_release_pending, input_now);
        if (!keyboard_owned_input && !mouse.capture && !launcher_release_pending)
            ObserveControllerSystemButtons(input_now);

        if (!keyboard_owned_input) {
        
        const auto now = std::chrono::steady_clock::now();

        if (!mouse.capture && !launcher_release_pending)
            SendControllerInputs(now);

        std::vector<brls::RawTouchState> touches;
        brls::Application::getPlatform()->getInputManager()->updateTouchStates(&touches);
        bool touch_down = false;
        brls::Point touch_position {};
        for (const auto& touch : touches) {
            if (touch.pressed) {
                touch_down = true;
                touch_position = touch.position;
                break;
            }
        }

        if (touch_down) {
            const int stream_width = std::max(1, session_->negotiated_stream_width());
            const int stream_height = std::max(1, session_->negotiated_stream_height());
            const auto target = nxnow::input::MapTouchToStream(
                touch_position.x, touch_position.y, x, y, width, height,
                stream_width, stream_height);

            if (!touch_pointer_initialized_ ||
                touch_pointer_stream_width_ != stream_width ||
                touch_pointer_stream_height_ != stream_height) {
                touch_pointer_x_ = stream_width * 0.5f;
                touch_pointer_y_ = stream_height * 0.5f;
                touch_pointer_stream_width_ = stream_width;
                touch_pointer_stream_height_ = stream_height;
                touch_pointer_initialized_ = true;
            }
            if (!touch_was_down_) {
                session_->record_ui_event(
                    "touch begin raw=" + std::to_string(touch_position.x) + "/" +
                    std::to_string(touch_position.y) + " target=" +
                    std::to_string(target.x) + "/" + std::to_string(target.y) +
                    " pointer=" + std::to_string(touch_pointer_x_) + "/" +
                    std::to_string(touch_pointer_y_) +
                    " frame=" + std::to_string(stream_width) + "x" +
                    std::to_string(stream_height) + " view=" +
                    std::to_string(width) + "x" + std::to_string(height));
                ReanchorRemotePointer(target.x, target.y);
            } else {
                const int16_t dx = ClampMouseDelta(target.x - touch_pointer_x_);
                const int16_t dy = ClampMouseDelta(target.y - touch_pointer_y_);
                if (dx != 0 || dy != 0) {
                    session_->send_mouse_move(dx, dy);
                    touch_pointer_x_ = std::clamp(touch_pointer_x_ + dx, 0.0f,
                                                  static_cast<float>(stream_width - 1));
                    touch_pointer_y_ = std::clamp(touch_pointer_y_ + dy, 0.0f,
                                                  static_cast<float>(stream_height - 1));
                }
            }
        }
        touch_was_down_ = touch_down;
        } else {
            touch_was_down_ = false;
        }
        UpdatePhysicalInputs(!keyboard_owned_input, escape_pulse, input_now);
        SyncRemoteMouseButton(touch_was_down_ || launcher_mouse_left_desired_ || (physical_mouse_buttons_ & 1));
        // Release any gamepad buttons before opening WoW chat or sending text.
        // Repeated neutral reports during typing can steal text focus in games.
        const bool neutral_ready = !virtual_keyboard_capture_.active() || keyboard_neutralization_.Complete(controller_connected_);
        pending_keyboard_->Drain(session_->ready_for_input() && neutral_ready, [this, input_now](const auto& event) {
            const bool sent = session_->send_keyboard_key(event.stroke.keycode, event.stroke.scancode,
                event.stroke.modifiers, event.pressed);
            if (sent && virtual_keyboard_capture_.active()) virtual_keyboard_capture_.Quiet(input_now);
            return sent;
        });
    }
    
    // Present the newest decoded picture instead of pacing video against the
    // audio playback clock. Audio-clock pacing added the audio buffer depth to
    // video latency and held frames whenever audio playback stalled.
    bool got_frame = false;
    AVFrameHolder::instance().get([this, vg, width, height, &got_frame](AVFrame* frame, uint64_t generation, bool reused) {
        if (session_) {
            session_->draw(vg, width, height, frame, generation);
            got_frame = true;
            launcher_hint_.FirstFrame(std::chrono::steady_clock::now());
        }
    });
    if (session_)
        UpdatePerformanceCounter();
    
    const auto preparing_visibility = !got_frame && stream_end_reason_ == nxnow::StreamEndReason::None
        ? brls::Visibility::VISIBLE : brls::Visibility::GONE;
    if (preparing_view_->getVisibility() != preparing_visibility)
        preparing_view_->setVisibility(preparing_visibility);
    if (!got_frame && stream_end_reason_ == nxnow::StreamEndReason::None)
        DrawPreparingStream(vg, x, y, width, height);
    else if (got_frame && debug_diagnostics_ && show_debug_overlay_ && stats_overlay_enabled_)
        DrawDebugOverlay(vg, x, y, width);

    const auto notice_now = std::chrono::steady_clock::now();
    if (stats_overlay_enabled_ &&
        stream_end_reason_ == nxnow::StreamEndReason::None)
        DrawPerformanceOverlay(vg, x, y);
    if (stream_end_reason_ == nxnow::StreamEndReason::None)
        DrawSessionLimitNotice(vg, x, y, width, notice_now);
    DrawStreamEndNotice(vg, x, y, width, notice_now);
    if (stream_end_reason_ == nxnow::StreamEndReason::None)
        DrawNteAutoLoginStatus(vg, x, y, width, height, notice_now);
    if (stream_end_reason_ == nxnow::StreamEndReason::None)
        DrawStreamOverlay(vg, x, y, width, height, notice_now);
    if (stream_end_reason_ == nxnow::StreamEndReason::None)
        DrawNetworkWarning(vg, x, y, width, height, notice_now);
    if (stream_end_reason_ == nxnow::StreamEndReason::None)
        DrawControllerNotice(vg, x, y, width, notice_now);
    if (stream_end_reason_ == nxnow::StreamEndReason::None) {
        DrawLauncherInputHint(vg, x, y, width, height, notice_now);
    }

    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

brls::View* StreamView::create(
    const std::string& signaling_url,
    const std::string& jwt_token,
    const std::string& session_id,
    const std::string& media_ip,
    int media_port,
    const std::vector<nxnow::IceServerInfo>& ice_servers,
    const nxnow::GfnClient& client,
    const nxnow::AuthSession& auth,
    const std::string& game_title,
    const nxnow::StreamSettings& settings, const std::string& image_url, const std::string& launch_store,
    std::shared_ptr<nxnow::input::DeferredKeyboardInput> pending_keyboard) {
    return new StreamView(signaling_url, jwt_token, session_id, media_ip, media_port,
                          ice_servers, client, auth, game_title, settings, image_url, launch_store, std::move(pending_keyboard));
}
