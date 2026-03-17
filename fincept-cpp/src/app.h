#pragma once
// Application — state machine routing between auth screens and dashboard
// Owns all screen instances as members (no static globals)
// Heavy screens use std::optional for lazy construction on first navigation.

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
#include "screens/backtesting/backtesting_screen.h"
#include "screens/algo_trading/algo_trading_screen.h"
#include "screens/ai_chat/ai_chat_screen.h"
#include "screens/ai_quant_lab/ai_quant_lab_screen.h"
#include "screens/data_mapping/data_mapping_screen.h"
#include "screens/screener/screener_screen.h"
#include "screens/equity_trading/equity_trading_screen.h"
#include "screens/report_builder/report_builder_screen.h"
#include "screens/derivatives/derivatives_screen.h"
#include "screens/excel/excel_screen.h"
#include "screens/polymarket/polymarket_screen.h"
#include "screens/akshare/akshare_screen.h"
#include "screens/asia_markets/asia_markets_screen.h"
#include "screens/relationship_map/relationship_map_screen.h"
#include "screens/alt_investments/alt_investments_screen.h"
#include "screens/maritime/maritime_screen.h"
#include "screens/mcp_servers/mcp_servers_screen.h"
#include "screens/alpha_arena/alpha_arena_screen.h"
#include "screens/trade_viz/trade_viz_screen.h"
#include <optional>
#include <chrono>

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
    std::chrono::steady_clock::time_point init_time_{};

    // Eagerly constructed screens (needed at startup)
    python::SetupScreen setup_screen_;
    auth::LoginScreen login_screen_;
    auth::RegisterScreen register_screen_;
    auth::ForgotPasswordScreen forgot_password_screen_;
    auth::PricingScreen pricing_screen_;
    dashboard::DashboardScreen dashboard_screen_;
    settings::SettingsScreen settings_screen_;

    // Lazily constructed screens — only allocated when first navigated to.
    // This avoids constructing ~35 screen objects at startup, reducing memory
    // pressure and constructor time.
    template<typename T> T& lazy(std::optional<T>& opt) {
        if (!opt) opt.emplace();
        return *opt;
    }

    std::optional<profile::ProfileScreen> profile_screen_;
    std::optional<news::NewsScreen> news_screen_;
    std::optional<research::ResearchScreen> research_screen_;
    std::optional<markets::MarketsScreen> markets_screen_;
    std::optional<surface::SurfaceScreen> surface_screen_;
    std::optional<portfolio::PortfolioScreen> portfolio_screen_;
    std::optional<geopolitics::GeopoliticsScreen> geopolitics_screen_;
    std::optional<crypto::CryptoTradingScreen> crypto_trading_screen_;
    std::optional<node_editor::NodeEditorScreen> node_editor_screen_;
    std::optional<agent_studio::AgentStudioScreen> agent_studio_screen_;
    std::optional<watchlist::WatchlistScreen> watchlist_screen_;
    std::optional<economics::EconomicsScreen> economics_screen_;
    std::optional<dbnomics::DBnomicsScreen> dbnomics_screen_;
    std::optional<quantlib::QuantLibScreen> quantlib_screen_;
    std::optional<notes::NotesScreen> notes_screen_;
    std::optional<data_sources::DataSourcesScreen> data_sources_screen_;
    std::optional<mna::MNAScreen> mna_screen_;
    std::optional<forum::ForumScreen> forum_screen_;
    std::optional<docs::DocsScreen> docs_screen_;
    std::optional<support::SupportScreen> support_screen_;
    std::optional<about::AboutScreen> about_screen_;
    std::optional<code_editor::CodeEditorScreen> code_editor_screen_;
    std::optional<gov_data::GovDataScreen> gov_data_screen_;
    std::optional<backtesting::BacktestingScreen> backtesting_screen_;
    std::optional<algo::AlgoTradingScreen> algo_trading_screen_;
    std::optional<ai_chat::AIChatScreen> ai_chat_screen_;
    std::optional<ai_quant_lab::AIQuantLabScreen> ai_quant_lab_screen_;
    std::optional<data_mapping::DataMappingScreen> data_mapping_screen_;
    std::optional<screener::ScreenerScreen> screener_screen_;
    std::optional<equity_trading::EquityTradingScreen> equity_trading_screen_;
    std::optional<report_builder::ReportBuilderScreen> report_builder_screen_;
    std::optional<derivatives::DerivativesScreen> derivatives_screen_;
    std::optional<excel::ExcelScreen> excel_screen_;
    std::optional<polymarket::PolymarketScreen> polymarket_screen_;
    std::optional<akshare::AkShareScreen> akshare_screen_;
    std::optional<asia_markets::AsiaMarketsScreen> asia_markets_screen_;
    std::optional<relationship_map::RelationshipMapScreen> relationship_map_screen_;
    std::optional<alt_investments::AltInvestmentsScreen> alt_investments_screen_;
    std::optional<maritime::MaritimeScreen> maritime_screen_;
    std::optional<mcp_servers::MCPServersScreen> mcp_servers_screen_;
    std::optional<alpha_arena::AlphaArenaScreen> alpha_arena_screen_;
    std::optional<trade_viz::TradeVizScreen> trade_viz_screen_;

    // Command bar state
    char cmd_buf_[128] = {};
    bool cmd_focused_ = false;
    int cmd_selected_ = 0;

    // Rendering sections
    void render_top_bar();
    void render_tab_bar();
    void render_command_bar();
    void render_footer();
    void render_loading();

    // Navigation logic
    void apply_auto_navigation();
    void handle_keyboard_shortcuts();
};

} // namespace fincept
