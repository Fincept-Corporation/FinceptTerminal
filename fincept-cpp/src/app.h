#pragma once
// Application — state machine routing between auth screens and dashboard
// Owns all screen instances as members (no static globals)

#include "core/app_screen.h"
#include "auth/login_screen.h"
#include "auth/register_screen.h"
#include "auth/forgot_password.h"
#include "auth/pricing_screen.h"
#include "screens/dashboard/dashboard_screen.h"
#include "screens/profile/profile_screen.h"
#include "screens/news/news_screen.h"
#include "screens/research/research_screen.h"
#include "screens/markets/markets_screen.h"
#include "python/setup_screen.h"
#include "screens/surface_analytics/surface_screen.h"
#include "screens/portfolio/portfolio_screen.h"
#include "screens/geopolitics/geopolitics_screen.h"
#include "screens/crypto_trading/crypto_trading_screen.h"
#include "screens/node_editor/node_editor_screen.h"
#include "screens/settings/settings_screen.h"
#include "screens/agent_studio/agent_studio_screen.h"
#include "screens/watchlist/watchlist_screen.h"
#include "screens/economics/economics_screen.h"
#include "screens/dbnomics/dbnomics_screen.h"
#include "screens/quantlib/quantlib_screen.h"
#include "screens/notes/notes_screen.h"
#include "screens/data_sources/data_sources_screen.h"
#include "screens/mna/mna_screen.h"
#include "screens/forum/forum_screen.h"
#include "screens/docs/docs_screen.h"
#include "screens/support/support_screen.h"
#include "screens/about/about_screen.h"
#include "screens/code_editor/code_editor_screen.h"
#include "screens/gov_data/gov_data_screen.h"

// Fullscreen toggle (implemented in main.cpp, delegates to Window)
void toggle_fullscreen();
// Request application exit (implemented in main.cpp, delegates to Window)
void request_exit();

namespace fincept {

class App {
public:
    void initialize();
    void render();
    void shutdown();

private:
    // State
    AppScreen current_screen_ = AppScreen::Loading;
    AppScreen next_screen_ = AppScreen::Loading;
    bool initialized_ = false;
    bool came_from_login_ = false;
    bool has_chosen_free_ = false;
    int active_tab_ = 0;

    // Screen instances (owned by App, not static globals)
    python::SetupScreen setup_screen_;
    auth::LoginScreen login_screen_;
    auth::RegisterScreen register_screen_;
    auth::ForgotPasswordScreen forgot_password_screen_;
    auth::PricingScreen pricing_screen_;
    dashboard::DashboardScreen dashboard_screen_;
    profile::ProfileScreen profile_screen_;
    news::NewsScreen news_screen_;
    research::ResearchScreen research_screen_;
    markets::MarketsScreen markets_screen_;
    surface::SurfaceScreen surface_screen_;
    portfolio::PortfolioScreen portfolio_screen_;
    geopolitics::GeopoliticsScreen geopolitics_screen_;
    crypto::CryptoTradingScreen crypto_trading_screen_;
    node_editor::NodeEditorScreen node_editor_screen_;
    settings::SettingsScreen settings_screen_;
    agent_studio::AgentStudioScreen agent_studio_screen_;
    watchlist::WatchlistScreen watchlist_screen_;
    economics::EconomicsScreen economics_screen_;
    dbnomics::DBnomicsScreen dbnomics_screen_;
    quantlib::QuantLibScreen quantlib_screen_;
    notes::NotesScreen notes_screen_;
    data_sources::DataSourcesScreen data_sources_screen_;
    mna::MNAScreen mna_screen_;
    forum::ForumScreen forum_screen_;
    docs::DocsScreen docs_screen_;
    support::SupportScreen support_screen_;
    about::AboutScreen about_screen_;
    code_editor::CodeEditorScreen code_editor_screen_;
    gov_data::GovDataScreen gov_data_screen_;

    // Rendering sections
    void render_top_bar();
    void render_tab_bar();
    void render_footer();
    void render_loading();

    // Navigation logic
    void apply_auto_navigation();
    void handle_keyboard_shortcuts();
};

} // namespace fincept
