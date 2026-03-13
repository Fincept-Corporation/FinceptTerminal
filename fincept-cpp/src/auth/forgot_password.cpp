#include "forgot_password.h"
#include "auth_manager.h"
#include "theme/bloomberg_theme.h"
#include "utils/validators.h"
#include <imgui.h>
#include <cstring>

namespace fincept::auth {

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

    ImVec2 display = ImGui::GetIO().DisplaySize;
    float panel_w = 420.0f;
    ImGui::SetNextWindowPos(ImVec2((display.x - panel_w) / 2, (display.y - 350) / 2), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_w, 0), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::colors::BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER);

    ImGui::Begin("##forgot_password_panel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    // Back button
    if (theme::SecondaryButton("<  Back to Login")) {
        next_screen = AppScreen::Login;
        reset();
    }
    ImGui::Spacing();

    // ========== Success ==========
    if (step_ == SUCCESS) {
        ImGui::TextColored(theme::colors::SUCCESS, "Password Reset Complete");
        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%s", success_msg_.c_str());
        ImGui::Spacing(); ImGui::Spacing();
        if (theme::AccentButton("Go to Login", ImVec2(-1, 32))) {
            next_screen = AppScreen::Login;
            reset();
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return;
    }

    // ========== OTP + New Password ==========
    if (step_ == OTP_RESET) {
        ImGui::TextColored(theme::colors::TEXT_PRIMARY, "Reset Password");
        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Enter the code sent to %s", email_);
        ImGui::Spacing();

        ImGui::PushItemWidth(-1);

        ImGui::TextColored(theme::colors::TEXT_DIM, "VERIFICATION CODE");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
        ImGui::InputText("##reset_otp", otp_, sizeof(otp_));
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_DIM, "NEW PASSWORD");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
        ImGui::InputText("##new_password", new_password_, sizeof(new_password_), ImGuiInputTextFlags_Password);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_DIM, "CONFIRM NEW PASSWORD");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
        ImGui::InputText("##confirm_new_password", confirm_password_, sizeof(confirm_password_), ImGuiInputTextFlags_Password);
        ImGui::PopStyleColor();

        ImGui::PopItemWidth();

        if (confirm_password_[0] != '\0' && std::strcmp(new_password_, confirm_password_) != 0) {
            ImGui::TextColored(theme::colors::ERROR_RED, "[X] Passwords do not match");
        }

        ImGui::Spacing();
        theme::ErrorMessage(error_.c_str());

        bool can_submit = otp_[0] != '\0' && new_password_[0] != '\0' &&
                          std::strcmp(new_password_, confirm_password_) == 0 &&
                          std::strlen(new_password_) >= 8 && !loading_;

        if (!can_submit) ImGui::BeginDisabled();
        if (theme::AccentButton("RESET PASSWORD", ImVec2(-1, 32))) {
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

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return;
    }

    // ========== Email Step ==========
    ImGui::TextColored(theme::colors::TEXT_PRIMARY, "Forgot Password");
    ImGui::Spacing();
    ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Enter your email to receive a reset code");
    ImGui::Spacing(); ImGui::Spacing();

    ImGui::PushItemWidth(-1);
    ImGui::TextColored(theme::colors::TEXT_DIM, "EMAIL");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    bool enter_pressed = ImGui::InputText("##forgot_email", email_, sizeof(email_), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Spacing();
    theme::ErrorMessage(error_.c_str());

    bool submit = enter_pressed || theme::AccentButton("SEND RESET CODE", ImVec2(-1, 32));
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

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace fincept::auth
