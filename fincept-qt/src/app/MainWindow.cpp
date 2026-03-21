#include "app/MainWindow.h"
#include "core/session/SessionManager.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/navigation/NavigationBar.h"
#include "ui/navigation/FKeyBar.h"
#include "ui/navigation/StatusBar.h"
#include "ui/navigation/ToolBar.h"
#include "auth/AuthManager.h"
#include "screens/auth/LoginScreen.h"
#include "screens/auth/RegisterScreen.h"
#include "screens/auth/ForgotPasswordScreen.h"
#include "screens/auth/PricingScreen.h"
#include "screens/markets/MarketsScreen.h"
#include "screens/dashboard/DashboardScreen.h"
#include "screens/report_builder/ReportBuilderScreen.h"
#include "screens/profile/ProfileScreen.h"
#include "screens/settings/SettingsScreen.h"
#include "screens/about/AboutScreen.h"
#include "screens/support/SupportScreen.h"
#include "screens/ComingSoonScreen.h"
#include "screens/news/NewsScreen.h"
#include "screens/crypto_trading/CryptoTradingScreen.h"
#include "screens/notes/NotesScreen.h"
#include "screens/watchlist/WatchlistScreen.h"
#include "screens/surface_analytics/SurfaceAnalyticsScreen.h"
#include "screens/dbnomics/DBnomicsScreen.h"
#include <QVBoxLayout>
#include <QScreen>
#include <QApplication>
#include <QCloseEvent>
#include <QStatusBar>

namespace fincept {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Fincept Terminal");
    setMinimumSize(1280, 720);

    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect geom = screen->availableGeometry();
        resize(geom.width() * 9 / 10, geom.height() * 9 / 10);
        move(geom.center() - rect().center());
    }

    auto* master_stack = new QStackedWidget;

    // ── Auth stack ───────────────────────────────────────────────────────────
    auth_stack_ = new QStackedWidget;
    setup_auth_screens();
    master_stack->addWidget(auth_stack_);

    // ── App shell ────────────────────────────────────────────────────────────
    auto* app_container = new QWidget;
    auto* vl = new QVBoxLayout(app_container);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Toolbar (menus + branding + clock + user + logout)
    auto* toolbar = new ui::ToolBar;
    vl->addWidget(toolbar);

    // Tab bar (14 tabs)
    auto* tab_bar = new ui::TabBar;
    vl->addWidget(tab_bar);

    // Screen content
    app_stack_ = new QStackedWidget;
    vl->addWidget(app_stack_, 1);

    // Status bar
    auto* status_bar = new ui::StatusBar;
    vl->addWidget(status_bar);

    master_stack->addWidget(app_container);
    setCentralWidget(master_stack);
    statusBar()->hide();

    stack_ = master_stack;
    router_ = new ScreenRouter(app_stack_, this);
    setup_app_screens();

    // Tab bar ↔ router
    connect(tab_bar, &ui::TabBar::tab_changed, router_, &ScreenRouter::navigate);
    connect(router_, &ScreenRouter::screen_changed, tab_bar, &ui::TabBar::set_active);

    // Toolbar Navigate menu → router
    connect(toolbar, &ui::ToolBar::navigate_to, router_, &ScreenRouter::navigate);

    // Toolbar actions
    connect(toolbar, &ui::ToolBar::action_triggered, this, [this](const QString& action) {
        if (action == "logout") auth::AuthManager::instance().logout();
        else if (action == "fullscreen") {
            if (isFullScreen()) showNormal(); else showFullScreen();
        }
    });

    // Toolbar logout
    connect(toolbar, &ui::ToolBar::logout_clicked, this, []() {
        auth::AuthManager::instance().logout();
    });

    // Toolbar plan label → pricing screen
    connect(toolbar, &ui::ToolBar::plan_clicked, this, [this]() {
        stack_->setCurrentIndex(0);
        auth_stack_->setCurrentIndex(3);  // PricingScreen
    });

    // Auth state
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed,
            this, &MainWindow::on_auth_state_changed);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::logged_out,
            this, &MainWindow::on_auth_state_changed);

    on_auth_state_changed();
}

void MainWindow::setup_auth_screens() {
    auto* login   = new screens::LoginScreen;
    auto* reg     = new screens::RegisterScreen;
    auto* forgot  = new screens::ForgotPasswordScreen;
    auto* pricing = new screens::PricingScreen;

    auth_stack_->addWidget(login);    // index 0
    auth_stack_->addWidget(reg);      // index 1
    auth_stack_->addWidget(forgot);   // index 2
    auth_stack_->addWidget(pricing);  // index 3

    connect(login, &screens::LoginScreen::navigate_register, this, &MainWindow::show_register);
    connect(login, &screens::LoginScreen::navigate_forgot_password, this, &MainWindow::show_forgot_password);
    connect(reg,   &screens::RegisterScreen::navigate_login, this, &MainWindow::show_login);
    connect(forgot,&screens::ForgotPasswordScreen::navigate_login, this, &MainWindow::show_login);
    connect(pricing, &screens::PricingScreen::navigate_dashboard, this, [this]() {
        stack_->setCurrentIndex(1);
        router_->navigate("dashboard");
    });
}

void MainWindow::setup_app_screens() {
    // ── Heavy screens: lazy-construct on first navigation ────────────────────
    // These spawn Python processes / timers / network requests in their constructors.
    router_->register_factory("dashboard",      []() { return new screens::DashboardScreen; });
    router_->register_factory("markets",        []() { return new screens::MarketsScreen; });
    router_->register_factory("crypto_trading", []() { return new screens::CryptoTradingScreen; });
    router_->register_factory("news",           []() { return new screens::NewsScreen; });
    router_->register_factory("watchlist",      []() { return new screens::WatchlistScreen; });

    // ── Lightweight screens: eager construction is fine ──────────────────────
    router_->register_screen("report_builder",  new screens::ReportBuilderScreen);
    router_->register_screen("profile",         new screens::ProfileScreen);
    router_->register_screen("settings",        new screens::SettingsScreen);
    router_->register_screen("about",           new screens::AboutScreen);
    router_->register_screen("support",         new screens::SupportScreen);
    router_->register_screen("notes",           new screens::NotesScreen);

    // ── Coming-soon placeholders (trivial widgets) ──────────────────────────
    router_->register_screen("portfolio",       new screens::ComingSoonScreen("Portfolio"));
    router_->register_screen("ai_chat",         new screens::ComingSoonScreen("AI Chat"));
    router_->register_screen("backtesting",     new screens::ComingSoonScreen("Backtesting"));
    router_->register_screen("algo_trading",    new screens::ComingSoonScreen("Algo Trading"));
    router_->register_screen("node_editor",     new screens::ComingSoonScreen("Node Editor"));
    router_->register_screen("code_editor",     new screens::ComingSoonScreen("Code Editor"));
    router_->register_screen("ai_quant_lab",    new screens::ComingSoonScreen("AI Quant Lab"));
    router_->register_screen("quantlib",        new screens::ComingSoonScreen("QuantLib"));
    router_->register_screen("screener",        new screens::ComingSoonScreen("Stock Screener"));
    router_->register_screen("economics",       new screens::ComingSoonScreen("Economics"));
    router_->register_screen("gov_data",        new screens::ComingSoonScreen("Government Data"));
    router_->register_factory("dbnomics",        []() { return new screens::DBnomicsScreen; });
    router_->register_screen("akshare",         new screens::ComingSoonScreen("AKShare Data"));
    router_->register_screen("asia_markets",    new screens::ComingSoonScreen("Asia Markets"));
    router_->register_screen("relationship_map",new screens::ComingSoonScreen("Relationship Map"));
    router_->register_screen("equity_trading",  new screens::ComingSoonScreen("Equity Trading"));
    router_->register_screen("alpha_arena",     new screens::ComingSoonScreen("Alpha Arena"));
    router_->register_screen("polymarket",      new screens::ComingSoonScreen("Polymarket"));
    router_->register_screen("derivatives",     new screens::ComingSoonScreen("Derivatives Pricing"));
    router_->register_screen("equity_research", new screens::ComingSoonScreen("Equity Research"));
    router_->register_screen("ma_analytics",    new screens::ComingSoonScreen("M&A Analytics"));
    router_->register_screen("alt_investments", new screens::ComingSoonScreen("Alternative Investments"));
    router_->register_screen("geopolitics",     new screens::ComingSoonScreen("Geopolitics"));
    router_->register_screen("maritime",        new screens::ComingSoonScreen("Maritime Intelligence"));
    router_->register_screen("market_sim",      new screens::ComingSoonScreen("Market Simulation"));
    router_->register_factory("surface_analytics", []() { return new fincept::surface::SurfaceAnalyticsScreen; });
    router_->register_screen("agent_config",    new screens::ComingSoonScreen("Agent Config"));
    router_->register_screen("mcp_servers",     new screens::ComingSoonScreen("MCP Servers"));
    router_->register_screen("data_mapping",    new screens::ComingSoonScreen("Data Mapping"));
    router_->register_screen("data_sources",    new screens::ComingSoonScreen("Data Sources"));
    router_->register_screen("excel",           new screens::ComingSoonScreen("Excel Workbook"));
    router_->register_screen("trade_viz",       new screens::ComingSoonScreen("Trade Visualization"));
    router_->register_screen("forum",           new screens::ComingSoonScreen("Forum"));
    router_->register_screen("docs",            new screens::ComingSoonScreen("Documentation"));
}

void MainWindow::setup_navigation() {}

void MainWindow::on_auth_state_changed() {
    auto& auth = auth::AuthManager::instance();
    if (auth.is_authenticated()) {
        if (auth.session().has_paid_plan()) {
            // Paid user → straight to dashboard
            stack_->setCurrentIndex(1);
            router_->navigate("dashboard");
        } else {
            // Free/no plan → show pricing gate
            stack_->setCurrentIndex(0);
            auth_stack_->setCurrentIndex(3);
        }
    } else {
        stack_->setCurrentIndex(0);
        auth_stack_->setCurrentIndex(0);
    }
}

void MainWindow::show_login()           { auth_stack_->setCurrentIndex(0); }
void MainWindow::show_register()        { auth_stack_->setCurrentIndex(1); }
void MainWindow::show_forgot_password() { auth_stack_->setCurrentIndex(2); }
void MainWindow::show_pricing()         { auth_stack_->setCurrentIndex(3); }

void MainWindow::restore_session() {}

void MainWindow::closeEvent(QCloseEvent* event) {
    SessionManager::instance().save_geometry(saveGeometry(), saveState());
    event->accept();
}

} // namespace fincept
