// src/app/WindowFrame_Setup.cpp
//
// Initial setup helpers — auth stack, docking mode, dock screens, and the
// (currently-stub) app/navigation hooks. Called from the WindowFrame
// constructor in WindowFrame.cpp.
//
// Part of the partial-class split of WindowFrame.cpp.

#include "app/WindowFrame.h"

#include "app/DockScreenRouter.h"
#include "app/TerminalShell.h"
#include "auth/PhaseOneAuthFlowBridge.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/lock/LockOverlayController.h"
#include "core/events/EventBus.h"
#include "core/keys/KeyConfigManager.h"
#include "core/keys/WindowCycler.h"
#include "core/layout/LayoutCatalog.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "core/config/AppConfig.h"
#include "multiuser/client/PhaseOneEndpointProvider.h"
#include "screens/common/ComingSoonScreen.h"
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
#include "screens/auth/PhaseOneBootstrapScreen.h"
#include "screens/auth/PricingScreen.h"
#include "screens/auth/RegisterScreen.h"
#include "screens/backtesting/BacktestingScreen.h"
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
#include "screens/forum/ForumScreen.h"
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

#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>

namespace fincept {

namespace {

QString phase_one_server_address_prompt(QWidget* parent, const QString& current_value) {
    bool accepted = false;
    const QString value = QInputDialog::getText(
        parent, QObject::tr("Phase-One Server Address"),
        QObject::tr("Enter the phase-one server host or http/https host:port"), QLineEdit::Normal, current_value,
        &accepted);
    if (!accepted)
        return {};
    return value;
}

} // namespace

void WindowFrame::setup_auth_screens() {
    auto* configure_server_action = new QAction(tr("Configure Server Address"), this);
    configure_server_action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    configure_server_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(configure_server_action, &QAction::triggered, this, &WindowFrame::configure_phase_one_server_address);
    addAction(configure_server_action);

    auto* fullscreen_action = new QAction(tr("Toggle Full Screen"), this);
    fullscreen_action->setShortcuts({QKeySequence(Qt::Key_F11), QKeySequence::FullScreen});
    fullscreen_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(fullscreen_action, &QAction::triggered, this, [this]() {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
    });
    addAction(fullscreen_action);

    auto* login = new screens::LoginScreen;
    auto* reg = new screens::RegisterScreen;
    auto* forgot = new screens::ForgotPasswordScreen;
    auto* bootstrap = new screens::PhaseOneBootstrapScreen;
    auto* pricing = new screens::PricingScreen;

    // Info screens stack (shared between auth and app via master stack index 2)
    info_stack_ = new QStackedWidget;
    auto* contact = new screens::ContactScreen;
    auto* terms = new screens::TermsScreen;
    auto* privacy = new screens::PrivacyScreen;
    auto* trademarks = new screens::TrademarksScreen;
    auto* help = new screens::HelpScreen(true);

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
    auth_stack_->addWidget(bootstrap);   // index 5

    // ── Auth navigation ──────────────────────────────────────────────────────
    connect(reg, &screens::RegisterScreen::navigate_login, this, &WindowFrame::show_login);
    connect(forgot, &screens::ForgotPasswordScreen::navigate_login, this, &WindowFrame::show_login);
    connect(bootstrap, &screens::PhaseOneBootstrapScreen::navigate_login, this, &WindowFrame::show_login);
    connect(pricing, &screens::PricingScreen::navigate_dashboard, this, [this]() {
        set_shell_visible(true);
        stack_->setCurrentIndex(1);
        dock_router_->navigate("dashboard");
    });

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
}

bool WindowFrame::ensure_phase_one_server_address_configured() {
    const QString env_value = QString::fromUtf8(qgetenv("FINCEPT_PHASE1_SERVER_URL")).trimmed();
    const auto initial = multiuser::PhaseOneEndpointProvider::instance().resolve();
    if (initial.ok)
        return true;

    if (!env_value.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Phase-One Server Address"), initial.message);
        return false;
    }

    const QString previous_value = AppConfig::instance().get_phase_one_server_url();
    QString current_value = previous_value;
    while (true) {
        const QString entered_value = phase_one_server_address_prompt(this, current_value);
        if (entered_value.isNull()) {
            if (previous_value.isEmpty())
                AppConfig::instance().remove_phase_one_server_url();
            else
                AppConfig::instance().set_phase_one_server_url(previous_value);
            return false;
        }

        AppConfig::instance().set_phase_one_server_url(entered_value);
        const auto resolved = multiuser::PhaseOneEndpointProvider::instance().resolve();
        if (resolved.ok)
            return true;

        current_value = entered_value.trimmed();
        QMessageBox::warning(this, tr("Invalid Phase-One Server Address"), resolved.message);
    }
}

void WindowFrame::configure_phase_one_server_address() {
    const QString env_value = QString::fromUtf8(qgetenv("FINCEPT_PHASE1_SERVER_URL")).trimmed();
    if (!env_value.isEmpty()) {
        QMessageBox::information(this, tr("Environment Override Active"),
                                 tr("`FINCEPT_PHASE1_SERVER_URL` is currently overriding the saved server address."));
    }

    if (ensure_phase_one_server_address_configured()) {
        const auto resolved = multiuser::PhaseOneEndpointProvider::instance().resolve();
        QMessageBox::information(this, tr("Phase-One Server Saved"), resolved.base_url);
    }
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
            ads::CDockManager::DefaultOpaqueConfig |
            ads::CDockManager::AlwaysShowTabs | ads::CDockManager::DockAreaHasTabsMenuButton |
            ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility |
            ads::CDockManager::FloatingContainerHasWidgetTitle | ads::CDockManager::FloatingContainerHasWidgetIcon |
            ads::CDockManager::EqualSplitOnInsertion       // prevents new panels from spawning tiny slivers
        );
        // Disable auto-hide entirely: the pin button converts panels to collapsible
        // sidebars which collapse when another panel is opened beside them.
        ads::CDockManager::setAutoHideConfigFlags(ads::CDockManager::AutoHideFlags(0));
        s_ads_configured = true;
    }

    // Create CDockManager — parent is a plain QWidget wrapper (not QMainWindow)
    // so ADS does NOT call setCentralWidget(). The wrapper goes into master_stack.
    auto* dock_wrapper = new QWidget;
    {
        QPalette pal = dock_wrapper->palette();
        pal.setColor(QPalette::Window, QColor(ui::colors::BG_BASE()));
        dock_wrapper->setPalette(pal);
        dock_wrapper->setAutoFillBackground(true);
    }
    auto* dock_layout = new QVBoxLayout(dock_wrapper);
    dock_layout->setContentsMargins(0, 0, 0, 0);
    dock_layout->setSpacing(0);

    tab_bar_ = new ui::TabBar(dock_wrapper);
    dock_layout->addWidget(tab_bar_);

    dock_manager_ = new ads::CDockManager(dock_wrapper);
    // ADS loads its own default.css via setStyleSheet() in its constructor.
    // That CSS uses palette(window)/palette(highlight) — the Windows system
    // gray/blue — which overrides our global QSS. Replace it with our own
    // theme-matched ADS stylesheet so our colors apply consistently.
    dock_manager_->setStyleSheet(ui::ThemeManager::instance().build_ads_qss());
    dock_layout->addWidget(dock_manager_);

    // ── Layout mutation watchdog ──────────────────────────────────────────────
    // Any ADS-level structural change (dock area created, widget added/removed,
    // floating window created) persists within ~500 ms so a crash doesn't lose
    // the user's layout. Per-widget signals (topLevelChanged, closeRequested)
    // are wired in DockScreenRouter::create_dock_widget() so user-driven float
    // or close is also captured.
    connect(dock_manager_, &ads::CDockManager::dockAreaCreated, this,
            [this](ads::CDockAreaWidget*) { schedule_dock_layout_save(); });
    connect(dock_manager_, &ads::CDockManager::dockWidgetAdded, this,
            [this](ads::CDockWidget*) { schedule_dock_layout_save(); });
    connect(dock_manager_, &ads::CDockManager::dockWidgetRemoved, this,
            [this](ads::CDockWidget*) { schedule_dock_layout_save(); });
    connect(dock_manager_, &ads::CDockManager::floatingWidgetCreated, this,
            [this](ads::CFloatingDockContainer*) { schedule_dock_layout_save(); });
}

void WindowFrame::setup_dock_screens() {
    dock_router_->register_factory("dashboard", []() { return new screens::DashboardScreen; });
    dock_router_->register_factory("markets", []() { return new screens::MarketsScreen; });
    dock_router_->register_factory("crypto_trading", []() { return new screens::CryptoTradingScreen; });
    dock_router_->register_factory("news", []() { return new screens::NewsScreen; });
    dock_router_->register_factory("forum", []() { return new screens::ForumScreen; });
    dock_router_->register_factory("watchlist", []() { return new screens::WatchlistScreen; });

    // Lazily constructed on first navigation — deferred to avoid startup cost.
    dock_router_->register_factory("report_builder", []() { return new screens::ReportBuilderScreen; });
    dock_router_->register_factory("profile", []() { return new screens::ProfileScreen; });
    dock_router_->register_factory("settings", []() { return new screens::SettingsScreen; });
    dock_router_->register_factory("about", []() { return new screens::AboutScreen; });
    dock_router_->register_factory("support", []() { return new screens::SupportScreen; });
    dock_router_->register_factory("notes", []() { return new screens::NotesScreen; });

    dock_router_->register_factory("portfolio", []() { return new screens::PortfolioScreen; });
    dock_router_->register_factory("ai_chat", []() { return new screens::AiChatScreen; });
    dock_router_->register_factory("backtesting", []() { return new screens::BacktestingScreen; });
    dock_router_->register_factory("algo_trading", []() { return new screens::AlgoTradingScreen; });
    dock_router_->register_factory("node_editor", []() { return new workflow::NodeEditorScreen; });
    dock_router_->register_factory("code_editor", []() { return new screens::CodeEditorScreen; });
    dock_router_->register_factory("ai_quant_lab", []() { return new screens::AIQuantLabScreen; });
    dock_router_->register_factory("quantlib", []() { return new screens::QuantLibScreen; });
    dock_router_->register_factory("economics", []() { return new screens::EconomicsScreen; });
    dock_router_->register_factory("crypto_center", []() { return new screens::CryptoCenterScreen; });
    dock_router_->register_factory("gov_data", []() { return new screens::GovDataScreen; });
    dock_router_->register_factory("dbnomics", []() { return new screens::DBnomicsScreen; });
    dock_router_->register_factory("akshare", []() { return new screens::AkShareScreen; });
    dock_router_->register_factory("asia_markets", []() { return new screens::AsiaMarketsScreen; });
    dock_router_->register_factory("relationship_map", []() { return new screens::RelationshipMapScreen; });
    dock_router_->register_factory("equity_trading", []() { return new screens::EquityTradingScreen; });
    dock_router_->register_factory("alpha_arena", []() { return new screens::AlphaArenaScreen; });
    dock_router_->register_factory("polymarket", []() { return new screens::PolymarketScreen; });
    dock_router_->register_factory("derivatives", []() { return new screens::DerivativesScreen; });
    dock_router_->register_factory("fno", []() { return new screens::fno::FnoScreen; });
    dock_router_->register_factory("equity_research", []() { return new screens::EquityResearchScreen; });
    dock_router_->register_factory("ma_analytics", []() { return new screens::MAAnalyticsScreen; });
    dock_router_->register_factory("alt_investments", []() { return new screens::AltInvestmentsScreen; });
    dock_router_->register_factory("geopolitics", []() { return new screens::GeopoliticsScreen; });
    dock_router_->register_factory("maritime", []() { return new screens::MaritimeScreen; });
    dock_router_->register_factory("surface_analytics", []() { return new fincept::surface::SurfaceAnalyticsScreen; });
    dock_router_->register_factory("agent_config", []() { return new screens::AgentConfigScreen; });
    dock_router_->register_factory("mcp_servers", []() { return new screens::McpServersScreen; });
    dock_router_->register_factory("data_mapping", []() { return new screens::DataMappingScreen; });
    dock_router_->register_factory("data_sources", []() { return new screens::DataSourcesScreen; });
    dock_router_->register_factory("file_manager", [this]() {
        auto* fm = new screens::FileManagerScreen;
        connect(fm, &screens::FileManagerScreen::open_file_in_screen, this,
                [this](const QString& route_id, const QString& /*file_path*/) { dock_router_->navigate(route_id); });
        return fm;
    });
    dock_router_->register_factory("excel", []() { return new screens::ExcelScreen; });
    dock_router_->register_factory("trade_viz", []() { return new screens::TradeVizScreen; });
    dock_router_->register_factory("docs", []() { return new screens::DocsScreen; });

    // Info/legal pages: static content, safe to construct eagerly.
    dock_router_->register_screen("contact", new screens::ContactScreen);
    dock_router_->register_screen("terms", new screens::TermsScreen);
    dock_router_->register_screen("privacy", new screens::PrivacyScreen);
    dock_router_->register_screen("trademarks", new screens::TrademarksScreen);
    dock_router_->register_screen("help", new screens::HelpScreen(false));
}

void WindowFrame::setup_app_screens() {}
void WindowFrame::setup_navigation() {}

} // namespace fincept
