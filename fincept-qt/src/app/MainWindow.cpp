#include "app/MainWindow.h"

#include "app/DockScreenRouter.h"
#include "ui/navigation/DockToolBar.h"
#include "ui/navigation/DockStatusBar.h"
#include <DockManager.h>
#include <DockWidget.h>
#include <DockAreaWidget.h>

#include "screens/chat_mode/ChatModeScreen.h"
#include "screens/auth/LockScreen.h"
#include "services/updater/UpdateService.h"
#include "ai_chat/AiChatBubble.h"
#include "ai_chat/AiChatScreen.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
#include "storage/repositories/SettingsRepository.h"
#include "screens/ComingSoonScreen.h"
#include "services/workspace/WorkspaceManager.h"
#include "ui/workspace/WorkspaceNewDialog.h"
#include "ui/workspace/WorkspaceOpenDialog.h"
#include "ui/workspace/WorkspaceSaveAsDialog.h"
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
#include "core/config/ProfileManager.h"
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QScreen>
#include <QShortcut>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWindow>

namespace fincept {

MainWindow::MainWindow(int window_id, QWidget* parent)
    : QMainWindow(parent), window_id_(window_id) {
    // Show active profile in title bar when using a non-default profile
    const QString profile = ProfileManager::instance().active();
    setWindowTitle(profile == "default"
                   ? "Fincept Terminal"
                   : QString("Fincept Terminal [%1]").arg(profile));
    // Load icon from the embedded Windows resource (IDI_ICON1 in app.rc).
    // Falls back to the .ico beside the executable on other platforms.
    QIcon app_icon;
#if defined(Q_OS_WIN)
    app_icon = QApplication::windowIcon(); // set by app.rc at link time
    if (app_icon.isNull()) {
        // Fallback: look next to the executable (works after windeployqt)
        const QString ico = QCoreApplication::applicationDirPath() + "/fincept.ico";
        if (QFile::exists(ico))
            app_icon = QIcon(ico);
    }
#else
    {
        const QString ico = QCoreApplication::applicationDirPath() + "/fincept.ico";
        if (QFile::exists(ico))
            app_icon = QIcon(ico);
    }
#endif
    if (!app_icon.isNull())
        setWindowIcon(app_icon);
    // Minimum size must be small enough to allow OS window-snap (half/third screen).
    // On a 1920-wide display, left-snap = 960px. Use 800x500 as minimum so snapping
    // works on any 1080p+ monitor without the window bouncing back to centre.
    setMinimumSize(800, 500);

    // Primary windows (id 0) restore saved geometry from last session.
    // Secondary windows (id > 0) restore their own saved geometry if available,
    // otherwise use smart multi-monitor placement (handled by the caller after show()).
    const QByteArray saved_geom = SessionManager::instance().load_geometry(window_id_);
    if (!saved_geom.isEmpty()) {
        restoreGeometry(saved_geom);
    } else {
        QScreen* screen = QApplication::primaryScreen();
        if (screen) {
            QRect geom = screen->availableGeometry();
            resize(geom.width() * 9 / 10, geom.height() * 9 / 10);
            // Only centre-place for the primary window; secondary windows
            // are positioned by the "new window" action after construction.
            if (window_id_ == 0)
                move(geom.center() - rect().center());
        }
    }

    auto* master_stack = new QStackedWidget;

    // ── Auth stack ───────────────────────────────────────────────────────────
    auth_stack_ = new QStackedWidget;
    setup_auth_screens();
    master_stack->addWidget(auth_stack_);

    // ── Chat Mode ─────────────────────────────────────────────────────────────
    chat_mode_screen_ = new chat_mode::ChatModeScreen;

    // ── Lock Screen ─────────────────────────────────────────────────────────
    lock_screen_ = new screens::LockScreen;

    // ── ADS Docking mode ─────────────────────────────────────────────────────
    setup_docking_mode();
    master_stack->addWidget(dock_manager_->parentWidget());  // index 1 — dock_wrapper
    master_stack->addWidget(chat_mode_screen_); // index 2
    master_stack->addWidget(lock_screen_);      // index 3 — lock/PIN screen
    connect(chat_mode_screen_, &chat_mode::ChatModeScreen::exit_requested,
            this, &MainWindow::toggle_chat_mode);

    // Lock screen signals
    connect(lock_screen_, &screens::LockScreen::unlocked, this, &MainWindow::on_terminal_unlocked);
    connect(lock_screen_, &screens::LockScreen::reauth_requested, this, [this]() {
        // Max PIN attempts exceeded — force full re-login
        auth::AuthManager::instance().logout();
    });

    // Inactivity guard → lock screen
    connect(&auth::InactivityGuard::instance(), &auth::InactivityGuard::lock_requested,
            this, &MainWindow::show_lock_screen);

    setCentralWidget(master_stack);

    dock_toolbar_ = new ui::DockToolBar(this);
    addToolBar(Qt::TopToolBarArea, dock_toolbar_);

    dock_status_bar_ = new ui::DockStatusBar(this);
    setStatusBar(dock_status_bar_);

    stack_ = master_stack;
    auto* toolbar = dock_toolbar_->inner();

    // ── Workspace + signal wiring ───────────────────────────────────────────
    WorkspaceManager::instance().set_main_window(this);
    connect(&WorkspaceManager::instance(), &WorkspaceManager::workspace_loaded,
            this, [this](const WorkspaceDef&) {
                update_window_title();
            });
    connect(&WorkspaceManager::instance(), &WorkspaceManager::workspace_error,
            this, [this](const QString& msg) {
                QMessageBox::warning(this, "Workspace Error", msg);
            });

    dock_router_ = new DockScreenRouter(dock_manager_, this);
    setup_dock_screens();

    // Tab bar navigation: open the selected screen exclusively — closes all other
    // panels and fills the full area. Each tab is a fresh independent screen,
    // not nested into whatever was previously open.
    connect(tab_bar_, &ui::TabBar::tab_changed, this, [this](const QString& id) {
        dock_router_->navigate(id, true);
    });
    connect(dock_router_, &DockScreenRouter::screen_changed, tab_bar_, &ui::TabBar::set_active);

    // Update the main window title bar to reflect the current screen name.
    connect(dock_router_, &DockScreenRouter::screen_changed, this, [this](const QString&) {
        update_window_title();
    });

    connect(this, &QObject::destroyed, this, [this]() {
        WorkspaceManager::instance().remove_window(this);
    });

    // Chat bubble — floats over dock_manager_ content area
    chat_bubble_ = new AiChatBubble(dock_manager_);
    {
        auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
        bool show = !r.is_ok() || r.value() != "false";
        chat_bubble_->setVisible(show);
        if (show) chat_bubble_->raise();
    }

    // Re-raise and reposition the bubble after each navigation — dock geometry
    // shifts when panels open/close, so the bubble needs to re-anchor itself.
    connect(dock_router_, &DockScreenRouter::screen_changed, this, [this](const QString&) {
        if (!chat_bubble_) return;
        auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
        bool show = !r.is_ok() || r.value() != "false";
        chat_bubble_->setVisible(show);
        if (show) { chat_bubble_->reposition(); chat_bubble_->raise(); }
    });

    connect(toolbar, &ui::ToolBar::chat_mode_toggled, this, &MainWindow::toggle_chat_mode);
    connect(toolbar, &ui::ToolBar::navigate_to, this, [this](const QString& id) {
        dock_router_->navigate(id, true);
    });

    connect(toolbar, &ui::ToolBar::dock_command, this,
            [this](const QString& action, const QString& primary, const QString& secondary) {
        if (action == "add")
            dock_router_->add_alongside(primary, secondary);
        else if (action == "remove")
            dock_router_->remove_screen(primary);
        else if (action == "replace")
            dock_router_->replace_screen(primary, secondary);
    });

    // MCP navigation tool → dock_router (cross-thread safe)
    EventBus::instance().subscribe("nav.switch_screen", [this](const QVariantMap& data) {
        QString screen_id = data["screen_id"].toString();
        if (!screen_id.isEmpty())
            QMetaObject::invokeMethod(dock_router_, [this, screen_id]() {
                dock_router_->navigate(screen_id);
            }, Qt::QueuedConnection);
    });

    auto* sc_chat = new QShortcut(QKeySequence(Qt::Key_F9), this);
    connect(sc_chat, &QShortcut::activated, this, &MainWindow::toggle_chat_mode);

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
        if (dock_toolbar_)     dock_toolbar_->setVisible(!focus_mode_);
        if (dock_status_bar_)  dock_status_bar_->setVisible(!focus_mode_);
    });

    auto* sc_refresh = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(sc_refresh, &QShortcut::activated, this, [this]() {
        if (dock_manager_) {
            auto* focused = dock_manager_->focusedDockWidget();
            if (focused && focused->widget())
                QMetaObject::invokeMethod(focused->widget(), "refresh", Qt::QueuedConnection);
        }
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
        if (action == "new_window") {
            auto* w = new MainWindow(MainWindow::next_window_id());
            w->setAttribute(Qt::WA_DeleteOnClose);
            // Place on the next available screen, or alongside this window
            const QList<QScreen*> screens = QGuiApplication::screens();
            if (screens.size() > 1) {
                // Find a screen that doesn't already contain this window's centre
                const QPoint this_centre = geometry().center();
                for (QScreen* s : screens) {
                    if (!s->geometry().contains(this_centre)) {
                        const QRect sg = s->availableGeometry();
                        w->resize(sg.width() * 9 / 10, sg.height() * 9 / 10);
                        w->move(sg.center() - w->rect().center());
                        break;
                    }
                }
            }
            w->show();
            w->raise();
            w->activateWindow();
        } else if (action.startsWith("panel_")) {
            // Float a screen — in ADS mode, navigate dock_router; in legacy, spawn window
            static const QMap<QString, QPair<QString,QString>> panel_map = {
                {"panel_dashboard",  {"Dashboard",      "dashboard"}},
                {"panel_watchlist",  {"Watchlist",      "watchlist"}},
                {"panel_news",       {"News Feed",      "news"}},
                {"panel_portfolio",  {"Portfolio",      "portfolio"}},
                {"panel_markets",    {"Markets",        "markets"}},
                {"panel_crypto",     {"Crypto Trading", "crypto_trading"}},
                {"panel_equity",     {"Equity Trading", "equity_trading"}},
                {"panel_algo",       {"Algo Trading",   "algo_trading"}},
                {"panel_research",   {"Equity Research","equity_research"}},
                {"panel_economics",  {"Economics",      "economics"}},
                {"panel_geopolitics",{"Geopolitics",    "geopolitics"}},
                {"panel_ai_chat",    {"AI Chat",        "ai_chat"}},
            };
            if (panel_map.contains(action)) {
                const auto [title, route] = panel_map[action];
                if (dock_router_)
                    dock_router_->navigate(route);
            }
        } else if (action == "perspective_save") {
            if (dock_manager_) {
                bool ok = false;
                const QString name = QInputDialog::getText(
                    this, "Save Layout", "Layout name:", QLineEdit::Normal,
                    QString(), &ok);
                if (ok && !name.trimmed().isEmpty()) {
                    dock_manager_->addPerspective(name.trimmed());
                    LOG_INFO("MainWindow", QString("Saved perspective: %1").arg(name.trimmed()));
                }
            }
        } else if (action.startsWith("perspective_")) {
            // Quick Switch — navigate directly to a preset screen layout
            // 2 screens = 1:1 side-by-side, 4 screens = 2x2 grid
            static const QMap<QString, QStringList> view_screens = {
                // Trading
                {"perspective_trading",       {"crypto_trading", "watchlist", "markets", "news"}},
                {"perspective_equity",        {"equity_trading", "watchlist"}},
                {"perspective_algo",          {"algo_trading", "backtesting"}},
                // Research
                {"perspective_research",      {"equity_research", "markets", "screener", "news"}},
                {"perspective_derivatives",   {"derivatives", "surface_analytics"}},
                {"perspective_ma",            {"ma_analytics", "news"}},
                // Portfolio
                {"perspective_portfolio",     {"portfolio", "markets", "watchlist", "news"}},
                // News & Markets
                {"perspective_news",          {"news", "markets"}},
                {"perspective_markets",       {"markets", "watchlist"}},
                // Economics & Data
                {"perspective_economics",     {"economics", "dbnomics", "gov_data", "asia_markets"}},
                {"perspective_data",          {"data_sources", "data_mapping"}},
                // Geopolitics
                {"perspective_geopolitics",   {"geopolitics", "maritime", "relationship_map", "news"}},
                // AI & Quant
                {"perspective_quant",         {"ai_quant_lab", "quantlib", "backtesting", "markets"}},
                {"perspective_ai",            {"ai_chat", "agent_config"}},
                // Tools
                {"perspective_tools",         {"code_editor", "node_editor"}},
            };
            const auto it = view_screens.find(action);
            if (it != view_screens.end() && dock_router_) {
                const QStringList& screens = it.value();
                if (!screens.isEmpty()) {
                    dock_router_->navigate(screens[0], true); // first exclusive
                    for (int i = 1; i < screens.size(); ++i)
                        dock_router_->navigate(screens[i]);   // fill grid
                }
                LOG_INFO("MainWindow", QString("Quick Switch: %1 (%2 panels)")
                         .arg(action).arg(screens.size()));
            }
        } else if (action == "logout") {
            auth::AuthManager::instance().logout();
        } else if (action == "fullscreen") {
            if (isFullScreen())
                showNormal();
            else
                showFullScreen();
        } else if (action == "focus_mode") {
            focus_mode_ = !focus_mode_;
            if (dock_toolbar_)    dock_toolbar_->setVisible(!focus_mode_);
            if (dock_status_bar_) dock_status_bar_->setVisible(!focus_mode_);
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
            if (dock_manager_) {
                auto* focused = dock_manager_->focusedDockWidget();
                if (focused && focused->widget())
                    QMetaObject::invokeMethod(focused->widget(), "refresh", Qt::QueuedConnection);
            }
        } else if (action == "new_workspace") {
            auto* dlg = new ui::WorkspaceNewDialog(this);
            if (dlg->exec() == QDialog::Accepted)
                WorkspaceManager::instance().new_workspace(
                    dlg->workspace_name(), dlg->workspace_description(), dlg->selected_template_id());
            dlg->deleteLater();
        } else if (action == "open_workspace") {
            auto* dlg = new ui::WorkspaceOpenDialog(this);
            if (dlg->exec() == QDialog::Accepted && !dlg->selected_path().isEmpty())
                WorkspaceManager::instance().open_workspace(dlg->selected_path());
            dlg->deleteLater();
        } else if (action == "save_workspace") {
            WorkspaceManager::instance().save_workspace();
        } else if (action == "save_workspace_as") {
            auto* dlg = new ui::WorkspaceSaveAsDialog(this);
            if (dlg->exec() == QDialog::Accepted)
                WorkspaceManager::instance().save_workspace_as(dlg->new_name(), dlg->chosen_path());
            dlg->deleteLater();
        } else if (action == "import_data") {
            QString path = QFileDialog::getOpenFileName(
                this, "Import Workspace", QDir::homePath(), "Fincept Workspace (*.fwsp)");
            if (!path.isEmpty())
                WorkspaceManager::instance().import_workspace(path);
        } else if (action == "export_data") {
            QString path = QFileDialog::getSaveFileName(
                this, "Export Workspace", QDir::homePath(), "Fincept Workspace (*.fwsp)");
            if (!path.isEmpty())
                WorkspaceManager::instance().export_workspace(path);
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

    // Restore window state (maximised, toolbar positions, etc.)
    // Note: restoreState() also restores QToolBar visibility, which can
    // hide the toolbar from stale/corrupt saved state. We force it visible
    // afterward unless the user is intentionally in focus/chat mode.
    const QByteArray saved_state = SessionManager::instance().load_state(window_id_);
    if (!saved_state.isEmpty())
        restoreState(saved_state);

    // Always ensure toolbar and status bar are visible after state restore.
    // Focus mode and chat mode will hide them explicitly when toggled.
    if (dock_toolbar_)    dock_toolbar_->setVisible(true);
    if (dock_status_bar_) dock_status_bar_->setVisible(true);

    // Restore ADS dock layout — must happen after all screens are registered
    // but BEFORE the initial navigate, so we don't create a widget that
    // conflicts with restoreState's layout.
    // Layout version: bump this whenever the dock layout format changes to
    // automatically discard stale/corrupt saved state from previous versions.
    static constexpr int kDockLayoutVersion = 4;
    bool dock_restored = false;

    if (dock_manager_) {
        QSettings persp_settings;
        SessionManager::instance().load_perspectives(persp_settings);
        dock_manager_->loadPerspectives(persp_settings);

        const int saved_version = SessionManager::instance().dock_layout_version(window_id_);
        if (saved_version == kDockLayoutVersion) {
            const QByteArray saved_dock = SessionManager::instance().load_dock_layout(window_id_);
            if (!saved_dock.isEmpty()) {
                dock_router_->ensure_all_registered();
                dock_restored = dock_manager_->restoreState(saved_dock);

                // Sanity check: if restoreState produced an unreasonable number
                // of visible dock areas (>6), the layout is likely corrupt.
                if (dock_restored && dock_manager_->openedDockAreas().size() > 6) {
                    LOG_WARN("MainWindow", QString("Dock layout corrupt: %1 open areas — resetting")
                             .arg(dock_manager_->openedDockAreas().size()));
                    dock_restored = false;
                }
            }
        } else if (saved_version != 0) {
            LOG_INFO("MainWindow", QString("Dock layout version mismatch (saved %1, expected %2) — resetting")
                     .arg(saved_version).arg(kDockLayoutVersion));
        }

        if (!dock_restored) {
            // Close all dock widgets that may have been opened by a failed restore
            for (auto* dw : dock_manager_->dockWidgetsMap())
                dw->toggleView(false);
        }

        // Save the current version so next startup knows the format.
        SessionManager::instance().set_dock_layout_version(window_id_, kDockLayoutVersion);
    }

    // Show the app or auth stack based on authentication state.
    // If dock layout was restored, the saved tabs are already visible.
    // Otherwise, navigate to dashboard as default.
    auto& auth_mgr = auth::AuthManager::instance();
    if (auth_mgr.is_authenticated() || auth_mgr.is_loading()) {
        // If user is authenticated and has a PIN, show lock screen first.
        // If no PIN, on_auth_state_changed will route to PIN setup.
        if (auth_mgr.is_authenticated() && auth::PinManager::instance().has_pin()) {
            LOG_INFO("MainWindow", "Session restored — showing PIN unlock");
            lock_screen_->show_unlock();
            stack_->setCurrentIndex(3);
        } else if (auth_mgr.is_authenticated()) {
            // Authenticated but no PIN yet — will be caught by on_auth_state_changed
            on_auth_state_changed();
        } else {
            // Still loading — show app stack temporarily (loading state)
            stack_->setCurrentIndex(1);
        }
        if (!dock_restored) {
            dock_router_->navigate("dashboard");
            LOG_INFO("MainWindow", "Applied clean default dock layout");
        } else {
            // Restore last-active screen as the focused tab and sync tab bar
            const QString last = SessionManager::instance().last_screen();
            if (!last.isEmpty()) {
                auto* dw = dock_router_->find_dock_widget(last);
                if (dw && !dw->isClosed()) {
                    dw->raise();
                    dw->setAsCurrentTab();
                }
                tab_bar_->set_active(last);
            }
        }
    } else {
        on_auth_state_changed();
    }

    // Periodic refresh of user credits/plan (every 3 minutes)
    user_refresh_timer_ = new QTimer(this);
    user_refresh_timer_->setInterval(3 * 60 * 1000);
    connect(user_refresh_timer_, &QTimer::timeout, this, []() {
        auto& auth = auth::AuthManager::instance();
        if (auth.is_authenticated())
            auth.refresh_user_data();
    });
    user_refresh_timer_->start();

    // Confetti overlay (parented to central widget so it covers the whole app)
    // Refresh user data when app regains focus (updates toolbar credits/plan)
    connect(qApp, &QApplication::applicationStateChanged, this, [](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            auto& auth = auth::AuthManager::instance();
            if (auth.is_authenticated())
                auth.refresh_user_data();
        }
    });
}

void MainWindow::toggle_chat_mode() {
    chat_mode_ = !chat_mode_;

    if (chat_mode_) {
        if (dock_toolbar_)     dock_toolbar_->setVisible(false);
        if (dock_status_bar_)  dock_status_bar_->setVisible(false);
        if (chat_bubble_)       chat_bubble_->setVisible(false);
        stack_->setCurrentIndex(2);
        LOG_INFO("MainWindow", "Entered Chat Mode");
    } else {
        stack_->setCurrentIndex(1);
        if (dock_toolbar_)     dock_toolbar_->setVisible(true);
        if (dock_status_bar_)  dock_status_bar_->setVisible(true);
        // Restore chat bubble based on setting
        if (chat_bubble_) {
            auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
            const bool show = !r.is_ok() || r.value() != "false";
            chat_bubble_->setVisible(show);
            if (show) { chat_bubble_->reposition(); chat_bubble_->raise(); }
        }
        LOG_INFO("MainWindow", "Exited Chat Mode");
    }
}

void MainWindow::setup_auth_screens() {
    auto* login = new screens::LoginScreen;
    auto* reg = new screens::RegisterScreen;
    auto* forgot = new screens::ForgotPasswordScreen;
    auto* pricing = new screens::PricingScreen;

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

    auth_stack_->addWidget(login);       // index 0
    auth_stack_->addWidget(reg);         // index 1
    auth_stack_->addWidget(forgot);      // index 2
    auth_stack_->addWidget(pricing);     // index 3
    auth_stack_->addWidget(info_stack_); // index 4

    // ── Auth navigation ──────────────────────────────────────────────────────
    connect(login, &screens::LoginScreen::navigate_register, this, &MainWindow::show_register);
    connect(login, &screens::LoginScreen::navigate_forgot_password, this, &MainWindow::show_forgot_password);
    connect(reg, &screens::RegisterScreen::navigate_login, this, &MainWindow::show_login);
    connect(forgot, &screens::ForgotPasswordScreen::navigate_login, this, &MainWindow::show_login);
    connect(pricing, &screens::PricingScreen::navigate_dashboard, this, [this]() {
        stack_->setCurrentIndex(1);
        dock_router_->navigate("dashboard");
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

void MainWindow::setup_docking_mode() {
    // ADS global config must be set before the first CDockManager is constructed.
    // Guard with a static flag since multiple MainWindow instances share one process.
    static bool s_ads_configured = false;
    if (!s_ads_configured) {
        ads::CDockManager::setConfigFlags(
            ads::CDockManager::DefaultOpaqueConfig
            | ads::CDockManager::FocusHighlighting
            | ads::CDockManager::AlwaysShowTabs
            | ads::CDockManager::DockAreaHasTabsMenuButton
            | ads::CDockManager::DockAreaDynamicTabsMenuButtonVisibility
            | ads::CDockManager::FloatingContainerHasWidgetTitle
            | ads::CDockManager::FloatingContainerHasWidgetIcon
            | ads::CDockManager::EqualSplitOnInsertion       // prevents new panels from spawning tiny slivers
            | ads::CDockManager::DoubleClickUndocksWidget    // double-click tab to float
        );
        // Disable auto-hide entirely: the pin button converts panels to collapsible
        // sidebars which collapse when another panel is opened beside them.
        ads::CDockManager::setAutoHideConfigFlags(
            ads::CDockManager::AutoHideFlags(0)
        );
        s_ads_configured = true;
    }

    // Create CDockManager — parent is a plain QWidget wrapper (not QMainWindow)
    // so ADS does NOT call setCentralWidget(). The wrapper goes into master_stack.
    auto* dock_wrapper = new QWidget;
    auto* dock_layout = new QVBoxLayout(dock_wrapper);
    dock_layout->setContentsMargins(0, 0, 0, 0);
    dock_layout->setSpacing(0);

    tab_bar_ = new ui::TabBar(dock_wrapper);
    dock_layout->addWidget(tab_bar_);

    dock_manager_ = new ads::CDockManager(dock_wrapper);
    dock_layout->addWidget(dock_manager_);

    // ADS styling is handled by ThemeManager's global QSS — no widget-level
    // stylesheet needed here. The global QSS has higher priority and won't be
    // overridden by theme changes.
}

void MainWindow::setup_dock_screens() {
    dock_router_->register_factory("dashboard", []() { return new screens::DashboardScreen; });
    dock_router_->register_factory("markets", []() { return new screens::MarketsScreen; });
    dock_router_->register_factory("crypto_trading", []() { return new screens::CryptoTradingScreen; });
    dock_router_->register_factory("news", []() { return new screens::NewsScreen; });
    dock_router_->register_factory("forum", []() { return new screens::ForumScreen; });
    dock_router_->register_factory("watchlist", []() { return new screens::WatchlistScreen; });

    // Eagerly constructed: lightweight screens with no data fetching or timers.
    dock_router_->register_screen("report_builder", new screens::ReportBuilderScreen);
    dock_router_->register_screen("profile", new screens::ProfileScreen);
    dock_router_->register_screen("settings", new screens::SettingsScreen);
    dock_router_->register_screen("about", new screens::AboutScreen);
    dock_router_->register_screen("support", new screens::SupportScreen);
    dock_router_->register_screen("notes", new screens::NotesScreen);

    dock_router_->register_factory("portfolio", []() { return new screens::PortfolioScreen; });
    dock_router_->register_factory("ai_chat", []() { return new screens::AiChatScreen; });
    dock_router_->register_factory("backtesting", []() { return new screens::BacktestingScreen; });
    dock_router_->register_factory("algo_trading", []() { return new screens::AlgoTradingScreen; });
    dock_router_->register_factory("node_editor", []() { return new workflow::NodeEditorScreen; });
    dock_router_->register_factory("code_editor", []() { return new screens::CodeEditorScreen; });
    dock_router_->register_factory("ai_quant_lab", []() { return new screens::AIQuantLabScreen; });
    dock_router_->register_factory("quantlib", []() { return new screens::QuantLibScreen; });
    dock_router_->register_factory("economics", []() { return new screens::EconomicsScreen; });
    dock_router_->register_factory("gov_data", []() { return new screens::GovDataScreen; });
    dock_router_->register_factory("dbnomics", []() { return new screens::DBnomicsScreen; });
    dock_router_->register_factory("akshare", []() { return new screens::AkShareScreen; });
    dock_router_->register_factory("asia_markets", []() { return new screens::AsiaMarketsScreen; });
    dock_router_->register_factory("relationship_map", []() { return new screens::RelationshipMapScreen; });
    dock_router_->register_factory("equity_trading", []() { return new screens::EquityTradingScreen; });
    dock_router_->register_factory("alpha_arena", []() { return new screens::AlphaArenaScreen; });
    dock_router_->register_factory("polymarket", []() { return new screens::PolymarketScreen; });
    dock_router_->register_factory("derivatives", []() { return new screens::DerivativesScreen; });
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
        connect(fm, &screens::FileManagerScreen::open_file_in_screen,
                this, [this](const QString& route_id, const QString& /*file_path*/) {
                    dock_router_->navigate(route_id);
                });
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
    dock_router_->register_screen("help", new screens::HelpScreen);
}

void MainWindow::setup_app_screens() {}
void MainWindow::setup_navigation() {}

void MainWindow::on_auth_state_changed() {
    auto& auth = auth::AuthManager::instance();

    // Still validating saved session — don't flash the login screen.
    // Stay on whatever is currently shown until validation completes.
    if (auth.is_loading())
        return;

    if (auth.is_authenticated()) {
        // Don't redirect if user is already on the app stack (dashboard/workspace
        // at index 1, or chat mode at index 2) or is intentionally viewing the
        // pricing screen from the toolbar.
        if (stack_->currentIndex() == 1 || stack_->currentIndex() == 2) {
            LOG_DEBUG("MainWindow", QString("on_auth_state_changed: skipping redirect, "
                      "stack index=%1, chat_mode=%2")
                      .arg(stack_->currentIndex()).arg(chat_mode_));
            return;
        }
        if (stack_->currentIndex() == 0 && auth_stack_->currentIndex() == 3)
            return;  // user is on pricing screen — let PricingScreen handle it

        // ── PIN gate: require PIN setup or PIN unlock before proceeding ──
        // On first login (no PIN configured): show mandatory PIN setup.
        // On subsequent launches (PIN exists): show PIN unlock.
        // Skip if user is already on the lock screen (index 3).
        if (stack_->currentIndex() != 3) {
            if (auth.needs_pin_setup()) {
                LOG_INFO("MainWindow", "Authenticated but no PIN — showing PIN setup");
                lock_screen_->show_setup();
                stack_->setCurrentIndex(3);
                return;
            }
            if (auth::PinManager::instance().has_pin()) {
                LOG_INFO("MainWindow", "Authenticated with PIN — showing PIN unlock");
                lock_screen_->show_unlock();
                stack_->setCurrentIndex(3);
                return;
            }
        }

        if (auth.session().has_paid_plan()) {
            // Paid user → straight to dashboard
            stack_->setCurrentIndex(1);
            WorkspaceManager::instance().load_last_workspace();
            // Trigger silent update check after login (delayed so UI settles first)
            QTimer::singleShot(3000, this, []() {
                services::UpdateService::instance().check_for_updates(true);
            });
        } else {
            // Free/no plan → show pricing gate
            stack_->setCurrentIndex(0);
            auth_stack_->setCurrentIndex(3);
        }
    } else {
        // Disable inactivity guard when logged out
        auth::InactivityGuard::instance().set_enabled(false);
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
void MainWindow::show_info_contact() {
    info_stack_->setCurrentIndex(0);
    auth_stack_->setCurrentIndex(4);
}
void MainWindow::show_info_terms() {
    info_stack_->setCurrentIndex(1);
    auth_stack_->setCurrentIndex(4);
}
void MainWindow::show_info_privacy() {
    info_stack_->setCurrentIndex(2);
    auth_stack_->setCurrentIndex(4);
}
void MainWindow::show_info_trademarks() {
    info_stack_->setCurrentIndex(3);
    auth_stack_->setCurrentIndex(4);
}
void MainWindow::show_info_help() {
    info_stack_->setCurrentIndex(4);
    auth_stack_->setCurrentIndex(4);
}

void MainWindow::show_lock_screen() {
    auto& auth = auth::AuthManager::instance();
    if (!auth.is_authenticated())
        return;

    // Don't lock if already on lock screen or login screen
    if (stack_->currentIndex() == 3 || stack_->currentIndex() == 0)
        return;

    LOG_INFO("MainWindow", "Inactivity timeout — showing lock screen");
    lock_screen_->activate();
    stack_->setCurrentIndex(3);

    // Hide toolbar/statusbar while locked
    if (dock_toolbar_)    dock_toolbar_->setVisible(false);
    if (dock_status_bar_) dock_status_bar_->setVisible(false);
    if (chat_bubble_)     chat_bubble_->setVisible(false);
}

void MainWindow::on_terminal_unlocked() {
    auto& auth = auth::AuthManager::instance();
    LOG_INFO("MainWindow", "Terminal unlocked via PIN");

    // Enable/restart inactivity guard
    auto& guard = auth::InactivityGuard::instance();
    if (!guard.is_enabled()) {
        qApp->installEventFilter(&guard);
        guard.set_enabled(true);
    }
    guard.reset_timer();

    // Reset PIN lockout on successful unlock
    auth::PinManager::instance().reset_lockout();

    if (auth.session().has_paid_plan()) {
        stack_->setCurrentIndex(1);
        // Restore toolbar/statusbar
        if (dock_toolbar_)    dock_toolbar_->setVisible(!focus_mode_ && !chat_mode_);
        if (dock_status_bar_) dock_status_bar_->setVisible(!focus_mode_ && !chat_mode_);
        // Restore chat bubble based on setting
        if (chat_bubble_) {
            auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
            bool show = !r.is_ok() || r.value() != "false";
            chat_bubble_->setVisible(show);
            if (show) { chat_bubble_->reposition(); chat_bubble_->raise(); }
        }
        WorkspaceManager::instance().load_last_workspace();
        QTimer::singleShot(3000, this, []() {
            services::UpdateService::instance().check_for_updates(true);
        });
    } else {
        // Free/no plan → pricing gate
        stack_->setCurrentIndex(0);
        auth_stack_->setCurrentIndex(3);
    }

    emit auth.terminal_unlocked();
}

void MainWindow::update_window_title() {
    QString title = "Fincept Terminal";

    const QString profile = ProfileManager::instance().active();
    if (profile != "default")
        title += QString(" [%1]").arg(profile);

    if (WorkspaceManager::instance().has_current_workspace()) {
        const QString ws = WorkspaceManager::instance().current_workspace_name();
        if (!ws.isEmpty())
            title += QString(" — %1").arg(ws);
    }

    if (dock_router_) {
        const QString id = dock_router_->current_screen_id();
        if (!id.isEmpty())
            title += QString(" — %1").arg(DockScreenRouter::title_for_id(id));
    }

    setWindowTitle(title);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    WorkspaceManager::instance().save_workspace();
    SessionManager::instance().save_geometry(window_id_, saveGeometry(), saveState());
    if (dock_manager_) {
        SessionManager::instance().save_dock_layout(window_id_, dock_manager_->saveState());
        QSettings tmp;
        dock_manager_->savePerspectives(tmp);
        SessionManager::instance().save_perspectives(tmp);
    }
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (chat_bubble_)
        chat_bubble_->reposition();
}

} // namespace fincept
