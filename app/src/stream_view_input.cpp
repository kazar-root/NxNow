#include "StreamView.hpp"
#include "keyboard_input_policy.hpp"
#include "nte_autologin_log.hpp"
#include "stream_diagnostics.hpp"
#include "localization.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

int16_t StreamView::ClampMouseDelta(float value)
{
    return static_cast<int16_t>(std::clamp<long>(std::lround(value), -32768L, 32767L));
}

#ifdef __SWITCH__
StreamView* StreamView::active_keyboard_view_ = nullptr;
#endif

void StreamView::SendKeyboardCharacter(char character) {
    if (!session_)
        return;

    nxnow::input::KeyboardStroke stroke {};
    if (character == '\b')
        stroke = {0x08, 0x0e, 0};
    else if (character == '\n' || character == '\r')
        stroke = {0x0d, 0x1c, 0};
    else
    {
        if (!pending_keyboard_->AppendCharacter(static_cast<unsigned char>(character), keyboard_layout_))
            brls::Application::notify(nxnow::Tr("Some characters are not supported by the remote keyboard"));
        return;
    }

    SendKeyboardStroke(stroke);
}

void StreamView::SendKeyboardStroke(nxnow::input::KeyboardStroke stroke) {
    if (!session_)
        return;

    if (!pending_keyboard_->Append(stroke))
        brls::Application::notify(nxnow::Tr("Keyboard buffer full. Wait for the game to connect."));
}

void StreamView::SetKeyboardUiBlocked(bool blocked)
{
    if (keyboard_ui_blocked_ == blocked) return;
    keyboard_ui_blocked_ = blocked;
    if (blocked) brls::Application::blockInputs();
    else brls::Application::unblockInputs();
}

void StreamView::SendNteClick(float normalized_x, float normalized_y) {
    if (!session_) {
        nxnow::AppendNteAutoLoginLog("CLICK skipped reason=no_session");
        return;
    }

    const int stream_width = std::max(1, session_->negotiated_stream_width());
    const int stream_height = std::max(1, session_->negotiated_stream_height());
    if (!touch_pointer_initialized_ ||
        touch_pointer_stream_width_ != stream_width ||
        touch_pointer_stream_height_ != stream_height) {
        touch_pointer_x_ = stream_width * 0.5f;
        touch_pointer_y_ = stream_height * 0.5f;
        touch_pointer_stream_width_ = stream_width;
        touch_pointer_stream_height_ = stream_height;
        touch_pointer_initialized_ = true;
    }

    const float target_x = std::clamp(normalized_x, 0.0f, 1.0f) * (stream_width - 1);
    const float target_y = std::clamp(normalized_y, 0.0f, 1.0f) * (stream_height - 1);
    nxnow::AppendNteAutoLoginLog(
        "CLICK normalized=" + std::to_string(normalized_x) + "/" +
        std::to_string(normalized_y) + " target=" + std::to_string(target_x) + "/" +
        std::to_string(target_y) + " anchor=top_left stream=" +
        std::to_string(stream_width) + "x" +
        std::to_string(stream_height));
    ReanchorRemotePointer(target_x, target_y);
    session_->send_mouse_left_button(true);
    session_->send_mouse_left_button(false);
}

void StreamView::ReanchorRemotePointer(float target_x, float target_y) {
    if (!session_)
        return;

    const int stream_width = std::max(1, session_->negotiated_stream_width());
    const int stream_height = std::max(1, session_->negotiated_stream_height());
    target_x = std::clamp(target_x, 0.0f, static_cast<float>(stream_width - 1));
    target_y = std::clamp(target_y, 0.0f, static_cast<float>(stream_height - 1));

    // GFN exposes relative mouse movement. Two oversized moves guarantee that
    // the remote cursor reaches the top-left edge regardless of its real
    // position, eliminating drift between our model and the streamed cursor.
    session_->send_mouse_move(-4096, -4096);
    session_->send_mouse_move(-4096, -4096);
    session_->send_mouse_move(ClampMouseDelta(target_x), ClampMouseDelta(target_y));
    touch_pointer_x_ = target_x;
    touch_pointer_y_ = target_y;
    touch_pointer_stream_width_ = stream_width;
    touch_pointer_stream_height_ = stream_height;
    touch_pointer_initialized_ = true;
}

void StreamView::ClearNteFocusedField() {
    if (!session_)
        return;
    // Modifier bit 1 is Ctrl in the NVST keyboard protocol. Clearing the
    // active field makes retries idempotent instead of appending credentials.
    constexpr uint16_t kControl = 0x0002;
    SendKeyboardStroke({'A', 0x1e, kControl});
    SendKeyboardStroke({0x08, 0x0e, 0});
    nxnow::AppendNteAutoLoginLog("FIELD clear select_all_backspace content=redacted");
}

void StreamView::SubmitNteFocusedField(const char* stage) {
    SendKeyboardCharacter('\n');
    nxnow::AppendNteAutoLoginLog(
        "FORM submit method=enter stage=" + std::string(stage ? stage : "unknown"));
}

void StreamView::StartNteAutoLogin(std::chrono::steady_clock::time_point now) {
    if (!is_nte_session_ || nte_stage_ != NteAutoLoginStage::Idle)
        return;
    if (keyboard_visible_) {
        nte_status_ = "Close the on-screen keyboard before NTE Auto-login";
        nte_status_until_ = now + std::chrono::seconds(4);
        return;
    }

    nte_credentials_ = nxnow::LoadNteCredentials();
    if (!nte_credentials_.valid()) {
        nxnow::ResetNteAutoLoginLog("Neverness to Everness");
        nxnow::AppendNteAutoLoginLog("START rejected reason=credentials_missing_or_invalid");
        nte_status_ = "NTE Auto-login is not configured on the game page";
        nte_status_until_ = now + std::chrono::seconds(5);
        brls::Application::notify(nxnow::Tr("Configure NTE Auto-login on the Neverness to Everness game page"));
        return;
    }

    nte_stage_ = NteAutoLoginStage::ClickEmailProvider;
    UpdateGameplayInputCapture(true, now);
    nxnow::SetSensitiveInputLoggingSuppressed(true);
    nxnow::ResetNteAutoLoginLog("Neverness to Everness");
    nxnow::AppendNteAutoLoginLog(
        "FLOW revision=0.9.4.5 submit=email_enter,password_enter field_clear=ctrl_a_backspace");
    nxnow::AppendNteAutoLoginLog("START accepted credentials=present_and_redacted");
    if (session_)
        nxnow::AppendNteAutoLoginLog("TRANSPORT snapshot\n" + session_->get_debug_info());
    nte_next_action_ = now;
    nte_status_ = "NTE Auto-login: opening email sign-in";
    nte_status_until_ = now + std::chrono::seconds(20);
    if (session_)
        session_->record_ui_event("nte_autologin started");
}

void StreamView::CancelNteAutoLogin() {
    if (nte_stage_ == NteAutoLoginStage::Idle)
        return;
    std::fill(nte_text_buffer_.begin(), nte_text_buffer_.end(), '\0');
    nte_text_buffer_.clear();
    nte_text_index_ = 0;
    nte_stage_ = NteAutoLoginStage::Idle;
    keyboard_release_guard_ = true;
    nxnow::SetSensitiveInputLoggingSuppressed(false);
    nxnow::AppendNteAutoLoginLog("CANCEL source=user_button_b");
    nte_status_ = "NTE Auto-login cancelled";
    nte_status_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    if (session_)
        session_->record_ui_event("nte_autologin cancelled");
}

void StreamView::UpdateNteAutoLogin(std::chrono::steady_clock::time_point now) {
    if (nte_stage_ == NteAutoLoginStage::Idle || now < nte_next_action_)
        return;

    switch (nte_stage_) {
        case NteAutoLoginStage::ClickEmailProvider:
            nxnow::AppendNteAutoLoginLog("STAGE click_email_provider");
            SendNteClick(0.59f, 0.43f);
            nte_stage_ = NteAutoLoginStage::WaitEmailPage;
            nte_next_action_ = now + std::chrono::milliseconds(2500);
            nte_status_ = "NTE Auto-login: waiting for email form";
            break;
        case NteAutoLoginStage::WaitEmailPage:
            nxnow::AppendNteAutoLoginLog("STAGE email_page_wait_complete delay_ms=2500");
            nte_stage_ = NteAutoLoginStage::FocusEmail;
            nte_next_action_ = now;
            break;
        case NteAutoLoginStage::FocusEmail:
            nxnow::AppendNteAutoLoginLog("STAGE focus_email_field");
            SendNteClick(0.55f, 0.35f);
            ClearNteFocusedField();
            nte_text_buffer_ = nte_credentials_.email;
            nte_text_index_ = 0;
            nte_stage_ = NteAutoLoginStage::TypeEmail;
            nte_next_action_ = now + std::chrono::milliseconds(220);
            nte_status_ = "NTE Auto-login: entering email";
            nxnow::AppendNteAutoLoginLog("TYPE email begin content=redacted");
            break;
        case NteAutoLoginStage::TypeEmail:
            if (nte_text_index_ < nte_text_buffer_.size()) {
                SendKeyboardCharacter(nte_text_buffer_[nte_text_index_++]);
                nte_next_action_ = now + std::chrono::milliseconds(35);
            } else {
                nte_text_buffer_.clear();
                nxnow::AppendNteAutoLoginLog("TYPE email complete content=redacted");
                nte_stage_ = NteAutoLoginStage::SubmitEmail;
                nte_next_action_ = now + std::chrono::milliseconds(300);
            }
            break;
        case NteAutoLoginStage::SubmitEmail:
            nxnow::AppendNteAutoLoginLog("STAGE submit_email method=enter");
            SubmitNteFocusedField("email");
            nte_stage_ = NteAutoLoginStage::WaitPasswordPage;
            nte_next_action_ = now + std::chrono::milliseconds(5500);
            nte_status_ = "NTE Auto-login: waiting for password form";
            nxnow::AppendNteAutoLoginLog(
                "GUARD password_input_deferred_until_page_transition delay_ms=5500");
            break;
        case NteAutoLoginStage::WaitPasswordPage:
            nxnow::AppendNteAutoLoginLog("STAGE password_page_wait_complete delay_ms=5500");
            nte_stage_ = NteAutoLoginStage::FocusPassword;
            nte_next_action_ = now;
            break;
        case NteAutoLoginStage::FocusPassword:
            nxnow::AppendNteAutoLoginLog("STAGE focus_password_field");
            SendNteClick(0.55f, 0.45f);
            ClearNteFocusedField();
            nte_text_buffer_ = nte_credentials_.password;
            nte_text_index_ = 0;
            nte_stage_ = NteAutoLoginStage::TypePassword;
            nte_next_action_ = now + std::chrono::milliseconds(220);
            nte_status_ = "NTE Auto-login: entering password";
            nxnow::AppendNteAutoLoginLog("TYPE password begin content=redacted");
            break;
        case NteAutoLoginStage::TypePassword:
            if (nte_text_index_ < nte_text_buffer_.size()) {
                SendKeyboardCharacter(nte_text_buffer_[nte_text_index_++]);
                nte_next_action_ = now + std::chrono::milliseconds(35);
            } else {
                std::fill(nte_text_buffer_.begin(), nte_text_buffer_.end(), '\0');
                nte_text_buffer_.clear();
                nxnow::AppendNteAutoLoginLog("TYPE password complete content=redacted");
                nte_stage_ = NteAutoLoginStage::SubmitLogin;
                nte_next_action_ = now + std::chrono::milliseconds(300);
            }
            break;
        case NteAutoLoginStage::SubmitLogin:
            nxnow::AppendNteAutoLoginLog("STAGE submit_login method=enter");
            SubmitNteFocusedField("password");
            nte_stage_ = NteAutoLoginStage::Idle;
            keyboard_release_guard_ = true;
            nxnow::SetSensitiveInputLoggingSuppressed(false);
            nte_status_ = "NTE Auto-login: sign-in submitted";
            nte_status_until_ = now + std::chrono::seconds(5);
            if (session_)
                session_->record_ui_event("nte_autologin submitted");
            nxnow::AppendNteAutoLoginLog("COMPLETE login_submitted");
            break;
        case NteAutoLoginStage::Idle:
            break;
    }
}

bool StreamView::HandleKeyboardText(const char* text) {
    if (!keyboard_visible_ || !session_)
        return false;
    const std::string updated = text ? text : "";
    if (!nxnow::input::IsLayoutTextSupported(updated, keyboard_layout_) ||
        !pending_keyboard_->AppendEdit(keyboard_text_, updated, keyboard_layout_))
    {
#ifdef __SWITCH__
        swkbdInlineSetInputText(&inline_keyboard_, keyboard_text_.c_str());
        std::vector<char32_t> previous;
        nxnow::input::DecodeRemoteText(keyboard_text_, keyboard_layout_, previous);
        swkbdInlineSetCursorPos(&inline_keyboard_, static_cast<s32>(previous.size()));
#endif
        brls::Application::notify(nxnow::Tr(nxnow::input::IsLayoutTextSupported(updated, keyboard_layout_)
            ? "Keyboard buffer full. Wait for the game to connect."
            : "Some characters are not supported by the remote keyboard"));
        return false;
    }
    std::fill(keyboard_text_.begin(), keyboard_text_.end(), '\0');
    keyboard_text_ = updated;
    return true;
}

void StreamView::OpenWowChatKeyboard(const brls::ControllerState& state)
{
    if (!is_wow_session_ || !session_)
        return;
    OpenInlineKeyboard(state);
    if (!keyboard_visible_)
        return;
    ReleaseRemoteMouseButton();
    keyboard_chat_mode_ = true;
    SendKeyboardCharacter('\n');
}

void StreamView::OpenInlineKeyboard(const brls::ControllerState& state) {
#ifdef __SWITCH__
    if (keyboard_visible_ || !session_)
        return;

    if (!keyboard_launched_) {
        Result rc = swkbdInlineCreate(&inline_keyboard_);
        if (R_FAILED(rc)) {
            if (session_)
                session_->record_ui_event("keyboard create failed rc=" + std::to_string(rc));
            brls::Application::notify(nxnow::Tr("Unable to create the on-screen keyboard"));
            return;
        }

        rc = swkbdInlineLaunchForLibraryApplet(
            &inline_keyboard_, static_cast<u8>(SwkbdInlineMode_AppletDisplay), 1);
        if (R_FAILED(rc)) {
            swkbdInlineClose(&inline_keyboard_);
            if (session_)
                session_->record_ui_event("keyboard launch failed rc=" + std::to_string(rc));
            brls::Application::notify(nxnow::Tr("Unable to launch the on-screen keyboard"));
            return;
        }

        keyboard_launched_ = true;
        active_keyboard_view_ = this;
        swkbdInlineSetChangedStringCallback(&inline_keyboard_, KeyboardChangedCallback);
        swkbdInlineSetDecidedEnterCallback(&inline_keyboard_, KeyboardEnterCallback);
        swkbdInlineSetDecidedCancelCallback(&inline_keyboard_, KeyboardCancelCallback);
        swkbdInlineSetAlphaEnabledInInputMode(&inline_keyboard_, true);
        swkbdInlineSetKeytopBgAlpha(&inline_keyboard_, 0.72f);
        swkbdInlineSetFooterBgAlpha(&inline_keyboard_, 0.62f);
        swkbdInlineSetKeytopAsFloating(&inline_keyboard_, true);
        swkbdInlineSetFooterScalable(&inline_keyboard_, true);
        swkbdInlineSetTouchFlag(&inline_keyboard_, true);
        swkbdInlineSetDirectionalButtonAssignFlag(&inline_keyboard_, true);
    }

    keyboard_text_.clear();
    swkbdInlineSetInputText(&inline_keyboard_, "");
    swkbdInlineSetCursorPos(&inline_keyboard_, 0);
    SwkbdAppearArg appear {};
    swkbdInlineMakeAppearArg(&appear, SwkbdType_Latin);
    // Credentials must not be learned by the system keyboard dictionary.
    appear.dicFlag = 0;
    swkbdInlineAppearArgSetStringLenMax(&appear, 500);
    swkbdInlineAppearArgSetOkButtonText(&appear, nxnow::Tr("Enter").c_str());
    swkbdInlineAppear(&inline_keyboard_, &appear);
    keyboard_visible_ = true;
    keyboard_chat_mode_ = false;
    ReleaseRemoteMouseButton();
    ReleasePhysicalInputs();
    (void)state;
    virtual_keyboard_capture_.Begin(std::chrono::steady_clock::now());
    keyboard_neutralization_.Reset();
    SetKeyboardUiBlocked(true);
    nxnow::SetSensitiveInputLoggingSuppressed(true);
    UpdateGameplayInputCapture(true, std::chrono::steady_clock::now());
    session_->record_ui_event("keyboard opened");
#else
    (void)state;
#endif
}

void StreamView::HideInlineKeyboard(bool send_enter) {
#ifdef __SWITCH__
    if (!keyboard_visible_)
        return;
    if (send_enter)
        SendKeyboardCharacter('\n');
    else if (keyboard_chat_mode_)
        SendKeyboardStroke(nxnow::input::MapKeyboardShortcut(nxnow::input::KeyboardShortcut::Escape));
    keyboard_chat_mode_ = false;
    swkbdInlineDisappear(&inline_keyboard_);
    keyboard_visible_ = false;
    virtual_keyboard_capture_.Quiet(std::chrono::steady_clock::now());
    std::fill(keyboard_text_.begin(), keyboard_text_.end(), '\0');
    keyboard_text_.clear();
    swkbdInlineSetInputText(&inline_keyboard_, "");
    swkbdInlineSetCursorPos(&inline_keyboard_, 0);
    nxnow::SetSensitiveInputLoggingSuppressed(nte_stage_ != NteAutoLoginStage::Idle);
    suppress_b_until_release_ = true;
    keyboard_release_guard_ = true;
    ReleasePhysicalInputs();
    if (session_)
        session_->record_ui_event(send_enter ? "keyboard submitted" : "keyboard closed");
#else
    (void)send_enter;
#endif
}

void StreamView::UpdateInlineKeyboard() {
#ifdef __SWITCH__
    if (!keyboard_launched_)
        return;
    if (keyboard_visible_ && (!session_ ||
        stream_end_reason_ != nxnow::StreamEndReason::None))
        HideInlineKeyboard(false);
    SwkbdState state = SwkbdState_Inactive;
    const Result rc = swkbdInlineUpdate(&inline_keyboard_, &state);
    if (R_FAILED(rc) && session_)
        session_->record_ui_event("keyboard update failed rc=" + std::to_string(rc));
#endif
}

#ifdef __SWITCH__
void StreamView::KeyboardChangedCallback(const char* text, SwkbdChangedStringArg*) {
    if (active_keyboard_view_)
        active_keyboard_view_->HandleKeyboardText(text);
}

void StreamView::KeyboardEnterCallback(const char* text, SwkbdDecidedEnterArg*) {
    if (!active_keyboard_view_ || !active_keyboard_view_->keyboard_visible_)
        return;
    if (active_keyboard_view_->HandleKeyboardText(text))
        active_keyboard_view_->HideInlineKeyboard(true);
}

void StreamView::KeyboardCancelCallback() {
    if (!active_keyboard_view_)
        return;
    active_keyboard_view_->HideInlineKeyboard(false);
}
#endif
