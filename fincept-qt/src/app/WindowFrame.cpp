#include "app/WindowFrame.h"

#include "ai_chat/AiChatBubble.h"
#include "ai_chat/AiChatScreen.h"
#include "ai_chat/LlmService.h"
#include "app/DockScreenRouter.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/lock/LockOverlayController.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolGroup.h"
#include "screens/IStatefulScreen.h"
#include "core/config/ProfileManager.h"
#include "core/events/EventBus.h"
#include "app/TerminalShell.h"
#include "core/actions/ActionRegistry.h"
#include "core/actions/builtin_actions.h"
#include "core/keys/KeyConfigManager.h"
#include "core/keys/WindowCycler.h"
#include "core/window/WindowRegistry.h"
#include "core/logging/Logger.h"
#include "core/session/SessionManager.h"
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
#include "screens/auth/LockScreen.h"
#include "screens/auth/LoginScreen.h"
#include "screens/auth/PricingScreen.h"
#include "screens/auth/RegisterScreen.h"
#include "screens/backtesting/BacktestingScreen.h"
#include "screens/chat_mode/ChatModeScreen.h"
#include "screens/code_editor/CodeEditorScreen.h"
#include "screens/crypto_trading/CryptoTradingScreen.h"
#include "screens/dashboard/DashboardScreen.h"
#include "screens/data_mapping/DataMappingScreen.h"
#include "screens/data_sources/DataSourcesScreen.h"
#include "screens/dbnomics/DBnomicsScreen.h"
#include "screens/derivatives/DerivativesScreen.h"
#include "screens/docs/DocsScreen.h"
#include "screens/crypto_center/CryptoCenterScreen.h"
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
#include "services/updater/UpdateService.h"
#include "services/workspace/WorkspaceManager.h"
#include "trading/instruments/InstrumentService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/navigation/DockStatusBar.h"
#include "ui/navigation/DockToolBar.h"
#include "ui/navigation/FKeyBar.h"
#include "ui/command/QuickCommandBar.h"
#include "ui/command/CommandPalette.h"
#include "ui/components/ComponentBrowserDialog.h"
#include "ui/debug/DebugOverlay.h"
#include "ui/navigation/NavigationBar.h"
#include "ui/navigation/StatusBar.h"
#include "ui/navigation/ToolBar.h"
#include "ui/pushpins/PushpinBar.h"
#include "ui/theme/Theme.h"
#include "ui/workspace/WorkspaceNewDialog.h"
#include "ui/workspace/WorkspaceOpenDialog.h"
#include "ui/workspace/WorkspaceSaveAsDialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPalette>
#include <QScreen>
#include <QShortcut>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWindow>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <FloatingDockContainer.h>

#include <algorithm>

namespace fincept {

int WindowFrame::next_window_id() {
    // Seed from the max of (persisted window IDs, live window IDs) so a new
    // window never reuses an ID that already owns saved geometry/dock layout.
    // Live IDs matter too: during a single session we may have deleted a
    // secondary window without clearing its QSettings keys yet, and we still
    // must not collide with any WindowFrame currently on screen.
    int max_id = 0;
    const auto saved = SessionManager::instance().load_window_ids();
    for (int id : saved)
        max_id = std::max(max_id, id);
    // Live frames (Phase 1: WindowRegistry replaces the topLevelWidgets walk).
    for (int id : WindowRegistry::instance().frame_ids())
        max_id = std::max(max_id, id);
    return max_id + 1;
}

WindowFrame::WindowFrame(int window_id, QWidget* parent) : QMainWindow(parent), window_id_(window_id) {
    // Phase 8: seed the dynamic property DataHub uses to gate
    // pause_when_inactive topics. Initial state = active (we wouldn't be
    // constructing the frame if we didn't intend to show it). changeEvent
    // updates this on every minimise/restore.
    setProperty("fincept.active_for_work", true);

    // Phase 6 trim: connect QWindow::screenChanged so paint-cached widgets
    // can invalidate at the new DPR. The QWindow is created lazily on
    // first show — queue the connect via QMetaObject::invokeMethod with
    // QueuedConnection so it runs after the event loop has had a chance
    // to construct the window handle.
    QMetaObject::invokeMethod(this, [this]() {
        if (auto* wh = windowHandle()) {
            connect(wh, &QWindow::screenChanged, this,
                    [this](QScreen* scr) { emit screen_changed(scr); });
        }
    }, Qt::QueuedConnection);

    // Show active profile in title bar when using a non-default profile
    const QString profile = ProfileManager::instance().active();
    setWindowTitle(profile == "default" ? "Fincept Terminal" : QString("Fincept Terminal [%1]").arg(profile));
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
    bool geometry_applied = false;
    if (!saved_geom.isEmpty() && restoreGeometry(saved_geom)) {
        // Verify the restored frame actually intersects a currently-connected
        // screen — if the user unplugged a monitor between sessions, Qt will
        // happily hand back off-screen coordinates and the window becomes
        // invisible/unreachable. Guard against that by falling through to
        // smart placement if no intersection exists.
        const QRect frame = frameGeometry();
        bool on_some_screen = false;
        const auto screens = QGuiApplication::screens();
        for (QScreen* s : screens) {
            if (s->availableGeometry().intersects(frame)) {
                on_some_screen = true;
                break;
            }
        }
        geometry_applied = on_some_screen;
    }

    if (!geometry_applied) {
        // Prefer the screen the window lived on last session (by name); fall
        // back to primary if that screen is gone.
        QScreen* target = nullptr;
        const QString saved_screen = SessionManager::instance().load_screen_name(window_id_);
        if (!saved_screen.isEmpty()) {
            for (QScreen* s : QGuiApplication::screens()) {
                if (s->name() == saved_screen) {
                    target = s;
                    break;
                }
            }
        }
        if (!target)
            target = QApplication::primaryScreen();
        if (target) {
            QRect geom = target->availableGeometry();
            resize(geom.width() * 9 / 10, geom.height() * 9 / 10);
            // Only centre-place for the primary window; secondary windows
            // are positioned by the "new window" action after construction.
            if (window_id_ == 0)
                move(geom.center() - rect().center());
            else
                move(geom.center() - rect().center());
        }
    }

    // Restore always-on-top before the first show() so the OS hint is
    // applied cleanly. Cannot use set_always_on_top() here — it would call
    // show() which only works after the widget has a fully built-out layout.
    always_on_top_ = SessionManager::instance().load_window_flag(window_id_, "always_on_top", false);
    if (always_on_top_)
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    // Bootstrap pin_gate_cleared_ from the process-wide locked flag. Without
    // this, opening a second WindowFrame (--new-window IPC) while the user is
    // already PIN-unlocked would treat the new window as un-gated and force
    // a redundant PIN re-entry. The singleton flag is the source of truth for
    // "user has cleared the PIN gate this session"; the per-window field is
    // a cache so on_auth_state_changed() can skip re-prompting.
    pin_gate_cleared_ = !auth::InactivityGuard::instance().is_terminal_locked()
                        && auth::AuthManager::instance().is_authenticated()
                        && auth::PinManager::instance().has_pin();

    auto* master_stack = new QStackedWidget;

    // ── Auth stack ───────────────────────────────────────────────────────────
    auth_stack_ = new QStackedWidget;
    setup_auth_screens();
    master_stack->addWidget(auth_stack_);

    // ── Chat Mode ─────────────────────────────────────────────────────────────
    chat_mode_screen_ = new chat_mode::ChatModeScreen;

    // ── Lock Screen ─────────────────────────────────────────────────────────
    // Phase 1c lift: LockOverlayController owns LockScreen widgets. The
    // frame asks for its widget via the controller; master_stack reparents
    // it via addWidget() below. WindowFrame keeps a non-owning pointer for
    // the existing apply_lock_state / show_unlock / show_setup paths that
    // already exist (preserving security-audit invariants — see
    // auth/lock/LockOverlayController.h).
    lock_screen_ = auth::LockOverlayController::instance().lock_screen_for(this);

    // ── ADS Docking mode ─────────────────────────────────────────────────────
    setup_docking_mode();
    master_stack->addWidget(dock_manager_->parentWidget()); // index 1 — dock_wrapper
    master_stack->addWidget(chat_mode_screen_);             // index 2
    master_stack->addWidget(lock_screen_);                  // index 3 — lock/PIN screen
    connect(chat_mode_screen_, &chat_mode::ChatModeScreen::exit_requested, this, &WindowFrame::toggle_chat_mode);

    // Lock screen signals
    connect(lock_screen_, &screens::LockScreen::unlocked, this, &WindowFrame::on_terminal_unlocked);
    connect(lock_screen_, &screens::LockScreen::reauth_requested, this, []() {
        // Max PIN attempts exceeded — wipe the PIN (this is the ONLY path
        // that should clear it) and force a full re-login so the server
        // re-validates the user before they can configure a new PIN.
        auth::PinManager::instance().clear_pin();
        auth::AuthManager::instance().logout();
    });

    // Inactivity guard → lock screen. The originator window handles
    // lock_requested by flipping the process-wide locked flag; the
    // terminal_locked_changed signal then drives every WindowFrame
    // (including this one, idempotently) to apply the lock UI. Without
    // this lock-step, a secondary window would keep showing live data
    // behind the back of the lock screen on the primary.
    connect(&auth::InactivityGuard::instance(), &auth::InactivityGuard::lock_requested, this,
            &WindowFrame::show_lock_screen);
    connect(&auth::InactivityGuard::instance(), &auth::InactivityGuard::terminal_locked_changed,
            this, &WindowFrame::apply_lock_state);

    setCentralWidget(master_stack);

    dock_toolbar_ = new ui::DockToolBar(this);
    addToolBar(Qt::TopToolBarArea, dock_toolbar_);

    // Pushpin bar lives on its own toolbar row below the main toolbar so the
    // chips are always visible and drop targets across all panels. Start a
    // new toolbar line via addToolBarBreak().
    addToolBarBreak(Qt::TopToolBarArea);
    pushpin_bar_ = new ui::PushpinBar;
    pushpin_toolbar_ = new QToolBar(this);
    pushpin_toolbar_->setObjectName("pushpinToolbar");
    pushpin_toolbar_->setMovable(false);
    pushpin_toolbar_->setFloatable(false);
    pushpin_toolbar_->setAllowedAreas(Qt::TopToolBarArea);
    pushpin_toolbar_->addWidget(pushpin_bar_);
    addToolBar(Qt::TopToolBarArea, pushpin_toolbar_);
    // Hidden until auth completes; set_shell_visible() shows it.
    pushpin_toolbar_->setVisible(false);

    dock_status_bar_ = new ui::DockStatusBar(this);
    setStatusBar(dock_status_bar_);

    stack_ = master_stack;
    auto* toolbar = dock_toolbar_->inner();

    // ── Workspace + signal wiring ───────────────────────────────────────────
    WorkspaceManager::instance().set_main_window(this);
    WindowCycler::instance().register_window(this);
    connect(this, &QObject::destroyed, this,
            [this]() { WindowCycler::instance().unregister_window(this); });
    connect(&WorkspaceManager::instance(), &WorkspaceManager::workspace_loaded, this,
            [this](const WorkspaceDef&) { update_window_title(); });
    connect(&WorkspaceManager::instance(), &WorkspaceManager::workspace_error, this,
            [this](const QString& msg) { QMessageBox::warning(this, "Workspace Error", msg); });

    dock_router_ = new DockScreenRouter(dock_manager_, this);
    setup_dock_screens();

    // Tab bar navigation: open the selected screen exclusively — closes all other
    // panels and fills the full area. Each tab is a fresh independent screen,
    // not nested into whatever was previously open.
    connect(tab_bar_, &ui::TabBar::tab_changed, this, [this](const QString& id) {
        if (!locked_) dock_router_->navigate(id, true);
    });
    connect(dock_router_, &DockScreenRouter::screen_changed, tab_bar_, &ui::TabBar::set_active);

    // Update the main window title bar to reflect the current screen name.
    connect(dock_router_, &DockScreenRouter::screen_changed, this, [this](const QString&) { update_window_title(); });

    // Per-screen tool filter wiring removed (Tool RAG / Tier-0 model).
    // The LLM now sees a 6-tool Tier-0 prefix on every turn and discovers
    // the rest via tool.list — making per-screen scoping unnecessary and
    // counterproductive (it would prevent the LLM from finding tools the
    // user might want regardless of which screen happens to be active).

    connect(this, &QObject::destroyed, this, [this]() { WorkspaceManager::instance().remove_window(this); });

    // Chat bubble — floats over dock_manager_ content area
    chat_bubble_ = new AiChatBubble(dock_manager_);
    {
        auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
        bool show = !r.is_ok() || r.value() != "false";
        chat_bubble_->setVisible(show);
        if (show)
            chat_bubble_->raise();
    }

    // Re-raise and reposition the bubble after each navigation — dock geometry
    // shifts when panels open/close, so the bubble needs to re-anchor itself.
    connect(dock_router_, &DockScreenRouter::screen_changed, this, [this](const QString&) {
        if (!chat_bubble_)
            return;
        auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
        bool show = !r.is_ok() || r.value() != "false";
        chat_bubble_->setVisible(show);
        if (show) {
            chat_bubble_->reposition();
            chat_bubble_->raise();
        }
    });

    connect(toolbar, &ui::ToolBar::chat_mode_toggled, this, &WindowFrame::toggle_chat_mode);
    connect(toolbar, &ui::ToolBar::navigate_to, this, [this](const QString& id) {
        if (!locked_) dock_router_->navigate(id, true);
    });

    // Debounced dock-layout save: ADS emits a burst of signals during
    // add/replace/remove; coalesce into one write ~500 ms after the last change
    // so rapid edits don't hammer QSettings and so crashes preserve layout.
    dock_layout_save_timer_ = new QTimer(this);
    dock_layout_save_timer_->setSingleShot(true);
    dock_layout_save_timer_->setInterval(500);
    connect(dock_layout_save_timer_, &QTimer::timeout, this, [this]() {
        if (!dock_manager_)
            return;
        SessionManager::instance().save_dock_layout(window_id_, dock_manager_->saveState());
        if (WorkspaceManager::instance().has_current_workspace())
            WorkspaceManager::instance().save_workspace();
    });

    connect(toolbar, &ui::ToolBar::dock_command, this,
            [this](const QString& action, const QString& primary, const QString& secondary) {
                if (locked_) return;
                if (action == "add")
                    dock_router_->add_alongside(primary, secondary);
                else if (action == "remove")
                    dock_router_->remove_screen(primary);
                else if (action == "replace")
                    dock_router_->replace_screen(primary, secondary);
                schedule_dock_layout_save();
            });

    // MCP navigation tool → dock_router (cross-thread safe)
    EventBus::instance().subscribe("nav.switch_screen", [this](const QVariantMap& nav_data) {
        QString screen_id = nav_data["screen_id"].toString();
        if (!screen_id.isEmpty())
            QMetaObject::invokeMethod(
                dock_router_, [this, screen_id]() {
                    if (!locked_) dock_router_->navigate(screen_id);
                }, Qt::QueuedConnection);
    });

    // ── Keyboard shortcuts via ActionRegistry (Phase 4) ───────────────────────
    //
    // Every global action is now defined once in core/actions/builtin_actions.cpp
    // and registered with ActionRegistry. KeyConfigManager still owns the
    // QAction-per-KeyAction (so live re-binding via Settings → Keybindings
    // continues to work), but each frame routes the QAction's `triggered`
    // signal through ActionRegistry::invoke(action_id, ctx) instead of the
    // pre-Phase-4 captured-`this` lambdas.
    //
    // The captured-`this` form had a latent bug in multi-window setups: an
    // action triggered from frame B would still execute against frame A's
    // state (locked_, focus_mode_, etc.) because `this` was fixed at bind
    // time. The registry resolves the focused frame live at invoke time.
    auto& km = KeyConfigManager::instance();
    auto& reg = ActionRegistry::instance();
    for (KeyAction a : km.all_actions()) {
        const QString action_id = actions::action_id_for(a);
        if (action_id.isEmpty())
            continue;                                  // per-screen action; handled elsewhere
        if (!reg.contains(action_id)) {
            LOG_WARN("WindowFrame", QString("KeyAction → action_id mapping references unregistered id: %1")
                                        .arg(action_id));
            continue;
        }
        auto* act = km.action(a);
        if (!act)
            continue;
        // LockNow runs even when this frame isn't focused (e.g. user pressed
        // it from a dialog) — it's a global "secure my session" command.
        // Everything else is window-scoped so only the focused frame fires.
        act->setShortcutContext(a == KeyAction::LockNow ? Qt::ApplicationShortcut
                                                        : Qt::WindowShortcut);
        addAction(act);
        connect(act, &QAction::triggered, this, [action_id]() {
            // Build a fresh context every invocation so the focused frame
            // is resolved live rather than captured. This is the bug-fix
            // that motivated the refactor.
            CommandContext ctx;
            ctx.shell = &TerminalShell::instance();
            ctx.focused_frame = WindowCycler::instance().focused_frame();
            // ctx.focused_panel left null — Phase 7 wires PanelRegistry's
            // focused-panel lookup once panels are UUID-keyed.
            auto r = ActionRegistry::instance().invoke(action_id, ctx);
            if (r.is_err()) {
                LOG_DEBUG("WindowFrame", QString("Action %1 returned error: %2")
                                             .arg(action_id, QString::fromStdString(r.error())));
            }
        });
    }

    // Toolbar actions
    connect(toolbar, &ui::ToolBar::action_triggered, this, [this](const QString& action) {
        if (locked_) return;
        if (action == "browse_components") {
            // Same path as the keyboard shortcut so the behavior stays in one
            // place — the KeyConfigManager action will be triggered directly.
            if (auto* act = KeyConfigManager::instance().action(KeyAction::BrowseComponents))
                act->trigger();
            return;
        }
        if (action == "new_window") {
            auto* w = new WindowFrame(WindowFrame::next_window_id());
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
        } else if (action.startsWith("move_to_monitor:")) {
            // Move this window to the named monitor. We resolve by QScreen::name
            // (not index) because index ordering flips on plug/unplug events.
            const QString target_name = action.mid(QStringLiteral("move_to_monitor:").size());
            QScreen* target = nullptr;
            for (QScreen* s : QGuiApplication::screens()) {
                if (s->name() == target_name) {
                    target = s;
                    break;
                }
            }
            if (!target) {
                LOG_WARN("WindowFrame", QString("Move to monitor: screen '%1' not found").arg(target_name));
                return;
            }
            move_to_screen(target);
        } else if (action.startsWith("panel_")) {
            // Float a screen — in ADS mode, navigate dock_router; in legacy, spawn window
            static const QMap<QString, QPair<QString, QString>> panel_map = {
                {"panel_dashboard", {"Dashboard", "dashboard"}},
                {"panel_watchlist", {"Watchlist", "watchlist"}},
                {"panel_news", {"News Feed", "news"}},
                {"panel_portfolio", {"Portfolio", "portfolio"}},
                {"panel_markets", {"Markets", "markets"}},
                {"panel_crypto", {"Crypto Trading", "crypto_trading"}},
                {"panel_equity", {"Equity Trading", "equity_trading"}},
                {"panel_algo", {"Algo Trading", "algo_trading"}},
                {"panel_research", {"Equity Research", "equity_research"}},
                {"panel_economics", {"Economics", "economics"}},
                {"panel_geopolitics", {"Geopolitics", "geopolitics"}},
                {"panel_ai_chat", {"AI Chat", "ai_chat"}},
            };
            if (panel_map.contains(action)) {
                const auto [title, route] = panel_map[action];
                if (dock_router_)
                    dock_router_->navigate(route);
            }
        } else if (action == "perspective_save") {
            if (dock_manager_) {
                bool ok = false;
                const QString name =
                    QInputDialog::getText(this, "Save Layout", "Layout name:", QLineEdit::Normal, QString(), &ok);
                if (ok && !name.trimmed().isEmpty()) {
                    dock_manager_->addPerspective(name.trimmed());
                    LOG_INFO("WindowFrame", QString("Saved perspective: %1").arg(name.trimmed()));
                }
            }
        } else if (action.startsWith("perspective_")) {
            // Quick Switch — navigate directly to a preset screen layout
            // 2 screens = 1:1 side-by-side, 4 screens = 2x2 grid
            static const QMap<QString, QStringList> view_screens = {
                // Trading
                {"perspective_trading", {"crypto_trading", "watchlist", "markets", "news"}},
                {"perspective_equity", {"equity_trading", "watchlist"}},
                {"perspective_algo", {"algo_trading", "backtesting"}},
                // Research
                {"perspective_research", {"equity_research", "markets", "screener", "news"}},
                {"perspective_derivatives", {"derivatives", "surface_analytics"}},
                {"perspective_ma", {"ma_analytics", "news"}},
                // Portfolio
                {"perspective_portfolio", {"portfolio", "markets", "watchlist", "news"}},
                // News & Markets
                {"perspective_news", {"news", "markets"}},
                {"perspective_markets", {"markets", "watchlist"}},
                // Economics & Data
                {"perspective_economics", {"economics", "dbnomics", "gov_data", "asia_markets"}},
                {"perspective_data", {"data_sources", "data_mapping"}},
                // Geopolitics
                {"perspective_geopolitics", {"geopolitics", "maritime", "relationship_map", "news"}},
                // AI & Quant
                {"perspective_quant", {"ai_quant_lab", "quantlib", "backtesting", "markets"}},
                {"perspective_ai", {"ai_chat", "agent_config"}},
                // Tools
                {"perspective_tools", {"code_editor", "node_editor"}},
            };
            const auto it = view_screens.find(action);
            if (it != view_screens.end() && dock_router_) {
                const QStringList& screens = it.value();
                if (!screens.isEmpty()) {
                    dock_router_->navigate(screens[0], true); // first exclusive
                    for (int i = 1; i < screens.size(); ++i)
                        dock_router_->navigate(screens[i]); // fill grid
                }
                LOG_INFO("WindowFrame", QString("Quick Switch: %1 (%2 panels)").arg(action).arg(screens.size()));
            }
        } else if (action == "logout") {
            auth::AuthManager::instance().logout();
        } else if (action == "fullscreen") {
            if (isFullScreen())
                showNormal();
            else
                showFullScreen();
        } else if (action == "focus_mode") {
            // Auth screens must never reveal the shell via focus-mode toggle.
            if (stack_ && stack_->currentIndex() == 0) return;
            focus_mode_ = !focus_mode_;
            if (dock_toolbar_)
                dock_toolbar_->setVisible(!focus_mode_);
            if (dock_status_bar_)
                dock_status_bar_->setVisible(!focus_mode_);
        } else if (action == "always_on_top") {
            set_always_on_top(!always_on_top_);
        } else if (action == "refresh") {
            if (dock_manager_) {
                auto* focused = dock_manager_->focusedDockWidget();
                if (focused && focused->widget())
                    QMetaObject::invokeMethod(focused->widget(), "refresh", Qt::QueuedConnection);
            }
        } else if (action == "new_workspace") {
            auto* dlg = new ui::WorkspaceNewDialog(this);
            if (dlg->exec() == QDialog::Accepted)
                WorkspaceManager::instance().new_workspace(dlg->workspace_name(), dlg->workspace_description(),
                                                           dlg->selected_template_id());
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
            QString path =
                QFileDialog::getOpenFileName(this, "Import Workspace", QDir::homePath(), "Fincept Workspace (*.fwsp)");
            if (!path.isEmpty())
                WorkspaceManager::instance().import_workspace(path);
        } else if (action == "export_data") {
            QString path =
                QFileDialog::getSaveFileName(this, "Export Workspace", QDir::homePath(), "Fincept Workspace (*.fwsp)");
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
                LOG_INFO("WindowFrame", QString("Screenshot saved: %1").arg(path));
            }
        }
    });

    // Toolbar logout
    connect(toolbar, &ui::ToolBar::logout_clicked, this, []() { auth::AuthManager::instance().logout(); });

    // Toolbar plan label → pricing screen
    connect(toolbar, &ui::ToolBar::plan_clicked, this, [this]() {
        set_shell_visible(false);
        stack_->setCurrentIndex(0);
        auth_stack_->setCurrentIndex(3); // PricingScreen
    });

    // Auth state
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &WindowFrame::on_auth_state_changed);
    connect(&auth::AuthManager::instance(), &auth::AuthManager::logged_out, this, &WindowFrame::on_auth_state_changed);

    // Restore window state (maximised, toolbar positions, etc.)
    // Note: restoreState() also restores QToolBar visibility, which can
    // hide the toolbar from stale/corrupt saved state. We force it visible
    // afterward unless the user is intentionally in focus/chat mode.
    const QByteArray saved_state = SessionManager::instance().load_state(window_id_);
    if (!saved_state.isEmpty())
        restoreState(saved_state);

    // Toolbar/status bar visibility is controlled by set_shell_visible() based on
    // auth state. Start hidden — on_auth_state_changed() will show them if the user
    // is already authenticated and lands on the app stack.
    if (dock_toolbar_)
        dock_toolbar_->setVisible(false);
    if (dock_status_bar_)
        dock_status_bar_->setVisible(false);

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
                    LOG_WARN("WindowFrame", QString("Dock layout corrupt: %1 open areas — resetting")
                                               .arg(dock_manager_->openedDockAreas().size()));
                    dock_restored = false;
                }
            }
        } else if (saved_version != 0) {
            LOG_INFO("WindowFrame", QString("Dock layout version mismatch (saved %1, expected %2) — resetting")
                                       .arg(saved_version)
                                       .arg(kDockLayoutVersion));
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
            LOG_INFO("WindowFrame", "Session restored — showing PIN unlock");
            lock_screen_->show_unlock();
            locked_ = true;
            set_shell_visible(false);
            stack_->setCurrentIndex(3);
        } else if (auth_mgr.is_authenticated()) {
            // Authenticated but no PIN yet — will be caught by on_auth_state_changed
            on_auth_state_changed();
        } else {
            // Still loading — show app stack temporarily (loading state)
            set_shell_visible(true);
            stack_->setCurrentIndex(1);
        }
        if (!dock_restored) {
            dock_router_->navigate("dashboard");
            LOG_INFO("WindowFrame", "Applied clean default dock layout");
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

    // Periodic refresh of user credits/plan (every 3 minutes). Skip while
    // the terminal is locked — we shouldn't be making authenticated API
    // calls behind the back of the PIN gate, and the user can't see the
    // refreshed data anyway. The timer keeps running (cheap) but the
    // callback short-circuits.
    user_refresh_timer_ = new QTimer(this);
    user_refresh_timer_->setInterval(3 * 60 * 1000);
    connect(user_refresh_timer_, &QTimer::timeout, this, []() {
        auto& auth = auth::AuthManager::instance();
        if (!auth.is_authenticated())
            return;
        if (auth::InactivityGuard::instance().is_terminal_locked())
            return;
        auth.refresh_user_data();
    });
    user_refresh_timer_->start();

    // Confetti overlay (parented to central widget so it covers the whole app)
    // Refresh user data when app regains focus (updates toolbar credits/plan)
    connect(qApp, &QApplication::applicationStateChanged, this, [](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            auto& auth = auth::AuthManager::instance();
            // Skip refresh while locked — same reason as the periodic
            // timer above. check_for_resume_lock() below still runs so
            // a wall-clock-elapsed lock fires immediately on wake.
            if (auth.is_authenticated() && !auth::InactivityGuard::instance().is_terminal_locked())
                auth.refresh_user_data();
            // Resume-from-sleep lock: QTimer pauses during OS suspend, so a
            // 5-minute timer can carry 4:59 of remaining time across a
            // 30-minute nap. Ask the guard to check wall-clock delta and
            // force a lock if the user was effectively idle for the full
            // timeout. No-op when the guard is disabled or already locked.
            auth::InactivityGuard::instance().check_for_resume_lock();
        }
    });
}

void WindowFrame::toggle_chat_mode() {
    if (locked_) return;
    chat_mode_ = !chat_mode_;

    if (chat_mode_) {
        if (dock_toolbar_)
            dock_toolbar_->setVisible(false);
        if (dock_status_bar_)
            dock_status_bar_->setVisible(false);
        if (pushpin_toolbar_)
            pushpin_toolbar_->setVisible(false);
        if (chat_bubble_)
            chat_bubble_->setVisible(false);
        stack_->setCurrentIndex(2);
        LOG_INFO("WindowFrame", "Entered Chat Mode");
    } else {
        stack_->setCurrentIndex(1);
        if (dock_toolbar_)
            dock_toolbar_->setVisible(true);
        if (dock_status_bar_)
            dock_status_bar_->setVisible(true);
        if (pushpin_toolbar_)
            pushpin_toolbar_->setVisible(true);
        // Restore chat bubble based on setting
        if (chat_bubble_) {
            auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
            const bool show = !r.is_ok() || r.value() != "false";
            chat_bubble_->setVisible(show);
            if (show) {
                chat_bubble_->reposition();
                chat_bubble_->raise();
            }
        }
        LOG_INFO("WindowFrame", "Exited Chat Mode");
    }
}

void WindowFrame::setup_auth_screens() {
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
    connect(login, &screens::LoginScreen::navigate_register, this, &WindowFrame::show_register);
    connect(login, &screens::LoginScreen::navigate_forgot_password, this, &WindowFrame::show_forgot_password);
    connect(reg, &screens::RegisterScreen::navigate_login, this, &WindowFrame::show_login);
    connect(forgot, &screens::ForgotPasswordScreen::navigate_login, this, &WindowFrame::show_login);
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
    dock_router_->register_screen("help", new screens::HelpScreen);
}

void WindowFrame::setup_app_screens() {}
void WindowFrame::setup_navigation() {}

void WindowFrame::on_auth_state_changed() {
    auto& auth = auth::AuthManager::instance();

    // If logged out while the lock screen is active (e.g. max PIN attempts → reauth),
    // clear locked_ so the login screen can show. Otherwise stay on the lock screen.
    if (locked_) {
        if (!auth.is_authenticated()) {
            locked_ = false; // fall through to show login screen
        } else {
            return; // still authenticated — stay on lock screen
        }
    }

    // Still validating saved session — don't flash the login screen.
    // Stay on whatever is currently shown until validation completes.
    if (auth.is_loading())
        return;

    if (auth.is_authenticated()) {
        // Don't redirect if user is already on the app stack (dashboard/workspace
        // at index 1, or chat mode at index 2) — UNLESS the PIN gate hasn't been
        // cleared yet (auth completed while loading state showed dashboard early).
        //
        // pin_gate_cleared_ is critical: once the user has entered their PIN this
        // session, subsequent auth_state_changed events (periodic refresh_user_data,
        // focus-driven applicationStateChanged refresh, subscription_fetched, etc.)
        // must NOT re-lock the terminal. Without this guard, any auth refresh
        // while the user is working on the dashboard will re-show the PIN prompt.
        if (stack_->currentIndex() == 1 || stack_->currentIndex() == 2) {
            if (!pin_gate_cleared_) {
                if (auth.needs_pin_setup()) {
                    LOG_INFO("WindowFrame", "Authenticated (on app stack) but no PIN — showing PIN setup");
                    lock_screen_->show_setup();
                    locked_ = true;
                    set_shell_visible(false);
                    stack_->setCurrentIndex(3);
                    // Raise the process-wide locked flag so SessionGuard's
                    // pulse path and DockScreenRouter both know the user
                    // hasn't passed the PIN gate yet — without this, a
                    // backend 401 during the PIN-setup window can force a
                    // logout behind the lock screen.
                    auth::InactivityGuard::instance().set_terminal_locked(true);
                    return;
                }
                if (auth::PinManager::instance().has_pin()) {
                    LOG_INFO("WindowFrame", "Authenticated (on app stack) with PIN — showing PIN unlock");
                    lock_screen_->show_unlock();
                    locked_ = true;
                    set_shell_visible(false);
                    stack_->setCurrentIndex(3);
                    auth::InactivityGuard::instance().set_terminal_locked(true);
                    return;
                }
            }
            LOG_DEBUG("WindowFrame", QString("on_auth_state_changed: skipping redirect, "
                                            "stack index=%1, chat_mode=%2, gate_cleared=%3")
                                        .arg(stack_->currentIndex())
                                        .arg(chat_mode_)
                                        .arg(pin_gate_cleared_));
            return;
        }
        if (stack_->currentIndex() == 0 && auth_stack_->currentIndex() == 3)
            return; // user is on pricing screen — let PricingScreen handle it

        // ── PIN gate: require PIN setup or PIN unlock before proceeding ──
        // On first login (no PIN configured): show mandatory PIN setup.
        // On subsequent launches (PIN exists): show PIN unlock.
        // Skip if user is already on the lock screen (index 3).
        if (stack_->currentIndex() != 3) {
            if (auth.needs_pin_setup()) {
                LOG_INFO("WindowFrame", "Authenticated but no PIN — showing PIN setup");
                lock_screen_->show_setup();
                locked_ = true;
                set_shell_visible(false);
                stack_->setCurrentIndex(3);
                auth::InactivityGuard::instance().set_terminal_locked(true);
                return;
            }
            if (auth::PinManager::instance().has_pin()) {
                LOG_INFO("WindowFrame", "Authenticated with PIN — showing PIN unlock");
                lock_screen_->show_unlock();
                locked_ = true;
                set_shell_visible(false);
                stack_->setCurrentIndex(3);
                auth::InactivityGuard::instance().set_terminal_locked(true);
                return;
            }
        }

        if (auth.session().has_paid_plan()) {
            // Defensive: at this point the PIN gate above must have either
            // routed us to the lock screen (and returned) or confirmed
            // pin_gate_cleared_. If we are about to show the shell while
            // still locked or ungated, log a warning so the regression is
            // visible rather than leaking the dashboard for one frame.
            if (locked_ || !pin_gate_cleared_) {
                LOG_WARN("WindowFrame",
                         QString("on_auth_state_changed: shell would become visible while "
                                 "locked=%1 gate_cleared=%2 — forcing lock screen")
                             .arg(locked_).arg(pin_gate_cleared_));
                if (auth::PinManager::instance().has_pin())
                    lock_screen_->show_unlock();
                else
                    lock_screen_->show_setup();
                locked_ = true;
                set_shell_visible(false);
                stack_->setCurrentIndex(3);
                return;
            }

            // Paid user → straight to dashboard
            set_shell_visible(true);
            stack_->setCurrentIndex(1);
            WorkspaceManager::instance().load_last_workspace();
            // Trigger silent update check after login (delayed so UI settles first).
            // UpdateService de-dupes silent checks across the session, so the
            // post-PIN-unlock path below won't fire a second request.
            QTimer::singleShot(3000, this, [this]() {
                services::UpdateService::instance().set_dialog_parent(this);
                services::UpdateService::instance().check_for_updates(true);
            });
            // Warm instrument cache in background — only loaded if not already cached.
            // Runs concurrently while the user reads the dashboard (3-5s head start).
            fincept::trading::InstrumentService::instance().load_from_db_async("zerodha");
            fincept::trading::InstrumentService::instance().load_from_db_async("angelone");
            fincept::trading::InstrumentService::instance().load_from_db_async("groww");
        } else {
            // Free/no plan → show pricing gate
            set_shell_visible(false);
            stack_->setCurrentIndex(0);
            auth_stack_->setCurrentIndex(3);
        }
    } else {
        // Disable inactivity guard when logged out and drop the locked flag
        // so a subsequent login is not immediately router-blocked.
        auth::InactivityGuard::instance().set_enabled(false);
        auth::InactivityGuard::instance().set_terminal_locked(false);
        pin_gate_cleared_ = false;
        set_shell_visible(false);
        stack_->setCurrentIndex(0);
        auth_stack_->setCurrentIndex(0);
    }
}

// Centralised transition into the auth stack. Any path that shows a login /
// register / forgot / pricing / info screen must go through here so the
// privileged shell (menus, command bar, CHAT, LOGOUT, status ticker) cannot
// be seen by an unauthenticated user. Title is also reset by set_shell_visible
// so the last-visited screen name does not leak.
void WindowFrame::enter_auth_stack(int auth_index) {
    set_shell_visible(false);
    stack_->setCurrentIndex(0);
    auth_stack_->setCurrentIndex(auth_index);
}

void WindowFrame::show_login() {
    enter_auth_stack(0);
}
void WindowFrame::show_register() {
    enter_auth_stack(1);
}
void WindowFrame::show_forgot_password() {
    enter_auth_stack(2);
}
void WindowFrame::show_pricing() {
    enter_auth_stack(3);
}
void WindowFrame::show_info_contact() {
    info_stack_->setCurrentIndex(0);
    enter_auth_stack(4);
}
void WindowFrame::show_info_terms() {
    info_stack_->setCurrentIndex(1);
    enter_auth_stack(4);
}
void WindowFrame::show_info_privacy() {
    info_stack_->setCurrentIndex(2);
    enter_auth_stack(4);
}
void WindowFrame::show_info_trademarks() {
    info_stack_->setCurrentIndex(3);
    enter_auth_stack(4);
}
void WindowFrame::show_info_help() {
    info_stack_->setCurrentIndex(4);
    enter_auth_stack(4);
}

void WindowFrame::show_lock_screen() {
    auto& auth = auth::AuthManager::instance();
    if (!auth.is_authenticated()) {
        LOG_DEBUG("WindowFrame", "show_lock_screen: ignored — not authenticated");
        return;
    }

    // Raise the process-wide locked flag. Every WindowFrame listens for
    // terminal_locked_changed and applies the lock UI itself in
    // apply_lock_state() — including this one, idempotently. Stop the
    // inactivity guard so the activity stream on the lock screen does
    // not generate no-op ticks until the PIN is accepted.
    auth::InactivityGuard::instance().set_enabled(false);
    auth::InactivityGuard::instance().set_terminal_locked(true);
}

void WindowFrame::apply_lock_state(bool locked) {
    if (locked) {
        // Idempotent — if already on the lock screen, nothing to do.
        if (stack_->currentIndex() == 3)
            return;
        // Skip if this window is currently on the auth stack — login screen
        // is its own gate. Spurious lock during auth transition would push
        // a lock screen over the login screen which is meaningless.
        if (stack_->currentIndex() == 0) {
            LOG_DEBUG("WindowFrame", "apply_lock_state(true): on auth stack — skipping");
            return;
        }
        LOG_INFO("WindowFrame", QString("Locking window %1").arg(window_id_));
        lock_screen_->activate();
        locked_ = true;
        pin_gate_cleared_ = false;
        set_shell_visible(false);
        stack_->setCurrentIndex(3);
        if (chat_bubble_)
            chat_bubble_->setVisible(false);

        // Disable widgets behind the lock screen so keyboard shortcuts,
        // focus traversal, and dock-manager hit-testing cannot mutate state.
        if (dock_manager_ && dock_manager_->parentWidget())
            dock_manager_->parentWidget()->setEnabled(false);
        if (dock_toolbar_)    dock_toolbar_->setEnabled(false);
        if (pushpin_toolbar_) pushpin_toolbar_->setEnabled(false);
        if (dock_status_bar_) dock_status_bar_->setEnabled(false);
        return;
    }

    // Unlock — only the window the user typed the PIN into emits the
    // unlocked signal that lands in on_terminal_unlocked(). Secondary
    // windows take this path via terminal_locked_changed(false). For
    // them we just restore the dashboard chrome; PIN gate state is
    // per-window and stays cleared after the originator set it.
    if (stack_->currentIndex() != 3)
        return; // already unlocked
    LOG_INFO("WindowFrame", QString("Unlocking window %1 (sibling)").arg(window_id_));
    locked_ = false;
    pin_gate_cleared_ = true;
    if (dock_manager_ && dock_manager_->parentWidget())
        dock_manager_->parentWidget()->setEnabled(true);
    if (dock_toolbar_)    dock_toolbar_->setEnabled(true);
    if (pushpin_toolbar_) pushpin_toolbar_->setEnabled(true);
    if (dock_status_bar_) dock_status_bar_->setEnabled(true);
    set_shell_visible(true);
    stack_->setCurrentIndex(1);
}

void WindowFrame::on_terminal_unlocked() {
    auto& auth = auth::AuthManager::instance();
    LOG_INFO("WindowFrame", QString("Terminal unlocked via PIN (window %1)").arg(window_id_));
    locked_ = false;
    pin_gate_cleared_ = true;

    // Re-enable this window's widgets. Sibling MainWindows are handled by
    // apply_lock_state(false) once we flip the flag at the bottom of this
    // function — keep the flag-flip last so siblings don't race ahead and
    // restore their own UI before this originator is fully ready.
    if (dock_manager_ && dock_manager_->parentWidget())
        dock_manager_->parentWidget()->setEnabled(true);
    if (dock_toolbar_)    dock_toolbar_->setEnabled(true);
    if (pushpin_toolbar_) pushpin_toolbar_->setEnabled(true);
    if (dock_status_bar_) dock_status_bar_->setEnabled(true);

    // Enable/restart inactivity guard. It is disabled in show_lock_screen()
    // and on logout, so it may currently be off even though the filter is
    // already installed on qApp from a previous unlock. Re-read the saved
    // timeout from settings so a value changed in Settings → Security takes
    // effect on the very next lock cycle even if the guard had been stopped
    // (e.g. across a lock/unlock or logout/login round-trip).
    auto& guard = auth::InactivityGuard::instance();
    {
        auto r = SettingsRepository::instance().get("security.lock_timeout_minutes");
        if (r.is_ok() && !r.value().isEmpty()) {
            int minutes = r.value().toInt();
            if (minutes > 0)
                guard.set_timeout_minutes(minutes);
        }
    }
    if (!guard.is_enabled()) {
        qApp->installEventFilter(&guard);
        guard.set_enabled(true);
    }
    guard.reset_timer();

    // Reset PIN lockout on successful unlock
    auth::PinManager::instance().reset_lockout();

    if (auth.session().has_paid_plan()) {
        set_shell_visible(true);
        stack_->setCurrentIndex(1);
        // Restore chat bubble based on setting
        if (chat_bubble_) {
            auto r = SettingsRepository::instance().get("appearance.show_chat_bubble");
            bool show = !r.is_ok() || r.value() != "false";
            chat_bubble_->setVisible(show);
            if (show) {
                chat_bubble_->reposition();
                chat_bubble_->raise();
            }
        }
        WorkspaceManager::instance().load_last_workspace();
        // Silent update check — UpdateService de-dupes across call sites so
        // the login path + this post-unlock path won't fire two requests.
        QTimer::singleShot(3000, this, [this]() {
            services::UpdateService::instance().set_dialog_parent(this);
            services::UpdateService::instance().check_for_updates(true);
        });
    } else {
        // Free/no plan → pricing gate
        set_shell_visible(false);
        stack_->setCurrentIndex(0);
        auth_stack_->setCurrentIndex(3);
    }

    // Flip the process-wide locked flag LAST. Sibling MainWindows fan out
    // via terminal_locked_changed → apply_lock_state(false) and restore
    // their own dashboard chrome from this single source of truth.
    auth::InactivityGuard::instance().set_terminal_locked(false);

    emit auth.terminal_unlocked();
}

void WindowFrame::set_shell_visible(bool visible) {
    if (dock_toolbar_)
        dock_toolbar_->setVisible(visible && !focus_mode_ && !chat_mode_);
    if (dock_status_bar_)
        dock_status_bar_->setVisible(visible && !focus_mode_ && !chat_mode_);
    if (pushpin_toolbar_)
        pushpin_toolbar_->setVisible(visible && !focus_mode_ && !chat_mode_);
    if (!visible) {
        // Reset title to plain app name — no screen suffix while on auth screens
        const QString profile = ProfileManager::instance().active();
        setWindowTitle(profile == "default" ? "Fincept Terminal"
                                            : QString("Fincept Terminal [%1]").arg(profile));
    }
}

void WindowFrame::update_window_title() {
    QString title = "Fincept Terminal";

    const QString profile = ProfileManager::instance().active();
    if (profile != "default")
        title += QString(" [%1]").arg(profile);

    // Workspace / screen name must never appear in the title while the user
    // is on the auth or lock stack — that would leak the last-visited screen
    // to an unauthenticated viewer.
    const bool shell_visible = stack_ && stack_->currentIndex() == 1;
    if (shell_visible) {
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
    }

    setWindowTitle(title);
}

void WindowFrame::set_always_on_top(bool on) {
    if (always_on_top_ == on)
        return;
    always_on_top_ = on;
    Qt::WindowFlags flags = windowFlags();
    if (on)
        flags |= Qt::WindowStaysOnTopHint;
    else
        flags &= ~Qt::WindowStaysOnTopHint;
    setWindowFlags(flags);
    // setWindowFlags hides the window on most platforms; re-show it so the
    // change is visible. Preserve maximised/fullscreen state across the toggle.
    if (isVisible() || true) // always re-show; isVisible is false after the flag change
        show();
    SessionManager::instance().save_window_flag(window_id_, "always_on_top", on);
    LOG_INFO("WindowFrame",
             QString("Window %1 always-on-top = %2").arg(window_id_).arg(on ? "on" : "off"));
}

void WindowFrame::move_to_screen(QScreen* target) {
    if (!target || screen() == target)
        return;

    // Preserve maximised/fullscreen state across the move: Qt can't directly
    // relocate a maximised window to a different screen, so we restore →
    // move → re-apply the state.
    const bool was_maximised = isMaximized();
    const bool was_fullscreen = isFullScreen();
    if (was_maximised || was_fullscreen)
        showNormal();

    const QRect sg = target->availableGeometry();
    QSize new_size = size();
    if (new_size.width() > sg.width() || new_size.height() > sg.height())
        new_size = QSize(sg.width() * 9 / 10, sg.height() * 9 / 10);
    resize(new_size);
    move(sg.center() - QPoint(new_size.width() / 2, new_size.height() / 2));

    if (was_fullscreen)
        showFullScreen();
    else if (was_maximised)
        showMaximized();

    LOG_INFO("WindowFrame", QString("Moved window %1 to monitor '%2'")
                               .arg(window_id_).arg(target->name()));
}

WindowId WindowFrame::frame_uuid() const {
    if (frame_uuid_.is_null())
        frame_uuid_ = WindowId::generate();
    return frame_uuid_;
}

// ── Phase 6 final: capture / apply layout ───────────────────────────────────

layout::FrameLayout WindowFrame::capture_layout() const {
    layout::FrameLayout fl;
    fl.window_id = frame_uuid();
    fl.focus_mode = focus_mode_;
    fl.always_on_top = always_on_top_;
    fl.schema_version = layout::kFrameLayoutVersion;

    if (!dock_manager_ || !dock_router_)
        return fl;

    // ADS dock state — opaque blob covering splits, tab order, sizes.
    fl.dock_state = dock_manager_->saveState();

    // Active panel — whichever dock widget currently has focus.
    if (auto* focused = dock_manager_->focusedDockWidget()) {
        const QString id = focused->objectName();
        const auto uuid = dock_router_->panel_uuid_for(id);
        if (!uuid.is_null())
            fl.active_panel = uuid;
    }

    // Per-panel state. Walk every dock widget the manager knows about
    // (not just openedDockWidgets) — closed-but-registered panels still
    // belong in the layout so reopen restores their state. The router's
    // panel_uuid_for() returns the stable PanelInstanceId minted at
    // create_dock_widget() time.
    for (const auto& entry : dock_manager_->dockWidgetsMap().toStdMap()) {
        const QString& id = entry.first;
        ads::CDockWidget* dw = entry.second;
        if (!dw)
            continue;

        layout::PanelState ps;
        ps.instance_id = dock_router_->panel_uuid_for(id);
        ps.title = dw->windowTitle();

        // Strip "#dupN" suffix from the type id so two watchlists share
        // type_id="watchlist" but have distinct instance_ids.
        QString type_id = id;
        const int dup_idx = type_id.indexOf("#dup");
        if (dup_idx > 0)
            type_id = type_id.left(dup_idx);
        ps.type_id = type_id;

        // IGroupLinked screens carry a link group letter. SymbolGroup is
        // a single-char enum class; we serialise as the letter.
        if (auto* w = dw->widget()) {
            if (auto* linked = dynamic_cast<IGroupLinked*>(w)) {
                const SymbolGroup g = linked->group();
                if (g != SymbolGroup::None)
                    ps.link_group = QString(QChar(symbol_group_letter(g)));
            }
            if (auto* stateful = dynamic_cast<screens::IStatefulScreen*>(w)) {
                stateful->flush_pending_state();
                const QVariantMap state = stateful->save_state();
                QJsonObject obj;
                for (auto it = state.constBegin(); it != state.constEnd(); ++it)
                    obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
                ps.state_blob = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                ps.state_version = stateful->state_version();
            }
        }
        ps.schema_version = layout::kPanelStateVersion;
        fl.panels.append(ps);
    }

    LOG_INFO("WindowFrame", QString("capture_layout: window %1 -> %2 panels, dock_state=%3 bytes")
                                .arg(window_id_).arg(fl.panels.size()).arg(fl.dock_state.size()));
    return fl;
}

bool WindowFrame::apply_layout(const layout::FrameLayout& fl) {
    if (!dock_manager_ || !dock_router_) {
        LOG_WARN("WindowFrame", "apply_layout: dock manager/router not ready");
        return false;
    }

    LOG_INFO("WindowFrame", QString("apply_layout: window %1, %2 panels, dock_state=%3 bytes")
                                .arg(window_id_).arg(fl.panels.size()).arg(fl.dock_state.size()));

    // Phase 1: ensure every panel from the layout is registered with the
    // router and PanelRegistry. The dock_state blob from ADS references
    // dock widgets by objectName, so they must exist before restoreState
    // can reattach them. The router's create_dock_widget mints a fresh
    // PanelInstanceId; we override with the saved one so persisted state
    // round-trips cleanly.
    for (const auto& ps : fl.panels) {
        QString id = ps.type_id;
        // Synthesise a #dup suffix for non-first instances of the same
        // type_id. Walk earlier panels: if any prior panel shares this
        // type_id, we need a suffix.
        int dup_index = 0;
        for (const auto& other : fl.panels) {
            if (&other == &ps) break;
            if (other.type_id == ps.type_id) ++dup_index;
        }
        if (dup_index > 0)
            id = QString("%1#dup%2").arg(ps.type_id).arg(dup_index + 1);

        // Trigger create_dock_widget via navigate (false = non-exclusive).
        // After navigate, override the minted UUID with the saved one so
        // state restoration via TabSessionStore::load_screen_state_by_uuid
        // finds the right row.
        dock_router_->navigate(id, /*exclusive=*/false);
        if (!ps.instance_id.is_null())
            dock_router_->adopt_panel_instance(id, ps.instance_id);
    }

    // Phase 2: restore the ADS dock layout blob. CDockManager::restoreState
    // returns false if the blob is incompatible (version mismatch, missing
    // dock widgets). The caller (WorkspaceMatcher) decides whether to
    // fall back to a default layout.
    bool ok = true;
    if (!fl.dock_state.isEmpty()) {
        ok = dock_manager_->restoreState(fl.dock_state);
        if (!ok) {
            LOG_WARN("WindowFrame", "apply_layout: CDockManager::restoreState rejected dock blob");
        }
    }

    // Phase 3: chrome flags.
    if (focus_mode_ != fl.focus_mode)
        toggle_focus_mode();
    if (always_on_top_ != fl.always_on_top)
        set_always_on_top(fl.always_on_top);

    // Phase 4: activate the saved active panel.
    if (!fl.active_panel.is_null()) {
        const auto map = dock_manager_->dockWidgetsMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (dock_router_->panel_uuid_for(it.key()) == fl.active_panel) {
                if (auto* dw = it.value()) {
                    dw->raise();
                    dw->setAsCurrentTab();
                }
                break;
            }
        }
    }

    return ok;
}

bool WindowFrame::is_active_for_work() const {
    // Best-effort per decision 9.1. Two cheap checks cover the obvious cases:
    //   - isMinimized(): user clicked the minimise button or pressed Win+M.
    //   - isVisible(): the widget tree is hidden (rare for top-level frames
    //     in this app, but cheap to check).
    // Everything else (virtual-desktop occupancy, occlusion by other apps,
    // alt-tab status) is intentionally NOT detected — platform-specific and
    // unreliable.
    if (isMinimized())
        return false;
    if (!isVisible())
        return false;
    return true;
}

namespace {
// Phase 8 / decision 9.2: DataHub reads this dynamic property via
// QObject::property() to decide whether to fan out pause_when_inactive
// topics. Using a dynamic property (rather than a typed accessor) keeps
// the datahub layer free of any "app/WindowFrame.h" include — DataHub
// only needs Qt and the property name.
constexpr const char* kActiveForWorkProp = "fincept.active_for_work";
} // namespace

void WindowFrame::toggle_focus_mode() {
    // Don't let focus mode toggle shell visibility while the user is on an
    // auth screen — the toolbar must stay hidden there. Mirrors the gate
    // that lived inside the original inline lambda.
    if (stack_ && stack_->currentIndex() == 0)
        return;
    focus_mode_ = !focus_mode_;
    if (dock_toolbar_)
        dock_toolbar_->setVisible(!focus_mode_);
    if (dock_status_bar_)
        dock_status_bar_->setVisible(!focus_mode_);
    if (pushpin_toolbar_)
        pushpin_toolbar_->setVisible(!focus_mode_);
}

void WindowFrame::toggle_chat_mode_action() {
    // Public facade so action handlers in builtin_actions.cpp can call into
    // the existing private implementation without becoming friends of this
    // class. Caller has already gated on is_locked().
    toggle_chat_mode();
}

void WindowFrame::refresh_focused_panel() {
    if (!dock_manager_)
        return;
    auto* focused = dock_manager_->focusedDockWidget();
    if (focused && focused->widget())
        QMetaObject::invokeMethod(focused->widget(), "refresh", Qt::QueuedConnection);
}

void WindowFrame::open_component_browser() {
    auto* dlg = new ui::ComponentBrowserDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &ui::ComponentBrowserDialog::component_chosen, this, [this](const QString& id) {
        if (dock_router_ && !id.isEmpty())
            dock_router_->navigate(id);
    });
    dlg->show();
}

void WindowFrame::toggle_debug_overlay() {
    // Lazy construction so users who never enable the overlay don't pay
    // for the QTimer + QLabel cost.
    if (!debug_overlay_) {
        debug_overlay_ = new ui::DebugOverlay(this);
        // Parent it to the main window so resizes auto-trigger reposition
        // via our resizeEvent. The overlay uses
        // WA_TransparentForMouseEvents so it never blocks dock interaction.
    }
    debug_overlay_->toggle_visible();
}

void WindowFrame::toggle_command_bar() {
    // Phase 9: lazy construction. Wraps a `ui::CommandBar` in a bottom
    // QToolBar so QMainWindow lays it out above the status bar.
    if (!command_bar_) {
        auto* bar = new ui::QuickCommandBar(this);
        auto* host = new QToolBar(this);
        host->setObjectName("commandBarHost");
        host->setMovable(false);
        host->setFloatable(false);
        host->setAllowedAreas(Qt::BottomToolBarArea);
        host->addWidget(bar);
        addToolBar(Qt::BottomToolBarArea, host);
        host->setVisible(false);
        host->setProperty("commandBarInner", QVariant::fromValue(static_cast<QWidget*>(bar)));
        command_bar_ = host;
    }
    if (command_bar_) {
        const bool was_visible = command_bar_->isVisible();
        command_bar_->setVisible(!was_visible);
        if (!was_visible) {
            if (auto* inner = command_bar_->property("commandBarInner").value<QWidget*>()) {
                if (auto* cb = qobject_cast<ui::QuickCommandBar*>(inner))
                    cb->surface();
            }
        }
    }
}

void WindowFrame::open_command_palette() {
    ui::CommandPalette::show_for(this);
}

void WindowFrame::schedule_dock_layout_save() {
    if (dock_layout_save_timer_)
        dock_layout_save_timer_->start();
}

void WindowFrame::closeEvent(QCloseEvent* event) {
    WorkspaceManager::instance().save_workspace();
    SessionManager::instance().save_geometry(window_id_, saveGeometry(), saveState());
    // Record the monitor this window is currently on so startup can place
    // it back on the same screen even if saveGeometry's absolute coordinates
    // would otherwise land off-screen (e.g. monitor disconnected, DPI changed).
    if (QScreen* scr = screen())
        SessionManager::instance().save_screen_name(window_id_, scr->name());
    if (dock_manager_) {
        SessionManager::instance().save_dock_layout(window_id_, dock_manager_->saveState());
        QSettings tmp;
        dock_manager_->savePerspectives(tmp);
        SessionManager::instance().save_perspectives(tmp);
    }

    // Persist the set of still-open windows (this one minus itself, since
    // we're about to be destroyed). On next launch, main.cpp will iterate
    // this list and restore each secondary window. We also save the count
    // for legacy callers.
    //
    // WindowRegistry is the source of truth for live frames as of Phase 1
    // (decision 1.7); the previous QApplication::topLevelWidgets() walk
    // worked but pulled in non-WindowFrame toplevels (dialogs, ADS floats)
    // that needed to be cast away. Registry is cleaner and faster.
    QList<int> open_ids;
    for (int id : WindowRegistry::instance().frame_ids()) {
        if (id != window_id_)
            open_ids.append(id);
    }
    // Ensure this window is still saved for next run if it's the last one
    // being closed — user would expect to land back in their last layout,
    // including the primary window's dock state.
    if (open_ids.isEmpty())
        open_ids.append(window_id_);
    std::sort(open_ids.begin(), open_ids.end());
    SessionManager::instance().save_window_ids(open_ids);
    SessionManager::instance().save_window_count(static_cast<int>(open_ids.size()));

    // Phase 2: force a workspace snapshot at close. The save_* calls above
    // each request a snapshot but the ring rate-limits to 60s — closing a
    // window after rapid changes could otherwise miss the latest state.
    // Forced flush bypasses the rate limit. Cheap (one INSERT).
    SessionManager::instance().flush_snapshot_now();

    event->accept();
}

void WindowFrame::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (chat_bubble_)
        chat_bubble_->reposition();
    if (debug_overlay_ && debug_overlay_->isVisible())
        debug_overlay_->reposition();
}

void WindowFrame::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() != QEvent::WindowStateChange)
        return;

    // Phase 8 / decision 9.1: emit active_for_work_changed when the flag
    // actually flips. Fires on minimise (true → false) AND restore
    // (false → true), so DataHub (and any other subscriber) can flush
    // pending updates when the user brings the window forward. We compute
    // is_active_for_work() now — Qt has already updated windowState by
    // the time WindowStateChange fires.
    {
        const bool now_active = is_active_for_work();
        if (now_active != last_active_for_work_) {
            last_active_for_work_ = now_active;
            // Update the dynamic property DataHub reads. Setting it BEFORE
            // emitting the signal so any DataHub subscriber that reacts to
            // the signal sees the up-to-date property.
            setProperty(kActiveForWorkProp, now_active);
            emit active_for_work_changed(now_active);
        }
    }

    if (!(windowState() & Qt::WindowMinimized))
        return;
    // Only lock if the user opted in and we are actually in an authenticated
    // session with a configured PIN — otherwise there is nothing to lock.
    auto& auth = auth::AuthManager::instance();
    if (!auth.is_authenticated() || !auth::PinManager::instance().has_pin())
        return;
    auto r = SettingsRepository::instance().get("security.lock_on_minimize");
    const bool lock_on_min = r.is_ok() && r.value() == "true";
    if (!lock_on_min)
        return;

    // Multi-window correctness (decision 8.8): minimising one window in a
    // multi-window setup is normal user behaviour, not a "walked away from
    // desk" event. Only trigger the lock when EVERY frame is minimised.
    //
    // Qt fires WindowStateChange before the frame is actually marked as
    // minimised in some platform queues, so re-check via WindowRegistry
    // *after* the next event-loop tick — at that point all minimisations
    // queued together (Win+M, "Show desktop") have been applied. The 200ms
    // delay also rides through transient lid-close/wake cycles where
    // Qt sees a minimise then immediately a restore.
    QTimer::singleShot(200, &auth::InactivityGuard::instance(), []() {
        if (WindowRegistry::instance().all_minimised())
            auth::InactivityGuard::instance().trigger_manual_lock();
    });
}

} // namespace fincept
