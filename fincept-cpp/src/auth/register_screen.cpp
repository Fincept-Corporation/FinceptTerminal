#include "register_screen.h"
#include "auth_manager.h"
#include "auth_ui.h"
#include "core/validators.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace fincept::auth {

using namespace ui_detail;

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

    // Geometric background
    DrawAuthBackground();

    // Responsive centered panel
    ::fincept::ui::CenteredFrame centered("##register_panel", 520.0f, theme::colors::BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER);
    centered.begin();

    DrawPanelGlow();

    // ========== OTP Verification Step ==========
    if (step_ == OTP_VERIFICATION) {
        BrandHeader("Verify Your Email");
        ImGui::Spacing();

        ImGui::TextColored(theme::colors::TEXT_SECONDARY, "We sent a verification code to");
        ImGui::TextColored(theme::colors::ACCENT, "%s", email_);
        ImGui::TextColored(theme::colors::TEXT_DIM, "Check your spam folder if you don't see it.");
        ImGui::Spacing(); ImGui::Spacing();

        FieldLabel("VERIFICATION CODE");
        bool enter_pressed = StyledInput("##otp", otp_, sizeof(otp_), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();
        theme::ErrorMessage(error_.c_str());

        bool submit = enter_pressed || theme::AccentButton("VERIFY", ImVec2(-1, 36));
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
        AccentSeparator();

        // Back + Resend side by side
        if (theme::SecondaryButton("<  Back", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 30))) {
            step_ = FORM;
            std::memset(otp_, 0, sizeof(otp_));
            error_.clear();
        }
        ImGui::SameLine(0, ImGui::GetContentRegionAvail().x * 0.1f);
        if (LinkButton("Resend Code") && !loading_) {
            loading_ = true;
            error_.clear();
            std::string username = utils::to_lower(utils::sanitize_input(std::string(first_name_) + std::string(last_name_)));
            std::string sanitized_email = utils::to_lower(utils::sanitize_input(email_));
            auth.signup_async(username, sanitized_email, password_, phone_, "", country_code_);
        }

        ImGui::Spacing();
        VersionTag();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        centered.end();
        return;
    }

    // ========== Registration Form ==========

    BrandHeader("Create Your Account");

    ImGui::Spacing();

    float half_width = (ImGui::GetContentRegionAvail().x - 8) / 2;

    // First + Last name (side by side)
    ImGui::BeginGroup();
    ImGui::PushItemWidth(half_width);
    FieldLabel("FIRST NAME");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::InputText("##first_name", first_name_, sizeof(first_name_));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0, 8);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(half_width);
    FieldLabel("LAST NAME");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::InputText("##last_name", last_name_, sizeof(last_name_));
    ImGui::PopStyleVar(2);
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
    FieldLabel("EMAIL");
    StyledInput("##reg_email", email_, sizeof(email_));

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
    FieldLabel("CODE");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::InputText("##country_code", country_code_, sizeof(country_code_));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SameLine(0, 8);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    FieldLabel("PHONE NUMBER");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::InputText("##phone", phone_, sizeof(phone_));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::Spacing();

    // Password
    FieldLabel("PASSWORD");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme::colors::BG_HOVER);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##reg_password", password_, sizeof(password_), ImGuiInputTextFlags_Password)) {
        pw_strength_ = utils::validate_password(password_);
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (password_[0] != '\0') {
        auto indicator = [](bool ok, const char* label) {
            ImGui::TextColored(ok ? theme::colors::SUCCESS : theme::colors::TEXT_DISABLED,
                "%s %s", ok ? "[OK]" : "[  ]", label);
        };
        indicator(pw_strength_.min_length, "8+ characters");
        ImGui::SameLine(0, 16);
        indicator(pw_strength_.has_upper, "Uppercase");
        ImGui::SameLine(0, 16);
        indicator(pw_strength_.has_number, "Number");
    }

    ImGui::Spacing();

    // Confirm password
    FieldLabel("CONFIRM PASSWORD");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::colors::BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme::colors::BG_HOVER);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##confirm_password", confirm_password_, sizeof(confirm_password_), ImGuiInputTextFlags_Password)) {
        passwords_match_ = (std::strcmp(password_, confirm_password_) == 0);
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (confirm_password_[0] != '\0' && !passwords_match_) {
        ImGui::TextColored(theme::colors::ERROR_RED, "[X] Passwords do not match");
    }

    ImGui::Spacing(); ImGui::Spacing();

    theme::ErrorMessage(error_.c_str());

    bool can_submit = first_name_[0] != '\0' && last_name_[0] != '\0' &&
                      email_[0] != '\0' && password_[0] != '\0' &&
                      confirm_password_[0] != '\0' && passwords_match_ &&
                      pw_strength_.is_valid() && !loading_;

    if (!can_submit) ImGui::BeginDisabled();
    bool submit = theme::AccentButton("CREATE ACCOUNT", ImVec2(-1, 38));
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
    AccentSeparator();

    // Login link
    {
        float content_w = ImGui::GetContentRegionAvail().x;
        const char* prefix = "Already have an account?  ";
        const char* link = "Sign In";
        float total_w = ImGui::CalcTextSize(prefix).x + ImGui::CalcTextSize(link).x;
        ImGui::SetCursorPosX((content_w - total_w) / 2.0f);
        ImGui::TextColored(theme::colors::TEXT_DIM, "%s", prefix);
        ImGui::SameLine(0, 0);
        if (LinkButton(link)) {
            next_screen = AppScreen::Login;
        }
    }

    ImGui::Spacing();
    VersionTag();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    centered.end();
}

} // namespace fincept::auth
