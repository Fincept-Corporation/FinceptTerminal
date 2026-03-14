#include "forgot_password.h"
#include "auth_manager.h"
#include "auth_ui.h"
#include "core/validators.h"
#include <imgui.h>
#include <cstring>

namespace fincept::auth {

using namespace ui_detail;

void ForgotPasswordScreen::reset() {
    step_ = EMAIL;
    std::memset(email_, 0, sizeof(email_));
    std::memset(otp_, 0, sizeof(otp_));
    std::memset(new_password_, 0, sizeof(new_password_));
    std::memset(confirm_password_, 0, sizeof(confirm_password_));
    loading_ = false;
    error_.clear();
    success_msg_.clear();
}

void ForgotPasswordScreen::render(AppScreen& next_screen) {
    auto& auth = AuthManager::instance();

    // Check async results
    if (auth.has_pending_result()) {
        auto result = auth.consume_result();
        loading_ = false;

        if (result.success) {
            if (step_ == EMAIL) {
                step_ = OTP_RESET;
                error_.clear();
            } else if (step_ == OTP_RESET) {
                step_ = SUCCESS;
                success_msg_ = "Password reset successfully. You can now sign in.";
            }
        } else {
            error_ = result.error;
        }
    }

    // Geometric background
    DrawAuthBackground();

    // Responsive centered panel
    ::fincept::ui::CenteredFrame centered("##forgot_password_panel", 480.0f, theme::colors::BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER);
    centered.begin();

    DrawPanelGlow();

    // ========== Success ==========
    if (step_ == SUCCESS) {
        BrandHeader("Password Reset Complete");
        ImGui::Spacing();

        // Success icon (centered checkmark)
        {
            float content_w = ImGui::GetContentRegionAvail().x;
            const char* check = "[OK]";
            float check_w = ImGui::CalcTextSize(check).x;
            ImGui::SetCursorPosX((content_w - check_w) / 2.0f);
            ImGui::TextColored(theme::colors::SUCCESS, "%s", check);
        }
        ImGui::Spacing();

        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%s", success_msg_.c_str());
        ImGui::Spacing(); ImGui::Spacing();

        if (theme::AccentButton("Go to Login", ImVec2(-1, 36))) {
            next_screen = AppScreen::Login;
            reset();
        }

        ImGui::Spacing();
        VersionTag();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        centered.end();
        return;
    }

    // ========== OTP + New Password ==========
    if (step_ == OTP_RESET) {
        BrandHeader("Reset Password");
        ImGui::Spacing();

        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Enter the code sent to");
        ImGui::TextColored(theme::colors::ACCENT, "%s", email_);
        ImGui::Spacing(); ImGui::Spacing();

        FieldLabel("VERIFICATION CODE");
        StyledInput("##reset_otp", otp_, sizeof(otp_));

        ImGui::Spacing();

        FieldLabel("NEW PASSWORD");
        StyledInput("##new_password", new_password_, sizeof(new_password_), ImGuiInputTextFlags_Password);

        ImGui::Spacing();

        FieldLabel("CONFIRM NEW PASSWORD");
        StyledInput("##confirm_new_password", confirm_password_, sizeof(confirm_password_), ImGuiInputTextFlags_Password);

        if (confirm_password_[0] != '\0' && std::strcmp(new_password_, confirm_password_) != 0) {
            ImGui::TextColored(theme::colors::ERROR_RED, "[X] Passwords do not match");
        }

        ImGui::Spacing();
        theme::ErrorMessage(error_.c_str());

        bool can_submit = otp_[0] != '\0' && new_password_[0] != '\0' &&
                          std::strcmp(new_password_, confirm_password_) == 0 &&
                          std::strlen(new_password_) >= 8 && !loading_;

        if (!can_submit) ImGui::BeginDisabled();
        if (theme::AccentButton("RESET PASSWORD", ImVec2(-1, 36))) {
            loading_ = true;
            error_.clear();
            std::string sanitized_email = utils::to_lower(utils::sanitize_input(email_));
            auth.reset_password_async(sanitized_email, utils::sanitize_input(otp_), new_password_);
        }
        if (!can_submit) ImGui::EndDisabled();

        if (loading_) {
            ImGui::Spacing();
            theme::LoadingSpinner("Resetting password...");
        }

        ImGui::Spacing();
        AccentSeparator();

        if (theme::SecondaryButton("<  Back to Login", ImVec2(-1, 30))) {
            next_screen = AppScreen::Login;
            reset();
        }

        ImGui::Spacing();
        VersionTag();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        centered.end();
        return;
    }

    // ========== Email Step ==========
    BrandHeader("Forgot Password");
    ImGui::Spacing();

    ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Enter your email to receive a reset code");
    ImGui::Spacing(); ImGui::Spacing();

    FieldLabel("EMAIL");
    bool enter_pressed = StyledInput("##forgot_email", email_, sizeof(email_), ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::Spacing();
    theme::ErrorMessage(error_.c_str());

    bool submit = enter_pressed || theme::AccentButton("SEND RESET CODE", ImVec2(-1, 36));
    if (submit && !loading_ && email_[0] != '\0') {
        auto email_result = utils::validate_email(email_);
        if (!email_result.valid) {
            error_ = email_result.error;
        } else {
            loading_ = true;
            error_.clear();
            auth.forgot_password_async(utils::to_lower(utils::sanitize_input(email_)));
        }
    }

    if (loading_) {
        ImGui::Spacing();
        theme::LoadingSpinner("Sending code...");
    }

    ImGui::Spacing();
    AccentSeparator();

    // Back to login
    {
        float content_w = ImGui::GetContentRegionAvail().x;
        const char* label = "Back to Login";
        float label_w = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPosX((content_w - label_w) / 2.0f);
        if (LinkButton(label)) {
            next_screen = AppScreen::Login;
            reset();
        }
    }

    ImGui::Spacing();
    VersionTag();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    centered.end();
}

} // namespace fincept::auth
