#include "algo_engine/AlgoEngineProducer.h"
#include "algo_engine/ScanMonitor.h"
#include "algo_engine/UniverseScanSelftest.h"
#include "algo_engine/fno/FnoAlgoSelftest.h"
#include "app/InstanceKey.h"
#include "app/InstanceLock.h"
#include "app/LegacyRootAdoption.h"
#include "app/MonitorPickerDialog.h"
#include "app/ScreenSmokeTest.h"
#include "app/StartupPrewarm.h"
#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/SessionGuard.h"
#include "core/components/ComponentCatalog.h"
#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/config/AppSettings.h"
#include "core/config/EditionIdentity.h"
#include "core/config/ProfileManager.h"
#include "core/crash/CrashHandler.h"
#include "core/debug/UiStallMonitor.h"
#include "core/currency/CurrencyManager.h"
#include "core/market/MarketSessions.h"
#include "ui/formatting/FormatSettingsStore.h"
#include "core/i18n/LanguageManager.h"
#include "core/keys/KeyConfigManager.h"
#include "core/layout/DockLayoutSelftest.h"
#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/ShutdownReason.h"
#include "ui/navigation/KeyRouting.h"
#include "core/session/SessionManager.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"
#include "core/window/WindowRegistry.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "mcp/McpInit.h"
#include "mcp/ProviderToolFormatSelfTest.h"
#include "mcp/ToolSelfTest.h"
#include "mcp/core/CoreToolsSelfTest.h"
#include "mcp/server/McpServerSelfTest.h"
#include "network/http/HttpClient.h"
#include "python/PythonSetupManager.h"
#include "python/PythonRunner.h"
#include "python/PythonWorker.h"
#include "screens/launchpad/LaunchpadScreen.h"
#include "screens/recovery/CrashRecoveryDialog.h"
#include "screens/setup/SetupScreen.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/agents/AgentService.h"
#include "services/alpha_arena/ArenaEngine.h"
#include "services/alpha_arena/ArenaSelftest.h"
#include "services/billing/FeeDiscountService.h"
#include "services/billing/TierService.h"
#include "services/cloud/AgentConfigCloudAdapter.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/cloud/DashboardCloudAdapter.h"
#include "services/cloud/NewsFeedCloudAdapter.h"
#include "services/cloud/NewsMonitorCloudAdapter.h"
#include "services/cloud/NotebookCloudAdapter.h"
#include "services/cloud/NotesCloudAdapter.h"
#include "services/cloud/PortfolioCloudAdapter.h"
#include "services/cloud/ReportCloudAdapter.h"
#include "services/cloud/SettingsCloudAdapter.h"
#include "services/cloud/WatchlistCloudAdapter.h"
#include "services/cloud/WorkflowCloudAdapter.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/connectors/ConnectorService.h"
#include "services/dbnomics/DBnomicsService.h"
#include "services/economics/EconomicsService.h"
#include "services/economics/MacroCalendarService.h"
#include "services/feeds/FeedSelfTest.h"
#include "services/geopolitics/GeopoliticsService.h"
#include "services/gov_data/GovDataService.h"
#include "services/instruments/InstrumentSync.h"
#include "services/llm/LlmService.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "services/maritime/MaritimeService.h"
#include "services/maritime/PortsCatalog.h"
#include "services/markets/MarketDataService.h"
#include "services/news/NewsService.h"
#include "services/notebooks/NotebookLibraryService.h"
#include "services/options/FiiDiiService.h"
#include "services/options/OISnapshotter.h"
#include "services/options/OptionChainService.h"
#include "services/polymarket/PolymarketWebSocket.h"
#include "services/prediction/PredictionCredentialStore.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "services/prediction/fincept_internal/FinceptInternalAdapter.h"
#include "services/prediction/kalshi/KalshiAdapter.h"
#include "services/prediction/polymarket/PolymarketAdapter.h"
#include "services/relationship_map/RelationshipMapService.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/updater/UpdateService.h"
#include "services/wallet/BuybackBurnService.h"
#include "services/wallet/RealYieldService.h"
#include "services/wallet/StakingService.h"
#include "services/wallet/TokenMetadataService.h"
#include "services/wallet/TreasuryService.h"
#include "services/wallet/WalletService.h"
#include "storage/HistoricalDataStore.h"
#include "storage/StorageManager.h"
#include "storage/repositories/NewsArticleRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"
#include "storage/sqlite/SqliteSupport.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"
#include "trading/AccountManager.h"
#include "trading/DataStreamManager.h"
#include "trading/ExchangeService.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderThreadPool.h"
#include "services/markets/WatchlistAlertService.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTradingSelftest.h"
#include "trading/UnifiedPortfolioService.h"
#include "trading/replication/PortfolioReplicationSelftest.h"
#include "ui/notifications/DesktopNotifier.h"
#include "ui/charts/ChartKitSelftest.h"
#include "ui/tables/LiveTableSelftest.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/UpdateDialog.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QLibrary>
#include <QMessageBox>
#include <QPointer>
#include <QMutex>
#include <QPushButton>
#include <QSessionManager>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSslSocket>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
#    include <Windows.h>
#endif
#ifdef Q_OS_UNIX
#    include <QSocketNotifier>

#    include <csignal>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

// Run `steps` one per event-loop turn, in order. A single QTimer::singleShot(0)
// that does everything is still one uninterruptible main-thread block — the
// window is frozen for its full duration whether it lands just before or just
// after the first paint, and that freeze is what users read as "slow startup".
// Chaining lets the event loop breathe (repaint, input, queued hub deliveries)
// between groups. Steps must be independent of each other's completion within a
// turn; only their relative ORDER is guaranteed.
static void post_chain(std::vector<std::function<void()>> steps, std::size_t i = 0) {
    if (i >= steps.size())
        return;
    QTimer::singleShot(0, qApp, [steps = std::move(steps), i]() mutable {
        steps[i]();
        post_chain(std::move(steps), i + 1);
    });
}

// ── In-app updater hooks (services/updater, ui/widgets/UpdateDialog) ─────────
// Set by quit_for_update(): the lastWindowClosed handler below then leaves the
// quit to it (no Launchpad surfacing while the terminal restarts into an update).
static bool g_update_restart_in_progress = false;

// UpdateService::restart_requested — the update is staged and this process must
// end so the new version can start. Same path as closing every window with
// Settings → General → "On last window close: Quit" (the default): each
// WindowFrame::closeEvent saves its screens, dock layout, geometry, the window set
// and a workspace snapshot; then, after the deferred deletes, quit() runs the
// aboutToQuit shutdown (MCP, Python, shell, databases). No exit confirmation
// exists on this path; the update window goes with its frame.
static void quit_for_update() {
    if (std::exchange(g_update_restart_in_progress, true))
        return;
    fincept::shutdown::note(fincept::shutdown::Reason::Update);
    LOG_INFO("App", "Update ready — closing every window and quitting so the new version can start");
    for (fincept::WindowFrame* frame : fincept::WindowRegistry::instance().frames()) {
        if (frame)
            frame->close();
    }
    QTimer::singleShot(0, qApp, []() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        // quit() is refused while some window will not close. The new version waits
        // for this process to end, so nothing may hold it open: exit() after a grace
        // period unless the shutdown (aboutToQuit) has begun by then.
        auto* fallback = new QTimer(qApp);
        fallback->setSingleShot(true);
        QObject::connect(fallback, &QTimer::timeout, qApp, []() {
            LOG_WARN("App", "Quit for the update was refused — exiting");
            QCoreApplication::exit(0);
        });
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, fallback, &QTimer::stop);
        fallback->start(5000);
        QCoreApplication::quit();
    });
}

#ifdef Q_OS_UNIX
// ── SIGTERM / SIGINT / SIGHUP end the session like closing its windows ───────
//
// `kill`, a desktop log-out, a service stop or Ctrl+C in the launching terminal
// used to end the process with the signal's default action: no window saved its
// layout and the clean-shutdown marker was never written, so the next start
// offered "Recover Previous Session" for a session that had not crashed.
//
// The handler only writes one byte to a socket pair (nothing else is safe in a
// signal handler); the event loop reads it and takes the normal quit path —
// every WindowFrame::closeEvent saves its state, then aboutToQuit runs the
// shutdown that marks the session clean. The handler is one-shot
// (SA_RESETHAND): a second signal ends a shutdown that hangs.
static int g_quit_signal_fds[2] = {-1, -1};

static void quit_signal_handler(int sig) {
    const char byte = static_cast<char>(sig); // the event loop names it in the shutdown line
    const ssize_t written = ::write(g_quit_signal_fds[0], &byte, 1);
    (void)written;
}

static const char* quit_signal_name(int sig) {
    switch (sig) {
        case SIGTERM:
            return "SIGTERM";
        case SIGINT:
            return "SIGINT";
        case SIGHUP:
            return "SIGHUP";
        default:
            return "other";
    }
}

static void install_quit_signal_handlers() {
    if (g_quit_signal_fds[0] != -1 || ::socketpair(AF_UNIX, SOCK_STREAM, 0, g_quit_signal_fds) != 0)
        return;
    auto* notifier = new QSocketNotifier(g_quit_signal_fds[1], QSocketNotifier::Read, qApp);
    QObject::connect(notifier, &QSocketNotifier::activated, qApp, [notifier]() {
        notifier->setEnabled(false);
        char byte = 0;
        const ssize_t got = ::read(g_quit_signal_fds[1], &byte, 1);
        (void)got;
        const QString signal_kv = QStringLiteral("signal=") + QLatin1String(quit_signal_name(byte));
        fincept::shutdown::note(fincept::shutdown::Reason::Signal, signal_kv);
        LOG_INFO("App", "Termination signal received " + signal_kv + " action=save_and_quit");
        // On disk now: if the shutdown hangs, a second signal ends the process.
        fincept::Logger::instance().flush();
        for (fincept::WindowFrame* frame : fincept::WindowRegistry::instance().frames()) {
            if (frame)
                frame->close();
        }
        QTimer::singleShot(0, qApp, []() {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            // quit() is refused while some window will not close; the sender of
            // the signal expects the process to end.
            auto* fallback = new QTimer(qApp);
            fallback->setSingleShot(true);
            QObject::connect(fallback, &QTimer::timeout, qApp, []() { QCoreApplication::exit(0); });
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, fallback, &QTimer::stop);
            fallback->start(3000);
            QCoreApplication::quit();
        });
    });
    for (int sig : {SIGTERM, SIGINT, SIGHUP}) {
        struct sigaction current {};
        // A signal the launcher asked to be ignored (nohup) stays ignored.
        if (::sigaction(sig, nullptr, &current) == 0 && current.sa_handler == SIG_IGN)
            continue;
        struct sigaction action {};
        action.sa_handler = quit_signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART | SA_RESETHAND;
        ::sigaction(sig, &action, nullptr);
    }
}
#endif

// The updater's app-shell hooks. Called once, right after the first window is
// shown, on both startup paths — never in --smoke-test runs (see the callers);
// --selftest-* / --dump-tools return before any window exists, and an offscreen /
// minimal platform skips the check and its prompts.
static void wire_updater() {
    static bool wired = false;
    if (std::exchange(wired, true))
        return;
    QObject::connect(&fincept::services::UpdateService::instance(),
                     &fincept::services::UpdateService::restart_requested, qApp, &quit_for_update);
    fincept::ui::UpdateDialog::watch_service(); // prompts, and keeps a running update on screen
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal"))
        return;
    // Every start, a few seconds after the first window is up (sign-in and lock
    // screen included): say how the update the previous run started ended, then
    // look for a newer release. UpdateService re-checks every few hours after that.
    QTimer::singleShot(4000, qApp, []() {
        QString to_version;
        bool succeeded = false;
        if (fincept::services::UpdateService::take_last_update_result(to_version, succeeded))
            fincept::ui::UpdateDialog::show_previous_update_result(to_version, succeeded);
        fincept::services::UpdateService::instance().check_for_updates(/*user_initiated=*/false);
    });
}

// Wire the two app-level lifecycle handlers that fire after the primary
// window exists: InstanceLock::message_received (a re-launch of the exe
// asks us to bring the running instance to the front — args ignored, the
// request itself is the trigger) and QApplication::lastWindowClosed (surface the Launchpad
// instead of quitting; the Launchpad's own close handler quits explicitly).
// Called from both the post-setup-screen path and the no-setup path so the
// two branches stay in sync.
static void wire_app_lifecycle(QApplication& app, fincept::InstanceLock& lock) {
    QObject::connect(&lock, &fincept::InstanceLock::message_received, [](const QStringList& /*args*/) {
        // Re-launching the exe while an instance is already
        // running means "bring the running instance forward" —
        // the standard single-instance behaviour — NOT "open a
        // new window". Opening a new window (and the monitor
        // picker that goes with it) stays an EXPLICIT action:
        // the toolbar "New Window", Ctrl+Shift+N, the Launchpad
        // button, and tear-off. Routing relaunches through the
        // picker surprised users by prompting for a monitor on
        // every open even when they never asked for a new window.
        const auto frames = fincept::WindowRegistry::instance().frames();
        if (!frames.isEmpty()) {
            // Lowest window_id (the primary) is the predictable
            // target. Activating one window pulls the whole app
            // forward on every platform we support.
            fincept::WindowFrame* target = frames.first();
            if (target->isMinimized())
                target->showNormal();
            target->raise();
            target->activateWindow();
            LOG_INFO("App", "Secondary instance request — raised existing window");
        } else {
            // No live frames (e.g. the user closed to the
            // Launchpad). Surface it instead of silently no-op'ing.
            fincept::screens::LaunchpadScreen::instance()->surface();
            LOG_INFO("App", "Secondary instance request — surfaced Launchpad");
        }
    });
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, []() {
        if (g_update_restart_in_progress)
            return; // quit_for_update() closed the windows and quits by itself
        // Settings → General → "On last window close" controls behaviour.
        // Default = "quit" so closing the last window quits the app like
        // every normal desktop app. Power users opt in to the Launchpad.
        const auto r = fincept::SettingsRepository::instance().get(QStringLiteral("general.on_last_window_close"),
                                                                   QStringLiteral("quit"));
        const QString choice = r.is_ok() ? r.value() : QStringLiteral("quit");

        if (choice == QStringLiteral("show_launchpad")) {
            // The process stays up for the Launchpad, but every terminal window
            // is gone: the next one (Launchpad, or a relaunch of the exe, which
            // surfaces the Launchpad) is a fresh opening of the terminal and asks
            // for the PIN, like a launch. A WindowFrame built while locked starts
            // on the lock screen instead of inheriting the unlocked session.
            auto& auth = fincept::auth::AuthManager::instance();
            if (fincept::auth::PinManager::instance().has_pin() && auth.is_authenticated()) {
                auto& guard = fincept::auth::InactivityGuard::instance();
                guard.set_enabled(false); // re-enabled at the next unlock
                guard.set_terminal_locked(true);
            }
            fincept::screens::LaunchpadScreen::instance()->surface();
        } else {
            // "quit" or any unknown value → quit (the safe default).
            //
            // On macOS quit() goes through NSApplication terminate:, which calls exit()
            // directly: exec() never returns, and the just-closed windows (WA_DeleteOnClose
            // → deleteLater) were destroyed from inside exit() after the app style was gone,
            // crashing on every Cmd+Q in chart / node-canvas teardown. Run the pending
            // deferred deletes first, on the next loop turn while everything is alive, then
            // quit.
            fincept::shutdown::note(fincept::shutdown::Reason::LastWindowClosed);
            QTimer::singleShot(0, qApp, []() {
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                QCoreApplication::quit();
            });
        }
    });
}

#ifdef Q_OS_LINUX
// Choose the windowing platform. Must run before QApplication is constructed:
// Qt reads QT_QPA_PLATFORM and -platform only while it starts.
//
// The Linux bundle carries both the xcb and the wayland platform plugins (with
// xcb alone it aborted on Wayland sessions without XWayland: "no Qt platform
// plugin could be initialized"). Left alone, Qt would now pick native Wayland on
// every Wayland session, but the docking system (floating / torn-off dock
// windows, drag-to-dock) and the custom window chrome have only ever run on
// X11 / XWayland. So X11 stays preferred whenever there is an X display:
//   -platform <name>          untouched (Qt's own override)
//   QT_QPA_PLATFORM=<list>    untouched, except a lone "wayland" with an X
//                             display gets ";xcb" appended (#415: Ubuntu Budgie
//                             26.04's labwc session exports QT_QPA_PLATFORM=wayland)
//   FINCEPT_WAYLAND=1         "wayland;xcb" (opt into native Wayland)
//   DISPLAY set               "xcb;wayland"
//   DISPLAY empty             Qt's default (Wayland when the session has it)
// Returns a one-line summary for stderr and the log.
static QByteArray choose_linux_platform(int argc, char* argv[]) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (qstrcmp(argv[i], "-platform") == 0 || qstrcmp(argv[i], "--platform") == 0)
            return QByteArray("-platform ") + argv[i + 1] + " (command line, unchanged)";
    }
    const QByteArray requested = qgetenv("QT_QPA_PLATFORM").trimmed();
    const bool x_display = !qEnvironmentVariableIsEmpty("DISPLAY");
    QByteArray value;
    QByteArray reason;
    if (!requested.isEmpty()) {
        if (!requested.startsWith("wayland") || requested.contains(';') || !x_display)
            return "QT_QPA_PLATFORM=" + requested + " (from the environment, unchanged)";
        value = requested + ";xcb";
        reason = "session forces wayland, xcb added as fallback";
    } else if (qEnvironmentVariableIntValue("FINCEPT_WAYLAND") == 1) {
        value = "wayland;xcb";
        reason = "FINCEPT_WAYLAND=1";
    } else if (x_display) {
        value = "xcb;wayland";
        reason = "X display available, X11 preferred";
    } else {
        return "Qt default (DISPLAY empty)";
    }
    qputenv("QT_QPA_PLATFORM", value);
    return "QT_QPA_PLATFORM=" + value + " (" + reason + ")";
}

#endif

// Result<T>::value()/error() read a std::variant with std::get, which this check counts as a
// possible throw. Every access below is guarded by is_ok()/is_err(), and an exception escaping
// main() would end in std::terminate either way.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
#ifdef Q_OS_LINUX
    // stderr: the log file does not exist until after QApplication, and this line
    // is what explains a platform-plugin abort that happens inside its constructor.
    const QByteArray inherited_qpa = qgetenv("QT_QPA_PLATFORM");
    const QByteArray qpa_choice = choose_linux_platform(argc, argv);
    std::fprintf(stderr, "[Platform] %s\n", qpa_choice.constData());
    std::fflush(stderr);
#endif
#ifdef Q_OS_WIN
    // Text rendering. Qt's default Windows font engines draw with ClearType,
    // whose subpixel colour fringes show as a blue / red cast on the small grey
    // text of the neutral themes, and they ignore QFont::NoSubpixelAntialias for
    // fonts set through stylesheets. The FreeType engine renders the same fonts
    // in clean greyscale. Chosen only when neither the environment nor the
    // command line names a platform; must be set before QApplication.
    bool fincept_set_qpa = false;
    {
        bool platform_arg = false;
        for (int i = 1; i < argc; ++i)
            platform_arg = platform_arg || qstrcmp(argv[i], "-platform") == 0 || qstrcmp(argv[i], "--platform") == 0;
        if (!platform_arg && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            qputenv("QT_QPA_PLATFORM", "windows:fontengine=freetype");
            fincept_set_qpa = true;
        }
    }
#endif

    // ── TLS backend selection (must happen before any Qt plugin loading) ────
    // Force QtNetwork to use the OpenSSL TLS backend across platforms.
    //   - macOS: Apple SecureTransport (qtls_st.cpp) double-frees inside
    //     SSLWrite when QWebSocket sends a close frame after a protocol
    //     error. Reproducible by opening the crypto tab — Kraken/Polymarket
    //     WS reconnect path crashes the main thread. SecureTransport was
    //     deprecated by Apple in 10.15.
    //   - Windows: Schannel has had similar instability around WS close
    //     and certificate revocation paths in Qt 6.8. OpenSSL is the
    //     consistent backend on every platform Qt supports.
    // QT_TLS_BACKEND is read by QTlsBackendFactory at plugin-loader time, so
    // it has to be set BEFORE QCoreApplication/QApplication is constructed —
    // setActiveBackend() after the fact does not always take effect because
    // singleton factories may already be bound.
    qputenv("QT_TLS_BACKEND", "openssl");

#ifdef Q_OS_MACOS
    // Pre-load OpenSSL from Homebrew so the openssl plugin's runtime dlopen
    // succeeds. Qt's QLibrary search defaults don't include /opt/homebrew/...
    // Order matters: libcrypto first (libssl depends on it).
    {
        const QStringList crypto_candidates = {
            QStringLiteral("/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib"),
            QStringLiteral("/usr/local/opt/openssl@3/lib/libcrypto.3.dylib"),
        };
        const QStringList ssl_candidates = {
            QStringLiteral("/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib"),
            QStringLiteral("/usr/local/opt/openssl@3/lib/libssl.3.dylib"),
        };
        for (const auto& p : crypto_candidates) {
            if (QFile::exists(p)) {
                QLibrary(p).load();
                break;
            }
        }
        for (const auto& p : ssl_candidates) {
            if (QFile::exists(p)) {
                QLibrary(p).load();
                break;
            }
        }
    }
#endif

    // ── Parse --profile <name> from argv before Qt initialises ───────────────
    // This must happen first so that:
    //   1. AppPaths returns the correct per-profile directories
    //   2. InstanceLock uses a profile-scoped IPC key so two different
    //      profiles can run simultaneously as independent primary instances
    {
        for (int i = 1; i < argc - 1; ++i) {
            if (qstrcmp(argv[i], "--profile") == 0) {
                fincept::ProfileManager::instance().set_active(QString::fromUtf8(argv[i + 1]));
                break;
            }
        }
        // AppPaths::root() must exist before ensure_all() so ProfileManager can
        // write the manifest. Create root now (single mkdir, idempotent).
        QDir().mkpath(fincept::AppPaths::root());
    }

    // Install the unhandled-exception filter BEFORE any Qt object is
    // constructed. On Windows this writes a minidump to AppPaths::crashdumps()
    // when the process dies from an access violation, stack overflow, or GS
    // cookie check failure (STATUS_STACK_BUFFER_OVERRUN — see issue #215).
    // Do it early so a crash in Qt's own startup still produces a dump.
    fincept::crash::install();

    // Required before QApplication when any dock panel contains an OpenGL widget
    // (Qt Charts, QOpenGLWidget) — prevents black rendering in floating windows.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // QApplication first — InstanceLock's QLocalServer needs an event loop
    // owner. Then the lock probes for an existing primary; if found, it
    // ships our argv to the primary (which opens a new WindowFrame in
    // response) and we exit cleanly.
    //
    // Why not SingleApplication? Its QSharedMemory + QSystemSemaphore lock
    // leaks on macOS Qt 6.6+ (QTBUG-111855), causing silent exits on Finder
    // launch. See InstanceLock.h for the fuller story (issues #234, #252).
    //
    // The instance key is scoped to the active profile name, so
    // "FinceptTerminal --profile work" and "FinceptTerminal --profile personal"
    // run as two independent primaries.
    QApplication app(argc, argv);
#ifdef Q_OS_LINUX
    // Native Wayland matches a window to its .desktop entry by this id (app icon,
    // taskbar grouping); unset, the window gets a generic icon. Flatpak installs
    // in.fincept.FinceptTerminal.desktop; the .deb (and older releases' AppImage
    // and .rpm) install the edition's fincept-terminal-community.desktop
    // (EditionIdentity.h) — the private build owns fincept-terminal.desktop.
    QGuiApplication::setDesktopFileName(
        (qEnvironmentVariableIsSet("FLATPAK_ID") || QFile::exists(QStringLiteral("/.flatpak-info")))
            ? QString::fromLatin1(fincept::edition::kFlatpakAppId)
            : QString::fromLatin1(fincept::edition::kDesktopFileName));
#endif
#ifdef Q_OS_WIN
    // The platform is loaded; do not hand our font-engine choice to processes we launch.
    if (fincept_set_qpa)
        qunsetenv("QT_QPA_PLATFORM");
#endif
#ifdef Q_OS_LINUX
    // The platform is loaded. Give processes we launch (xdg-open -> the user's
    // browser or file manager) the session's QT_QPA_PLATFORM, not our preference.
    if (inherited_qpa.isNull())
        qunsetenv("QT_QPA_PLATFORM");
    else
        qputenv("QT_QPA_PLATFORM", inherited_qpa);
#endif
    // Edition identity (EditionIdentity.h): keys the QSettings store, the
    // QStandardPaths App*/Cache folders and WM_CLASS. Not Fincept/FinceptTerminal,
    // which the private build owns. The visible product name stays "Fincept Terminal".
    app.setApplicationName(QString::fromLatin1(fincept::edition::kSettingsApplication));
    app.setOrganizationName(QString::fromLatin1(fincept::edition::kSettingsOrganization));

    // The data folder is part of this process's identity (app/InstanceKey.h):
    // FINCEPT_DATA_DIR naming another folder than the edition's default one makes
    // a separate terminal — its own single-instance key below, and its own
    // settings store inside that folder, so a test or scratch run neither opens a
    // window in the user's running terminal nor writes the user's window layout.
    // (app_settings::open() and QSettings() users follow — core/config/AppSettings.h.
    // The default folder's store is untouched. A refused override counts as none.)
    const QString instance_override_root = fincept::instance_key::canonical_root(fincept::AppPaths::root_override());
    const QString instance_default_root = fincept::instance_key::canonical_root(
        fincept::AppPaths::platform_data_base() + QLatin1Char('/') + QLatin1String(fincept::edition::kDataDirName));
    const bool own_data_root = !instance_override_root.isEmpty() && instance_override_root != instance_default_root;
    if (own_data_root)
        fincept::app_settings::use_store_under(fincept::AppPaths::root());
#ifndef FINCEPT_VERSION_STRING
#    define FINCEPT_VERSION_STRING "0.0.0-dev"
#endif
    app.setApplicationVersion(QStringLiteral(FINCEPT_VERSION_STRING));

    // Quit only when LaunchpadScreen::closeEvent calls QCoreApplication::quit().
    // Default Qt behaviour fires lastWindowClosed AND schedules an auto-quit;
    // we connect a slot to surface() the Launchpad on lastWindowClosed, and
    // the auto-quit would race that slot — sometimes killing the app while
    // the launchpad is mid-show. With this off, the only quit path is the
    // explicit one in LaunchpadScreen::closeEvent.
    app.setQuitOnLastWindowClosed(false);

    // One rule for keys the shell and a screen both want (ui/navigation/KeyRouting.h):
    // the focused input / list / pop-up and the screen's own shortcut first,
    // modifier shortcuts and F-keys stay the shell's unless a screen claims them.
    fincept::ui::install_shell_key_router(&app);

    // Belt-and-braces: if QT_TLS_BACKEND wasn't honoured for some reason
    // (e.g. plugin load order on a particular platform), retry the switch
    // explicitly. This is a no-op if openssl is already active.
    {
        const auto backends = QSslSocket::availableBackends();
        if (QSslSocket::activeBackend() != QStringLiteral("openssl") && backends.contains(QStringLiteral("openssl"))) {
            QSslSocket::setActiveBackend(QStringLiteral("openssl"));
        }
    }

    // ── Single-instance lock + new-window IPC ────────────────────────────────
    // Edition-scoped: the private build's primary listens on "FinceptTerminal-<profile>",
    // and a shared key handed this build's argv to it and exited. Data-folder
    // scoped: a FINCEPT_DATA_DIR other than the default folder adds a hash of
    // that folder, so it is its own primary (the default folder's key is unchanged).
    const QString profile_key = fincept::instance_key::make(QLatin1String(fincept::edition::kInstanceKeyPrefix),
                                                            fincept::ProfileManager::instance().active(),
                                                            instance_override_root, instance_default_root);
    if (own_data_root)
        LOG_INFO("App", QStringLiteral("Separate instance for its own data folder root_tag=%1")
                            .arg(fincept::instance_key::root_tag(instance_override_root)));
    fincept::InstanceLock instance_lock;
    const auto lock_status = instance_lock.acquire(profile_key, QCoreApplication::arguments());

    // ── Secondary instance: argv was already shipped to the primary. Exit. ──
    if (lock_status == fincept::InstanceLock::Status::Secondary) {
#ifdef Q_OS_WIN
        // Grant the primary process permission to bring its new window to
        // the foreground — Windows blocks focus-steal without this. Pre-
        // SingleApplication this used app.primaryPid(); without that we
        // call AllowSetForegroundWindow(ASFW_ANY) which whitelists the
        // whole foreground request from this process.
        AllowSetForegroundWindow(ASFW_ANY);
#endif
        return 0;
    }

    // ── Primary instance from here on ────────────────────────────────────────

    // First start under the edition's own data root: carry a community install
    // over from the com.fincept.terminal folder released versions shared with the
    // private build, or leave that folder untouched (LegacyRootAdoption.h). Before
    // anything opens a file under AppPaths::root(); never shows UI.
    fincept::legacy_root::adopt_legacy_root();

    // Bring up the TerminalShell. This is the multi-window refactor's
    // process-level coordinator. Phase 1 ships a skeleton: it bootstraps
    // ProfilePaths, resolves the active ProfileId, and warms the registries
    // (WindowRegistry, ActionRegistry) so WindowFrame constructors don't race
    // their singleton init.
    //
    // Must run BEFORE any service init so future phases that lift services
    // into the shell can rely on it being present.
    fincept::TerminalShell::instance().initialise();
#ifdef Q_OS_UNIX
    // From here on a session exists to save: a termination signal quits cleanly.
    install_quit_signal_handlers();
#endif
    // Log-off / shutdown / restart of the OS (Windows WM_QUERYENDSESSION, the X11 /
    // Wayland session manager): Qt asks the application to save before the session
    // ends. Record why the process is about to end; the windows' closeEvents and
    // aboutToQuit do the saving.
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &app, [](QSessionManager&) {
        fincept::shutdown::note(fincept::shutdown::Reason::SessionEnd);
        LOG_INFO("App", "OS session ending action=save_and_quit");
    });
    // Decrypted credentials cached for Python children (PythonRunner) leave
    // memory when the terminal locks; quit drops them in PythonRunner::shutdown().
    QObject::connect(&fincept::auth::InactivityGuard::instance(),
                     &fincept::auth::InactivityGuard::terminal_locked_changed, &app, [](bool locked) {
                         if (locked)
                             fincept::python::PythonRunner::forget_credentials();
                     });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        // One line, first: why and after how long. (Nothing recorded = an explicit
        // quit: the Launchpad's close, a Quit action.)
        LOG_INFO("App", QStringLiteral("Shutting down %1 uptime_s=%2")
                            .arg(fincept::shutdown::describe(fincept::shutdown::Reason::Quit))
                            .arg(fincept::shutdown::uptime_s()));
        fincept::Logger::instance().flush(); // the reason is on disk even if teardown hangs
        // MCP first, shell second — deliberate ordering.
        //
        // shutdown_mcp() stops TerminalMcpBridge and then every external MCP
        // server child process (npx/uvx/python). It had ZERO call sites, so
        // those children were orphaned on every exit and kept running as the
        // user. It is idempotent, so calling it here is safe even if some other
        // teardown path reaches it too.
        //
        // It must run BEFORE TerminalShell::shutdown() because the shell
        // teardown closes the workspace DB and deletes CrashRecovery /
        // WorkspaceSnapshotRing, while MCP's workspace/dashboard tool handlers
        // read exactly those. Draining MCP first guarantees no tool call is
        // in flight against a half-torn-down shell.
        fincept::mcp::shutdown_mcp();

        // Destroy the windows now, while QApplication, its style and the stylesheet
        // engine are still alive. Left to WA_DeleteOnClose (deleteLater), the frames were
        // destroyed during ~QApplication after the app style was gone, so any screen with
        // a QGraphicsScene (Equity Research Financials charts, the Node Editor canvas)
        // crashed in QGraphicsWidget / QStyleSheetStyle teardown and macOS reported
        // "Fincept Terminal quit unexpectedly" on every Cmd+Q. frames() returns a copy, and
        // each frame unregisters itself in its destructor.
        for (fincept::WindowFrame* frame : fincept::WindowRegistry::instance().frames())
            delete frame;

        // Stop the yfinance daemon before the shell tears anything down.
        //
        // PythonWorker owns a QProcess plus repeating QTimers (a deadline-sweep
        // timer can call proc_->kill()). It is a singleton, so without this call
        // its destructor would run at STATIC destruction — after QApplication is
        // gone and after the crash handler is unregistered. A timer or process
        // callback firing in that window crashes with no minidump and no log
        // line, which is exactly the kind of exit failure that is
        // near-impossible to diagnose after the fact. stop() is idempotent.
        fincept::python::PythonWorker::instance().stop();

        // Kill any running uv / curl / tar child together with its own
        // children (Job Object / process group) so nothing keeps installing —
        // or holding cache locks — after the app is gone. No-op when idle.
        fincept::python::PythonSetupManager::instance().cancel(/*wait_for_exit=*/true);

        // Every running script together with what it started (a connector
        // behind `fincept_connector.py run`, a backtest's workers): killed as a
        // tree, queue dropped, cached credentials forgotten. No-op when no
        // script ever ran.
        fincept::python::PythonRunner::shutdown();

        fincept::TerminalShell::instance().shutdown();

        // Orderly close of fincept.db and cache.db: PRAGMA optimize, then
        // wal_checkpoint(TRUNCATE) so no -wal is left behind for a plain file
        // copy to miss, then close. Registered here but RUN as a post-routine
        // (from ~QCoreApplication, after every aboutToQuit handler), because
        // handlers connected later than this one still write to the databases:
        // CodingAgentService marks unfinished runs interrupted, ReportBuilder
        // flushes its autosave, StorageManager joins its retention sweep. Pool
        // work gets a bounded 2 s to finish first; a task still running after the
        // close gets "connection unavailable" from its next call instead of
        // writing into a closed file.
        static bool db_close_registered = false;
        if (!db_close_registered) {
            db_close_registered = true;
            qAddPostRoutine([]() {
                QDeadlineTimer deadline(2000);
                const auto remaining = [&deadline]() { return int(qMax<qint64>(0, deadline.remainingTime())); };
                const bool pools_idle = QThreadPool::globalInstance()->waitForDone(remaining()) &&
                                        fincept::trading::order_thread_pool().waitForDone(remaining()) &&
                                        fincept::trading::blocking_thread_pool().waitForDone(remaining());
                if (!pools_idle)
                    LOG_WARN("App", "Background tasks still running after 2 s; closing the databases anyway");

                auto& cache = fincept::CacheDatabase::instance();
                if (cache.is_open()) {
                    QSqlDatabase& conn = cache.raw_db();
                    if (conn.isOpen()) {
                        QSqlQuery q(conn);
                        q.exec(QStringLiteral("PRAGMA analysis_limit = 400"));
                        q.exec(QStringLiteral("PRAGMA optimize"));
                    }
                    fincept::db::checkpoint_truncate(conn);
                    cache.close();
                }
                fincept::Database::instance().shutdown();
                // Last post-routine before the logger's own flush (post-routines run in
                // reverse order of registration) — also reached when exec() never
                // returns (macOS quit goes through exit()).
                LOG_INFO("App", QStringLiteral("Shutdown complete %1 uptime_s=%2")
                                    .arg(fincept::shutdown::describe(fincept::shutdown::Reason::Quit))
                                    .arg(fincept::shutdown::uptime_s()));
            });
        }
    });

    // Register DataHub payload meta-types (QuoteData, HistoryPoint, InfoData,
    // NewsArticle, EconomicsResult) so they can flow through QVariant-keyed
    // topics and cross-thread queued signals. Phase 0 — see
    // fincept-qt/DATAHUB_ARCHITECTURE.md.
    // Phase 2: register MarketDataService as the `market:quote:*` producer.
    fincept::datahub::register_metatypes();
    // SymbolContext payload types — signals cross threads when a producer
    // service (not just UI) publishes a group change.
    qRegisterMetaType<fincept::SymbolRef>("fincept::SymbolRef");
    qRegisterMetaType<fincept::SymbolGroup>("fincept::SymbolGroup");
    // Phase 6: load the Component Browser catalogue. Try the build-side copy
    // first (present after cmake configure copies resources) and fall back to
    // the source-tree path for local dev runs without install step.
    // bundled_resource() also covers the shipped macOS bundle (Contents/Resources/)
    // and Linux FHS installs (share/fincept-terminal/resources/: Flatpak, cmake --install).
    fincept::ComponentCatalog::instance().load_with_fallbacks({
        fincept::AppPaths::bundled_resource(QStringLiteral("component_catalog.json")),
        QCoreApplication::applicationDirPath() + "/component_catalog.json",
        "resources/component_catalog.json",
    });
    fincept::services::MarketDataService::instance().ensure_registered_with_hub();

    // ── Sync services needed by the default dashboard ─────────────────────────
    // Anything a default dashboard widget subscribes to during its first show
    // must be registered with the hub before the window paints. Everything
    // else is deferred to a single QTimer::singleShot(0) below — the event
    // loop runs that batch immediately after the first paint, so cold-start
    // perceived latency drops without changing functional behavior.
    fincept::services::NewsService::instance().ensure_registered_with_hub();
    fincept::services::EconomicsService::instance().ensure_registered_with_hub();
    fincept::services::MacroCalendarService::instance().ensure_registered_with_hub();
    fincept::trading::DataStreamManager::instance().ensure_registered_with_hub();
    fincept::services::geo::GeopoliticsService::instance().ensure_registered_with_hub();
    fincept::services::maritime::MaritimeService::instance().ensure_registered_with_hub();
    fincept::services::maritime::PortsCatalog::instance().ensure_registered_with_hub();
    fincept::services::RelationshipMapService::instance().ensure_registered_with_hub();
    fincept::services::ma::MAAnalyticsService::instance().ensure_registered_with_hub();
    fincept::wallet::TokenMetadataService::instance().load_from_storage();

    // ── Pre-warm the dashboard topics ────────────────────────────────────────
    // The user spends real time on the login / setup / recovery flow before
    // the dashboard ever paints. Kick the hub now so producers start fetching
    // immediately; by the time the dashboard widgets subscribe in showEvent,
    // peek() returns a fresh value and deliver_initial_value() paints it on
    // the first frame instead of showing the loading overlay.
    //
    // Set is the union of topics used by every widget in the default
    // `portfolio_manager` template plus the global indices/forex/crypto/
    // commodities universes (so any user-customised default still hits the
    // warm cache). Late-registered producers (the QTimer::singleShot(0)
    // batch below) warm themselves on their own next scheduler pass once
    // subscribers exist; pre-warming them here would orphan-log because
    // ensure_registered_with_hub() hasn't run for them yet.
    //
    // Free topics only: nothing that costs credits runs automatically. Every
    // topic passes app/StartupPrewarm.h's free_only() — a billed or undeclared
    // one (the economic calendar, geopolitics events, vessel data, …) is dropped
    // and fails a debug build here; tests/tst_startup_prewarm.cpp pins the lists.
    QTimer::singleShot(0, qApp, []() {
        auto& hub = fincept::datahub::DataHub::instance();
        QStringList topics;

        auto add_quotes = [&](const QStringList& syms) {
            topics.reserve(topics.size() + syms.size());
            for (const auto& s : syms)
                topics.append(QStringLiteral("market:quote:") + s);
        };

        // Default-template widget symbol sets (kept aligned with the widget
        // source — if a widget's hardcoded list changes, update here too).
        add_quotes(fincept::services::MarketDataService::indices_symbols());
        add_quotes(fincept::services::MarketDataService::forex_symbols());
        add_quotes(fincept::services::MarketDataService::crypto_symbols());
        add_quotes(fincept::services::MarketDataService::commodity_symbols());
        add_quotes({"^GSPC", "^IXIC", "^DJI", "^RUT", "^VIX", "GC=F"});                                  // performance
        add_quotes({"^VIX", "SPY", "QQQ", "IWM", "TLT", "NVDA", "TSLA", "AMD", "META", "PLTR", "COIN"}); // risk_metrics
        add_quotes({"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "TSLA", "META", "JPM"}); // watchlist default

        // Non-quote topics used by the default template — the free ones (its
        // economic calendar is billed: the tile offers LOAD CALENDAR instead).
        topics.append(fincept::startup_prewarm::default_template_topics());

        // De-duplicate (several add_quotes calls overlap on common symbols).
        topics.removeDuplicates();

        QStringList not_free;
        topics = fincept::startup_prewarm::free_only(topics, &not_free);
        Q_ASSERT_X(not_free.isEmpty(), "startup pre-warm",
                   "a billed or undeclared topic was added to the pre-warm list (see app/StartupPrewarm.h)");
        if (!not_free.isEmpty())
            LOG_WARN("App", fincept::logf("Start-up pre-warm skipped topics not declared free",
                                          {{"count", static_cast<int>(not_free.size())},
                                           {"topics", not_free.join(QLatin1Char(','))}}));

        // force=true bypasses min_interval_ms so the very first cold-start
        // fetch isn't gated by an unrelated test refresh; producer rate
        // limits still apply at dispatch (DataHub::flush_coalesced_requests).
        hub.request(topics, /*force=*/true);
        LOG_DEBUG("App", QString("Pre-warmed %1 dashboard topics during login screen").arg(topics.size()));
    });

    // ── Deferred service init — fires after first window paint ───────────────
    // These services back tab-specific screens (F&O, prediction markets,
    // alpha arena, agents, wallet/treasury/staking, etc.) — none of them is
    // needed for the dashboard's first paint, so registering them here would
    // only add latency to the user-visible cold start. Late registration is
    // safe: the hub's scheduler tick picks up matching subscriptions on the
    // next pass once the producer is registered.
    //
    // Split into three groups run one per event-loop turn (post_chain above).
    // As a single lambda this was ~180 lines of uninterruptible main-thread
    // work — 20 hub registrations, prediction-adapter construction with
    // SecureStorage credential loads, 12 cloud adapters plus a network
    // refresh_all(), 15 policy patterns, wallet restore, and a live broker
    // ping sweep — and the window stayed frozen for all of it. Order WITHIN a
    // group is preserved; the groups only touch their own singletons.

    // ── Group 1: DataHub producer registrations ─────────────────────────────
    auto init_hub_producers = []() {
        // F&O / Options chain — `option:chain:*`, `option:tick:*`,
        // `option:atm_iv:*`, `fno:pcr:*`, `fno:max_pain:*`.
        fincept::services::options::OptionChainService::instance().ensure_registered_with_hub();
        // F&O OI snapshotter — subscribes to option:chain:* and persists
        // minute-aligned OI/LTP/Vol/IV rows to SQLite. Producer for
        // oi:history:* (window queries).
        fincept::services::options::OISnapshotter::instance().ensure_registered_with_hub();
        // F&O FII/DII flows — daily NSE cash-market institutional buy/sell.
        fincept::services::options::FiiDiiService::instance().ensure_registered_with_hub();
        // Multi-broker session manager — `ws:kraken:*` / `ws:hyperliquid:*`.
        fincept::trading::ExchangeSessionManager::instance().ensure_registered_with_hub();
        // Prediction Markets — `prediction:polymarket:*`.
        fincept::services::polymarket::PolymarketWebSocket::instance().ensure_registered_with_hub();
        // Alpha Arena engine — init() is idempotent and only scans for
        // crashed competitions (no-op with none). Not a DataHub Producer
        // (callback-style by design).
        fincept::arena::ArenaEngine::instance().init();
        {
            auto& reg = fincept::services::prediction::PredictionExchangeRegistry::instance();
            reg.register_adapter(std::make_unique<fincept::services::prediction::polymarket_ns::PolymarketAdapter>());
            reg.register_adapter(std::make_unique<fincept::services::prediction::kalshi_ns::KalshiAdapter>());
            // Fincept internal prediction-market adapter (demo mode until
            // `fincept.markets_endpoint` is configured).
            reg.register_adapter(
                std::make_unique<fincept::services::prediction::fincept_internal::FinceptInternalAdapter>());

            // Hydrate credentials from SecureStorage if previously saved.
            if (auto* pm = dynamic_cast<fincept::services::prediction::polymarket_ns::PolymarketAdapter*>(
                    reg.adapter(QStringLiteral("polymarket")))) {
                pm->reload_credentials();
            }
            if (auto* ks = dynamic_cast<fincept::services::prediction::kalshi_ns::KalshiAdapter*>(
                    reg.adapter(QStringLiteral("kalshi")))) {
                if (auto creds = fincept::services::prediction::PredictionCredentialStore::load_kalshi()) {
                    ks->set_credentials(*creds);
                }
                // Register the Kalshi WS producer + push-only topic policies
                // (prediction:kalshi:price:*, prediction:kalshi:orderbook:*) with the hub.
                ks->ensure_registered_with_hub();
            }
            if (auto* fi = reg.adapter(QStringLiteral("fincept"))) {
                fi->ensure_registered_with_hub();
            }
        }
        // Specialized data sources.
        fincept::services::DBnomicsService::instance().ensure_registered_with_hub();
        fincept::services::GovDataService::instance().ensure_registered_with_hub();
        // Agents — `agent:*` push-only producer.
        fincept::services::AgentService::instance().ensure_registered_with_hub();
        // Python data connectors catalogue — `connector:<name>:<command>:<args-hash>`.
        fincept::services::connectors::ConnectorService::instance().ensure_registered_with_hub();

        // Algo Engine — `algo:metrics:*`, `algo:trade:*`, `algo:state:*`.
        fincept::algo::AlgoEngineProducer::instance().ensure_registered_with_hub();
    };

    // ── Group 2: Fincept Cloud sync ─────────────────────────────────────────
    auto init_cloud_sync = []() {
        // Drains the durable outbox (push) + pulls cloud→local. NOT a DataHub
        // producer; reads stay on the local repo cache. Every adapter must be
        // registered before initialize(), which is why they share one group.
        // See fincept-qt/CLOUD_SYNC_PLAN.md.
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::WatchlistCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::NotesCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::PortfolioCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::AgentConfigCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::ReportCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::WorkflowCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::DashboardCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::SettingsCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::NewsMonitorCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::NewsFeedCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().register_adapter(
            &fincept::services::cloud::NotebookCloudAdapter::instance());
        fincept::services::cloud::CloudSyncEngine::instance().initialize();
    };

    // ── Group 3: wallet / treasury / billing + broker session monitor ───────
    auto init_wallet_treasury_and_monitors = []() {
        // Token metadata refresh — network call to Jupiter aggregator.
        fincept::wallet::TokenMetadataService::instance().refresh_from_jupiter_async();
        // Wallet — `wallet:balance:*`, `market:price:token:*`.
        fincept::wallet::WalletService::instance().ensure_registered_with_hub();
        fincept::wallet::WalletService::instance().restore_from_storage();

        // Fee-discount eligibility producer (display only; the discount is planned and not applied anywhere).
        {
            static fincept::billing::FeeDiscountService discount_service;
            auto& hub = fincept::datahub::DataHub::instance();
            hub.register_producer(&discount_service);
            fincept::datahub::TopicPolicy p;
            p.ttl_ms = 60 * 1000;
            p.min_interval_ms = 15 * 1000;
            hub.set_policy_pattern(QStringLiteral("billing:fncpt_discount:*"), p);
        }

        // Buyback & burn / treasury producers (treasury:*).
        {
            static fincept::wallet::BuybackBurnService buyback_burn_service;
            static fincept::wallet::TreasuryService treasury_service;
            auto& hub = fincept::datahub::DataHub::instance();
            hub.register_producer(&buyback_burn_service);
            hub.register_producer(&treasury_service);

            fincept::datahub::TopicPolicy epoch_p;
            epoch_p.ttl_ms = 60 * 1000;
            epoch_p.min_interval_ms = 30 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:buyback_epoch"), epoch_p);

            fincept::datahub::TopicPolicy burn_p;
            burn_p.ttl_ms = 5 * 60 * 1000;
            burn_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:burn_total"), burn_p);

            fincept::datahub::TopicPolicy supply_p;
            supply_p.ttl_ms = 60 * 60 * 1000;
            supply_p.min_interval_ms = 5 * 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:supply_history"), supply_p);

            fincept::datahub::TopicPolicy reserves_p;
            reserves_p.ttl_ms = 5 * 60 * 1000;
            reserves_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:reserves"), reserves_p);

            fincept::datahub::TopicPolicy runway_p;
            runway_p.ttl_ms = 5 * 60 * 1000;
            runway_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:runway"), runway_p);
        }

        // STAKE tab producers (veFNCPT lock + real-yield + tier system).
        {
            auto& hub = fincept::datahub::DataHub::instance();
            hub.register_producer(&fincept::wallet::StakingService::instance());
            hub.register_producer(&fincept::wallet::RealYieldService::instance());
            hub.register_producer(&fincept::billing::TierService::instance());

            fincept::datahub::TopicPolicy locks_p;
            locks_p.ttl_ms = 60 * 1000;
            locks_p.min_interval_ms = 30 * 1000;
            hub.set_policy_pattern(QStringLiteral("wallet:locks:*"), locks_p);
            hub.set_policy_pattern(QStringLiteral("wallet:vefncpt:*"), locks_p);

            fincept::datahub::TopicPolicy yield_p;
            yield_p.ttl_ms = 5 * 60 * 1000;
            yield_p.min_interval_ms = 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("wallet:yield:*"), yield_p);

            fincept::datahub::TopicPolicy revenue_p;
            revenue_p.ttl_ms = 60 * 60 * 1000;
            revenue_p.min_interval_ms = 5 * 60 * 1000;
            hub.set_policy_pattern(QStringLiteral("treasury:revenue"), revenue_p);

            // billing:tier:* derived from wallet:vefncpt:* — TTL is just a
            // safety net; the service republishes whenever vefncpt emits.
            fincept::datahub::TopicPolicy tier_p;
            tier_p.ttl_ms = 60 * 1000;
            tier_p.min_interval_ms = 15 * 1000;
            hub.set_policy_pattern(QStringLiteral("billing:tier:*"), tier_p);
        }

        // Broker session monitor — re-validates each connected broker account's
        // access token on a 5-min cadence and silently refreshes where supported
        // (Zerodha/Angel One TOTP re-login, Fyers refresh token). Keeps the
        // connection indicator honest instead of showing a stale "green".
        fincept::trading::AccountManager::instance().start_session_monitor();

        // Periodically auto-download historical candles for any watchlisted
        // series. Double-gated to a no-op: does nothing unless the Historify
        // watchlist has entries AND a broker account is connected.
        auto* historify_timer = new QTimer(qApp);
        historify_timer->setInterval(15 * 60 * 1000); // 15 min
        QObject::connect(historify_timer, &QTimer::timeout, qApp,
                         []() { fincept::storage::HistoricalDataStore::instance().refresh_watchlist(); });
        historify_timer->start();

        fincept::services::InstrumentSync::instance().start(); // symbol index (seed, broker masters) + master sync

        LOG_DEBUG("App", "Deferred service init complete");
    };

    // Create all application directories under %LOCALAPPDATA%/com.fincept.terminal
    fincept::AppPaths::ensure_all();
    // Record every time the GUI thread stops processing events (a freeze the
    // user feels) in <logs>/ui_stalls.log — see UiStallMonitor.h.
    fincept::debug::UiStallMonitor::start(fincept::AppPaths::logs() + "/ui_stalls.log");
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() { fincept::debug::UiStallMonitor::stop(); });

    // Optional user-supplied exchange holiday overrides (missing file is fine).
    fincept::core::market::load_holiday_overrides(fincept::AppPaths::data() + "/market_holidays.json");

    // ── One-time migration from legacy %APPDATA% location ─────────────────
    // Current locations (under %LOCALAPPDATA%\com.fincept.terminal\):
    //   Log: <root>/logs/fincept.log    DB: <root>/data/fincept.db
    {
        const QString old_base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const auto migrate_file = [](const QString& old_path, const QString& new_path) {
            if (QFile::exists(old_path) && !QFile::exists(new_path))
                QFile::rename(old_path, new_path);
        };
        migrate_file(old_base + "/fincept.db", fincept::AppPaths::data() + "/fincept.db");
        migrate_file(old_base + "/cache.db", fincept::AppPaths::data() + "/cache.db");
        migrate_file(old_base + "/fincept.log", fincept::AppPaths::logs() + "/fincept.log");
        migrate_file(old_base + "/fincept-files", fincept::AppPaths::files());
        // Remove stale WAL/SHM from old location too
        QFile::remove(old_base + "/fincept.db-wal");
        QFile::remove(old_base + "/fincept.db-shm");
        QFile::remove(old_base + "/cache.db-wal");
        QFile::remove(old_base + "/cache.db-shm");
    }

    // SQLite owns its own .db-wal / .db-shm files. Pre-deleting them is
    // destructive: any committed transaction that has not yet been checkpointed
    // back into the main .db file lives entirely in the WAL. Auto-checkpoint
    // only fires past ~1000 pages, and the on-close checkpoint can fail
    // silently — small writes (e.g. pin_hash on the secure_credentials table)
    // routinely persist only in the WAL across runs. SQLite reads the WAL on
    // open to recover any uncheckpointed or crash-truncated state, so we leave
    // these files for SQLite to manage. Single-instance enforcement is owned
    // by InstanceLock above.

    // The legacy v3 settings database (fincept_settings.db in the FinceptTerminal folder of
    // %LOCALAPPDATA%, and the macOS / Linux equivalents) is NOT cleaned here
    // any more: that folder is shared with the separate private build of Fincept
    // Terminal, and deleting a database's -wal throws away its committed but
    // uncheckpointed transactions (see the note above). It is only read, below.

    fincept::Logger::instance().set_file(fincept::AppPaths::logs() + "/fincept.log");

    // Seed the prebuilt Fincept Notebook library into the File Manager on first
    // run (idempotent — guarded by a marker file). Makes the curated notebooks
    // appear in both the Notebook Library and the File Manager out of the box.
    fincept::services::NotebookLibraryService::instance().seed_into_files();

    // P3.18 — route Qt's own qDebug/qWarning/qCritical messages into our log
    // file so framework/3rd-party warnings are visible in Release builds.
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
        // Tag "Qt" for plain qWarning() (category "default"), "Qt.<category>" otherwise.
        QString tag = QStringLiteral("Qt");
        if (ctx.category && *ctx.category && qstrcmp(ctx.category, "default") != 0)
            tag += QLatin1Char('.') + QLatin1String(ctx.category);
        auto& log = fincept::Logger::instance();
        switch (type) {
            case QtDebugMsg:
            case QtInfoMsg:
                // Framework chatter: developer detail, never an INFO event of ours.
                if (log.should_log(fincept::LogLevel::Debug, tag))
                    log.debug(tag, msg);
                break;
            case QtWarningMsg: {
                if (!log.should_log(fincept::LogLevel::Warn, tag))
                    break;
                // The same framework warning can repeat per paint or per row: one line
                // per distinct message (first 60 characters) per minute, with a count.
                // Immortal, like the logger: Qt still warns while static objects are being
                // destroyed at exit ("QProcess: Destroyed while process is still running"
                // from a QProcess owned by a static), and a destructible map here had
                // already been freed by then — a use-after-free that crashed the self-tests
                // at exit on Linux and macOS.
                static QMutex& rl_mutex = *new QMutex;
                static auto& rl_seen = *new QHash<QString, QPair<qint64, int>>; // key -> (last written ms, held back)
                const QString key = tag + QLatin1Char('|') + msg.left(60);
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                int held = 0;
                {
                    QMutexLocker lock(&rl_mutex);
                    if (rl_seen.size() > 512)
                        rl_seen.clear();
                    auto& e = rl_seen[key];
                    if (e.first != 0 && now - e.first < 60'000) {
                        ++e.second;
                        break;
                    }
                    held = e.second;
                    e = {now, 0};
                }
                QString line = fincept::privacy::redact_text(msg);
                if (held > 0)
                    line += QStringLiteral(" (suppressed=%1)").arg(held);
                log.warn(tag, line);
                break;
            }
            case QtCriticalMsg:
                log.error(tag, fincept::privacy::redact_text(msg));
                break;
            case QtFatalMsg:
                fincept::shutdown::note_fatal(QStringLiteral("source=qt_fatal"));
                log.error(tag, fincept::privacy::redact_text(msg));
                log.error("App", QStringLiteral("Shutting down %1 uptime_s=%2")
                                     .arg(fincept::shutdown::describe())
                                     .arg(fincept::shutdown::uptime_s()));
                log.flush_and_close();
                break;
        }
    });
    {
        auto& log = fincept::Logger::instance();
        auto& cfg = fincept::AppConfig::instance();

        // Global level
        const QString gl = cfg.get("log/global_level", "Info").toString();
        const QHash<QString, fincept::LogLevel> lvl_map = {
            {"Trace", fincept::LogLevel::Trace}, {"Debug", fincept::LogLevel::Debug},
            {"Info", fincept::LogLevel::Info},   {"Warn", fincept::LogLevel::Warn},
            {"Error", fincept::LogLevel::Error}, {"Fatal", fincept::LogLevel::Fatal}};
        log.set_level(lvl_map.value(gl, fincept::LogLevel::Info));

        // JSON output mode (persisted in Settings → Logging)
        log.set_json_mode(cfg.get("log/json_mode", false).toBool());

        // Per-tag overrides
        const int count = cfg.get("log/tag_count", 0).toInt();
        for (int i = 0; i < count; ++i) {
            const QString tag = cfg.get(QString("log/tag_%1_name").arg(i)).toString();
            const QString level = cfg.get(QString("log/tag_%1_level").arg(i)).toString();
            if (!tag.isEmpty() && lvl_map.contains(level))
                log.set_tag_level(tag, lvl_map.value(level));
        }
    }
    {
        // One start line with everything support needs (policy B.4). Built outside the
        // LOG_ macro: preprocessor branches may not sit inside macro arguments.
        static const char* const kLevelNames[] = {"trace", "debug", "info", "warn", "error", "fatal"};
        const int level = qBound(0, static_cast<int>(fincept::Logger::instance().global_level()), 5);
#ifdef NDEBUG
        const QString build_kind = QStringLiteral("release");
#else
        const QString build_kind = QStringLiteral("debug");
#endif
        const QString start_fields =
            QStringList{QStringLiteral("version=") + QStringLiteral(FINCEPT_VERSION_STRING),
                        QStringLiteral("build=") + build_kind,
                        QStringLiteral("edition=") + QLatin1String(fincept::edition::kId),
                        QStringLiteral("os=\"") + QSysInfo::prettyProductName() + QLatin1Char('"'),
                        QStringLiteral("arch=") + QSysInfo::currentCpuArchitecture(),
                        QStringLiteral("qt=") + QString::fromLatin1(qVersion()),
                        QStringLiteral("pid=") + QString::number(QCoreApplication::applicationPid()),
                        QStringLiteral("tls=") + QSslSocket::activeBackend(),
                        QStringLiteral("data_dir=") + fincept::privacy::redact_home(fincept::AppPaths::root()),
                        QStringLiteral("profile=") + fincept::ProfileManager::instance().active(),
                        QStringLiteral("log_level=") + QLatin1String(kLevelNames[level])}
                .join(QLatin1Char(' '));
        LOG_INFO("App", "Fincept Terminal starting " + start_fields);
    }
    // The active backend is a field of the start line; the list of the others is detail.
    LOG_DEBUG("App", QString("TLS backend: %1 (available: %2)")
                         .arg(QSslSocket::activeBackend(), QSslSocket::availableBackends().join(", ")));
#ifdef Q_OS_LINUX
    // CI's installed-package checks (release.yml, job verify-deb) read this line.
    LOG_INFO("App", QString("Qt platform: %1 (%2)")
                        .arg(QGuiApplication::platformName(), QString::fromLocal8Bit(qpa_choice)));
#endif

    // Theme is applied after DB is open so saved font/theme are respected from the start.

    // Initialize config
    auto& config = fincept::AppConfig::instance();
    fincept::HttpClient::instance().set_base_url(config.api_base_url());
    // URL paths on the API host are ours and may be logged (never the query).
    fincept::log_url_add_first_party_host(config.api_base_url());
    // Note: auth tokens are managed by AuthManager::initialize() which loads
    // from SecureStorage (DPAPI) and SQLite — not from QSettings/Registry.

    // Register migrations explicitly (avoids MSVC /OPT:REF stripping static-init TUs)
    fincept::register_migration_v001();
    fincept::register_migration_v002();
    fincept::register_migration_v003();
    fincept::register_migration_v004();
    fincept::register_migration_v005();
    fincept::register_migration_v006();
    fincept::register_migration_v007();
    fincept::register_migration_v008();
    fincept::register_migration_v009();
    fincept::register_migration_v010();
    fincept::register_migration_v011();
    fincept::register_migration_v012();
    fincept::register_migration_v013();
    fincept::register_migration_v014();
    fincept::register_migration_v015();
    fincept::register_migration_v016();
    fincept::register_migration_v017();
    fincept::register_migration_v018();
    fincept::register_migration_v019();
    fincept::register_migration_v020();
    fincept::register_migration_v021();
    fincept::register_migration_v022();
    fincept::register_migration_v023();
    fincept::register_migration_v024();
    fincept::register_migration_v025();
    fincept::register_migration_v026();
    fincept::register_migration_v027();
    fincept::register_migration_v028();
    fincept::register_migration_v029();
    fincept::register_migration_v030();
    fincept::register_migration_v031();
    fincept::register_migration_v032();
    fincept::register_migration_v033();
    fincept::register_migration_v034();
    fincept::register_migration_v035();
    fincept::register_migration_v036();
    fincept::register_migration_v037();
    fincept::register_migration_v038();
    fincept::register_migration_v039();
    fincept::register_migration_v040();
    fincept::register_migration_v041();
    fincept::register_migration_v042();
    fincept::register_migration_v043();
    fincept::register_migration_v044();
    fincept::register_migration_v045();
    fincept::register_migration_v046();
    fincept::register_migration_v047();
    fincept::register_migration_v048();
    fincept::register_migration_v049();
    fincept::register_migration_v050();
    fincept::register_migration_v051();
    fincept::register_migration_v052();
    fincept::register_migration_v053();
    fincept::register_migration_v054();
    fincept::register_migration_v055();
    fincept::register_migration_v056();
    fincept::register_migration_v057();
    fincept::register_migration_v058();
    fincept::register_migration_v059();
    fincept::register_migration_v060();
    fincept::register_migration_v061();
    fincept::register_migration_v062(); // Data Sources health + Data Mapping v2
    fincept::register_migration_v063(); // Chat Mode run artefacts, templates, schedules
    fincept::register_migration_v064(); // M&A saved cases, Alt Investments fund book / holdings / DD
    fincept::register_migration_v065(); // Derivatives position book + saved strategies
    fincept::register_migration_v066(); // Notes board (v065 is the Derivatives position book)

    // ── Open the main database ────────────────────────────────────────────────
    // The terminal never runs without its database. Every failure ends in a
    // blocking, plain-language dialog (Retry / recover / Quit) — or, in headless
    // --smoke-test / --selftest-* / --dump-tools runs where nobody can answer a
    // dialog, in one clear stderr + log line and a non-zero exit. Raw driver text
    // goes to the log only.
#ifndef FINCEPT_DB_LINEAGE
#    define FINCEPT_DB_LINEAGE "community"
#endif
    const bool db_headless = [argc, argv]() {
        for (int i = 1; i < argc; ++i) {
            if (qstrcmp(argv[i], "--smoke-test") == 0 || qstrncmp(argv[i], "--selftest", 10) == 0 ||
                qstrcmp(argv[i], "--dump-tools") == 0)
                return true;
        }
        return false;
    }();
    int db_exit_code = 0;
    const bool db_ready = [&db_exit_code, db_headless]() -> bool {
        using K = fincept::db::SqlErrorKind;
        constexpr int kExitMigrationFailed = 1; // unchanged from before
        constexpr int kExitSchemaRefused = 3;   // a database from a newer / different build
        constexpr int kExitOpenFailed = 4;      // cannot open, damaged, locked, SQLite too old

        auto& database = fincept::Database::instance();
        const QString data_dir = fincept::AppPaths::data();
        const QString shared_path = data_dir + QStringLiteral("/fincept.db");
        // This build's own file. Used once the shared fincept.db was refused as
        // another build's — and from then on in preference to it, without asking.
        const QString lineage_path = data_dir + QLatin1String("/fincept." FINCEPT_DB_LINEAGE ".db");
        const QString log_file = QDir::toNativeSeparators(fincept::AppPaths::logs() + QStringLiteral("/fincept.log"));

        const auto fail_headless = [&db_exit_code](int code, const QString& what) {
            LOG_ERROR("App", what);
            std::fprintf(stderr, "Fincept Terminal: %s\n", qPrintable(what));
            std::fflush(stderr);
            db_exit_code = code;
            return false;
        };
        // Blocking question; returns the index of the chosen button. The last
        // button is also the Escape / close-box answer, so it must be the safe
        // one (Quit).
        const auto ask = [](QMessageBox::Icon icon, const QString& text, const QString& info,
                            const QStringList& buttons) -> int {
            QMessageBox box(icon, QObject::tr("Fincept Terminal"), text);
            box.setInformativeText(info);
            QList<QAbstractButton*> added;
            for (int i = 0; i < buttons.size(); ++i) {
                added << box.addButton(buttons.at(i),
                                       i + 1 == buttons.size() ? QMessageBox::RejectRole : QMessageBox::AcceptRole);
            }
            box.setDefaultButton(qobject_cast<QPushButton*>(added.first()));
            box.setEscapeButton(added.last());
            box.exec();
            const int chosen = int(added.indexOf(box.clickedButton()));
            return chosen < 0 ? int(buttons.size()) - 1 : chosen;
        };

        QString db_path = QFile::exists(lineage_path) ? lineage_path : shared_path;
        if (db_path == lineage_path)
            LOG_INFO("App", "Using this build's own database path=" + fincept::privacy::redact_home(lineage_path));
        QStringList tried_backups; // a backup that turned out damaged is not offered again

        for (;;) {
            auto opened = database.open(db_path);
            if (opened.is_ok())
                return true;
            const std::string& err = opened.error();
            LOG_ERROR("App", QStringLiteral("Database could not be opened path=%1 error=\"%2\"")
                                 .arg(fincept::privacy::redact_home(db_path),
                                      fincept::privacy::redact_text(QString::fromStdString(err)).left(300)));
            const QString shown_path = QDir::toNativeSeparators(db_path);

            // ── Written by a newer or different build: refused, not modified ──
            const bool newer = fincept::MigrationRunner::is_schema_newer_error(err);
            if (newer || fincept::MigrationRunner::is_schema_incompatible_error(err)) {
                const QString text = newer ? QObject::tr("This profile's database was created by a newer or "
                                                         "different version of Fincept Terminal.")
                                           : QObject::tr("This profile's database was created by a different "
                                                         "version of Fincept Terminal and is missing data tables "
                                                         "this version needs.");
                QString info = QObject::tr("Database: %1\nIts schema version: v%2. This version supports up to v%3."
                                           "\n\nThe file has not been modified.")
                                   .arg(shown_path, // one call: a "%2" in the path is not re-scanned
                                        QString::number(database.found_schema_version()),
                                        QString::number(fincept::MigrationRunner::highest_registered_version()));
                if (db_headless)
                    return fail_headless(kExitSchemaRefused, text + QLatin1Char(' ') + info.simplified());
                if (db_path != lineage_path) {
                    info += QObject::tr("\n\nYou can keep using it with the version that created it. This version "
                                        "can use a separate database of its own instead, starting empty; the data "
                                        "in the file above stays where it is and does not appear here.");
                    if (ask(QMessageBox::Warning, text, info,
                            {QObject::tr("Use a separate database for this version"), QObject::tr("Quit")}) == 0) {
                        LOG_WARN("App", "User chose a separate database for this build path=" +
                                            fincept::privacy::redact_home(lineage_path));
                        db_path = lineage_path;
                        continue;
                    }
                } else {
                    info += QObject::tr("\n\nThis is already this version's own database, so there is no other "
                                        "database to switch to. Open it with the version of Fincept Terminal that "
                                        "created it.");
                    ask(QMessageBox::Critical, text, info, {QObject::tr("Quit")});
                }
                db_exit_code = kExitSchemaRefused;
                return false;
            }

            // ── SQLite library too old ──
            if (err.rfind(fincept::Database::kErrSqliteTooOld, 0) == 0) {
                const QString found = database.sqlite_version();
                const QString text = QObject::tr("Fincept Terminal cannot start: its database library is too old.");
                const QString info = QObject::tr("This copy uses SQLite %1, but version %2.%3 or newer is required. "
                                                 "Install an official Fincept Terminal package, or update the "
                                                 "system's Qt and SQLite packages.\n\nNothing was changed.")
                                         .arg(found.isEmpty() ? QObject::tr("(unknown)") : found)
                                         .arg(fincept::Database::kMinSqliteMajor)
                                         .arg(fincept::Database::kMinSqliteMinor);
                if (db_headless)
                    return fail_headless(kExitOpenFailed, text + QLatin1Char(' ') + info.simplified());
                ask(QMessageBox::Critical, text, info, {QObject::tr("Quit")});
                db_exit_code = kExitOpenFailed;
                return false;
            }

            // ── A migration failed. Booting on would wire every repository to a
            // schema that is neither the old nor the new one, so stop. ──
            if (fincept::MigrationRunner::is_migration_failure(err)) {
                // Disk full (most often: no room for the safety copy taken before
                // updating) is fixable by the user, and migrations resume where
                // they stopped, so offer Retry.
                if (fincept::db::classify_text(QString::fromStdString(err)) == K::Full) {
                    const QString text =
                        QObject::tr("There is not enough free disk space to update the terminal database.");
                    const QString info = QObject::tr("Updating needs free space for the new data and for a safety "
                                                     "copy of the database, which is saved first. Your data has not "
                                                     "been deleted.\n\nFree some space on the drive that holds:\n%1\n"
                                                     "then choose Retry.")
                                             .arg(shown_path);
                    if (db_headless)
                        return fail_headless(kExitMigrationFailed, text + QLatin1Char(' ') + info.simplified());
                    if (ask(QMessageBox::Critical, text, info, {QObject::tr("Retry"), QObject::tr("Quit")}) == 0)
                        continue;
                    db_exit_code = kExitMigrationFailed;
                    return false;
                }
                const QString text = QObject::tr("Fincept Terminal could not update its database to this version.");
                QString info = QObject::tr("Database: %1\n\n").arg(shown_path);
                const QString backup = database.migration_backup_path();
                if (!backup.isEmpty()) {
                    info += QObject::tr("A copy of the database made just before the update is here:\n%1\n\n")
                                .arg(QDir::toNativeSeparators(backup));
                }
                info += QObject::tr("Your data has not been deleted. The terminal will not start on a partly "
                                    "updated database. Technical details are in the log file:\n%1")
                            .arg(log_file);
                if (db_headless)
                    return fail_headless(kExitMigrationFailed, text + QLatin1Char(' ') + info.simplified());
                ask(QMessageBox::Critical, text, info, {QObject::tr("Quit")});
                db_exit_code = kExitMigrationFailed;
                return false;
            }

            const K kind = database.open_error_kind();

            // ── Damaged, or not a database at all ──
            if (kind == K::Corrupt || kind == K::NotADb) {
                const QString text = kind == K::NotADb
                                         ? QObject::tr("The terminal database file is not a valid database.")
                                         : QObject::tr("The terminal database is damaged and cannot be used safely.");
                // Headless runs never move user files: recovery is a decision for
                // the user, made in the dialog below on a normal start.
                if (db_headless) {
                    return fail_headless(kExitOpenFailed,
                                         text + QObject::tr(" Database: %1. Left unchanged (headless run); start the "
                                                            "terminal normally to recover it.")
                                                    .arg(shown_path));
                }
                QString backup;
                for (const QString& b : fincept::MigrationRunner::list_backups(db_path)) {
                    if (!tried_backups.contains(b)) {
                        backup = b;
                        break;
                    }
                }
                QString info = QObject::tr("Database: %1\n\nIt will be moved aside, unchanged, to %2.corrupt-<date-time> "
                                           "in the same folder. Your saved broker accounts, API keys and other "
                                           "credentials, settings and portfolios are stored in that file: a new "
                                           "database starts empty, and they would have to be added again.")
                                   .arg(shown_path, QFileInfo(db_path).fileName());
                QStringList buttons;
                if (!backup.isEmpty()) {
                    info += QObject::tr("\n\nThe latest backup was saved on %1:\n%2\nRestoring it brings your data "
                                        "back as it was at that time.")
                                .arg(QFileInfo(backup).lastModified().toString(QStringLiteral("d MMM yyyy, hh:mm")),
                                     QDir::toNativeSeparators(backup));
                    buttons << QObject::tr("Restore latest backup");
                }
                buttons << QObject::tr("Start with a new database") << QObject::tr("Quit");
                const int choice = ask(QMessageBox::Critical, text, info, buttons);
                const bool restore = !backup.isEmpty() && choice == 0;
                const bool start_new = (choice == int(buttons.size()) - 2);
                if (!restore && !start_new) {
                    db_exit_code = kExitOpenFailed;
                    return false;
                }

                auto moved = fincept::Database::quarantine_files(db_path);
                if (moved.is_err()) {
                    LOG_ERROR("App", "Could not quarantine the damaged database error=\"" +
                                         fincept::privacy::redact_text(QString::fromStdString(moved.error())) + "\"");
                    if (ask(QMessageBox::Critical, QObject::tr("The damaged database could not be moved aside."),
                            QObject::tr("Database: %1\n\nClose any other program that may be using it, then choose "
                                        "Retry.")
                                .arg(shown_path),
                            {QObject::tr("Retry"), QObject::tr("Quit")}) == 0)
                        continue;
                    db_exit_code = kExitOpenFailed;
                    return false;
                }
                if (restore) {
                    tried_backups << backup;
                    auto restored = fincept::Database::restore_backup(backup, db_path);
                    if (restored.is_err()) {
                        LOG_ERROR("App", "Backup restore failed error=\"" +
                                             fincept::privacy::redact_text(QString::fromStdString(restored.error())) + "\"");
                        if (ask(QMessageBox::Critical, QObject::tr("The backup could not be restored."),
                                QObject::tr("Backup: %1\n\nThe damaged database was moved aside to:\n%2")
                                    .arg(QDir::toNativeSeparators(backup), QDir::toNativeSeparators(moved.value())),
                                {QObject::tr("Start with a new database"), QObject::tr("Quit")}) != 0) {
                            db_exit_code = kExitOpenFailed;
                            return false;
                        }
                    }
                }
                continue; // open the restored copy, or create a new database
            }

            // ── Cannot open: locked, unwritable, disk full, other ──
            QString text;
            QString info;
            switch (kind) {
                case K::Busy:
                case K::Locked:
                    text = QObject::tr("Another Fincept Terminal is using this profile.");
                    info = QObject::tr("Close the other Fincept Terminal window, or any program that has this "
                                       "database open, then choose Retry.\n\nDatabase: %1")
                               .arg(shown_path);
                    break;
                case K::CantOpen:
                case K::ReadOnly:
                    text = QObject::tr("Fincept Terminal cannot open its database.");
                    info = fincept::db::user_message(kind, QObject::tr("The terminal database")) +
                           QObject::tr("\n\nDatabase: %1\n\nFix the folder or its permissions, then choose Retry.")
                               .arg(shown_path);
                    break;
                case K::Full:
                    text = QObject::tr("There is not enough free disk space to open the terminal database.");
                    info = QObject::tr("Free some space on the drive that holds:\n%1\nthen choose Retry.").arg(shown_path);
                    break;
                default:
                    text = QObject::tr("Fincept Terminal cannot open its database.");
                    info = fincept::db::user_message(kind, QObject::tr("The terminal database")) +
                           QObject::tr("\n\nDatabase: %1\n\nTechnical details are in the log file:\n%2")
                               .arg(shown_path, log_file);
                    break;
            }
            if (db_headless)
                return fail_headless(kExitOpenFailed, text + QLatin1Char(' ') + info.simplified());
            if (ask(QMessageBox::Critical, text, info, {QObject::tr("Retry"), QObject::tr("Quit")}) == 0)
                continue;
            db_exit_code = kExitOpenFailed;
            return false;
        }
    }();
    if (!db_ready) {
        // Returning from main() never reaches exec(), so aboutToQuit never
        // fires and the shell's clean-shutdown marker would never be
        // written — the next launch would greet the user with a spurious
        // crash-recovery dialog on top of the database error. shutdown() is
        // idempotent (it clears initialised_), so calling it here is safe.
        fincept::shutdown::note(fincept::shutdown::Reason::StartupFailed, QStringLiteral("stage=database"));
        LOG_INFO("App", QStringLiteral("Shutting down %1 exit=%2 uptime_s=%3")
                            .arg(fincept::shutdown::describe())
                            .arg(db_exit_code)
                            .arg(fincept::shutdown::uptime_s()));
        fincept::Logger::instance().flush();
        fincept::TerminalShell::instance().shutdown();
        // Its stop() is otherwise reached only through aboutToQuit; returning
        // with the watcher std::thread still joinable is std::terminate (abort).
        fincept::debug::UiStallMonitor::stop();
        return db_exit_code; // do NOT continue into the UI
    } else {
        // Load broker accounts now that the DB is open. The AccountManager
        // singleton loads eagerly in its constructor on first access; if anything
        // touched it before this point (before open()), it found an unusable DB
        // and loaded nothing. This explicit main-thread reload guarantees the
        // account map is populated from the now-open DB, so configured brokers
        // survive restarts instead of vanishing.
        fincept::trading::AccountManager::instance().reload_from_db();

        // Prune news articles older than 30 days, OFF the GUI thread: it is a
        // bulk DELETE whose FTS trigger fires per row. NewsArticleRepository goes
        // through Database::execute(), which gives the pool thread its own
        // connection. Posted after the first event-loop turn so it does not
        // compete with startup reads; skipped in headless runs, which exit long
        // before it matters (and a sweep in flight at teardown is a crash risk).
        if (!db_headless) {
            const int64_t news_cutoff = QDateTime::currentSecsSinceEpoch() - (30LL * 86400);
            QTimer::singleShot(0, qApp, [news_cutoff]() {
                QThreadPool::globalInstance()->start([news_cutoff]() {
                    auto pruned = fincept::NewsArticleRepository::instance().prune_older_than(news_cutoff);
                    if (pruned.is_ok())
                        LOG_DEBUG("App", "News articles pruned (keeping 30 days)");
                    else
                        LOG_WARN("App", "News prune failed: " + QString::fromStdString(pruned.error()));
                });
            });
        }

        // Retention sweeper for the append-only tables that had NO reader and NO
        // retention policy, so they grew for the life of the install:
        // workflow_audit_log (>90d), telemetry_events (>30d), sync_outbox rows
        // dead-lettered after 20 failed attempts, and expired unified_cache —
        // which was previously swept once at startup only, so a terminal left
        // open for days never reclaimed anything. Runs once now and every ~15
        // minutes thereafter; idempotent, so a second call creates no second
        // timer. Started here (inside the DB-open branch) because every policy
        // is a DELETE against the main DB.
        //
        // NOT in --smoke-test / --selftest-* runs: those are short headless
        // processes that construct screens and exit. Background maintenance has
        // no value there, and a sweep still in flight when the process tears the
        // databases down is a shutdown crash with no dump (the crash handler is
        // already gone by static-destruction time). Detected by scanning argv
        // because smoke_mode is not computed until much later in main().
        const bool headless_run = [argc, argv]() {
            for (int i = 1; i < argc; ++i) {
                if (qstrcmp(argv[i], "--smoke-test") == 0 || qstrncmp(argv[i], "--selftest", 10) == 0)
                    return true;
            }
            return false;
        }();
        if (!headless_run)
            fincept::StorageManager::instance().start_retention_sweeper();

        // Load the persisted appearance (theme, font, density) and apply it
        // before any window is shown — eliminates a flash of the wrong theme
        // or font on startup.
        {
            auto& repo = fincept::SettingsRepository::instance();
            auto& tm = fincept::ui::ThemeManager::instance();
            auto r_theme = repo.get("appearance.theme");
            auto r_family = repo.get("appearance.font_family");
            auto r_size = repo.get("appearance.font_size");
            auto r_density = repo.get("appearance.density");
            // FINCEPT_THEME overrides the saved theme for this run only (used
            // by the screen walk to exercise both themes).
            QString theme = qEnvironmentVariable("FINCEPT_THEME");
            if (theme.isEmpty())
                theme = r_theme.is_ok() && !r_theme.value().isEmpty() ? r_theme.value() : "Dark";
            QString family = r_family.is_ok() && !r_family.value().isEmpty() ? r_family.value()
                                                                              : fincept::ui::ThemeManager::kDefaultFontFamily;
            // "Consolas" was the built-in default before the terminal shipped its
            // own font, and the Appearance page stored it whenever any setting was
            // saved — so a stored "Consolas" is the old default, not a choice.
            // Move it to the new default once; a user who picks Consolas again
            // afterwards keeps it.
            if (family == QLatin1String("Consolas")) {
                auto r_migrated = repo.get("appearance.font_default_v2");
                if (r_migrated.is_ok() && r_migrated.value() != QLatin1String("1")) {
                    family = fincept::ui::ThemeManager::kDefaultFontFamily;
                    repo.set("appearance.font_family", family, "appearance");
                }
            }
            repo.set("appearance.font_default_v2", "1", "appearance");
            QString size_s = r_size.is_ok() ? r_size.value() : "14px";
            int size_px = size_s.left(size_s.indexOf("px")).toInt();
            if (size_px <= 0)
                size_px = fincept::ui::ThemeManager::kDefaultFontPx;
            QString density = r_density.is_ok() && !r_density.value().isEmpty() ? r_density.value() : "Default";
            // One coalesced apply — a single qApp->setStyleSheet() (see
            // ThemeManager re: the Plasma 6 / Wayland double-restyle crash,
            // issue #247).
            tm.apply_appearance(theme, family, size_px, density);
            LOG_DEBUG("App", "Theme: " + tm.theme_choice() + " (" + tm.current_theme_name() + "), font: " + family +
                                 " " + QString::number(tm.font_size_px()) + "px, density: " + density);
        }

        // Load persisted language and install the matching QTranslator before
        // any windows are shown — eliminates an English-flash on first paint
        // when the user has previously chosen another language.
        fincept::i18n::LanguageManager::instance().initialize();

        // Load persisted display-currency preference so the symbol is correct
        // on first paint of any calculator/analytics surface.
        fincept::currency::CurrencyManager::instance().initialize();
        fincept::ui::formatting::load_format_settings();
    }

    // Open cache database (non-fatal if fails). When this build runs on its own
    // fincept.<lineage>.db, its cache is separate too (cache.<lineage>.db), so
    // the two builds do not keep recreating each other's cache.
    const bool own_lineage_db =
        QFileInfo(fincept::Database::instance().path()).fileName() == QLatin1String("fincept." FINCEPT_DB_LINEAGE ".db");
    QString cache_path = fincept::AppPaths::data() +
                         (own_lineage_db ? QString::fromLatin1("/cache." FINCEPT_DB_LINEAGE ".db")
                                         : QStringLiteral("/cache.db"));
    auto cache_result = fincept::CacheDatabase::instance().open(cache_path);
    if (cache_result.is_err()) {
        LOG_WARN("App", "Cache database unavailable, continuing without it error=\"" +
                            fincept::privacy::redact_text(QString::fromStdString(cache_result.error())) + "\"");
    }

    // Background services start only now that both databases are open. Queued
    // earlier, this chain ran inside the event loop of a database recovery
    // dialog — before any database existed — and the services kept that empty
    // state (cloud sync off, failed housekeeping) for the whole session.
    post_chain({init_hub_producers, init_cloud_sync, init_wallet_treasury_and_monitors});

    // Assign a unique session ID so ScreenStateManager can tag each state write.
    // This lets us distinguish cross-session restores from same-session saves.
    {
        const QString sid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        fincept::ScreenStateManager::instance().set_session_id(sid);
        LOG_DEBUG("App", "Screen-state session id=" + sid);
    }

    // One-time migration: copy settings from old DB (Local\FinceptTerminal\fincept_settings.db)
    // to new DB (Roaming\Fincept\FinceptTerminal\fincept.db) if the new DB has no settings yet.
    {
        auto existing = fincept::SettingsRepository::instance().get("fincept_session");
        bool new_db_empty = existing.is_err() || existing.value().isEmpty();
        if (new_db_empty) {
            QString local_base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
            // AppLocalDataLocation = .../Local/Fincept/FinceptTerminal — strip to .../Local/FinceptTerminal
            QString old_db_path = local_base.section('/', 0, -3) + "/FinceptTerminal/fincept_settings.db";
            if (!QFile::exists(old_db_path)) {
                // Try without the org subfolder
                old_db_path = local_base.replace("Fincept/FinceptTerminal", "FinceptTerminal") + "/fincept_settings.db";
            }
            if (QFile::exists(old_db_path)) {
                QSqlDatabase old_db = QSqlDatabase::addDatabase("QSQLITE", "legacy_migration");
                old_db.setDatabaseName(old_db_path);
                // Read-only: the file is in a folder shared with the private build;
                // its values are copied, the file is never written or locked for writing.
                old_db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
                if (old_db.open()) {
                    QSqlQuery src(old_db);
                    if (src.exec("SELECT key, value FROM settings")) {
                        int count = 0;
                        while (src.next()) {
                            fincept::SettingsRepository::instance().set(src.value(0).toString(),
                                                                        src.value(1).toString(), "migrated");
                            ++count;
                        }
                        LOG_INFO("App", QString("Migrated %1 settings from legacy DB").arg(count));
                    }
                    old_db.close();
                }
                QSqlDatabase::removeDatabase("legacy_migration");
            }
        }
    }

    // Start session
    fincept::SessionManager::instance().start_session();

    // Phase 1 final lift: shell owns auth/lock service initialisation.
    // bootstrap_auth() runs AuthManager::initialize(), warms PinManager
    // from SecureStorage, and configures InactivityGuard's lock timeout
    // from SettingsRepository. The previous in-line block here is folded
    // into TerminalShell::bootstrap_auth.
    fincept::TerminalShell::instance().bootstrap_auth();

    // Session guard — auto-logout on 401. Lives on the stack here so its
    // destructor runs on shutdown via QApplication::exec returning.
    fincept::auth::SessionGuard session_guard;

    // Force the ReportBuilderService singleton onto the main thread before
    // MCP tools register — tools route into it via QMetaObject::invokeMethod
    // with BlockingQueuedConnection from worker threads, so the service must
    // already exist with main-thread affinity.
    (void)fincept::services::ReportBuilderService::instance();

    // Initialize MCP tool system — registers all internal tools and starts
    // external MCP servers in the background (non-blocking).
    //
    // The core tool catalogue (ten tools). Nothing on the first frame consumes
    // it, so it is deferred to the first event-loop turn — EXCEPT for --selftest-tools / --dump-tools, which
    // read the registry synchronously and early-return below without ever
    // reaching QApplication::exec(). Those two must have it up front.
    {
        bool tools_needed_synchronously = false;
        for (int i = 1; i < argc; ++i) {
            if (qstrcmp(argv[i], "--selftest-tools") == 0 || qstrcmp(argv[i], "--selftest-llm-tools") == 0 ||
                qstrcmp(argv[i], "--dump-tools") == 0)
                tools_needed_synchronously = true;
        }
        if (tools_needed_synchronously)
            fincept::mcp::initialize_all_tools();
        else
            QTimer::singleShot(0, qApp, []() { fincept::mcp::initialize_all_tools(); });
    }

    // ── Headless tool-system self-test / catalog dump ────────────────────────
    // Runs after the real tool registration above but before any window or
    // network init, so it exercises exactly what ships. Exits without starting
    // the GUI — used by the dev loop and CI to measure tool retrieval recall
    // and registry integrity (no LLM / API key required).
    // Single source of truth for the headless self-test suites. The dispatch below
    // AND --selftest-list both read this table, so CI enumerates the suites at
    // runtime instead of hard-coding them.
    //
    // A hand-maintained list is exactly how --selftest-live-table came to be
    // dispatched here while running in no workflow at all: main.cpp had 10 flags,
    // the CI loop listed 9, and nothing connected the two. Add a suite here and it
    // is picked up by every job automatically.
    //
    // --dump-tools is deliberately NOT in this table: it is a diagnostic dump, not
    // a pass/fail suite, and CI must not run it as one.
    struct SelftestSuite {
        const char* flag;
        int (*run)();
    };
    static constexpr SelftestSuite kSelftestSuites[] = {
        {"--selftest-tools", &fincept::mcp::run_tool_selftest},
        {"--selftest-llm-tools", &fincept::mcp::run_provider_tool_format_selftest},
        {"--selftest-mcp-server", &fincept::mcp::run_mcp_server_selftest},
        {"--selftest-ui-tools", &fincept::mcp::core::run_core_tools_selftest},
        {"--selftest-feeds", &fincept::feeds::run_feed_selftest},
        {"--selftest-dock-layout", &fincept::layout::run_dock_layout_selftest},
        {"--selftest-live-table", &fincept::ui::run_live_table_selftest},
        {"--selftest-chart-kit", &fincept::ui::run_chart_kit_selftest},
        {"--selftest-fno-algo", &fincept::algo::fno::run_fno_algo_selftest},
        {"--selftest-universe-scan", &fincept::algo::run_universe_scan_selftest},
        {"--selftest-paper", &fincept::trading::run_paper_trading_selftest},
        {"--selftest-portfolio-monitor", &fincept::trading::run_portfolio_monitor_selftest},
        {"--selftest-portfolio-replication", &fincept::trading::replication::run_portfolio_replication_selftest},
        {"--selftest-arena", &fincept::arena::run_arena_selftest},
    };

    for (int i = 1; i < argc; ++i) {
        // Machine-readable suite enumeration for CI: one flag per line, exit 0.
        if (qstrcmp(argv[i], "--selftest-list") == 0) {
            for (const auto& suite : kSelftestSuites)
                std::printf("%s\n", suite.flag);
            std::fflush(stdout);
            return 0;
        }
        if (qstrcmp(argv[i], "--dump-tools") == 0)
            return fincept::mcp::dump_tools_json();
        for (const auto& suite : kSelftestSuites) {
            if (qstrcmp(argv[i], suite.flag) == 0)
                return suite.run();
        }
    }

    // ── PIN gate at launch ───────────────────────────────────────────────────
    // A configured PIN is asked for before the terminal shows anything, on every
    // launch. Raise the process-wide lock now, before any window exists (and
    // after the headless self-tests above, which drive the UI tools and must not
    // meet a lock on a developer machine with a PIN):
    //  - WindowFrame reads "not locked + signed in + PIN set" as "the user has
    //    already unlocked this session" (an extra window opened later). With the
    //    lock still down, a window built after the saved-session check had
    //    finished opened straight into the terminal with no PIN prompt — which is
    //    what happens behind the crash-recovery dialog (SKIP and RESTORE) and the
    //    Python setup screen, since both run an event loop before the first
    //    window exists.
    //  - A window built while that check is still running shows the terminal
    //    until it ends; LockOverlayController re-applies the lock when the
    //    window is shown, before its first paint.
    //  - While locked, navigation, the MCP UI tools and the background account
    //    polls (InactivityGuard::user_active) stand down.
    // Only with a saved session: without one the sign-in screen comes first and
    // asks for the PIN right after sign-in. A rejected session (401) signs out,
    // which drops the lock again (WindowFrame::on_auth_state_changed).
    {
        auto& auth = fincept::auth::AuthManager::instance();
        if (fincept::auth::PinManager::instance().has_pin() && (auth.is_authenticated() || auth.is_loading())) {
            // DEBUG: InactivityGuard writes the one "Terminal locked" line.
            LOG_DEBUG("App", "PIN set and a saved session — the terminal starts locked");
            fincept::auth::InactivityGuard::instance().set_terminal_locked(true);
        }
    }

    // Start the scan-watch background service. Runs after Database::open() (which
    // applies the scan_watches migration) and after bootstrap_auth() (broker
    // creds, needed by the first candle poll), and after the headless self-test
    // early-returns above so it is skipped on --selftest-tools / --dump-tools.
    // Placed before the Python-setup branch so both GUI paths (setup screen and
    // normal startup) start it exactly once. Candle fetching is native C++ (broker
    // REST / native Yahoo), so it does not require the Python env to be ready.
    fincept::algo::ScanMonitor::instance().start();

    // Centralized paper mark-to-market + order matching. Runs independent of which
    // screen is open so paper positions (equity AND F&O) keep their P&L live and
    // resting limit/stop/SL-TP orders fill continuously — not only while the
    // Equity tab is focused. Placed alongside ScanMonitor (after the self-test
    // early-returns, so it stays off in headless --selftest runs).
    //
    // Deferred: start() calls resync() inline, which runs pt_get_positions()
    // (SQLite) once per active paper account — pre-window main-thread work with
    // nothing on the first frame depending on it. The singleShot still sits
    // after the self-test early-returns, so the headless guard is preserved.
    QTimer::singleShot(0, qApp, []() { fincept::trading::PaperMarkService::instance().start(); });
    // Watchlist price/volume alerts keep evaluating while the screen is closed.
    QTimer::singleShot(0, qApp, []() { fincept::services::WatchlistAlertService::instance().start(); });

    // Native desktop notifications (Win toast / macOS Notification Center / Linux
    // libnotify) via a tray icon — also surfaces every in-app ToastService toast.
    fincept::ui::DesktopNotifier::instance().init();

    // ── Python environment check ─────────────────────────────────────────────
    // check_status() is filesystem-only (no process is spawned), so it is safe
    // on the GUI thread. Anything it cannot prove resolves to needs_setup /
    // needs_package_sync and the async pipeline verifies it for real.
    auto setup_status = fincept::python::PythonSetupManager::instance().check_status();

    // Agents (agno & co.) arrive with the optional full library set. Whenever a
    // run leaves that set complete — the background sync below, an early launch
    // from the setup screen, or an install started later from Settings → Python
    // Env / the status-bar chip — re-warm agent discovery so agent lists fill
    // without a restart. An essentials-only run has no agents to find.
    QObject::connect(&fincept::python::PythonSetupManager::instance(),
                     &fincept::python::PythonSetupManager::setup_complete, &app, [](bool success, const QString&) {
                         if (success && fincept::python::PythonSetupManager::instance().is_full_installed())
                             fincept::services::AgentService::instance().discover_agents();
                     });

    // --smoke-test: CI/clean-machine screen-construction walk. Force the normal
    // boot path (we need a real WindowFrame + router to navigate every screen),
    // and skip the Python first-run SetupScreen — the smoke test only verifies
    // that screens CONSTRUCT on the bundled runtime, not that data fetches work.
    // The actual walk is scheduled just after the primary window is shown.
    bool smoke_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--smoke-test") == 0)
            smoke_mode = true;
    }
    if (smoke_mode) {
        LOG_INFO("Smoke", "Smoke-test mode — forcing normal boot, skipping setup/recovery");
        setup_status.needs_setup = false;
    }

    if (setup_status.needs_setup) {
        LOG_INFO("App", "Python environment not ready — showing setup screen");

        // Use QPointer so the hand-off lambda is safe against double-fire.
        // Closing this window before the hand-off quits the app (see
        // SetupScreen::closeEvent) — quit-on-last-window is off app-wide.
        auto* setup_screen = new fincept::screens::SetupScreen;
        QPointer<fincept::screens::SetupScreen> screen_guard(setup_screen);
        setup_screen->resize(1040, 660);
        setup_screen->show();

        // Hand off to the main window. launch_requested fires automatically as
        // soon as the essentials are installed (first run installs only those —
        // whatever is left finishes in the background, owned by
        // PythonSetupManager), on LAUNCH TERMINAL, on "Skip for now" /
        // "Continue without Python", or on "finish in the background".
        // Qt::SingleShotConnection (Qt 6.0+) makes it fire exactly once.
        QObject::connect(
            setup_screen, &fincept::screens::SetupScreen::launch_requested, setup_screen,
            [&app, &instance_lock, screen_guard]() {
                if (!screen_guard)
                    return; // already cleaned up — ignore
                // The setup window stays up (showing "Opening the terminal…") while the
                // main window is built below — about two seconds on a first run. Hiding
                // it first left the user with no window at all for that time.

                auto& setup = fincept::python::PythonSetupManager::instance();
                if (setup.is_running()) {
                    // Early launch: the status-bar chip now reports progress and
                    // a failure surfaces as a notification. Agent discovery is
                    // re-warmed by the setup_complete hook above once agno & co.
                    // are actually installed.
                    setup.set_background_mode();
                    QObject::connect(
                        &setup, &fincept::python::PythonSetupManager::setup_complete, &app,
                        [](bool success, const QString& error) {
                            if (success)
                                LOG_INFO("App", "Background Python install finished after early launch");
                            else
                                LOG_WARN("App", "Background Python install failed (non-fatal): " + error);
                        },
                        Qt::SingleShotConnection);
                }

                fincept::KeyConfigManager::instance(); // init before WindowFrame registers shortcuts

                // Phase 6 final: if the previous session ended uncleanly and a
                // workspace snapshot is available, give the user the option to
                // restore. On accept, WorkspaceShell::apply already constructs
                // the frames it needs — we skip our own primary-window creation
                // path. On skip (or no recovery available), fall through.
                bool recovered = false;
                if (auto* recovery = fincept::TerminalShell::instance().crash_recovery();
                    recovery && recovery->needs_recovery()) {
                    fincept::screens::CrashRecoveryDialog dlg(recovery,
                                                              fincept::TerminalShell::instance().snapshot_ring());
                    dlg.exec();
                    recovered = dlg.was_restored();
                }

                if (!recovered) {
                    // Single primary window by default — see the matching no-setup
                    // path below for the full rationale. Extra windows stay an
                    // explicit user action ("New Window" / Ctrl+Shift+N / tear-off).
                    const QList<int> saved_ids = fincept::SessionManager::instance().load_window_ids();
                    const int primary_id = saved_ids.isEmpty() ? 0 : saved_ids.first();
                    auto* window = new fincept::WindowFrame(primary_id);
                    window->setAttribute(Qt::WA_DeleteOnClose);
                    window->show();
                }

                // The main window is on screen (or the recovered workspace is): the
                // setup window can go now.
                if (screen_guard) {
                    screen_guard->hide();
                    screen_guard->deleteLater();
                }

                // Wire new-window handler + Launchpad surface now that the
                // primary window exists. Single source of truth — see
                // wire_app_lifecycle() at the top of this file.
                wire_app_lifecycle(app, instance_lock);
                // Update check + restart hook (this path never runs --smoke-test).
                wire_updater();

                if (!fincept::ai_chat::LlmService::instance().is_configured())
                    LOG_INFO("App", "LLM provider not configured — AI chat will ask the user to set one up");

                // Warm agent discovery cache (same reason as the main path).
                QTimer::singleShot(0, &app, []() { fincept::services::AgentService::instance().discover_agents(); });

                LOG_INFO("App", QStringLiteral("Application ready startup_ms=%1 python=ready path=after_setup")
                                    .arg(QDateTime::currentMSecsSinceEpoch() -
                                         fincept::shutdown::detail::kProcessStartMs));
            },
            Qt::SingleShotConnection);

        return app.exec();
    }

    // Ensure KeyConfigManager is initialized before WindowFrame registers shortcuts
    fincept::KeyConfigManager::instance();

    // Phase 6 final: offer crash recovery before constructing the primary
    // window. If the user accepts and restoration succeeds, WorkspaceShell
    // has already built the frames it needs from the snapshot — we skip
    // both the primary-window creation and the SessionManager-based
    // secondary-window restoration paths to avoid duplicating windows.
    bool recovered = false;
    if (auto* recovery = fincept::TerminalShell::instance().crash_recovery();
        !smoke_mode && recovery && recovery->needs_recovery()) {
        fincept::screens::CrashRecoveryDialog dlg(recovery, fincept::TerminalShell::instance().snapshot_ring());
        dlg.exec();
        recovered = dlg.was_restored();
    }

    // Restore a SINGLE primary window at startup. The previous session may
    // have had several windows spread across multiple monitors, but auto-
    // reopening all of them surprised multi-monitor users — every launch
    // popped a second terminal on the second screen. Opening additional
    // windows stays an EXPLICIT action (toolbar "New Window", Ctrl+Shift+N,
    // the Launchpad button, tear-off), consistent with the single-instance
    // relaunch policy in wire_app_lifecycle(). We reopen the lowest saved
    // window_id (the primary) so its geometry + dock layout come back; the
    // user spawns extra windows on demand. closeEvent self-heals the saved
    // id set to the surviving windows, so this converges to [primary] cleanly.
    if (!recovered) {
        const QList<int> saved_ids = fincept::SessionManager::instance().load_window_ids();
        const int primary_id = saved_ids.isEmpty() ? 0 : saved_ids.first();
        auto* primary = new fincept::WindowFrame(primary_id);
        primary->setAttribute(Qt::WA_DeleteOnClose);
        primary->show();

        // Smoke test: once the window has painted, walk every screen and exit
        // with the result. Deferred so the shell + router are fully wired. The
        // CI job runs this with Qt/VS stripped from PATH, so a missing bundled
        // runtime (DLL, plugin, or data file like QtWebEngineProcess.exe) shows
        // up as a hard process abort or a non-constructing screen here — exactly
        // the class of failure the static dependency gate cannot detect.
        if (smoke_mode) {
            QPointer<fincept::WindowFrame> w = primary;
            QTimer::singleShot(2500, &app, [w]() {
                const int rc = fincept::run_screen_smoke_test(w ? w->dock_router() : nullptr);
                std::fprintf(stderr, "[Smoke] exit %d\n", rc);
                std::fflush(stderr);
                QCoreApplication::exit(rc);
            });
        }
    }

    // Wire new-window handler + Launchpad surface — see wire_app_lifecycle()
    // at the top of this file for the contract.
    wire_app_lifecycle(app, instance_lock);
    // Update check + restart hook — interactive runs only.
    if (!smoke_mode)
        wire_updater();

    // The terminal is usable but some Python libraries need installing or
    // verifying (app update changed a requirements file, an early launch last
    // session, or a pre-4.5.1 install). Only what the user has opted into is
    // kept in sync: the essentials, or — once the full set was requested from
    // Settings / on demand, or an older full install exists — everything.
    // Same pipeline as first run, in the background; the PYTHON ENV status chip
    // shows progress and a failure surfaces as a notification. Never during
    // --smoke-test.
    if (setup_status.needs_package_sync && !smoke_mode) {
        LOG_INFO("App",
                 QString("Python libraries need a sync (%1) — running setup in the background")
                     .arg(setup_status.full_requested ? QStringLiteral("full set") : QStringLiteral("essentials")));
        auto& mgr = fincept::python::PythonSetupManager::instance();
        mgr.set_background_mode();
        QObject::connect(
            &mgr, &fincept::python::PythonSetupManager::setup_complete, &app,
            [](bool success, const QString& error) {
                if (success)
                    LOG_INFO("App", "Background package sync completed successfully");
                else
                    LOG_WARN("App", "Background package sync failed (non-fatal): " + error);
            },
            Qt::SingleShotConnection);
        mgr.run_setup();
    }

    // Deferred, and NOT just to save a few ms: is_configured() runs
    // LlmService::ensure_config(), which on its FIRST call bakes the MCP
    // tool-category discovery hint into the system prompt and caches it in a
    // function-local static for the process lifetime. Tool registration is now
    // deferred (see the initialize_all_tools singleShot above), so calling this
    // inline would permanently cache a hint built against an empty registry.
    // Posting it here keeps it strictly after that turn.
    QTimer::singleShot(0, &app, []() {
        if (!fincept::ai_chat::LlmService::instance().is_configured())
            LOG_INFO("App", "LLM provider not configured — AI chat will ask the user to set one up");
    });

    // Warm the agent discovery cache on startup. This populates
    // AgentService::cached_agents() so any screen that lists agents
    // (Agent Config, Portfolio → Agent Runner, Node Editor) shows the
    // full finagent_core set immediately instead of falling back to the
    // much smaller DB-only list. Run deferred so Python is fully ready.
    QTimer::singleShot(0, &app, []() { fincept::services::AgentService::instance().discover_agents(); });

    // Agentic Mode: recover tasks the last session left "running" (they come
    // back paused so RESUME works) and arm the schedule timer for the next due
    // run. One Python process, deferred past the first paint.
    QTimer::singleShot(0, &app, []() { fincept::services::AgentService::instance().initialize_deferred(); });

    // Agent Workbench: prune old runs, mark crash-orphaned runs interrupted and
    // arm recurring schedules (no-op when none are enabled).
    QTimer::singleShot(0, &app, []() {
        fincept::services::coding_agents::CodingAgentService::instance().initialize_deferred();
    });

    // Chat Mode run engine: mark runs a quit interrupted, load recent runs and
    // register the run executors (no network until a run starts).
    QTimer::singleShot(0, &app, []() { fincept::agent_tasks::TaskRunService::instance().initialize_deferred(); });

    LOG_INFO("App", QStringLiteral("Application ready startup_ms=%1 python=%2")
                        .arg(QDateTime::currentMSecsSinceEpoch() - fincept::shutdown::detail::kProcessStartMs)
                        .arg(setup_status.needs_setup         ? QStringLiteral("setup_needed")
                             : setup_status.needs_package_sync ? QStringLiteral("sync_needed")
                                                               : QStringLiteral("ready")));
    return app.exec();
}
