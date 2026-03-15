#include "app.h"
#include "auth/auth_manager.h"
#include "storage/database.h"
#include "storage/cache_service.h"
#include "core/config.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "mcp/mcp_init.h"
#include "ui/theme.h"
#include <imgui.h>
#include <ctime>
#include <cstdio>

namespace fincept {

void App::initialize() {
    // Initialize databases before anything else
    db::Database::instance().initialize();
    db::CacheDatabase::instance().initialize();
    CacheService::instance().initialize();

    auth::AuthManager::instance().initialize();

    // Initialize MCP system (registers all internal tools, starts external servers)
    mcp::initialize_all_tools();

    // Subscribe to MCP navigation events
    core::EventBus::instance().subscribe("nav.switch_tab", [this](const core::Event& e) {
        active_tab_ = e.get<int>("tab_index");
    });

    initialized_ = true;

    if (setup_screen_.needs_setup()) {
        current_screen_ = AppScreen::PythonSetup;
    } else {
        current_screen_ = AppScreen::Login;
    }
    LOG_INFO("App", "Initialized, screen: %d", static_cast<int>(current_screen_));
}

void App::shutdown() {
    LOG_INFO("App", "Shutting down");
    mcp::shutdown_mcp();
    CacheService::instance().shutdown();
    db::CacheDatabase::instance().close();
    db::Database::instance().close();
}

// =============================================================================
// Keyboard shortcuts — extracted for clarity
// =============================================================================
void App::handle_keyboard_shortcuts() {
    if (ImGui::IsKeyPressed(ImGuiKey_F11)) toggle_fullscreen();

    if (current_screen_ != AppScreen::Dashboard) return;

    struct ShortcutBinding { ImGuiKey key; int tab; };
    static constexpr ShortcutBinding bindings[] = {
        {ImGuiKey_F1,  0},   // Dashboard
        {ImGuiKey_F2,  1},   // Markets
        {ImGuiKey_F3,  4},   // News
        {ImGuiKey_F4,  3},   // Portfolio
        {ImGuiKey_F5,  6},   // Backtesting
        {ImGuiKey_F6,  14},  // Surface Analytics
        {ImGuiKey_F9,  2},   // Crypto Trading
        {ImGuiKey_F10, 5},  // AI Chat
        {ImGuiKey_F7,  15},  // Geopolitics
        {ImGuiKey_F8,  16},  // Watchlist
        {ImGuiKey_F12, 13},  // Profile
    };

    for (const auto& b : bindings) {
        if (ImGui::IsKeyPressed(b.key)) {
            active_tab_ = b.tab;
            break;
        }
    }
}

// =============================================================================
// Top Menu Bar
// =============================================================================
void App::render_top_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, BG_DARKEST);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
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
            if (ImGui::MenuItem("Exit", "Alt+F4")) { request_exit(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Navigate")) {
            // ── Markets & Data ──
            ImGui::TextColored(ACCENT, "Markets & Data");
            ImGui::Separator();
            if (ImGui::MenuItem("Markets", "F2"))           { active_tab_ = 1; }
            if (ImGui::MenuItem("Watchlist", "F8"))         { active_tab_ = 16; }
            if (ImGui::MenuItem("Economics"))               { active_tab_ = 17; }
            if (ImGui::MenuItem("Govt Data"))               { active_tab_ = 26; }
            if (ImGui::MenuItem("DBnomics"))                { active_tab_ = 18; }
            if (ImGui::MenuItem("Data Sources"))            { active_tab_ = 20; }
            if (ImGui::MenuItem("Data Mapping"))             { active_tab_ = 28; }

            ImGui::Spacing();

            // ── Trading & Portfolio ──
            ImGui::TextColored(ACCENT, "Trading & Portfolio");
            ImGui::Separator();
            if (ImGui::MenuItem("Crypto Trading", "F9"))    { active_tab_ = 2; }
            if (ImGui::MenuItem("Portfolio", "F4"))         { active_tab_ = 3; }
            if (ImGui::MenuItem("Backtesting", "F5"))       { active_tab_ = 6; }
            if (ImGui::MenuItem("Algo Trading"))            { active_tab_ = 7; }
            if (ImGui::MenuItem("Surface Analytics", "F6")) { active_tab_ = 14; }

            ImGui::Spacing();

            // ── Research & Intelligence ──
            ImGui::TextColored(ACCENT, "Research & Intelligence");
            ImGui::Separator();
            if (ImGui::MenuItem("AI Chat", "F10"))          { active_tab_ = 5; }
            if (ImGui::MenuItem("AI Quant Lab"))            { active_tab_ = 10; }
            if (ImGui::MenuItem("M&A Analytics"))           { active_tab_ = 21; }
            if (ImGui::MenuItem("Geopolitics", "F7"))       { active_tab_ = 15; }

            ImGui::Spacing();

            // ── Tools ──
            ImGui::TextColored(ACCENT, "Tools");
            ImGui::Separator();
            if (ImGui::MenuItem("QuantLib"))                { active_tab_ = 11; }
            if (ImGui::MenuItem("Node Editor"))             { active_tab_ = 8; }
            if (ImGui::MenuItem("Code Editor"))             { active_tab_ = 9; }
            if (ImGui::MenuItem("Agent Studio"))            { active_tab_ = 27; }
            if (ImGui::MenuItem("Notes"))                   { active_tab_ = 19; }
            if (ImGui::MenuItem("Settings"))                { active_tab_ = 12; }
            if (ImGui::MenuItem("Profile", "F12"))          { active_tab_ = 13; }

            ImGui::Spacing();

            // ── Community & Support ──
            ImGui::TextColored(ACCENT, "Community & Support");
            ImGui::Separator();
            if (ImGui::MenuItem("Forum"))                   { active_tab_ = 22; }
            if (ImGui::MenuItem("Documentation"))           { active_tab_ = 23; }
            if (ImGui::MenuItem("Support"))                 { active_tab_ = 24; }
            if (ImGui::MenuItem("About"))                   { active_tab_ = 25; }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Fullscreen", "F11")) { toggle_fullscreen(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Documentation")) { active_tab_ = 23; }
            if (ImGui::MenuItem("Keyboard Shortcuts")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("About Fincept")) { active_tab_ = 25; }
            if (ImGui::MenuItem("Support")) { active_tab_ = 24; }
            if (ImGui::MenuItem("Forum")) { active_tab_ = 22; }
            ImGui::EndMenu();
        }

        // === CENTER: DateTime | Version | Brand ===
        {
            time_t now = time(nullptr);
            struct tm* t = localtime(&now);
            char datetime[32];
            std::strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", t);

            char ver_str[16];
            std::snprintf(ver_str, sizeof(ver_str), "v%s", config::APP_VERSION);
            const char* brand = "FINCEPT TERMINAL";

            float dt_w = ImGui::CalcTextSize(datetime).x;
            float pipe_w = ImGui::CalcTextSize("|").x;
            float sp = 6.0f;
            float sep_w = pipe_w + sp * 2;
            float ver_w = ImGui::CalcTextSize(ver_str).x;
            float brand_w = ImGui::CalcTextSize(brand).x;
            float center_total = dt_w + sep_w + ver_w + sep_w + brand_w;

            float window_w = ImGui::GetWindowWidth();
            float center_x = (window_w - center_total) * 0.5f;
            float cursor_x = ImGui::GetCursorPosX();
            if (center_x > cursor_x) ImGui::SameLine(center_x);
            else ImGui::SameLine();

            ImGui::TextColored(TEXT_DIM, "%s", datetime);
            ImGui::SameLine(0, sp);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, sp);
            ImGui::TextColored(SUCCESS, "%s", ver_str);
            ImGui::SameLine(0, sp);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, sp);
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
                username = session.user_info.username.empty()
                    ? session.user_info.email : session.user_info.username;
                plan = session.user_info.account_type;
                if (!plan.empty()) plan[0] = static_cast<char>(std::toupper(plan[0]));
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
            float logout_w = ImGui::CalcTextSize("LOGOUT").x +
                             ImGui::GetStyle().FramePadding.x * 2 + 20;
            float right_total = user_w + rsep_w +
                (cred_w > 0 ? cred_w + rsep_w : 0) + plan_w + rsep_w + logout_w + 16;

            ImGui::SameLine(ImGui::GetWindowWidth() - right_total);

            ImGui::TextColored(WARNING, "%s", username.c_str());
            ImGui::SameLine(0, rsp);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, rsp);

            if (credits_buf[0]) {
                float cbal = static_cast<float>(session.user_info.credit_balance);
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
                pricing_screen_.reset();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("View Plans & Pricing");
            ImGui::SameLine(0, rsp);
            ImGui::TextColored(BORDER_BRIGHT, "|");
            ImGui::SameLine(0, rsp);

            // LOGOUT button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.2f));
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
// Tab Navigation Bar
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
        // Primary tabs only — matches Tauri app's tab bar
        // All other tabs are accessible via Navigate dropdown menu
        struct TabDef { const char* label; const char* shortcut; int tab_idx; };
        static constexpr TabDef primary_tabs[] = {
            {"Dashboard",      "F1",  0},
            {"Markets",        "F2",  1},
            {"Crypto Trading", "F9",  2},
            {"Portfolio",      "F4",  3},
            {"News",           "F3",  4},
            {"AI Chat",        "F10", 5},
            {"Backtesting",    "F5",  6},
            {"Algo Trading",   nullptr, 7},
            {"Node Editor",    nullptr, 8},
            {"Code Editor",    nullptr, 9},
            {"AI Quant Lab",   nullptr, 10},
            {"QuantLib",       nullptr, 11},
            {"Settings",       nullptr, 12},
            {"Profile",        "F12", 13},
        };
        constexpr int n_tabs = sizeof(primary_tabs) / sizeof(primary_tabs[0]);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 3));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

        for (int i = 0; i < n_tabs; i++) {
            bool is_active = (primary_tabs[i].tab_idx == active_tab_);

            if (is_active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_BG);
                ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
                ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            }

            if (ImGui::Button(primary_tabs[i].label)) {
                active_tab_ = primary_tabs[i].tab_idx;
            }

            if (primary_tabs[i].shortcut && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s (%s)", primary_tabs[i].label, primary_tabs[i].shortcut);
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
// Footer / Status Bar
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

        std::string user = session.is_registered()
            ? (session.user_info.username.empty() ? session.user_info.email
                                                   : session.user_info.username)
            : "Guest";
        ImGui::TextColored(TEXT_SECONDARY, "%s", user.c_str());
    }

    ImGui::SameLine(0, 16);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 16);

    ImGui::TextColored(TEXT_DIM, "%s", config::APP_NAME);

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
// Loading Screen
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

// =============================================================================
// Auto Navigation
// =============================================================================
void App::apply_auto_navigation() {
    auto& auth = auth::AuthManager::instance();

    if (auth.is_loading() || auth.is_logging_out()) return;

    if (auth.is_authenticated()) {
        auto& session = auth.session();
        bool has_paid = session.has_paid_plan();

        if (session.is_registered() && has_paid) {
            if (current_screen_ != AppScreen::Dashboard &&
                current_screen_ != AppScreen::Pricing)
                current_screen_ = AppScreen::Dashboard;
        }
        else if (session.is_registered() && !has_paid &&
                 current_screen_ != AppScreen::Pricing &&
                 current_screen_ != AppScreen::Dashboard &&
                 (came_from_login_ || !has_chosen_free_)) {
            current_screen_ = AppScreen::Pricing;
        }
    } else {
        if (current_screen_ == AppScreen::Dashboard ||
            current_screen_ == AppScreen::Pricing) {
            current_screen_ = AppScreen::Login;
            has_chosen_free_ = false;
            came_from_login_ = false;
        }
    }
}

// =============================================================================
// Main Render
// =============================================================================
void App::render() {
    auto& auth = auth::AuthManager::instance();

    handle_keyboard_shortcuts();

    if (initialized_ && !auth.is_loading())
        apply_auto_navigation();

    // Loading state — no chrome
    if (!initialized_ || auth.is_loading() || auth.is_logging_out()) {
        render_loading();
        return;
    }

    // Only show app chrome (menu bar, tab bar, footer) on screens that need it
    bool show_chrome = screen_has_chrome(current_screen_);

    if (show_chrome) {
        render_top_bar();
        render_tab_bar();
    }

    next_screen_ = current_screen_;

    switch (current_screen_) {
        // Auth screens — no chrome, clean centered layout
        case AppScreen::PythonSetup:
            if (setup_screen_.render()) {
                next_screen_ = AppScreen::Login;
            }
            break;
        case AppScreen::Login:          login_screen_.render(next_screen_); break;
        case AppScreen::Register:       register_screen_.render(next_screen_); break;
        case AppScreen::ForgotPassword: forgot_password_screen_.render(next_screen_); break;

        // App screens — with chrome
        case AppScreen::Pricing:        pricing_screen_.render(next_screen_); break;
        case AppScreen::Dashboard:
            if (active_tab_ == 26)      gov_data_screen_.render();
            else if (active_tab_ == 6)  backtesting_screen_.render();
            else if (active_tab_ == 25) about_screen_.render();
            else if (active_tab_ == 24) support_screen_.render();
            else if (active_tab_ == 23) docs_screen_.render();
            else if (active_tab_ == 22) forum_screen_.render();
            else if (active_tab_ == 21) mna_screen_.render();
            else if (active_tab_ == 20) data_sources_screen_.render();
            else if (active_tab_ == 19) notes_screen_.render();
            else if (active_tab_ == 18) dbnomics_screen_.render();
            else if (active_tab_ == 17) economics_screen_.render();
            else if (active_tab_ == 16) watchlist_screen_.render();
            else if (active_tab_ == 15) geopolitics_screen_.render();
            else if (active_tab_ == 14) surface_screen_.render();
            else if (active_tab_ == 13) profile_screen_.render();
            else if (active_tab_ == 4)  news_screen_.render();
            else if (active_tab_ == 3)  portfolio_screen_.render();
            else if (active_tab_ == 9)  code_editor_screen_.render();
            else if (active_tab_ == 8)  node_editor_screen_.render();
            else if (active_tab_ == 12) settings_screen_.render();
            else if (active_tab_ == 5)  ai_chat_screen_.render();
            else if (active_tab_ == 27) agent_studio_screen_.render();
            else if (active_tab_ == 2)  crypto_trading_screen_.render();
            else if (active_tab_ == 10) ai_quant_lab_screen_.render();
            else if (active_tab_ == 11) quantlib_screen_.render();
            else if (active_tab_ == 1)  markets_screen_.render();
            else if (active_tab_ == 7)  algo_trading_screen_.render();
            else if (active_tab_ == 28) data_mapping_screen_.render();
            else                        dashboard_screen_.render();
            break;
        default:
            render_loading();
            break;
    }

    if (show_chrome) {
        render_footer();
    }

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
