#include "register_screen.h"
#include "auth_manager.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace fincept::auth {

void RegisterScreen::reset() {
    step_ = FORM;
    std::memset(first_name_, 0, sizeof(first_name_));
    std::memset(last_name_, 0, sizeof(last_name_));
    std::memset(email_, 0, sizeof(email_));
    std::memset(phone_, 0, sizeof(phone_));
    std::memset(password_, 0, sizeof(password_));
    std::memset(confirm_password_, 0, sizeof(confirm_password_));
    std::memset(otp_, 0, sizeof(otp_));
    std::strncpy(country_code_, "+1", sizeof(country_code_) - 1);
    loading_ = false;
    error_.clear();
    pw_strength_ = {};
    passwords_match_ = true;
}

void RegisterScreen::render(AppScreen& next_screen) {
    auto& auth = AuthManager::instance();

    // Check async results
    if (auth.has_pending_result()) {
        auto result = auth.consume_result();
        loading_ = false;

        if (result.success) {
            if (step_ == FORM) {
                step_ = OTP_VERIFICATION;
                error_.clear();
            } else if (step_ == OTP_VERIFICATION) {
                step_ = COMPLETE;
                // Navigate based on plan
                if (auth.session().has_paid_plan()) {
                    next_screen = AppScreen::Dashboard;
                } else {
                    next_screen = AppScreen::Pricing;
                }
                reset();
                return;
            }
        } else {
            error_ = result.error;
        }
    }

    // Center panel
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float panel_w = 460.0f;
    ImGui::SetNextWindowPos(ImVec2((display.x - panel_w) / 2, display.y * 0.1f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panel_w, 0), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::colors::BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER);

    ImGui::Begin("##register_panel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    // ========== OTP Verification Step ==========
    if (step_ == OTP_VERIFICATION) {
        // Back button
        if (theme::SecondaryButton("<  Back")) {
            step_ = FORM;
            std::memset(otp_, 0, sizeof(otp_));
            error_.clear();
        }
        ImGui::SameLine();
        ImGui::TextColored(theme::colors::TEXT_PRIMARY, "Verify Your Email");
        ImGui::Spacing();

        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "We sent a verification code to");
        ImGui::TextColored(theme::colors::TEXT_PRIMARY, "%s", email_);
        ImGui::TextColored(theme::colors::TEXT_DIM, "Check your spam folder if you don't see it.");
        ImGui::Spacing(); ImGui::Spacing();

        ImGui::PushItemWidth(-1);
        ImGui::TextColored(theme::colors::TEXT_DIM, "VERIFICATION CODE");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
        bool enter_pressed = ImGui::InputText("##otp", otp_, sizeof(otp_), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::Spacing();
        theme::ErrorMessage(error_.c_str());

        bool submit = enter_pressed || theme::AccentButton("VERIFY", ImVec2(-1, 32));
        if (submit && !loading_ && otp_[0] != '\0') {
            loading_ = true;
            error_.clear();
            std::string sanitized_email = utils::sanitize_input(email_);
            std::transform(sanitized_email.begin(), sanitized_email.end(), sanitized_email.begin(), ::tolower);
            auth.verify_otp_async(sanitized_email, utils::sanitize_input(otp_));
        }

        if (loading_) {
            ImGui::Spacing();
            theme::LoadingSpinner("Verifying...");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Resend OTP
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
        if (ImGui::Button("Resend Code") && !loading_) {
            loading_ = true;
            error_.clear();
            std::string username = utils::to_lower(utils::sanitize_input(std::string(first_name_) + std::string(last_name_)));
            std::string sanitized_email = utils::to_lower(utils::sanitize_input(email_));
            auth.signup_async(username, sanitized_email, password_, phone_, "", country_code_);
        }
        ImGui::PopStyleColor(4);

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        return;
    }

    // ========== Registration Form ==========

    // Back button + Header
    if (theme::SecondaryButton("<  Back")) {
        next_screen = AppScreen::Login;
    }
    ImGui::SameLine();
    ImGui::TextColored(theme::colors::TEXT_PRIMARY, "Create Account");
    ImGui::Spacing();
    ImGui::TextColored(theme::colors::TEXT_SECONDARY, "Join Fincept Terminal");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float half_width = (ImGui::GetContentRegionAvail().x - 8) / 2;

    // First + Last name (side by side)
    ImGui::BeginGroup();
    ImGui::PushItemWidth(half_width);
    ImGui::TextColored(theme::colors::TEXT_DIM, "FIRST NAME");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::InputText("##first_name", first_name_, sizeof(first_name_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0, 8);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(half_width);
    ImGui::TextColored(theme::colors::TEXT_DIM, "LAST NAME");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::InputText("##last_name", last_name_, sizeof(last_name_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    // Username validation
    std::string username = utils::to_lower(utils::sanitize_input(std::string(first_name_) + std::string(last_name_)));
    if (!username.empty()) {
        bool valid = username.size() >= 3 && username.size() <= 50;
        ImGui::TextColored(valid ? theme::colors::SUCCESS : theme::colors::WARNING,
            "%s Username: %s (%zu chars)", valid ? "[OK]" : "[!]", username.c_str(), username.size());
    }

    ImGui::Spacing();

    // Email
    ImGui::PushItemWidth(-1);
    ImGui::TextColored(theme::colors::TEXT_DIM, "EMAIL");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::InputText("##reg_email", email_, sizeof(email_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // Email validation
    if (email_[0] != '\0') {
        auto email_result = utils::validate_email(email_);
        if (!email_result.valid) {
            ImGui::TextColored(theme::colors::ERROR_RED, "[X] %s", email_result.error.c_str());
        }
    }

    ImGui::Spacing();

    // Phone (country code + number side by side)
    ImGui::BeginGroup();
    ImGui::PushItemWidth(60);
    ImGui::TextColored(theme::colors::TEXT_DIM, "CODE");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::InputText("##country_code", country_code_, sizeof(country_code_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0, 8);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::TextColored(theme::colors::TEXT_DIM, "PHONE NUMBER");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::InputText("##phone", phone_, sizeof(phone_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::Spacing();

    // Password
    ImGui::PushItemWidth(-1);
    ImGui::TextColored(theme::colors::TEXT_DIM, "PASSWORD");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    if (ImGui::InputText("##reg_password", password_, sizeof(password_), ImGuiInputTextFlags_Password)) {
        pw_strength_ = utils::validate_password(password_);
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // Password strength indicators
    if (password_[0] != '\0') {
        auto indicator = [](bool ok, const char* label) {
            ImGui::TextColored(ok ? theme::colors::SUCCESS : theme::colors::TEXT_DISABLED,
                "%s %s", ok ? "[OK]" : "[  ]", label);
        };
        indicator(pw_strength_.min_length, "8+ characters");
        indicator(pw_strength_.has_upper, "Uppercase letter");
        indicator(pw_strength_.has_lower, "Lowercase letter");
        indicator(pw_strength_.has_number, "Number");
        indicator(pw_strength_.has_special, "Special character");
    }

    ImGui::Spacing();

    // Confirm password
    ImGui::PushItemWidth(-1);
    ImGui::TextColored(theme::colors::TEXT_DIM, "CONFIRM PASSWORD");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    if (ImGui::InputText("##confirm_password", confirm_password_, sizeof(confirm_password_), ImGuiInputTextFlags_Password)) {
        passwords_match_ = (std::strcmp(password_, confirm_password_) == 0);
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    if (confirm_password_[0] != '\0' && !passwords_match_) {
        ImGui::TextColored(theme::colors::ERROR_RED, "[X] Passwords do not match");
    }

    ImGui::Spacing(); ImGui::Spacing();

    // Error
    theme::ErrorMessage(error_.c_str());

    // Submit
    bool can_submit = first_name_[0] != '\0' && last_name_[0] != '\0' &&
                      email_[0] != '\0' && password_[0] != '\0' &&
                      confirm_password_[0] != '\0' && passwords_match_ &&
                      pw_strength_.is_valid() && !loading_;

    if (!can_submit) ImGui::BeginDisabled();
    bool submit = theme::AccentButton("CREATE ACCOUNT", ImVec2(-1, 34));
    if (!can_submit) ImGui::EndDisabled();

    if (submit && can_submit) {
        loading_ = true;
        error_.clear();

        std::string uname = utils::to_lower(utils::sanitize_input(std::string(first_name_) + std::string(last_name_)));
        std::string sanitized_email = utils::to_lower(utils::sanitize_input(email_));

        if (uname.size() < 3 || uname.size() > 50) {
            error_ = "Username must be between 3 and 50 characters.";
            loading_ = false;
        } else {
            auth.signup_async(uname, sanitized_email, password_, phone_, "", country_code_);
        }
    }

    if (loading_) {
        ImGui::Spacing();
        theme::LoadingSpinner("Creating account...");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Login link
    ImGui::TextColored(theme::colors::TEXT_DIM, "Already have an account?");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::colors::INFO);
    if (ImGui::Button("Sign In")) {
        next_screen = AppScreen::Login;
    }
    ImGui::PopStyleColor(4);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace fincept::auth
