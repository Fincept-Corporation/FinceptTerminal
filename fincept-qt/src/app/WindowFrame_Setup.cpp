// src/app/WindowFrame_Setup.cpp
//
// Initial setup helpers — auth stack, docking mode and dock-screen
// registration. Called from the WindowFrame constructor in WindowFrame.cpp.
//
// Part of the partial-class split of WindowFrame.cpp.

#include "app/DockScreenRouter.h"
#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/lock/LockOverlayController.h"
#include "core/events/EventBus.h"
#include "core/keys/KeyConfigManager.h"
#include "core/keys/WindowCycler.h"
#include "core/layout/LayoutCatalog.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "screens/about/AboutScreen.h"
#include "screens/agent_config/AgentConfigScreen.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "screens/ai_quant_lab/AIQuantLabScreen.h"
#include "screens/akshare/AkShareScreen.h"
#include "screens/algo_trading/AlgoTradingScreen.h"
#include "screens/alpha_arena/AlphaArenaScreen.h"
#include "screens/alt_investments/AltInvestmentsScreen.h"
#include "screens/asia_markets/AsiaMarketsScreen.h"
#include "screens/auth/ForgotPasswordScreen.h"
#include "screens/auth/LockScreen.h"
#include "screens/auth/LoginScreen.h"
#include "screens/auth/PricingScreen.h"
#include "screens/auth/RegisterScreen.h"
#include "screens/backtesting/BacktestingScreen.h"
#include "screens/calendars/CalendarsScreen.h"
#include "screens/chat_mode/ChatModeScreen.h"
#include "screens/code_editor/CodeEditorScreen.h"
#include "screens/crypto_center/CryptoCenterScreen.h"
#include "screens/crypto_trading/CryptoTradingScreen.h"
#include "screens/dashboard/DashboardScreen.h"
#include "screens/data_mapping/DataMappingScreen.h"
#include "screens/data_sources/DataSourcesScreen.h"
#include "screens/dbnomics/DBnomicsScreen.h"
#include "screens/derivatives/DerivativesScreen.h"
#include "screens/docs/DocsScreen.h"
#include "screens/economics/EconomicsScreen.h"
#include "screens/equity_research/EquityResearchScreen.h"
#include "screens/equity_trading/EquityTradingScreen.h"
#include "screens/excel/ExcelScreen.h"
#include "screens/file_manager/FileManagerScreen.h"
#include "screens/fno/FnoScreen.h"
#include "screens/geopolitics/GeopoliticsScreen.h"
#include "screens/gov_data/GovDataScreen.h"
#include "screens/info/ContactScreen.h"
#include "screens/info/HelpScreen.h"
#include "screens/info/PrivacyScreen.h"
#include "screens/info/TermsScreen.h"
#include "screens/info/TrademarksScreen.h"
#include "screens/ma_analytics/MAAnalyticsScreen.h"
#include "screens/maritime/MaritimeScreen.h"
#include "screens/markets/MarketsScreen.h"
#include "screens/mcp_servers/McpServersScreen.h"
#include "screens/news/NewsScreen.h"
#include "screens/node_editor/NodeEditorScreen.h"
#include "screens/notes/NotesScreen.h"
#include "screens/polymarket/PolymarketScreen.h"
#include "screens/portfolio/PortfolioScreen.h"
#include "screens/profile/ProfileScreen.h"
#include "screens/quantlib/QuantLibScreen.h"
#include "screens/relationship_map/RelationshipMapScreen.h"
#include "screens/report_builder/ReportBuilderScreen.h"
#include "screens/screener/ScreenerScreen.h"
#include "screens/settings/SettingsScreen.h"
#include "screens/support/SupportScreen.h"
#include "screens/surface_analytics/SurfaceAnalyticsScreen.h"
#include "screens/trade_viz/TradeVizScreen.h"
#include "screens/watchlist/WatchlistScreen.h"
#include "ui/navigation/DockStatusBar.h"
#include "ui/navigation/DockToolBar.h"
#include "ui/navigation/FKeyBar.h"
#include "ui/pushpins/PushpinBar.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PythonSetupBanner.h"

#include <QStackedWidget>
#include <QVBoxLayout>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

namespace fincept {

void WindowFrame::setup_auth_screens() {
    // Every screen is built inside the stack it will live in (moving a finished
    // screen into a new parent re-computes the stylesheet of all its widgets).
    auto* login = new screens::LoginScreen(auth_stack_);
    auto* reg = new screens::RegisterScreen(auth_stack_);
    auto* forgot = new screens::ForgotPasswordScreen(auth_stack_);
    auto* pricing = new screens::PricingScreen(auth_stack_);

    // Info screens stack (shared between auth and app via master stack index 2)
    info_stack_ = new QStackedWidget(auth_stack_);
    auto* contact = new screens::ContactScreen(info_stack_);
    auto* terms = new screens::TermsScreen(info_stack_);
    auto* privacy = new screens::PrivacyScreen(info_stack_);
    auto* trademarks = new screens::TrademarksScreen(info_stack_);
    auto* help = new screens::HelpScreen(info_stack_);

    info_stack_->addWidget(contact);    // index 0
    info_stack_->addWidget(terms);      // index 1
    info_stack_->addWidget(privacy);    // index 2
    info_stack_->addWidget(trademarks); // index 3
    info_stack_->addWidget(help);       // index 4

    auth_stack_->addWidget(login);       // index 0
    auth_stack_->addWidget(reg);         // index 1
    auth_stack_->addWidget(forgot);      // index 2
    auth_stack_->addWidget(pricing);     // index 3
    auth_stack_->addWidget(info_stack_); // index 4

    // ── Auth navigation ──────────────────────────────────────────────────────
    connect(login, &screens::LoginScreen::navigate_register, this, &WindowFrame::show_register);
    connect(login, &screens::LoginScreen::navigate_forgot_password, this, &WindowFrame::show_forgot_password);
    // Sign-in refused as "not verified": the email code is entered on the
    // registration screen's code page, opened for that address.
    connect(login, &screens::LoginScreen::navigate_verify_email, this, [this, reg](const QString& email) {
        reg->show_otp_for(email);
        show_register();
    });
    connect(reg, &screens::RegisterScreen::navigate_login, this, &WindowFrame::show_login);
    connect(forgot, &screens::ForgotPasswordScreen::navigate_login, this, &WindowFrame::show_login);
    // PricingScreen::back_requested is wired in WindowFrame.cpp (leave_pricing).

    // ── Info screen navigation ───────────────────────────────────────────────
    // Back from info → return to previous auth screen (login by default)
    connect(contact, &screens::ContactScreen::navigate_back, this, &WindowFrame::show_login);
    connect(terms, &screens::TermsScreen::navigate_back, this, &WindowFrame::show_login);
    connect(terms, &screens::TermsScreen::navigate_privacy, this, &WindowFrame::show_info_privacy);
    connect(terms, &screens::TermsScreen::navigate_contact, this, &WindowFrame::show_info_contact);
    connect(privacy, &screens::PrivacyScreen::navigate_back, this, &WindowFrame::show_login);
    connect(privacy, &screens::PrivacyScreen::navigate_terms, this, &WindowFrame::show_info_terms);
    connect(privacy, &screens::PrivacyScreen::navigate_contact, this, &WindowFrame::show_info_contact);
    connect(trademarks, &screens::TrademarksScreen::navigate_back, this, &WindowFrame::show_login);
    connect(help, &screens::HelpScreen::navigate_back, this, &WindowFrame::show_login);
    connect(help, &screens::HelpScreen::navigate_register, this, &WindowFrame::show_register);
    connect(help, &screens::HelpScreen::navigate_forgot_password, this, &WindowFrame::show_forgot_password);
}

void WindowFrame::setup_docking_mode() {
    // ADS global config must be set before the first CDockManager is constructed.
    // Guard with a static flag since multiple WindowFrame instances share one process.
    static bool s_ads_configured = false;
    if (!s_ads_configured) {
        // Phase 5 polish: ADS native float deliberately disabled.
        //
        // Pre-Phase-5 dragging a panel out of a frame's bounds spawned an
        // ADS CFloatingDockContainer — owned by the source frame's manager,
        // not a real WindowFrame. That conflicted with the multi-window
        // refactor's "every panel lives in some frame" decision (2.2): a
        // floating panel was neither in its origin frame nor a separate
        // frame. Reaching into ADS to intercept the drop and reparent
        // across CDockManagers is feasible but requires hooking
        // CDockAreaTabBar mouse handlers (not exposed). The cleaner v1
        // path is to disable native float and route tear-off through the
        // existing right-click "Tear off into new window" / "Move to
        // window > N" menu (Phase 5 close-and-recreate path).
        //
        // Removed flags vs Phase-pre-5:
        //   - DoubleClickUndocksWidget (used to float-on-tab-double-click)
        // Per-widget removal: DockWidgetFloatable feature (in
        // DockScreenRouter::create_dock_widget) — see note there.
        ads::CDockManager::setConfigFlags(
            ads::CDockManager::DefaultOpaqueConfig | ads::CDockManager::AlwaysShowTabs |
            ads::CDockManager::DockAreaHasTabsMenuButton | ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility |
            ads::CDockManager::FloatingContainerHasWidgetTitle | ads::CDockManager::FloatingContainerHasWidgetIcon |
            ads::CDockManager::EqualSplitOnInsertion // prevents new panels from spawning tiny slivers
        );
        // Disable auto-hide entirely: the pin button converts panels to collapsible
        // sidebars which collapse when another panel is opened beside them.
        ads::CDockManager::setAutoHideConfigFlags(ads::CDockManager::AutoHideFlags(0));
        s_ads_configured = true;
    }

    // Create CDockManager — parent is a plain QWidget wrapper (not QMainWindow)
    // so ADS does NOT call setCentralWidget(). The wrapper goes into master_stack.
    auto* dock_wrapper = new QWidget;
    // Filled with the application palette's Window colour — the theme's base
    // background, which ThemeManager keeps current across theme switches.
    dock_wrapper->setAutoFillBackground(true);
    auto* dock_layout = new QVBoxLayout(dock_wrapper);
    dock_layout->setContentsMargins(0, 0, 0, 0);
    dock_layout->setSpacing(0);

    tab_bar_ = new ui::TabBar(dock_wrapper);
    dock_layout->addWidget(tab_bar_);

    // Tells the user why data is not loading when the essential Python
    // environment is missing (setup skipped or failed). Hides itself once ready.
    dock_layout->addWidget(new ui::PythonSetupBanner(dock_wrapper));

    dock_manager_ = new ads::CDockManager(dock_wrapper);
    // ADS loads its own default.css via setStyleSheet() in its constructor.
    // That CSS uses palette(window)/palette(highlight) — the Windows system
    // gray/blue — which overrides our global QSS. Replace it with our own
    // theme-matched ADS stylesheet so our colors apply consistently. Marked as
    // the manager's own sheet: an appearance change rebuilds it (its dock-tab
    // height follows the font size) instead of translating it.
    ui::ThemeManager::instance().apply_ads_qss(dock_manager_);
    dock_layout->addWidget(dock_manager_);

    // ── Layout mutation watchdog ──────────────────────────────────────────────
    // Any ADS-level structural change (dock area created, widget added/removed,
    // floating window created) persists within ~500 ms so a crash doesn't lose
    // the user's layout. Per-widget signals (topLevelChanged, closeRequested)
    // are wired in DockScreenRouter::create_dock_widget() so user-driven float
    // or close is also captured.
    connect(dock_manager_, &ads::CDockManager::dockAreaCreated, this,
            [this](ads::CDockAreaWidget*) { schedule_dock_layout_save(); });
    connect(dock_manager_, &ads::CDockManager::dockWidgetAdded, this, [this](ads::CDockWidget* dw) {
        schedule_dock_layout_save();
        // Per-widget close persistence: when a tab is closed via
        // the X button, ADS calls toggleView(false) which emits
        // viewToggled but no manager-level signal (dockWidgetRemoved
        // only fires on full removal, not hide). Without this
        // connection, closing a tab doesn't persist — the tab
        // reappears on next launch.
        connect(dw, &ads::CDockWidget::viewToggled, this, [this](bool) { schedule_dock_layout_save(); });
    });
    connect(dock_manager_, &ads::CDockManager::dockWidgetRemoved, this,
            [this](ads::CDockWidget*) { schedule_dock_layout_save(); });
    connect(dock_manager_, &ads::CDockManager::floatingWidgetCreated, this,
            [this](ads::CFloatingDockContainer*) { schedule_dock_layout_save(); });
}

void WindowFrame::setup_dock_screens() {
    // Each factory builds its screen inside the dock widget's content host
    // (`parent`) — see DockScreenRouter::ScreenFactory for why that matters.
    dock_router_->register_factory("dashboard", [](QWidget* parent) { return new screens::DashboardScreen(parent); });
    dock_router_->register_factory("markets", [](QWidget* parent) { return new screens::MarketsScreen(parent); });
    dock_router_->register_factory("crypto_trading", [](QWidget* parent) { return new screens::CryptoTradingScreen(parent); });
    dock_router_->register_factory("news", [](QWidget* parent) { return new screens::NewsScreen(parent); });
    dock_router_->register_factory("watchlist", [](QWidget* parent) { return new screens::WatchlistScreen(parent); });

    // Lazily constructed on first navigation — deferred to avoid startup cost.
    dock_router_->register_factory("report_builder", [](QWidget* parent) { return new screens::ReportBuilderScreen(parent); });
    dock_router_->register_factory("profile", [](QWidget* parent) { return new screens::ProfileScreen(parent); });
    dock_router_->register_factory("settings", [](QWidget* parent) { return new screens::SettingsScreen(parent); });
    dock_router_->register_factory("about", [](QWidget* parent) { return new screens::AboutScreen(parent); });
    dock_router_->register_factory("support", [](QWidget* parent) { return new screens::SupportScreen(parent); });
    dock_router_->register_factory("notes", [](QWidget* parent) { return new screens::NotesScreen(parent); });

    dock_router_->register_factory("portfolio", [](QWidget* parent) { return new screens::PortfolioScreen(parent); });
    dock_router_->register_factory("ai_chat", [](QWidget* parent) { return new screens::AiChatScreen(parent); });
    dock_router_->register_factory("backtesting", [](QWidget* parent) { return new screens::BacktestingScreen(parent); });
    dock_router_->register_factory("algo_trading", [](QWidget* parent) { return new screens::AlgoTradingScreen(parent); });
    dock_router_->register_factory("node_editor", [](QWidget* parent) { return new workflow::NodeEditorScreen(parent); });
    dock_router_->register_factory("code_editor", [](QWidget* parent) { return new screens::CodeEditorScreen(parent); });
    dock_router_->register_factory("ai_quant_lab", [](QWidget* parent) { return new screens::AIQuantLabScreen(parent); });
    dock_router_->register_factory("quantlib", [](QWidget* parent) { return new screens::QuantLibScreen(parent); });
    dock_router_->register_factory("economics", [](QWidget* parent) { return new screens::EconomicsScreen(parent); });
    dock_router_->register_factory("crypto_center", [](QWidget* parent) { return new screens::CryptoCenterScreen(parent); });
    dock_router_->register_factory("gov_data", [](QWidget* parent) { return new screens::GovDataScreen(parent); });
    dock_router_->register_factory("dbnomics", [](QWidget* parent) { return new screens::DBnomicsScreen(parent); });
    dock_router_->register_factory("akshare", [](QWidget* parent) { return new screens::AkShareScreen(parent); });
    dock_router_->register_factory("asia_markets", [](QWidget* parent) { return new screens::AsiaMarketsScreen(parent); });
    dock_router_->register_factory("relationship_map", [](QWidget* parent) { return new screens::RelationshipMapScreen(parent); });
    dock_router_->register_factory("equity_trading", [](QWidget* parent) { return new screens::EquityTradingScreen(parent); });
    dock_router_->register_factory("alpha_arena", [](QWidget* parent) { return new screens::AlphaArenaScreen(parent); });
    dock_router_->register_factory("polymarket", [](QWidget* parent) { return new screens::PolymarketScreen(parent); });
    dock_router_->register_factory("derivatives", [](QWidget* parent) { return new screens::DerivativesScreen(parent); });
    dock_router_->register_factory("fno", [](QWidget* parent) { return new screens::fno::FnoScreen(parent); });
    dock_router_->register_factory("equity_research", [](QWidget* parent) { return new screens::EquityResearchScreen(parent); });
    dock_router_->register_factory("screener", [](QWidget* parent) { return new screens::ScreenerScreen(parent); });
    dock_router_->register_factory("calendars", [](QWidget* parent) { return new screens::CalendarsScreen(parent); });
    dock_router_->register_factory("ma_analytics", [](QWidget* parent) { return new screens::MAAnalyticsScreen(parent); });
    dock_router_->register_factory("alt_investments", [](QWidget* parent) { return new screens::AltInvestmentsScreen(parent); });
    dock_router_->register_factory("geopolitics", [](QWidget* parent) { return new screens::GeopoliticsScreen(parent); });
    dock_router_->register_factory("maritime", [](QWidget* parent) { return new screens::MaritimeScreen(parent); });
    dock_router_->register_factory("surface_analytics", [](QWidget* parent) { return new fincept::surface::SurfaceAnalyticsScreen(parent); });
    // AGENT CONFIG holds the Agent Workbench too: "agent_workbench" is an alias
    // of it (app/ScreenAliases.h) that opens it on its RUNS section.
    dock_router_->register_factory("agent_config", [](QWidget* parent) { return new screens::AgentConfigScreen(parent); });
    dock_router_->register_factory("mcp_servers", [](QWidget* parent) { return new screens::McpServersScreen(parent); });
    dock_router_->register_factory("data_mapping", [](QWidget* parent) { return new screens::DataMappingScreen(parent); });
    dock_router_->register_factory("data_sources", [](QWidget* parent) { return new screens::DataSourcesScreen(parent); });
    dock_router_->register_factory("file_manager", [this](QWidget* parent) {
        auto* fm = new screens::FileManagerScreen(parent);
        connect(fm, &screens::FileManagerScreen::open_file_in_screen, this,
                [this](const QString& route_id, const QString& file_path) {
                    dock_router_->navigate(route_id);
                    // Notebooks open into the editor with the file loaded — hand
                    // the path to the live CodeEditorScreen once it is materialised
                    // (at once, or after a build of it already in flight).
                    if (route_id == "code_editor" && !file_path.isEmpty()) {
                        dock_router_->when_materialised(route_id, [file_path](QWidget* screen) {
                            if (auto* nb = qobject_cast<screens::CodeEditorScreen*>(screen))
                                nb->open_notebook_path(file_path);
                        });
                    }
                });
        return fm;
    });
    dock_router_->register_factory("excel", [](QWidget* parent) { return new screens::ExcelScreen(parent); });
    dock_router_->register_factory("trade_viz", [](QWidget* parent) { return new screens::TradeVizScreen(parent); });
    dock_router_->register_factory("docs", [](QWidget* parent) { return new screens::DocsScreen(parent); });

    // Info/legal pages. setup_auth_screens() above already builds one of each for
    // info_stack_, so an eager register_screen() here would create a SECOND instance
    // of all five per window, none of them visible at startup. The info screens now
    // build their page content on first show (e.g. HelpScreen::showEvent), so an
    // unshown copy is only a shell — but it is still a pointless one per window.
    // Factories construct the dock copy only when the user actually opens the panel;
    // the auth-stack copies stay the only eager instances.
    dock_router_->register_factory("contact", [](QWidget* parent) { return new screens::ContactScreen(parent); });
    dock_router_->register_factory("terms", [](QWidget* parent) { return new screens::TermsScreen(parent); });
    dock_router_->register_factory("privacy", [](QWidget* parent) { return new screens::PrivacyScreen(parent); });
    dock_router_->register_factory("trademarks", [](QWidget* parent) { return new screens::TrademarksScreen(parent); });
    dock_router_->register_factory("help", [](QWidget* parent) { return new screens::HelpScreen(parent); });
}

} // namespace fincept
