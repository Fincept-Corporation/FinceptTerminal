#include "status_bar.h"
#include "auth/auth_manager.h"
#include "ui/theme.h"
#include <imgui.h>
#include <ctime>
#include <cstdio>

namespace fincept::dashboard {

void StatusBar::render() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float bar_height = 24.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x,
        viewport->WorkPos.y + viewport->WorkSize.y - bar_height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, bar_height));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::colors::BG_DARKEST);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::colors::BORDER_DIM);

    ImGui::Begin("##StatusBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNavFocus);

    auto& auth = auth::AuthManager::instance();
    auto& session = auth.session();

    // Left: Connection status
    theme::StatusIndicator("API", true);

    ImGui::SameLine(0, 16);
    ImGui::TextColored(theme::colors::BORDER, "|");
    ImGui::SameLine(0, 16);

    // Credits
    ImGui::TextColored(theme::colors::TEXT_DIM, "Credits:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(theme::colors::MARKET_GREEN, "%.0f", session.user_info.credit_balance);

    ImGui::SameLine(0, 16);
    ImGui::TextColored(theme::colors::BORDER, "|");
    ImGui::SameLine(0, 16);

    // Session type
    ImGui::TextColored(theme::colors::TEXT_DIM, "Session:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(session.is_registered() ? theme::colors::SUCCESS : theme::colors::WARNING,
        "%s", session.is_registered() ? "Registered" : "Guest");

    // Right: Clock — cached per second, thread-safe
    static time_t cached_sb_time = 0;
    static char time_buf[32] = {};
    {
        time_t now = time(nullptr);
        if (now != cached_sb_time) {
            cached_sb_time = now;
            struct tm t_buf{};
#ifdef _WIN32
            localtime_s(&t_buf, &now);
#else
            localtime_r(&now, &t_buf);
#endif
            std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", t_buf.tm_hour, t_buf.tm_min, t_buf.tm_sec);
        }
    }

    float time_width = ImGui::CalcTextSize(time_buf).x + 16;
    ImGui::SameLine(ImGui::GetWindowWidth() - time_width);
    ImGui::TextColored(theme::colors::TEXT_SECONDARY, "%s", time_buf);

    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace fincept::dashboard
