#include "login_screen.h"
#include "auth_manager.h"
#include "auth_ui.h"
#include "core/validators.h"
#include <imgui.h>
#include <cstring>

namespace fincept::auth {

using namespace ui_detail;

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

    // Geometric background
    DrawAuthBackground();

    // Centered login panel
    ::fincept::ui::CenteredFrame centered("##login_panel", 480.0f, theme::colors::BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER);
    centered.begin();

    DrawPanelGlow();

    // ========== MFA Verification ==========
    if (show_mfa_) {
        BrandHeader("Two-Factor Authentication");
        ImGui::Spacing();

        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Enter the code from your authenticator app");
        ImGui::Spacing(); ImGui::Spacing();

        FieldLabel("VERIFICATION CODE");
        bool enter_pressed = StyledInput("##mfa_code", mfa_code_, sizeof(mfa_code_), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing(); ImGui::Spacing();

        theme::ErrorMessage(error_.c_str());

        bool submit = enter_pressed || theme::AccentButton("VERIFY", ImVec2(-1, 36));
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
        AccentSeparator();
        if (theme::SecondaryButton("<  Back to Login", ImVec2(-1, 30))) {
            show_mfa_ = false;
            std::memset(mfa_code_, 0, sizeof(mfa_code_));
            error_.clear();
        }

        VersionTag();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        centered.end();
        return;
    }

    // ========== Force Login Prompt ==========
    if (show_force_login_) {
        BrandHeader("Active Session Detected");
        ImGui::Spacing();

        ImGui::TextWrapped("%s", error_.c_str());
        ImGui::Spacing(); ImGui::Spacing();

        if (theme::AccentButton("Force Login (End Other Session)", ImVec2(-1, 36)) && !loading_) {
            loading_ = true;
            error_.clear();
            show_force_login_ = false;
            auth.login_async(saved_email_, password_, true);
        }

        ImGui::Spacing();
        if (theme::SecondaryButton("Cancel", ImVec2(-1, 30))) {
            show_force_login_ = false;
            error_.clear();
        }

        if (loading_) {
            ImGui::Spacing();
            theme::LoadingSpinner("Logging in...");
        }

        VersionTag();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        centered.end();
        return;
    }

    // ========== Main Login Form ==========

    BrandHeader("Sign in to your account");

    ImGui::Spacing();

    // Email field
    FieldLabel("EMAIL");
    StyledInput("##email", email_, sizeof(email_));

    ImGui::Spacing();

    // Password field
    FieldLabel("PASSWORD");
    bool enter_pressed = StyledInput("##password", password_, sizeof(password_),
        ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::Spacing(); ImGui::Spacing();

    // Error message
    theme::ErrorMessage(error_.c_str());

    // Login button
    bool submit = enter_pressed || theme::AccentButton("SIGN IN", ImVec2(-1, 38));
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
    {
        float content_w = ImGui::GetContentRegionAvail().x;
        const char* label = "Forgot Password?";
        float label_w = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPosX((content_w - label_w) / 2.0f);
        if (LinkButton(label)) {
            next_screen = AppScreen::ForgotPassword;
        }
    }

    ImGui::Spacing();
    AccentSeparator();

    // Register link
    {
        float content_w = ImGui::GetContentRegionAvail().x;
        const char* prefix = "Don't have an account?  ";
        const char* link = "Sign Up";
        float total_w = ImGui::CalcTextSize(prefix).x + ImGui::CalcTextSize(link).x;
        ImGui::SetCursorPosX((content_w - total_w) / 2.0f);
        ImGui::TextColored(theme::colors::TEXT_DIM, "%s", prefix);
        ImGui::SameLine(0, 0);
        if (LinkButton(link)) {
            next_screen = AppScreen::Register;
        }
    }

    ImGui::Spacing();

    // Guest access
    {
        float content_w = ImGui::GetContentRegionAvail().x;
        const char* label = "Continue as Guest";
        float label_w = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPosX((content_w - label_w) / 2.0f);
        if (LinkButton(label, theme::colors::TEXT_DIM)) {
            auth.setup_guest_async();
            loading_ = true;
        }
    }

    ImGui::Spacing();
    VersionTag();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    centered.end();
}

} // namespace fincept::auth
