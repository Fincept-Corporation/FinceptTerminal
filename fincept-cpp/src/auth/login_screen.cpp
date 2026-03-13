#include "login_screen.h"
#include "auth_manager.h"
#include "theme/bloomberg_theme.h"
#include "utils/validators.h"
#include <imgui.h>
#include <cstring>

namespace fincept::auth {

void LoginScreen::reset() {
    std::memset(email_, 0, sizeof(email_));
    std::memset(password_, 0, sizeof(password_));
    std::memset(mfa_code_, 0, sizeof(mfa_code_));
    show_mfa_ = false;
    show_force_login_ = false;
    loading_ = false;
    error_.clear();
    saved_email_.clear();
}

void LoginScreen::render(AppScreen& next_screen) {
    auto& auth = AuthManager::instance();

    // Check for async results
    if (auth.has_pending_result()) {
        auto result = auth.consume_result();
        loading_ = false;

        if (result.success) {
            if (auth.session().has_paid_plan()) {
                next_screen = AppScreen::Dashboard;
            } else {
                next_screen = AppScreen::Pricing;
            }
            reset();
            return;
        }

        if (result.mfa_required) {
            show_mfa_ = true;
            saved_email_ = email_;
            error_.clear();
        } else if (result.active_session) {
            show_force_login_ = true;
            saved_email_ = email_;
            error_ = result.error;
        } else {
            error_ = result.error;
        }
    }

    // Center the login panel
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float panel_w = 420.0f;
    float panel_h = show_mfa_ ? 320.0f : (show_force_login_ ? 340.0f : 400.0f);
    ImGui::SetNextWindowPos(ImVec2((display.x - panel_w) / 2, (display.y - panel_h) / 2), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_w, 0), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::colors::BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER);

    ImGui::Begin("##login_panel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);

    // ========== MFA Verification ==========
    if (show_mfa_) {
        ImGui::TextColored(theme::colors::TEXT_PRIMARY, "MFA Verification");
        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Enter the code from your authenticator app");
        ImGui::Spacing(); ImGui::Spacing();

        ImGui::PushItemWidth(-1);
        ImGui::TextColored(theme::colors::TEXT_DIM, "VERIFICATION CODE");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
        bool enter_pressed = ImGui::InputText("##mfa_code", mfa_code_, sizeof(mfa_code_), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::Spacing(); ImGui::Spacing();

        theme::ErrorMessage(error_.c_str());

        bool submit = enter_pressed || theme::AccentButton("VERIFY", ImVec2(-1, 32));
        if (submit && !loading_ && mfa_code_[0] != '\0') {
            loading_ = true;
            error_.clear();
            auth.verify_mfa_async(saved_email_, mfa_code_);
        }

        if (loading_) {
            ImGui::Spacing();
            theme::LoadingSpinner("Verifying...");
        }

        ImGui::Spacing();
        if (theme::SecondaryButton("<  Back to Login", ImVec2(-1, 28))) {
            show_mfa_ = false;
            std::memset(mfa_code_, 0, sizeof(mfa_code_));
            error_.clear();
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return;
    }

    // ========== Force Login Prompt ==========
    if (show_force_login_) {
        ImGui::TextColored(theme::colors::TEXT_PRIMARY, "Active Session Detected");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", error_.c_str());
        ImGui::Spacing(); ImGui::Spacing();

        if (theme::AccentButton("Force Login (End Other Session)", ImVec2(-1, 32)) && !loading_) {
            loading_ = true;
            error_.clear();
            show_force_login_ = false;
            auth.login_async(saved_email_, password_, true);
        }

        ImGui::Spacing();
        if (theme::SecondaryButton("Cancel", ImVec2(-1, 28))) {
            show_force_login_ = false;
            error_.clear();
        }

        if (loading_) {
            ImGui::Spacing();
            theme::LoadingSpinner("Logging in...");
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return;
    }

    // ========== Main Login Form ==========

    // Header
    ImGui::PushFont(nullptr); // Will use default (which should be JetBrains Mono)
    ImGui::TextColored(theme::colors::ACCENT, "FINCEPT TERMINAL");
    ImGui::PopFont();
    ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Sign in to your account");
    ImGui::Spacing(); ImGui::Spacing();

    ImGui::Separator();
    ImGui::Spacing();

    // Email field
    ImGui::PushItemWidth(-1);
    ImGui::TextColored(theme::colors::TEXT_DIM, "EMAIL");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::InputText("##email", email_, sizeof(email_));
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Password field
    ImGui::TextColored(theme::colors::TEXT_DIM, "PASSWORD");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    bool enter_pressed = ImGui::InputText("##password", password_, sizeof(password_),
        ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Spacing(); ImGui::Spacing();

    // Error message
    theme::ErrorMessage(error_.c_str());

    // Login button
    bool submit = enter_pressed || theme::AccentButton("SIGN IN", ImVec2(-1, 34));
    if (submit && !loading_ && email_[0] != '\0' && password_[0] != '\0') {
        loading_ = true;
        error_.clear();
        auth.login_async(email_, password_);
    }

    if (loading_) {
        ImGui::Spacing();
        theme::LoadingSpinner("Signing in...");
    }

    ImGui::Spacing(); ImGui::Spacing();

    // Forgot password link
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    if (ImGui::Button("Forgot Password?")) {
        next_screen = AppScreen::ForgotPassword;
    }
    ImGui::PopStyleColor(4);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Register link
    ImGui::TextColored(theme::colors::TEXT_DIM, "Don't have an account?");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    if (ImGui::Button("Sign Up")) {
        next_screen = AppScreen::Register;
    }
    ImGui::PopStyleColor(4);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace fincept::auth
