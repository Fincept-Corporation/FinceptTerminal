#include "app/TerminalShell.h"

#include "app/WindowFrame.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/lock/LockOverlayController.h"
#include "core/actions/ActionRegistry.h"
#include "core/actions/builtin_actions.h"
#include "core/config/ProfileManager.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelRegistry.h"
#include "core/profile/ProfilePaths.h"
#include "core/screen/MonitorWatcher.h"
#include "core/telemetry/ClientInfo.h"
#include "core/telemetry/UsageReporter.h"
#include "core/window/WindowRegistry.h"
#include "screens/ai_chat/ChatBubbleController.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/sqlite/SqliteSupport.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceDb.h"
#include "storage/workspace/WorkspaceFwspImporter.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QScreen>

namespace fincept {

namespace {
constexpr const char* kShellTag = "TerminalShell";
} // namespace

TerminalShell& TerminalShell::instance() {
    static TerminalShell s;
    return s;
}

void TerminalShell::initialise() {
    if (initialised_) {
        LOG_WARN(kShellTag, "initialise() called twice — ignoring");
        return;
    }

    LOG_DEBUG(kShellTag, "Shell initialising");

    // Resolve and cache the active profile UUID so every later phase has
    // a stable handle without re-reading the manifest. ProfileManager
    // mints + persists a UUID on first read if the manifest is legacy.
    active_profile_id_ = ProfileManager::instance().active_profile_id();
    LOG_DEBUG(kShellTag, QString("Active profile: %1 (id=%2)")
                            .arg(ProfileManager::instance().active())
                            .arg(active_profile_id_.to_string()));
    // Create the new per-profile directory tree (workspace.db, layouts/,
    // crashes/). Distinct from AppPaths::ensure_all() which creates the
    // legacy tree (data/, logs/, cache/, etc.) — both run, both are
    // idempotent.
    ProfilePaths::ensure_all();
    // Touch the registries so their static singletons construct *before*
    // any WindowFrame tries to register against them. Avoids a startup
    // race where the first window's constructor runs faster than the
    // registry's singleton init under aggressive optimisation.
    (void)WindowRegistry::instance();
    (void)ActionRegistry::instance();
    (void)PanelRegistry::instance();
    (void)ProfileManager::instance();
    // Phase 6 trim: MonitorWatcher boots here so its QGuiApplication signal
    // connections are in place before any frame restores. Phase 6's full
    // workspace-variant matcher will subscribe to topology_changed.
    (void)MonitorWatcher::instance();
    // Rescue off-screen windows when monitors come or go at runtime. The
    // raw `screenAdded` / `screenRemoved` signals fire mid-burst (some
    // OSes emit removed-then-added on dock unplug/replug); MonitorWatcher
    // debounces to a single topology_changed once the dust has settled.
    QObject::connect(&MonitorWatcher::instance(), &MonitorWatcher::topology_changed, &MonitorWatcher::instance(),
                     [](const layout::MonitorTopologyKey&) {
                         const auto screens = QGuiApplication::screens();
                         if (screens.isEmpty())
                             return;
                         for (WindowFrame* w : WindowRegistry::instance().frames()) {
                             if (!w)
                                 continue;
                             const QRect frame = w->frameGeometry();
                             // A window is "rescued" only if no currently-connected screen's
                             // available geometry intersects it at all — i.e. it would be
                             // completely invisible. Partial overlap is fine; the user can
                             // still grab the title bar.
                             bool on_some_screen = false;
                             for (QScreen* s : screens) {
                                 if (s && s->availableGeometry().intersects(frame)) {
                                     on_some_screen = true;
                                     break;
                                 }
                             }
                             if (on_some_screen)
                                 continue;
                             QScreen* target = QGuiApplication::primaryScreen();
                             if (!target)
                                 target = screens.first();
                             if (!target)
                                 continue;
                             const QRect ag = target->availableGeometry();
                             // Keep the window's size if it fits, otherwise clamp to 9/10
                             // of the target screen (same heuristic the constructor and the
                             // new-window path use).
                             QSize sz = w->size();
                             sz.setWidth(qMin(sz.width(), ag.width() * 9 / 10));
                             sz.setHeight(qMin(sz.height(), ag.height() * 9 / 10));
                             w->resize(sz);
                             w->move(ag.center() - QPoint(sz.width() / 2, sz.height() / 2));
                             LOG_INFO(kShellTag, QString("Rescued window %1 onto '%2' after topology change")
                                                     .arg(w->window_id())
                                                     .arg(target->name()));
                         }
                     });
    // Phase 1b skeleton: LockOverlayController construction. Currently a
    // no-op — the full lift is deferred (see auth/lock/LockOverlayController.h
    // for rationale). Constructing it here keeps the dependency direction
    // shell → controller correct so the future lift doesn't have to invert.
    auth::LockOverlayController::instance().initialise();
    // Phase 3 final: chat-bubble shell coordinator. Per-frame bubble
    // widgets stay where they are; this just centralises the shell-side
    // observation surface (frame add/remove tracking, future telemetry,
    // future cross-frame chat-session linking).
    ai_chat::ChatBubbleController::instance().initialise();
    // Phase 6: open the per-profile LayoutCatalog so Launchpad's recent-
    // layouts list + the layout.* actions can read/write immediately.
    {
        auto r = LayoutCatalog::instance().open();
        if (r.is_err()) {
            LOG_WARN(kShellTag, QString("LayoutCatalog open failed: %1").arg(QString::fromStdString(r.error())));
        } else {
            // One-shot: import legacy v3 *.fwsp filenames as empty named
            // layouts so users see them in the Launchpad list. No-op after
            // first successful run (tracked via _meta.fwsp_import_done).
            WorkspaceFwspImporter::run_once_if_needed();
        }
    }

    // Phase 4 Track B: populate the action registry with the built-ins that
    // replaced WindowFrame's hard-coded keyboard wiring. Must happen before
    // any WindowFrame is constructed so the per-frame hotkey-binding loop
    // sees the full action set.
    actions::register_builtins();
    // ── Phase 2: workspace persistence + crash recovery ────────────────
    workspace_db_ = &WorkspaceDb::instance();
    auto db_open = workspace_db_->open(ProfilePaths::workspace_db());
    if (db_open.is_err()) {
        // open() has already moved a damaged or other-build workspace.db aside
        // and created a fresh one, and it leaves the wrapper fully closed on
        // failure. What still fails here is environmental — another process
        // holding the file, a read-only folder, a full disk, no permission —
        // and a new file would fail the same way. Keep going without
        // persistence for this session (saves become no-ops, crash recovery is
        // off) rather than brick the terminal.
        const QString raw = QString::fromStdString(db_open.error());
        LOG_ERROR(kShellTag,
                  QString("Workspace persistence is off for this session. %1 Details: %2")
                      .arg(db::user_message(db::classify_text(raw), QStringLiteral("The workspace database")), raw));
        workspace_db_ = nullptr;
    } else {
        snapshot_ring_ = new WorkspaceSnapshotRing(workspace_db_);
        crash_recovery_ = new CrashRecovery(workspace_db_, snapshot_ring_);

        // Crash recovery check. Phase 2 logs only — Phase 6 wires the
        // dialog UI. The clean-shutdown marker write below would mask
        // the unclean state if it ran first, so check_for_recovery
        // happens *before* the new session row is opened.
        // Latch the answer at boot. needs_recovery() compares against
        // the latest auto snapshot, which the *current* session also
        // writes to — so within ~60s of startup the same call would start
        // returning false positives. Consumers that want a stable "we
        // started after a crash" signal must read started_after_crash_.
        started_after_crash_ = crash_recovery_->needs_recovery();
        if (started_after_crash_) {
            LOG_WARN(kShellTag, "Previous session ended uncleanly recovery_candidates=yes");
        }

        // Mint a fresh session_id and open a session_history row.
        // ended_at + exit_kind stay NULL until shutdown() updates them.
        // It is the process's run id, the one the Fincept API sees (X-Fincept-Run).
        session_id_ = telemetry::ClientInfo::run_id();
        const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
        auto sess =
            workspace_db_->execute("INSERT INTO session_history(session_id, profile_id, started_at) VALUES(?, ?, ?)",
                                   {session_id_, active_profile_id_.to_string(), now_ms});
        if (sess.is_err()) {
            LOG_WARN(kShellTag, QString("session_history insert failed: %1").arg(QString::fromStdString(sess.error())));
        } else {
            LOG_INFO(kShellTag, QString("Session started session_id=%1 previous_exit=%2")
                                    .arg(session_id_, started_after_crash_ ? QStringLiteral("crash")
                                                      : crash_recovery_    ? QStringLiteral("clean")
                                                                           : QStringLiteral("unknown")));
        }
    }

    // Usage report (core/telemetry/UsageReporter.h); inert in headless runs.
    telemetry::UsageReporter::instance().start(started_after_crash_ ? QStringLiteral("crash")
                                               : crash_recovery_    ? QStringLiteral("clean")
                                                                    : QStringLiteral("unknown"));

    initialised_ = true;
    LOG_DEBUG(kShellTag, "Shell initialised");
    emit started();
}

void TerminalShell::shutdown() {
    if (!initialised_)
        return;
    LOG_DEBUG(kShellTag, "Shell shutting down");
    emit shutting_down();
    // Unsent usage goes to a file for the next start — no network on the way out.
    telemetry::UsageReporter::instance().shutdown();

    // Update the session row + write the clean-shutdown marker. Order
    // matters: marker last, so that if the marker write fails the row's
    // ended_at is still recorded for diagnostics.
    if (workspace_db_ && workspace_db_->is_open()) {
        const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
        if (!session_id_.isEmpty()) {
            // frame_count + panel_count are best-effort: WindowRegistry
            // may have already started tearing down by aboutToQuit time
            // depending on which Qt finalizer ran first. Read it anyway —
            // a stale snapshot is fine for analytics.
            const int frame_count = WindowRegistry::instance().frame_count();
            auto upd =
                workspace_db_->execute("UPDATE session_history SET ended_at = ?, exit_kind = 'clean', frame_count = ? "
                                       "WHERE session_id = ?",
                                       {now_ms, frame_count, session_id_});
            if (upd.is_err()) {
                LOG_WARN(kShellTag,
                         QString("session_history update failed: %1").arg(QString::fromStdString(upd.error())));
            }
        }
        if (crash_recovery_)
            crash_recovery_->mark_clean_shutdown();
    }

    // Drop the in-memory "currently loaded layout" pointers so any code that
    // peeks at WorkspaceShell::current_id() during teardown (or across a
    // future profile-switch) doesn't see a stale value pointing at a layout
    // catalogue we're about to close.
    layout::WorkspaceShell::clear_current();

    // Tear down owned objects in reverse construction order.
    delete crash_recovery_;
    crash_recovery_ = nullptr;
    delete snapshot_ring_;
    snapshot_ring_ = nullptr;
    if (workspace_db_) {
        // Orderly: PRAGMA optimize + wal_checkpoint(TRUNCATE), then the
        // connection is closed and unregistered; late worker calls get a clean
        // "unavailable" instead of re-growing the -wal.
        workspace_db_->close();
        workspace_db_ = nullptr;
    }
    // Same for the layouts index (UI-thread store, opened in initialise()). A
    // late save/list after this reopens it lazily.
    LayoutCatalog::instance().close();

    initialised_ = false;
    // The underlying singletons (Logger, SessionManager, etc.) clean
    // themselves up via qAddPostRoutine and Qt's exit hooks. We don't
    // tear them down explicitly here — main.cpp's aboutToQuit handlers
    // own that.
}

void TerminalShell::bootstrap_auth() {
    if (auth_bootstrapped_) {
        LOG_WARN(kShellTag, "bootstrap_auth() called twice — ignoring");
        return;
    }

    LOG_DEBUG(kShellTag, "Bootstrapping auth");

    // 1. AuthManager — loads saved session, validates with server. The
    //    HTTP-bearing call here means we MUST be on the UI thread post
    //    QApplication construction; main.cpp calls bootstrap_auth() in
    //    that window.
    auth::AuthManager::instance().initialize();

    // 2. PinManager — touch the singleton so it loads PIN state from
    //    SecureStorage. Lazy construction would otherwise defer the load
    //    to the first lock event, racing the InactivityGuard timer below.
    (void)auth::PinManager::instance();

    // 3. InactivityGuard — auto-lock after idle timeout. Read the user's
    //    chosen timeout from SettingsRepository and apply it; the guard
    //    stays disabled until WindowFrame::on_terminal_unlocked() flips
    //    it on after a successful PIN verify.
    {
        auto& guard = auth::InactivityGuard::instance();
        auto timeout_r = SettingsRepository::instance().get("security.lock_timeout_minutes");
        if (timeout_r.is_ok() && !timeout_r.value().isEmpty()) {
            const int minutes = timeout_r.value().toInt();
            if (minutes > 0)
                guard.set_timeout_minutes(minutes);
        }
    }

    auth_bootstrapped_ = true;
    LOG_DEBUG(kShellTag, "Auth bootstrapped");
}

ProfileId TerminalShell::active_profile_id() const {
    return active_profile_id_;
}

WindowRegistry& TerminalShell::window_registry() {
    return WindowRegistry::instance();
}

ActionRegistry& TerminalShell::action_registry() {
    return ActionRegistry::instance();
}

PanelRegistry& TerminalShell::panel_registry() {
    return PanelRegistry::instance();
}

ProfileManager& TerminalShell::profile_manager() {
    return ProfileManager::instance();
}

} // namespace fincept
