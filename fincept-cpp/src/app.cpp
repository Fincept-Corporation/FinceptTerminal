#include "app.h"
#include "auth/auth_manager.h"
#include "auth/login_screen.h"
#include "auth/register_screen.h"
#include "auth/forgot_password.h"
#include "auth/pricing_screen.h"
#include "dashboard/dashboard_screen.h"
#include "profile/profile_screen.h"
#include "news/news_screen.h"
#include "research/research_screen.h"
#include "markets/markets_screen.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>
#include <ctime>
#include <cstdio>

namespace fincept {

// Screen instances
static auth::LoginScreen s_login;
static auth::RegisterScreen s_register;
static auth::ForgotPasswordScreen s_forgot_password;
static auth::PricingScreen s_pricing;
static dashboard::DashboardScreen s_dashboard;
static profile::ProfileScreen s_profile;
static news::NewsScreen s_news;
static research::ResearchScreen s_research;
static markets::MarketsScreen s_markets;

void App::initialize() {
    auth::AuthManager::instance().initialize();
    initialized_ = true;
    current_screen_ = AppScreen::Login;
}

void App::shutdown() {}

// =============================================================================
// Top Menu Bar — Bloomberg style (#0A0A0A bg, orange accents)
// Layout: [File|Navigate|View|Help]  ...  [DateTime | v3.3.1 | FINCEPT]  ...  [User | Credits | Plan | LOGOUT]
// =============================================================================
void App::render_top_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, BG_DARKEST);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

    if (ImGui::BeginMainMenuBar()) {

        // === LEFT: Dropdown Menus ===
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Workspace")) {}
            if (ImGui::MenuItem("Save Workspace")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Export Data")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Navigate")) {
            if (ImGui::MenuItem("Dashboard", "F1")) {}
            if (ImGui::MenuItem("Markets", "F2")) {}
            if (ImGui::MenuItem("News", "F3")) {}
            if (ImGui::MenuItem("Portfolio", "F4")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Crypto Trading", "F9")) {}
            if (ImGui::MenuItem("Algo Trading")) {}
            if (ImGui::MenuItem("Backtesting", "F5")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Settings")) {}
            if (ImGui::MenuItem("Profile", "F12")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Fullscreen", "F11")) { toggle_fullscreen(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Documentation")) {}
            if (ImGui::MenuItem("Keyboard Shortcuts")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("About Fincept")) {}
            if (ImGui::MenuItem("Support")) {}
            ImGui::EndMenu();
        }

        // === CENTER: DateTime | Version | Brand ===
        {
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);
            char datetime[32];
            std::strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", t);

            const char* ver = "v3.3.1";
            const char* brand = "FINCEPT TERMINAL";

            float dt_w = ImGui::CalcTextSize(datetime).x;
            float pipe_w = ImGui::CalcTextSize("|").x;
            float sp = 6.0f; // spacing on each side of pipe
            float sep_w = pipe_w + sp * 2; // total separator width
            float ver_w = ImGui::CalcTextSize(ver).x;
            float brand_w = ImGui::CalcTextSize(brand).x;
            float center_total = dt_w + sep_w + ver_w + sep_w + brand_w;

            float window_w = ImGui::GetWindowWidth();
            float center_x = (window_w - center_total) * 0.5f;
            float cursor_x = ImGui::GetCursorPosX();
            if (center_x > cursor_x) ImGui::SameLine(center_x);
            else ImGui::SameLine();

            ImGui::TextColored(TEXT_DIM, "%s", datetime);
            ImGui::SameLine(0, 6);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, 6);
            ImGui::TextColored(SUCCESS, "%s", ver);
            ImGui::SameLine(0, 6);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, 6);
            ImGui::TextColored(ACCENT, "%s", brand);
        }

        // === RIGHT: Session Info + Logout ===
        auto& auth = auth::AuthManager::instance();
        if (auth.is_authenticated()) {
            auto& session = auth.session();

            std::string username;
            std::string plan;
            char credits_buf[32] = "";

            if (session.is_registered()) {
                username = session.user_info.username.empty() ? session.user_info.email : session.user_info.username;
                plan = session.user_info.account_type;
                if (!plan.empty()) plan[0] = (char)std::toupper(plan[0]);
                plan += " Plan";
                double cbal = session.user_info.credit_balance;
                if (cbal >= 1000)
                    std::snprintf(credits_buf, sizeof(credits_buf), "%.0f Credits", cbal);
                else
                    std::snprintf(credits_buf, sizeof(credits_buf), "%.2f Credits", cbal);
            } else {
                username = "Guest";
                plan = "Free";
            }

            float user_w = ImGui::CalcTextSize(username.c_str()).x;
            float rpipe_w = ImGui::CalcTextSize("|").x;
            float rsp = 8.0f;
            float rsep_w = rpipe_w + rsp * 2;
            float cred_w = credits_buf[0] ? ImGui::CalcTextSize(credits_buf).x : 0;
            float plan_w = ImGui::CalcTextSize(plan.c_str()).x;
            float logout_w = ImGui::CalcTextSize("LOGOUT").x + ImGui::GetStyle().FramePadding.x * 2 + 20;
            float right_total = user_w + rsep_w + (cred_w > 0 ? cred_w + rsep_w : 0) + plan_w + rsep_w + logout_w + 16;

            ImGui::SameLine(ImGui::GetWindowWidth() - right_total);

            ImGui::TextColored(WARNING, "%s", username.c_str());
            ImGui::SameLine(0, rsp);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, rsp);

            if (credits_buf[0]) {
                float cbal = session.user_info.credit_balance;
                ImGui::TextColored(cbal > 0 ? MARKET_GREEN : MARKET_RED, "%s", credits_buf);
                ImGui::SameLine(0, rsp);
                ImGui::TextColored(BORDER_BRIGHT, "|");
                ImGui::SameLine(0, rsp);
            }

            // Clickable plan — navigates to Pricing screen
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 1));
            if (ImGui::SmallButton(plan.c_str())) {
                current_screen_ = AppScreen::Pricing;
                s_pricing.reset();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("View Plans & Pricing");
            ImGui::SameLine(0, rsp);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, rsp);

            // LOGOUT button — Bloomberg red style
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.2f));
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            ImGui::PushStyleColor(ImGuiCol_Border, ERROR_RED);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            if (ImGui::SmallButton("LOGOUT")) {
                auth.logout_async();
                current_screen_ = AppScreen::Login;
                came_from_login_ = false;
                has_chosen_free_ = false;
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
        }

        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleColor(5);
}

// =============================================================================
// Tab Navigation Bar — Bloomberg dark, sharp tabs with orange active indicator
// =============================================================================
void App::render_tab_bar() {
    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, tab_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::Begin("##tabbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (current_screen_ == AppScreen::Dashboard) {
        struct TabDef { const char* label; const char* shortcut; };
        TabDef tabs[] = {
            {"Dashboard", "F1"}, {"Markets", "F2"}, {"Crypto Trading", "F9"},
            {"Portfolio", "F4"}, {"News", "F3"}, {"AI Chat", "F10"},
            {"Backtesting", "F5"}, {"Algo Trading", nullptr}, {"Node Editor", nullptr},
            {"Code Editor", nullptr}, {"AI Quant Lab", nullptr}, {"QuantLib", nullptr},
            {"Settings", nullptr}, {"Profile", "F12"},
        };
        int n_tabs = sizeof(tabs) / sizeof(tabs[0]);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 3));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

        for (int i = 0; i < n_tabs; i++) {
            bool is_active = (i == active_tab_);

            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_BG);
                ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
                ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            }

            if (ImGui::Button(tabs[i].label)) {
                active_tab_ = i;
            }

            if (tabs[i].shortcut && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s (%s)", tabs[i].label, tabs[i].shortcut);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);

            if (i < n_tabs - 1) ImGui::SameLine();
        }

        ImGui::PopStyleVar(2);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// =============================================================================
// Footer / Status Bar — Bloomberg style: connection status, session, credits, time
// =============================================================================
void App::render_footer() {
    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float bar_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - bar_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARKEST);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::Begin("##footer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Left: Connection status
    ImGui::TextColored(MARKET_GREEN, "[*]");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "API: Connected");

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");

    // Session info
    auto& auth_mgr = auth::AuthManager::instance();
    if (auth_mgr.is_authenticated()) {
        auto& session = auth_mgr.session();
        ImGui::SameLine(0, 16);
        ImGui::TextColored(TEXT_DIM, "Session:");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(session.is_registered() ? SUCCESS : WARNING,
            "%s", session.is_registered() ? "Registered" : "Guest");

        ImGui::SameLine(0, 16);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 16);
        ImGui::TextColored(TEXT_DIM, "Credits:");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(session.user_info.credit_balance > 0 ? MARKET_GREEN : MARKET_RED,
            "%.2f", session.user_info.credit_balance);

        ImGui::SameLine(0, 16);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 16);

        // User
        std::string user = session.is_registered()
            ? (session.user_info.username.empty() ? session.user_info.email : session.user_info.username)
            : "Guest";
        ImGui::TextColored(TEXT_SECONDARY, "%s", user.c_str());
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);

    // Hot reload status
    ImGui::TextColored(TEXT_DIM, "C++ / ImGui");

    // Right: Timestamp
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tb[32];
    std::strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", t);

    float time_w = ImGui::CalcTextSize(tb).x + 12;
    ImGui::SameLine(ImGui::GetWindowWidth() - time_w);
    ImGui::TextColored(TEXT_SECONDARY, "%s", tb);

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// =============================================================================
void App::render_loading() {
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::colors::BG_DARKEST);

    ImGui::Begin("##loading", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDecoration);

    ImVec2 center(display.x / 2, display.y / 2);
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y - 30));
    ImGui::TextColored(theme::colors::ACCENT, "FINCEPT TERMINAL");
    ImGui::SetCursorPosX(center.x - 80);
    theme::LoadingSpinner("Initializing...");

    ImGui::End();
    ImGui::PopStyleColor();
}

void App::apply_auto_navigation() {
    auto& auth = auth::AuthManager::instance();

    if (auth.is_loading() || auth.is_logging_out()) return;

    if (auth.is_authenticated()) {
        auto& session = auth.session();
        bool has_paid = session.has_paid_plan();

        if (session.is_registered() && has_paid) {
            if (current_screen_ != AppScreen::Dashboard && current_screen_ != AppScreen::Pricing)
                current_screen_ = AppScreen::Dashboard;
        }
        else if (session.is_registered() && !has_paid &&
                 current_screen_ != AppScreen::Pricing && current_screen_ != AppScreen::Dashboard &&
                 (came_from_login_ || !has_chosen_free_)) {
            current_screen_ = AppScreen::Pricing;
        }
    } else {
        if (current_screen_ == AppScreen::Dashboard || current_screen_ == AppScreen::Pricing) {
            current_screen_ = AppScreen::Login;
            has_chosen_free_ = false;
            came_from_login_ = false;
        }
    }
}

void App::render() {
    auto& auth = auth::AuthManager::instance();

    // F11 fullscreen toggle
    if (ImGui::IsKeyPressed(ImGuiKey_F11)) toggle_fullscreen();

    // Tab navigation keyboard shortcuts (only when on Dashboard screen)
    // Tab indices: 0=Dashboard, 1=Markets, 2=Crypto Trading, 3=Portfolio,
    // 4=News, 5=AI Chat, 6=Backtesting, 7=Algo Trading, 8=Node Editor,
    // 9=Code Editor, 10=AI Quant Lab, 11=QuantLib, 12=Settings, 13=Profile
    if (current_screen_ == AppScreen::Dashboard) {
        if (ImGui::IsKeyPressed(ImGuiKey_F1))  active_tab_ = 0;   // Dashboard
        if (ImGui::IsKeyPressed(ImGuiKey_F2))  active_tab_ = 1;   // Markets
        if (ImGui::IsKeyPressed(ImGuiKey_F3))  active_tab_ = 4;   // News
        if (ImGui::IsKeyPressed(ImGuiKey_F4))  active_tab_ = 3;   // Portfolio
        if (ImGui::IsKeyPressed(ImGuiKey_F5))  active_tab_ = 6;   // Backtesting
        if (ImGui::IsKeyPressed(ImGuiKey_F9))  active_tab_ = 2;   // Crypto Trading
        if (ImGui::IsKeyPressed(ImGuiKey_F10)) active_tab_ = 5;   // AI Chat
        if (ImGui::IsKeyPressed(ImGuiKey_F12)) active_tab_ = 13;  // Profile
    }

    // Header
    render_top_bar();

    // Tab navigation
    render_tab_bar();

    // Auto-navigate
    if (initialized_ && !auth.is_loading())
        apply_auto_navigation();

    // Loading state
    if (!initialized_ || auth.is_loading() || auth.is_logging_out()) {
        render_loading();
        return;
    }

    next_screen_ = current_screen_;

    switch (current_screen_) {
        case AppScreen::Login:       s_login.render(next_screen_); break;
        case AppScreen::Register:    s_register.render(next_screen_); break;
        case AppScreen::ForgotPassword: s_forgot_password.render(next_screen_); break;
        case AppScreen::Pricing:     s_pricing.render(next_screen_); break;
        case AppScreen::Dashboard:
            // Tab indices: 0=Dashboard, 1=Markets, 2=Crypto Trading, 3=Portfolio,
            // 4=News, 5=AI Chat, 6=Backtesting, 7=Algo Trading, 8=Node Editor,
            // 9=Code Editor, 10=AI Quant Lab, 11=QuantLib, 12=Settings, 13=Profile
            if (active_tab_ == 13) s_profile.render();
            else if (active_tab_ == 4) s_news.render();
            else if (active_tab_ == 1) s_markets.render();
            else s_dashboard.render();
            break;
        default:                     render_loading(); break;
    }

    // Footer (always visible)
    render_footer();

    // Screen transitions
    if (next_screen_ != current_screen_) {
        if (current_screen_ == AppScreen::Login &&
            (next_screen_ == AppScreen::Dashboard || next_screen_ == AppScreen::Pricing))
            came_from_login_ = true;
        if (next_screen_ == AppScreen::Dashboard && current_screen_ == AppScreen::Pricing) {
            has_chosen_free_ = true;
            came_from_login_ = false;
        }
        current_screen_ = next_screen_;
    }
}

} // namespace fincept
