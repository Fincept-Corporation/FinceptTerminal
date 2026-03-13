#include "account_info.h"
#include "auth/auth_manager.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>

namespace fincept::dashboard::widgets {

void AccountInfo::render() {
    if (ImGui::Begin("Account")) {
        auto& session = auth::AuthManager::instance().session();

        theme::SectionHeader("ACCOUNT INFO");
        ImGui::Spacing();

        auto info_row = [](const char* label, const char* value, ImVec4 color = theme::colors::TEXT_SECONDARY) {
            ImGui::TextColored(theme::colors::TEXT_DIM, "%s", label);
            ImGui::SameLine(120);
            ImGui::TextColored(color, "%s", value);
        };

        if (session.is_registered()) {
            info_row("Username", session.user_info.username.c_str(), theme::colors::TEXT_PRIMARY);
            info_row("Email", session.user_info.email.c_str());

            std::string at = session.user_info.account_type;
            for (auto& c : at) c = (char)std::toupper(c);
            info_row("Plan", at.c_str(), theme::colors::ACCENT);

            char credits[32];
            std::snprintf(credits, sizeof(credits), "%.0f", session.user_info.credit_balance);
            info_row("Credits", credits, theme::colors::MARKET_GREEN);

            info_row("Verified", session.user_info.is_verified ? "Yes" : "No",
                session.user_info.is_verified ? theme::colors::SUCCESS : theme::colors::WARNING);

            info_row("MFA", session.user_info.mfa_enabled ? "Enabled" : "Disabled",
                session.user_info.mfa_enabled ? theme::colors::SUCCESS : theme::colors::TEXT_DIM);

            if (!session.user_info.country.empty()) {
                info_row("Country", session.user_info.country.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (!session.user_info.created_at.empty()) {
                info_row("Member Since", session.user_info.created_at.substr(0, 10).c_str());
            }
            if (!session.user_info.last_login_at.empty()) {
                info_row("Last Login", session.user_info.last_login_at.substr(0, 10).c_str());
            }
        } else {
            info_row("Session", "Guest", theme::colors::WARNING);
            info_row("Credits", "Limited", theme::colors::TEXT_DIM);

            ImGui::Spacing(); ImGui::Spacing();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Sign up for full access");
        }
    }
    ImGui::End();
}

} // namespace fincept::dashboard::widgets
