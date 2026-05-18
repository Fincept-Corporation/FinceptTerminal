#include "app/WindowFrame.h"

#include "screens/ai_chat/AiChatBubble.h"
#include "screens/ai_chat/AiChatScreen.h"
#include "services/llm/LlmService.h"
#include "app/DockScreenRouter.h"
#include "app/MonitorPickerDialog.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/lock/LockOverlayController.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolGroup.h"
#include "screens/common/IStatefulScreen.h"
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
#include "screens/common/ComingSoonScreen.h"
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
#include "screens/fno/FnoScreen.h"
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
#include "core/layout/LayoutCatalog.h"
#include "core/layout/WorkspaceShell.h"
#include "core/panel/PanelMaterialiser.h"
#include "services/updater/UpdateService.h"
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
#include "ui/workspace/LayoutOpenDialog.h"
#include "ui/workspace/LayoutSaveAsDialog.h"

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

WindowFrame::WindowFrame(int window_id, QWidget* parent, const WindowId& adopted_uuid)
    : QMainWindow(parent), window_id_(window_id) {
    // Adopt the saved UUID before any panel is created — see header rationale.
    // Null adopted_uuid = mint a fresh one lazily on first frame_uuid() call.
    if (!adopted_uuid.is_null())
        frame_uuid_ = adopted_uuid;
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

    // Pushpin chips are embedded directly inside the main toolbar (see
    // ToolBar::ToolBar). Visibility/enabled state now follows the parent
    // toolbar — no explicit gating needed.
    pushpin_bar_ = dock_toolbar_->inner()->pushpin_bar();

    dock_status_bar_ = new ui::DockStatusBar(this);
    setStatusBar(dock_status_bar_);

    stack_ = master_stack;
    auto* toolbar = dock_toolbar_->inner();

    // ── Window registry wiring ──────────────────────────────────────────────
    // Layout/workspace persistence lives in core/layout (LayoutCatalog +
    // WorkspaceShell + WorkspaceSnapshotRing). Title bar reads the active
    // layout name from WorkspaceShell::current_name() inside
    // update_window_title(), so no signal wiring is needed here.
    WindowCycler::instance().register_window(this);
    connect(this, &QObject::destroyed, this,
            [this]() { WindowCycler::instance().unregister_window(this); });

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
        // SessionManager.save_dock_layout fans out to WorkspaceSnapshotRing
        // (60s rate-limited auto snapshot). That covers what the legacy
        // 5-min WorkspaceManager autosave used to do, with better cadence.
        SessionManager::instance().save_dock_layout(window_id_, dock_manager_->saveState());
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
            // Ask the user which monitor when more than one is connected.
            // pick() returns the only screen if there's just one, and
            // nullptr if the user cancels the dialog — in which case no
            // window is created.
            QScreen* target = MonitorPickerDialog::pick(this, this->screen());
            if (!target)
                return;
            auto* w = new WindowFrame(WindowFrame::next_window_id());
            w->setAttribute(Qt::WA_DeleteOnClose);
            const QRect sg = target->availableGeometry();
            w->resize(sg.width() * 9 / 10, sg.height() * 9 / 10);
            w->move(sg.center() - w->rect().center());
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
                {"perspective_fno", {"fno", "surface_analytics", "equity_trading"}},
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
        } else if (action == "layout_new" || action == "layout_save_as") {
            // Both routes prompt for a name then save. layout.new = blank
            // workspace under that name; layout.save_as = capture-current
            // under that name. Handlers in builtin_actions distinguish.
            ui::LayoutSaveAsDialog dlg(this);
            if (dlg.exec() == QDialog::Accepted) {
                CommandContext ctx;
                ctx.shell = &TerminalShell::instance();
                ctx.focused_frame = this;
                ctx.args.insert(QStringLiteral("name"), dlg.name());
                const QString action_id = (action == "layout_new")
                    ? QStringLiteral("layout.new") : QStringLiteral("layout.save_as");
                auto r = ActionRegistry::instance().invoke(action_id, ctx);
                if (r.is_err())
                    LOG_WARN("WindowFrame", QString("%1 failed: %2")
                                                .arg(action_id, QString::fromStdString(r.error())));
            }
        } else if (action == "layout_open") {
            ui::LayoutOpenDialog dlg(this);
            if (dlg.exec() == QDialog::Accepted && !dlg.selected_id().is_null()) {
                CommandContext ctx;
                ctx.shell = &TerminalShell::instance();
                ctx.focused_frame = this;
                ctx.args.insert(QStringLiteral("name"), dlg.selected_id().to_string());
                auto r = ActionRegistry::instance().invoke(QStringLiteral("layout.switch"), ctx);
                if (r.is_err())
                    LOG_WARN("WindowFrame", QString("layout.switch failed: %1")
                                                .arg(QString::fromStdString(r.error())));
            }
        } else if (action == "layout_save") {
            // If a layout is currently loaded, save under its name. Otherwise
            // prompt for one (treat as save-as with no initial name).
            const QString cur = layout::WorkspaceShell::current_name();
            QString target = cur;
            if (target.isEmpty()) {
                ui::LayoutSaveAsDialog dlg(this);
                if (dlg.exec() != QDialog::Accepted) {
                    return;
                }
                target = dlg.name();
            }
            CommandContext ctx;
            ctx.shell = &TerminalShell::instance();
            ctx.focused_frame = this;
            ctx.args.insert(QStringLiteral("name"), target);
            auto r = ActionRegistry::instance().invoke(QStringLiteral("layout.save"), ctx);
            if (r.is_err())
                LOG_WARN("WindowFrame", QString("layout.save failed: %1")
                                            .arg(QString::fromStdString(r.error())));
        } else if (action == "import_data") {
            QString path = QFileDialog::getOpenFileName(
                this, "Import Layout", QDir::homePath(),
                "Fincept Layout (*.flayout *.fwsp);;All Files (*)");
            if (!path.isEmpty()) {
                auto r = LayoutCatalog::instance().import_from(path);
                if (r.is_err())
                    QMessageBox::warning(this, "Import Failed",
                                         QString::fromStdString(r.error()));
            }
        } else if (action == "export_data") {
            const LayoutId cur_id = layout::WorkspaceShell::current_id();
            if (cur_id.is_null()) {
                QMessageBox::information(
                    this, "Export Layout",
                    "Open or save a layout first, then export it.");
            } else {
                QString path = QFileDialog::getSaveFileName(
                    this, "Export Layout", QDir::homePath(),
                    "Fincept Layout (*.flayout)");
                if (!path.isEmpty()) {
                    auto r = LayoutCatalog::instance().export_to(cur_id, path);
                    if (r.is_err())
                        QMessageBox::warning(this, "Export Failed",
                                             QString::fromStdString(r.error()));
                }
            }
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
        // If user is authenticated and has a PIN, show lock screen first —
        // UNLESS this is an additional window opened while an existing
        // window has already cleared the PIN gate this session.
        // pin_gate_cleared_ was bootstrapped above from the process-wide
        // InactivityGuard flag (which is the single source of truth for
        // "is the terminal locked?"); skipping the prompt here just
        // mirrors the unlocked state into the new frame.
        if (auth_mgr.is_authenticated() && auth::PinManager::instance().has_pin()
            && !pin_gate_cleared_) {
            LOG_INFO("WindowFrame", "Session restored — showing PIN unlock");
            lock_screen_->show_unlock();
            locked_ = true;
            set_shell_visible(false);
            stack_->setCurrentIndex(3);
        } else if (auth_mgr.is_authenticated() && auth::PinManager::instance().has_pin()) {
            LOG_INFO("WindowFrame",
                     "Session already unlocked — skipping PIN prompt for additional window");
            set_shell_visible(true);
            stack_->setCurrentIndex(1);
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
            // Restore last-active screen as the focused tab and sync tab bar.
            // Per-window key so multi-window users don't override each other.
            const QString last = SessionManager::instance().last_screen(window_id_);
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

void WindowFrame::set_shell_visible(bool visible) {
    if (dock_toolbar_)
        dock_toolbar_->setVisible(visible && !focus_mode_ && !chat_mode_);
    if (dock_status_bar_)
        dock_status_bar_->setVisible(visible && !focus_mode_ && !chat_mode_);
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
        // Layout name set by WorkspaceShell::apply on every successful
        // layout switch / cold-boot restore. Empty until first apply.
        const QString ws = layout::WorkspaceShell::current_name();
        if (!ws.isEmpty())
            title += QString(" — %1").arg(ws);

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

void WindowFrame::adopt_frame_uuid(const WindowId& id) {
    if (id.is_null()) {
        LOG_WARN("WindowFrame", "adopt_frame_uuid: null id ignored");
        return;
    }
    if (!frame_uuid_.is_null() && frame_uuid_ != id) {
        // The frame already minted (or adopted) a different UUID — too late,
        // any panels created so far carry the old frame_id. Log and bail
        // rather than corrupting the PanelRegistry view.
        LOG_WARN("WindowFrame",
                 QString("adopt_frame_uuid: refusing to overwrite existing %1 with %2")
                     .arg(frame_uuid_.to_string(), id.to_string()));
        return;
    }
    frame_uuid_ = id;
}

// ── Phase 6 final: capture / apply layout ───────────────────────────────────

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

void WindowFrame::closeEvent(QCloseEvent* event) {
    // Force an immediate snapshot bypassing the 60s rate limit so a real
    // shutdown always has fresh state to restore from. Per Phase 6 the
    // legacy WorkspaceManager autosave is gone — the snapshot ring is the
    // single persistence path.
    SessionManager::instance().flush_snapshot_now();
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
