#include "qr_login_dialog.hpp"

#include "app_state.hpp"
#include "localization.hpp"
#include "qrcodegen.h"
#include "ui_theme.hpp"
#include "ui_loading_view.hpp"
#include <cmath>

#include <algorithm>
#include <utility>

namespace nxnow
{

QrLoginDialog::QrLoginDialog(
    const LoginProvider& provider,
    const GfnClient& client,
    std::function<void()> on_success)
    : brls::Box(brls::Axis::COLUMN),
      provider_(provider),
      client_(client),
      on_success_(std::move(on_success))
{
    setId("login");
    setPadding(28, 48, 24, 48);
    setBackgroundColor(ui::Ground());

    auto* header = new brls::Box(brls::Axis::ROW);
    header->setId("login/header");
    header->setHeight(80); header->setShrink(0);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setMarginBottom(20);
    auto* logo = new brls::Image();
    logo->setId("login/logo");
    logo->setWidth(80); logo->setHeight(80); logo->setShrink(0);
    logo->setScalingType(brls::ImageScalingType::FIT);
    logo->setImageFromRes("img/nxnow-logo-mark-small.png");
    logo->setMarginRight(20); header->addView(logo);
    auto* heading = ui::MakeLabel(Tr("Sign in with QR code"), 32, ui::Text(), ui::FontRole::Heading);
    heading->setSingleLine(true); heading->setGrow(1); heading->setShrink(1);
    header->addView(heading);
    auto* provider_badge = new brls::Box(brls::Axis::ROW);
    provider_badge->setWidth(140); provider_badge->setHeight(44); provider_badge->setShrink(0);
    provider_badge->setBackgroundColor(ui::Raised()); provider_badge->setCornerRadius(22);
    provider_badge->setBorderThickness(1); provider_badge->setBorderColor(ui::Rule());
    provider_badge->setAlignItems(brls::AlignItems::CENTER);
    provider_badge->setJustifyContent(brls::JustifyContent::CENTER);
    const auto provider_name = provider_.display_name.empty() ? "NVIDIA" : provider_.display_name;
    auto* provider_label = ui::MakeLabel(provider_name, 16, ui::Accent(), ui::FontRole::Heading);
    provider_label->setSingleLine(true); provider_label->setMaxWidth(120);
    provider_badge->addView(provider_label); header->addView(provider_badge);
    addView(header);

    auto* content = new brls::Box(brls::Axis::ROW);
    content->setId("login/content"); content->setGrow(1); content->setMinHeight(0);
    auto* qr_card = new brls::Box(brls::Axis::COLUMN);
    qr_card->setId("login/qr-card");
    qr_card->setWidth(432); qr_card->setShrink(0);
    qr_card->setPadding(20); qr_card->setCornerRadius(20);
    qr_card->setBackgroundColor(ui::Raised());
    qr_card->setBorderThickness(1); qr_card->setBorderColor(ui::Rule());
    qr_card->setAlignItems(brls::AlignItems::CENTER);
    qr_card->setJustifyContent(brls::JustifyContent::CENTER);
    auto* service = ui::MakeLabel("GeForce NOW", 16, ui::Muted(), ui::FontRole::Medium);
    service->setMarginBottom(12); service->setShrink(0); qr_card->addView(service);
    qr_space_ = new brls::Box(brls::Axis::COLUMN);
    qr_space_->setId("login/qr");
    qr_space_->setWidth(344); qr_space_->setHeight(344); qr_space_->setShrink(0);
    qr_space_->setCornerRadius(16); qr_space_->setBackgroundColor(ui::Ground());
    qr_space_->setBorderThickness(1); qr_space_->setBorderColor(ui::Rule());
    qr_space_->setAlignItems(brls::AlignItems::CENTER);
    qr_space_->setJustifyContent(brls::JustifyContent::CENTER);
    qr_loading_dots_ = new ui::LoadingDotsView();
    qr_space_->addView(qr_loading_dots_); qr_card->addView(qr_space_);
    user_code_label_ = ui::MakeLabel({}, 22, ui::Accent(), ui::FontRole::Mono);
    user_code_label_->setId("login/code");
    user_code_label_->setHeight(34); user_code_label_->setShrink(0);
    user_code_label_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    user_code_label_->setSingleLine(true); user_code_label_->setMarginTop(10);
    qr_card->addView(user_code_label_); content->addView(qr_card);

    auto* details = new brls::Box(brls::Axis::COLUMN);
    details->setId("login/details"); details->setGrow(1); details->setShrink(1); details->setMinWidth(0);
    details->setMarginLeft(24); details->setPadding(32);
    details->setCornerRadius(20); details->setBackgroundColor(ui::Raised());
    details->setBorderThickness(1); details->setBorderColor(ui::Rule());
    status_label_ = ui::MakeLabel(Tr("Preparing secure QR login..."), 28, ui::Text(), ui::FontRole::Heading);
    status_label_->setId("login/status");
    status_label_->setHeight(84); status_label_->setShrink(0); status_label_->setSingleLine(false);
    status_label_->setMarginBottom(12); details->addView(status_label_);
    instruction_label_ = ui::MakeLabel(Tr("Scan the code with your phone, approve the provider sign-in, and return here."),
        20, ui::Muted());
    instruction_label_->setId("login/instructions"); instruction_label_->setSingleLine(false);
    auto* instructions = new brls::ScrollingFrame();
    instructions->setId("login/instruction-scroll");
    instructions->setGrow(1); instructions->setMinHeight(80); instructions->setShrink(1);
    instructions->setContentView(instruction_label_); details->addView(instructions);
    auto* phone_hint = ui::MakeLabel(Tr("Sign in securely using your phone."), 16, ui::Muted());
    phone_hint->setSingleLine(false); phone_hint->setShrink(0); phone_hint->setMarginTop(12);
    phone_hint->setMarginBottom(20); details->addView(phone_hint);
    auto* actions = new brls::Box(brls::Axis::ROW);
    actions->setId("login/actions"); actions->setHeight(56); actions->setShrink(0);
    retry_button_ = new ui::ActionRow(Tr("New QR code"), {},
        [this](brls::View* view) { return RetryLogin(view); }, ui::ActionTone::Primary);
    retry_button_->setId("login/retry");
    retry_button_->setGrow(1); retry_button_->setWidth(0); retry_button_->setMarginRight(12);
    actions->addView(retry_button_);
    cancel_button_ = new ui::ActionRow(Tr("Cancel login"), {},
        [this](brls::View* view) { return CancelLogin(view); }, ui::ActionTone::Outlined);
    cancel_button_->setId("login/cancel");
    cancel_button_->setGrow(1); cancel_button_->setWidth(0); actions->addView(cancel_button_);
    retry_button_->setCustomNavigationRoute(brls::FocusDirection::RIGHT, cancel_button_);
    cancel_button_->setCustomNavigationRoute(brls::FocusDirection::LEFT, retry_button_);
    retry_button_->setCustomNavigationRoute(brls::FocusDirection::UP, instructions);
    cancel_button_->setCustomNavigationRoute(brls::FocusDirection::UP, instructions);
    details->addView(actions); content->addView(details); addView(content);

    registerAction(Tr("Cancel login"), brls::BUTTON_B,
        [this](brls::View* view) { return CancelLogin(view); });
    auto* footer = new brls::Box(brls::Axis::ROW);
    footer->setHeight(44); footer->setShrink(0); footer->setMarginTop(18);
    footer->setAlignItems(brls::AlignItems::CENTER);
    auto* hints = new brls::Hints();
    hints->setAddUnableAButtonAction(false); hints->setAllowAButtonTouch(true);
    hints->applyXMLAttribute("forceShown", "true"); footer->addView(hints); addView(footer);
}

QrLoginDialog::~QrLoginDialog()
{
    lifetime_guard_->store(false);
    is_cancelled_.store(true);
    if (worker_.joinable())
        worker_.join();
}

void QrLoginDialog::willAppear(bool reset_state)
{
    brls::Box::willAppear(reset_state);
    if (!started_)
    {
        started_ = true;
        StartLogin();
    }
    if (retry_button_)
        brls::Application::giveFocus(retry_button_);
}

void QrLoginDialog::willDisappear(bool reset_state)
{
    brls::Box::willDisappear(reset_state);
    is_cancelled_.store(true);
}

bool QrLoginDialog::RetryLogin(brls::View* view)
{
    (void)view;
    if (login_active_.load())
        return true;
    StartLogin();
    return true;
}

bool QrLoginDialog::CancelLogin(brls::View* view)
{
    (void)view;
    if (!login_active_.load())
    {
        brls::Application::popActivity();
        return true;
    }
    is_cancelled_.store(true);
    status_label_->setText(Tr("Cancelling QR login..."));
    instruction_label_->setText(Tr("The current device code will be discarded."));
    return true;
}

void QrLoginDialog::StartLogin()
{
    if (login_active_.exchange(true))
        return;
    if (worker_.joinable())
        worker_.join();

    is_cancelled_.store(false);
    qr_loading_dots_->setVisibility(brls::Visibility::VISIBLE);
    status_label_->setText(Tr("Preparing secure QR login..."));
    instruction_label_->setText(
        Tr("Requesting a short-lived device code for ") +
        (provider_.display_name.empty() ? "GeForce NOW" : provider_.display_name) + ".");
    user_code_label_->setText(Tr(""));
    {
        std::lock_guard<std::mutex> lock(qr_mutex_);
        qr_data_.clear();
        qr_size_ = 0;
    }

    const auto guard = lifetime_guard_;
    worker_ = std::thread([this, guard]() {
        try
        {
            AuthSession session = client_.LoginWithQrCode(
                provider_,
                [this, guard](const QrLoginChallenge& challenge) {
                    if (!guard->load() || is_cancelled_.load())
                        return;
                    SetQrCode(challenge.verification_uri_complete);
                    brls::sync([this, guard, challenge]() {
                        if (!guard->load() || is_cancelled_.load())
                            return;
                        qr_loading_dots_->setVisibility(brls::Visibility::GONE);
                        status_label_->setText(Tr("Scan to sign in"));
                        instruction_label_->setText(Tr(
                            "Open your camera, scan the QR code, then approve the provider request."));
                        user_code_label_->setText(
                            Tr("Code: ") + challenge.user_code);
                    });
                },
                [this, guard]() {
                    return is_cancelled_.load() || !guard->load();
                });

            if (is_cancelled_.load() || !guard->load())
                return;
            brls::sync([this, guard, session = std::move(session)]() mutable {
                if (guard->load())
                    CompleteLogin(session);
            });
        }
        catch (const std::exception& error)
        {
            const std::string message = error.what();
            brls::sync([this, guard, message]() {
                if (!guard->load())
                    return;
                login_active_.store(false);
                qr_loading_dots_->setVisibility(brls::Visibility::GONE);
                status_label_->setText(
                    is_cancelled_.load() ? Tr("QR login cancelled") : Tr("QR login failed"));
                instruction_label_->setText(
                    is_cancelled_.load()
                        ? Tr("Choose New QR code when you are ready to try again.")
                        : message);
                user_code_label_->setText(Tr(""));
                std::lock_guard<std::mutex> lock(qr_mutex_);
                qr_data_.clear();
                qr_size_ = 0;
            });
        }
    });
}

void QrLoginDialog::CompleteLogin(const AuthSession& session)
{
    AuthSession active_session = session;
    active_session.persistence_enabled = true;
    client_.SaveSession(active_session);
    AppState::Instance().SetLibraryGames({});
    AppState::Instance().ActivateSession(active_session);
    login_active_.store(false);
    brls::Application::notify(Tr("Account connected"));
    const auto completion = on_success_;
    brls::Application::popActivity(brls::TransitionAnimation::FADE, [completion] {
        if (completion) completion();
    });
}

void QrLoginDialog::SetQrCode(const std::string& url)
{
    std::vector<uint8_t> temporary(qrcodegen_BUFFER_LEN_MAX);
    std::vector<uint8_t> encoded(qrcodegen_BUFFER_LEN_MAX);
    const bool success = qrcodegen_encodeText(
        url.c_str(), temporary.data(), encoded.data(), qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);

    std::lock_guard<std::mutex> lock(qr_mutex_);
    if (!success)
    {
        qr_data_.clear();
        qr_size_ = 0;
        return;
    }
    qr_size_ = qrcodegen_getSize(encoded.data());
    qr_data_ = std::move(encoded);
}

void QrLoginDialog::draw(
    NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx)
{
    brls::Box::draw(vg, x, y, width, height, style, ctx);

    std::lock_guard<std::mutex> lock(qr_mutex_);
    if (qr_size_ <= 0 || qr_data_.empty())
        return;

    // Place a crisp black/white QR inside its real layout box. Keep a white
    // border of four modules; the violet chrome stays outside the scan area.
    const auto frame = qr_space_->getFrame();
    constexpr int quiet_modules = 4;
    const int cell_size = static_cast<int>(std::floor(std::min(frame.getWidth(), frame.getHeight()) /
        (qr_size_ + quiet_modules * 2)));
    if (cell_size < 1) return;
    const float qr_pixels = qr_size_ * cell_size;
    const float quiet = quiet_modules * cell_size;
    const float start_x = std::floor(frame.getMinX() + (frame.getWidth() - qr_pixels) / 2);
    const float start_y = std::floor(frame.getMinY() + (frame.getHeight() - qr_pixels) / 2);
    nvgSave(vg);
    nvgShapeAntiAlias(vg, 0);

    nvgBeginPath(vg);
    nvgRect(vg, start_x - quiet, start_y - quiet, qr_pixels + quiet * 2, qr_pixels + quiet * 2);
    nvgFillColor(vg, nvgRGB(255, 255, 255));
    nvgFill(vg);

    nvgBeginPath(vg);
    for (int row = 0; row < qr_size_; ++row)
    {
        for (int column = 0; column < qr_size_; ++column)
        {
            if (qrcodegen_getModule(qr_data_.data(), column, row))
            {
                nvgRect(vg, start_x + column * cell_size,
                        start_y + row * cell_size, cell_size, cell_size);
            }
        }
    }
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
    nvgRestore(vg);
}

} // namespace nxnow
