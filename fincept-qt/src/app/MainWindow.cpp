#include "app/MainWindow.h"

#include "ai_chat/AiChatBubble.h"
#include "ai_chat/AiChatScreen.h"
#include "auth/AuthManager.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "storage/repositories/SettingsRepository.h"
#include "screens/ComingSoonScreen.h"
#include "screens/about/AboutScreen.h"
#include "screens/agent_config/AgentConfigScreen.h"
#include "screens/ai_quant_lab/AIQuantLabScreen.h"
#include "screens/akshare/AkShareScreen.h"
#include "screens/algo_trading/AlgoTradingScreen.h"
#include "screens/alpha_arena/AlphaArenaScreen.h"
#include "screens/alt_investments/AltInvestmentsScreen.h"
#include "screens/asia_markets/AsiaMarketsScreen.h"
#include "screens/auth/ForgotPasswordScreen.h"
#include "screens/auth/LoginScreen.h"
#include "screens/auth/PricingScreen.h"
#include "screens/auth/RegisterScreen.h"
#include "screens/backtesting/BacktestingScreen.h"
#include "screens/code_editor/CodeEditorScreen.h"
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
#include "screens/payment/PaymentProcessingScreen.h"
#include "screens/payment/PaymentSuccessScreen.h"
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
#include "ui/navigation/FKeyBar.h"
#include "ui/navigation/NavigationBar.h"
#include "ui/navigation/StatusBar.h"
#include "ui/navigation/ToolBar.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QShortcut>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWindow>

namespace fincept {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Fincept Terminal");
    setWindowIcon(QIcon("C:/windowsdisk/finceptTerminal/fincept_icon.ico"));
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
    tab_bar_widget_ = tab_bar;

    // Screen content
    app_stack_ = new QStackedWidget;
    vl->addWidget(app_stack_, 1);

    // Status bar
    auto* status_bar = new ui::StatusBar;
    vl->addWidget(status_bar);
    status_bar_widget_ = status_bar;

    master_stack->addWidget(app_container);
    setCentralWidget(master_stack);
    statusBar()->hide();

    stack_ = master_stack;
    router_ = new ScreenRouter(app_stack_, this);
    setup_app_screens();

    // AI Chat Bubble — floats over app_stack_ content area
    chat_bubble_ = new AiChatBubble(app_stack_);
    {
        auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
        bool show = !r.is_ok() || r.value() != "false";
        chat_bubble_->setVisible(show);
        if (show) chat_bubble_->raise();
    }

    // Tab bar ↔ router
    connect(tab_bar, &ui::TabBar::tab_changed, router_, &ScreenRouter::navigate);
    connect(router_, &ScreenRouter::screen_changed, tab_bar, &ui::TabBar::set_active);

    // Re-raise bubble on every screen switch; respect show_chat_bubble setting
    connect(router_, &ScreenRouter::screen_changed, this, [this](const QString&) {
        if (!chat_bubble_) return;
        auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
        bool show = !r.is_ok() || r.value() != "false";
        chat_bubble_->setVisible(show);
        if (show) {
            chat_bubble_->reposition();
            chat_bubble_->raise();
        }
    });

    // Toolbar Navigate menu → router
    connect(toolbar, &ui::ToolBar::navigate_to, router_, &ScreenRouter::navigate);

    // MCP navigation tool → router (published from any thread, must invoke on UI thread)
    EventBus::instance().subscribe("nav.switch_screen", [this](const QVariantMap& data) {
        QString screen_id = data["screen_id"].toString();
        if (!screen_id.isEmpty())
            QMetaObject::invokeMethod(router_, [this, screen_id]() {
                router_->navigate(screen_id);
            }, Qt::QueuedConnection);
    });

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    auto* sc_fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(sc_fullscreen, &QShortcut::activated, this, [this]() {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
    });

    auto* sc_focus = new QShortcut(QKeySequence(Qt::Key_F10), this);
    connect(sc_focus, &QShortcut::activated, this, [this]() {
        focus_mode_ = !focus_mode_;
        if (tab_bar_widget_)
            tab_bar_widget_->setVisible(!focus_mode_);
        if (status_bar_widget_)
            status_bar_widget_->setVisible(!focus_mode_);
    });

    auto* sc_refresh = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(sc_refresh, &QShortcut::activated, this, [this]() {
        if (auto* w = app_stack_->currentWidget())
            QMetaObject::invokeMethod(w, "refresh", Qt::QueuedConnection);
    });

    auto* sc_screenshot = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_P), this);
    connect(sc_screenshot, &QShortcut::activated, this, [this]() {
        QScreen* scr = this->screen();
        if (!scr)
            scr = QApplication::primaryScreen();
        QPixmap px = scr->grabWindow(winId());
        const QString path = QFileDialog::getSaveFileName(
            this, "Save Screenshot",
            QDir::homePath() + "/fincept_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png",
            "PNG Images (*.png)");
        if (!path.isEmpty()) {
            px.save(path, "PNG");
            LOG_INFO("MainWindow", QString("Screenshot saved: %1").arg(path));
        }
    });

    // Toolbar actions
    connect(toolbar, &ui::ToolBar::action_triggered, this, [this](const QString& action) {
        if (action == "logout") {
            auth::AuthManager::instance().logout();
        } else if (action == "fullscreen") {
            if (isFullScreen())
                showNormal();
            else
                showFullScreen();
        } else if (action == "focus_mode") {
            focus_mode_ = !focus_mode_;
            if (tab_bar_widget_)
                tab_bar_widget_->setVisible(!focus_mode_);
            if (status_bar_widget_)
                status_bar_widget_->setVisible(!focus_mode_);
        } else if (action == "always_on_top") {
            always_on_top_ = !always_on_top_;
            Qt::WindowFlags flags = windowFlags();
            if (always_on_top_)
                flags |= Qt::WindowStaysOnTopHint;
            else
                flags &= ~Qt::WindowStaysOnTopHint;
            setWindowFlags(flags);
            show(); // required after setWindowFlags to re-show the window
        } else if (action == "refresh") {
            if (auto* w = app_stack_->currentWidget())
                QMetaObject::invokeMethod(w, "refresh", Qt::QueuedConnection);
        } else if (action == "screenshot") {
            QScreen* scr = this->screen();
            if (!scr)
                scr = QApplication::primaryScreen();
            QPixmap px = scr->grabWindow(winId());
            QString path = QFileDialog::getSaveFileName(
                this, "Save Screenshot",
                QDir::homePath() + "/fincept_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png",
                "PNG Images (*.png)");
            if (!path.isEmpty()) {
                px.save(path, "PNG");
                LOG_INFO("MainWindow", QString("Screenshot saved: %1").arg(path));
            }
        }
    });

    // Toolbar logout
    connect(toolbar, &ui::ToolBar::logout_clicked, this, []() { auth::AuthManager::instance().logout(); });

    // Toolbar plan label → pricing screen
    connect(toolbar, &ui::ToolBar::plan_clicked, this, [this]() {
        stack_->setCurrentIndex(0);
        auth_stack_->setCurrentIndex(3); // PricingScreen
    });

    // Auth state
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &MainWindow::on_auth_state_changed);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::logged_out, this, &MainWindow::on_auth_state_changed);

    on_auth_state_changed();
}

void MainWindow::setup_auth_screens() {
    auto* login = new screens::LoginScreen;
    auto* reg = new screens::RegisterScreen;
    auto* forgot = new screens::ForgotPasswordScreen;
    auto* pricing = new screens::PricingScreen;
    auto* pay_processing = new screens::PaymentProcessingScreen;
    auto* pay_success = new screens::PaymentSuccessScreen;

    // Info screens stack (shared between auth and app via master stack index 2)
    info_stack_ = new QStackedWidget;
    auto* contact = new screens::ContactScreen;
    auto* terms = new screens::TermsScreen;
    auto* privacy = new screens::PrivacyScreen;
    auto* trademarks = new screens::TrademarksScreen;
    auto* help = new screens::HelpScreen;

    info_stack_->addWidget(contact);    // index 0
    info_stack_->addWidget(terms);      // index 1
    info_stack_->addWidget(privacy);    // index 2
    info_stack_->addWidget(trademarks); // index 3
    info_stack_->addWidget(help);       // index 4

    auth_stack_->addWidget(login);          // index 0
    auth_stack_->addWidget(reg);            // index 1
    auth_stack_->addWidget(forgot);         // index 2
    auth_stack_->addWidget(pricing);        // index 3
    auth_stack_->addWidget(pay_processing); // index 4
    auth_stack_->addWidget(pay_success);    // index 5
    auth_stack_->addWidget(info_stack_);    // index 6

    // ── Auth navigation ──────────────────────────────────────────────────────
    connect(login, &screens::LoginScreen::navigate_register, this, &MainWindow::show_register);
    connect(login, &screens::LoginScreen::navigate_forgot_password, this, &MainWindow::show_forgot_password);
    connect(reg, &screens::RegisterScreen::navigate_login, this, &MainWindow::show_login);
    connect(forgot, &screens::ForgotPasswordScreen::navigate_login, this, &MainWindow::show_login);
    connect(pricing, &screens::PricingScreen::navigate_dashboard, this, [this]() {
        stack_->setCurrentIndex(1);
        router_->navigate("dashboard");
    });

    // Pricing → payment processing: when user initiates payment, navigate to polling screen
    connect(pricing, &screens::PricingScreen::payment_initiated, this,
            [this, pay_processing](const QString& order_id, const QString& plan_name) {
                auth_stack_->setCurrentIndex(4); // show processing screen
                pay_processing->start_polling(order_id, plan_name);
            });

    // ── Payment navigation ───────────────────────────────────────────────────
    connect(pay_processing, &screens::PaymentProcessingScreen::payment_completed, this, [this, pay_success]() {
        auth_stack_->setCurrentIndex(5);     // show success
        pay_success->show_success("", 0, 0); // details come from refreshed session
    });
    connect(pay_processing, &screens::PaymentProcessingScreen::navigate_back, this, &MainWindow::show_pricing);

    connect(pay_success, &screens::PaymentSuccessScreen::navigate_dashboard, this, [this]() {
        stack_->setCurrentIndex(1);
        router_->navigate("dashboard");
    });

    // ── Info screen navigation ───────────────────────────────────────────────
    // Back from info → return to previous auth screen (login by default)
    connect(contact, &screens::ContactScreen::navigate_back, this, &MainWindow::show_login);
    connect(terms, &screens::TermsScreen::navigate_back, this, &MainWindow::show_login);
    connect(terms, &screens::TermsScreen::navigate_privacy, this, &MainWindow::show_info_privacy);
    connect(terms, &screens::TermsScreen::navigate_contact, this, &MainWindow::show_info_contact);
    connect(privacy, &screens::PrivacyScreen::navigate_back, this, &MainWindow::show_login);
    connect(privacy, &screens::PrivacyScreen::navigate_terms, this, &MainWindow::show_info_terms);
    connect(privacy, &screens::PrivacyScreen::navigate_contact, this, &MainWindow::show_info_contact);
    connect(trademarks, &screens::TrademarksScreen::navigate_back, this, &MainWindow::show_login);
    connect(help, &screens::HelpScreen::navigate_back, this, &MainWindow::show_login);
    connect(help, &screens::HelpScreen::navigate_register, this, &MainWindow::show_register);
    connect(help, &screens::HelpScreen::navigate_forgot_password, this, &MainWindow::show_forgot_password);
}

void MainWindow::setup_app_screens() {
    // ── Heavy screens: lazy-construct on first navigation ────────────────────
    // These spawn Python processes / timers / network requests in their constructors.
    router_->register_factory("dashboard", []() { return new screens::DashboardScreen; });
    router_->register_factory("markets", []() { return new screens::MarketsScreen; });
    router_->register_factory("crypto_trading", []() { return new screens::CryptoTradingScreen; });
    router_->register_factory("news", []() { return new screens::NewsScreen; });
    router_->register_factory("forum", []() { return new screens::ForumScreen; });
    router_->register_factory("watchlist", []() { return new screens::WatchlistScreen; });

    // ── Lightweight screens: eager construction is fine ──────────────────────
    router_->register_screen("report_builder", new screens::ReportBuilderScreen);
    router_->register_screen("profile", new screens::ProfileScreen);
    router_->register_screen("settings", new screens::SettingsScreen);
    router_->register_screen("about", new screens::AboutScreen);
    router_->register_screen("support", new screens::SupportScreen);
    router_->register_screen("notes", new screens::NotesScreen);

    // ── Coming-soon placeholders (trivial widgets) ──────────────────────────
    router_->register_factory("portfolio", []() { return new screens::PortfolioScreen; });
    router_->register_factory("ai_chat", []() { return new screens::AiChatScreen; });
    router_->register_factory("backtesting", []() { return new screens::BacktestingScreen; });
    router_->register_factory("algo_trading", []() { return new screens::AlgoTradingScreen; });
    router_->register_factory("node_editor", []() { return new workflow::NodeEditorScreen; });
    router_->register_factory("code_editor", []() { return new screens::CodeEditorScreen; });
    router_->register_factory("ai_quant_lab", []() { return new screens::AIQuantLabScreen; });
    router_->register_factory("quantlib", []() { return new screens::QuantLibScreen; });
    router_->register_factory("economics", []() { return new screens::EconomicsScreen; });
    router_->register_factory("gov_data", []() { return new screens::GovDataScreen; });
    router_->register_factory("dbnomics", []() { return new screens::DBnomicsScreen; });
    router_->register_factory("akshare", []() { return new screens::AkShareScreen; });
    router_->register_factory("asia_markets", []() { return new screens::AsiaMarketsScreen; });
    router_->register_factory("relationship_map", []() { return new screens::RelationshipMapScreen; });
    router_->register_factory("equity_trading", []() { return new screens::EquityTradingScreen; });
    router_->register_factory("alpha_arena", []() { return new screens::AlphaArenaScreen; });
    router_->register_factory("polymarket", []() { return new screens::PolymarketScreen; });
    router_->register_factory("derivatives", []() { return new screens::DerivativesScreen; });
    router_->register_factory("equity_research", []() { return new screens::EquityResearchScreen; });
    router_->register_factory("ma_analytics", []() { return new screens::MAAnalyticsScreen; });
    router_->register_factory("alt_investments", []() { return new screens::AltInvestmentsScreen; });
    router_->register_factory("geopolitics", []() { return new screens::GeopoliticsScreen; });
    router_->register_factory("maritime", []() { return new screens::MaritimeScreen; });
    router_->register_factory("surface_analytics", []() { return new fincept::surface::SurfaceAnalyticsScreen; });
    router_->register_factory("agent_config", []() { return new screens::AgentConfigScreen; });
    router_->register_factory("mcp_servers", []() { return new screens::McpServersScreen; });
    router_->register_factory("data_mapping", []() { return new screens::DataMappingScreen; });
    router_->register_factory("data_sources", []() { return new screens::DataSourcesScreen; });
    router_->register_factory("file_manager", []() { return new screens::FileManagerScreen; });
    router_->register_factory("excel", []() { return new screens::ExcelScreen; });
    router_->register_factory("trade_viz", []() { return new screens::TradeVizScreen; });

    router_->register_factory("docs", []() { return new screens::DocsScreen; });

    // Info / legal pages (accessible from app navigation menu too)
    router_->register_screen("contact", new screens::ContactScreen);
    router_->register_screen("terms", new screens::TermsScreen);
    router_->register_screen("privacy", new screens::PrivacyScreen);
    router_->register_screen("trademarks", new screens::TrademarksScreen);
    router_->register_screen("help", new screens::HelpScreen);
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

void MainWindow::show_login() {
    auth_stack_->setCurrentIndex(0);
}
void MainWindow::show_register() {
    auth_stack_->setCurrentIndex(1);
}
void MainWindow::show_forgot_password() {
    auth_stack_->setCurrentIndex(2);
}
void MainWindow::show_pricing() {
    auth_stack_->setCurrentIndex(3);
}
void MainWindow::show_payment_processing() {
    auth_stack_->setCurrentIndex(4);
}
void MainWindow::show_info_contact() {
    info_stack_->setCurrentIndex(0);
    auth_stack_->setCurrentIndex(6);
}
void MainWindow::show_info_terms() {
    info_stack_->setCurrentIndex(1);
    auth_stack_->setCurrentIndex(6);
}
void MainWindow::show_info_privacy() {
    info_stack_->setCurrentIndex(2);
    auth_stack_->setCurrentIndex(6);
}
void MainWindow::show_info_trademarks() {
    info_stack_->setCurrentIndex(3);
    auth_stack_->setCurrentIndex(6);
}
void MainWindow::show_info_help() {
    info_stack_->setCurrentIndex(4);
    auth_stack_->setCurrentIndex(6);
}

void MainWindow::restore_session() {}

void MainWindow::closeEvent(QCloseEvent* event) {
    SessionManager::instance().save_geometry(saveGeometry(), saveState());
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (chat_bubble_)
        chat_bubble_->reposition();
}

} // namespace fincept
