#include "app/TerminalShell.h"

#include "ai_chat/ChatBubbleController.h"
#include "auth/lock/LockOverlayController.h"
#include "core/actions/ActionRegistry.h"
#include "core/actions/builtin_actions.h"
#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelRegistry.h"
#include "core/profile/ProfilePaths.h"
#include "core/layout/LayoutCatalog.h"
#include "core/screen/MonitorWatcher.h"
#include "core/telemetry/LocalTelemetrySink.h"
#include "core/telemetry/TelemetryProvider.h"
#include "core/window/WindowRegistry.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/workspace/CrashRecovery.h"
#include "storage/workspace/WorkspaceDb.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <QDateTime>
#include <QUuid>

namespace fincept {

namespace {
constexpr const char* kShellTag = "TerminalShell";
} // namespace

TerminalShell& TerminalShell::instance() {
    static TerminalShell s;
    return s;
}

// FT_TS: trace markers identical to FT_MARK in main.cpp, scoped to TerminalShell::initialise.
#ifdef Q_OS_WIN
#  include <windows.h>
#  include <stdio.h>
#  include <string.h>
#  define FT_TS(n) do { char _msg[64]; _snprintf_s(_msg, 64, _TRUNCATE, "FT_TS %d\n", (n)); OutputDebugStringA(_msg); HANDLE _h = CreateFileA("C:\\Users\\Tilak\\AppData\\Local\\Temp\\ft_marks.txt", FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); if (_h != INVALID_HANDLE_VALUE) { DWORD _w; SetFilePointer(_h, 0, NULL, FILE_END); WriteFile(_h, _msg, (DWORD)strlen(_msg), &_w, NULL); CloseHandle(_h); } } while(0)
#else
#  define FT_TS(n) do { fprintf(stderr, "FT_TS %d\n", (n)); fflush(stderr); } while(0)
#endif

void TerminalShell::initialise() {
    FT_TS(100);
    if (initialised_) {
        LOG_WARN(kShellTag, "initialise() called twice — ignoring");
        return;
    }

    LOG_INFO(kShellTag, "Shell initialising");
    FT_TS(101);

    // Resolve and cache the active profile UUID so every later phase has
    // a stable handle without re-reading the manifest. ProfileManager
    // mints + persists a UUID on first read if the manifest is legacy.
    active_profile_id_ = ProfileManager::instance().active_profile_id();
    FT_TS(102);
    LOG_INFO(kShellTag, QString("Active profile: %1 (id=%2)")
                          .arg(ProfileManager::instance().active())
                          .arg(active_profile_id_.to_string()));
    FT_TS(103);
    // Create the new per-profile directory tree (workspace.db, layouts/,
    // crashes/). Distinct from AppPaths::ensure_all() which creates the
    // legacy tree (data/, logs/, cache/, etc.) — both run, both are
    // idempotent.
    ProfilePaths::ensure_all();
    FT_TS(104);
    // Touch the registries so their static singletons construct *before*
    // any WindowFrame tries to register against them. Avoids a startup
    // race where the first window's constructor runs faster than the
    // registry's singleton init under aggressive optimisation.
    (void) WindowRegistry::instance();
    FT_TS(105);
    (void) ActionRegistry::instance();
    FT_TS(106);
    (void) PanelRegistry::instance();
    FT_TS(107);
    (void) ProfileManager::instance();
    FT_TS(108);
    // Phase 6 trim: MonitorWatcher boots here so its QGuiApplication signal
    // connections are in place before any frame restores. Phase 6's full
    // workspace-variant matcher will subscribe to topology_changed.
    (void) MonitorWatcher::instance();
    FT_TS(109);
    // Phase 1b skeleton: LockOverlayController construction. Currently a
    // no-op — the full lift is deferred (see auth/lock/LockOverlayController.h
    // for rationale). Constructing it here keeps the dependency direction
    // shell → controller correct so the future lift doesn't have to invert.
    auth::LockOverlayController::instance().initialise();
    FT_TS(110);
    // Phase 3 final: chat-bubble shell coordinator. Per-frame bubble
    // widgets stay where they are; this just centralises the shell-side
    // observation surface (frame add/remove tracking, future telemetry,
    // future cross-frame chat-session linking).
    ai_chat::ChatBubbleController::instance().initialise();
    FT_TS(111);
    // Phase 6: open the per-profile LayoutCatalog so Launchpad's recent-
    // layouts list + the layout.* actions can read/write immediately.
    {
        auto r = LayoutCatalog::instance().open();
        if (r.is_err()) {
            LOG_WARN(kShellTag, QString("LayoutCatalog open failed: %1")
                                    .arg(QString::fromStdString(r.error())));
        }
    }
    FT_TS(112);

    // Phase 10: install LocalTelemetrySink iff the user opted in. Settings
    // → Telemetry section (Phase 9 settings refactor) is the user-facing
    // toggle; for now it's an explicit SettingsRepository key. Default off
    // — no telemetry without consent.
    {
        auto r = SettingsRepository::instance().get("telemetry.local_enabled");
        const bool enabled = r.is_ok() && r.value() == "true";
        if (enabled) {
            static telemetry::LocalTelemetrySink local_sink;
            telemetry::TelemetrySink::instance().set_provider(&local_sink);
            LOG_INFO(kShellTag, "Local telemetry enabled");
        } else {
            LOG_DEBUG(kShellTag, "Telemetry off (telemetry.local_enabled != true)");
        }
    }

    // Phase 4 Track B: populate the action registry with the built-ins that
    // replaced WindowFrame's hard-coded keyboard wiring. Must happen before
    // any WindowFrame is constructed so the per-frame hotkey-binding loop
    // sees the full action set.
    FT_TS(113);
    actions::register_builtins();
    FT_TS(114);
    // ── Phase 2: workspace persistence + crash recovery ────────────────
    workspace_db_ = &WorkspaceDb::instance();
    FT_TS(115);
    QString _ws_path = ProfilePaths::workspace_db();
    FT_TS(1150);
#ifdef Q_OS_WIN
    char _path_msg[512];
    _snprintf_s(_path_msg, 512, _TRUNCATE, "FT_TS workspace_db path: %s\n", _ws_path.toUtf8().constData());
    OutputDebugStringA(_path_msg);
    {
        HANDLE _h = CreateFileA("C:\\Users\\Tilak\\AppData\\Local\\Temp\\ft_marks.txt", FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (_h != INVALID_HANDLE_VALUE) { DWORD _w; SetFilePointer(_h, 0, NULL, FILE_END); WriteFile(_h, _path_msg, (DWORD)strlen(_path_msg), &_w, NULL); CloseHandle(_h); }
    }
#endif
    auto db_open = workspace_db_->open(_ws_path);
    FT_TS(116);
    if (db_open.is_err()) {
        LOG_ERROR(kShellTag, QString("Failed to open workspace.db: %1")
                               .arg(QString::fromStdString(db_open.error())));
        // Keep going — the shell can still run without persistence; later
        // saves just become no-ops. We don't want a corrupted db to brick
        // the terminal entirely.
        workspace_db_ = nullptr;
    } else {
        snapshot_ring_ = new WorkspaceSnapshotRing(workspace_db_);
        crash_recovery_ = new CrashRecovery(workspace_db_, snapshot_ring_);

        // Crash recovery check. Phase 2 logs only — Phase 6 wires the
        // dialog UI. The clean-shutdown marker write below would mask
        // the unclean state if it ran first, so check_for_recovery
        // happens *before* the new session row is opened.
        if (crash_recovery_->needs_recovery()) {
            LOG_WARN(kShellTag, "Previous session ended uncleanly — recovery candidates available");
        }

        // Mint a fresh session_id and open a session_history row.
        // ended_at + exit_kind stay NULL until shutdown() updates them.
        session_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
        auto sess = workspace_db_->execute(
            "INSERT INTO session_history(session_id, profile_id, started_at) VALUES(?, ?, ?)",
            {session_id_, active_profile_id_.to_string(), now_ms});
        if (sess.is_err()) {
            LOG_WARN(kShellTag, QString("session_history insert failed: %1")
                                  .arg(QString::fromStdString(sess.error())));
        } else {
            LOG_INFO(kShellTag, QString("Session %1 started").arg(session_id_));
        }
    }

    FT_TS(120);
    initialised_ = true;
    LOG_INFO(kShellTag, "Shell initialised");
    emit started();
    FT_TS(121);
}

void TerminalShell::shutdown() {
    if (!initialised_)
        return;
    LOG_INFO(kShellTag, "Shell shutting down");
    emit shutting_down();

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
            auto upd = workspace_db_->execute(
                "UPDATE session_history SET ended_at = ?, exit_kind = 'clean', frame_count = ? "
                "WHERE session_id = ?",
                {now_ms, frame_count, session_id_});
            if (upd.is_err()) {
                LOG_WARN(kShellTag, QString("session_history update failed: %1")
                                      .arg(QString::fromStdString(upd.error())));
            }
        }
        if (crash_recovery_)
            crash_recovery_->mark_clean_shutdown();
    }

    // Tear down owned objects in reverse construction order.
    delete crash_recovery_;
    crash_recovery_ = nullptr;
    delete snapshot_ring_;
    snapshot_ring_ = nullptr;
    if (workspace_db_) {
        workspace_db_->close();
        workspace_db_ = nullptr;
    }

    initialised_ = false;
    // The underlying singletons (Logger, SessionManager, etc.) clean
    // themselves up via qAddPostRoutine and Qt's exit hooks. We don't
    // tear them down explicitly here — main.cpp's aboutToQuit handlers
    // own that.
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
