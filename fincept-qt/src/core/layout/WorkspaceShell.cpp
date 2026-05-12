#include "core/layout/WorkspaceShell.h"

#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "core/identity/Uuid.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/WorkspaceMatcher.h"
#include "core/logging/Logger.h"
#include "core/profile/ProfilePaths.h"
#include "core/screen/MonitorTopology.h"
#include "core/symbol/SymbolContext.h"
#include "core/window/WindowRegistry.h"
#include "storage/workspace/WorkspaceSnapshotRing.h"

#include <DockManager.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPixmap>
#include <QScreen>
#include <QSet>
#include <QWindow>

namespace fincept::layout {

namespace {
constexpr const char* kShellTag = "WorkspaceShell";

// Process-wide state — there is exactly one shell, so a file-scope cache
// is fine. Updated only on the UI thread inside apply().
QString g_current_name;
LayoutId g_current_id;

// L7: ensure restored geometry lands somewhere visible. If the saved rect
// has no overlap with any connected screen (e.g. external monitor was
// disconnected since the layout was saved, or DPI/topology shifted the
// geometry off-screen), recenter the window on the primary screen with
// the saved size preserved. Conservative: we only intervene when the rect
// is *entirely* off-screen — partially-visible windows are left alone so
// users with intentional overflow placements aren't disrupted.
QRect ensure_on_screen(const QRect& saved) {
    if (!saved.isValid() || saved.isEmpty())
        return saved;
    const auto screens = QGuiApplication::screens();
    for (auto* s : screens) {
        if (s && s->availableGeometry().intersects(saved))
            return saved;
    }
    auto* primary = QGuiApplication::primaryScreen();
    if (!primary)
        return saved; // headless / boot race — let Qt handle it
    const QRect avail = primary->availableGeometry();
    QSize sz = saved.size();
    // Clamp to the primary screen if the saved size is larger than the
    // current display (downsizing monitor case).
    if (sz.width()  > avail.width())  sz.setWidth(avail.width());
    if (sz.height() > avail.height()) sz.setHeight(avail.height());
    return QRect(avail.center() - QPoint(sz.width()/2, sz.height()/2), sz);
}
} // namespace

QString WorkspaceShell::current_name() { return g_current_name; }
LayoutId WorkspaceShell::current_id()  { return g_current_id; }

void WorkspaceShell::clear_current() {
    g_current_name.clear();
    g_current_id = LayoutId{};
}

Workspace WorkspaceShell::capture(const QString& name, const QString& kind,
                                  const Workspace* previous) {
    Workspace w;
    if (previous) {
        // Preserve identity + variant history. Caller's intent: "save the
        // current state as a new variant of this workspace."
        w.id = previous->id;
        w.created_at_unix = previous->created_at_unix;
        w.variants = previous->variants;
        w.author = previous->author;
        w.description = previous->description;
        w.thumbnail_path = previous->thumbnail_path;
    } else {
        w.id = LayoutId::generate();
        w.created_at_unix = QDateTime::currentSecsSinceEpoch();
    }
    w.name = name;
    w.kind = kind;
    w.updated_at_unix = QDateTime::currentSecsSinceEpoch();
    w.schema_version = kWorkspaceVersion;

    // Capture each frame's layout.
    const auto frames = fincept::WindowRegistry::instance().frames();
    for (auto* frame : frames) {
        if (!frame) continue;
        FrameLayout fl = frame->capture_layout();
        w.frames.append(fl);
    }

    // Capture / update the topology variant for the current monitor set.
    const MonitorTopologyKey current = fincept::MonitorTopology::current_key();
    WorkspaceVariant variant;
    variant.topology = current;
    for (auto* frame : frames) {
        if (!frame) continue;
        const WindowId id = frame->frame_uuid();
        variant.frame_geometries.insert(id, frame->geometry());
        if (auto* scr = frame->screen())
            variant.frame_screens.insert(id, fincept::MonitorTopology::stable_id(scr));
    }

    // Replace any existing variant for this exact topology, else append.
    bool replaced = false;
    for (int i = 0; i < w.variants.size(); ++i) {
        if (w.variants[i].topology == current) {
            w.variants[i] = variant;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        w.variants.append(variant);

    // Snapshot per-link-group SymbolContext (red/green/yellow links) so the
    // user's pinned symbols per group survive a layout switch / restart.
    // Stored opaquely under "symbol_context" — SymbolContext owns the schema.
    const QJsonObject ctx_json = SymbolContext::instance().to_json();
    const QByteArray ctx_bytes = QJsonDocument(ctx_json).toJson(QJsonDocument::Compact);
    w.link_state.insert(QStringLiteral("symbol_context"), ctx_bytes);

    // Phase L5: thumbnail capture. Grab the first live frame's dock area as
    // a 320×180 PNG under <profile>/layouts/thumbs/<uuid>.png. Path is
    // stored relative so the layouts dir can be moved without breaking
    // refs. Skipped silently if there's no live frame yet (capture from
    // Launchpad-only state) or if grab() returns null. Also skipped for
    // kind="auto" — those snapshots are throwaways trimmed by the ring,
    // and the per-call grab+scale+PNG write is the dominant cost in the
    // autosave hot path.
    if (kind != QStringLiteral("auto") && !frames.isEmpty() && frames.first()) {
        if (auto* dm = frames.first()->dock_manager()) {
            const QPixmap shot = dm->grab().scaled(
                320, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (!shot.isNull()) {
                const QString thumb_dir = ProfilePaths::layouts_dir() + "/thumbs";
                QDir().mkpath(thumb_dir);
                const QString thumb_rel = QStringLiteral("thumbs/%1.png").arg(w.id.to_string());
                const QString thumb_abs = ProfilePaths::layouts_dir() + "/" + thumb_rel;
                if (shot.save(thumb_abs, "PNG"))
                    w.thumbnail_path = thumb_rel;
            }
        }
    }

    LOG_INFO(kShellTag,
             QString("capture: '%1' (uuid=%2) → %3 frames, %4 variants, topology=%5")
                 .arg(name, w.id.to_string()).arg(w.frames.size()).arg(w.variants.size()).arg(current.value));
    return w;
}

int WorkspaceShell::apply(const Workspace& workspace) {
    if (workspace.frames.isEmpty()) {
        LOG_INFO(kShellTag, QString("apply: workspace '%1' has no frames — nothing to do").arg(workspace.name));
        return 0;
    }

    // Match the workspace's variants against the current topology to pick
    // the best frame geometry set. Falls back to nullptr if no variant
    // matches; we then apply layouts without geometry hints (frames keep
    // their current size/position).
    const MonitorTopologyKey current = fincept::MonitorTopology::current_key();
    const WorkspaceVariant* variant = WorkspaceMatcher::match(workspace, current);

    LOG_INFO(kShellTag,
             QString("apply: '%1' (uuid=%2) → %3 frames; matched variant: %4")
                 .arg(workspace.name, workspace.id.to_string()).arg(workspace.frames.size())
                 .arg(variant ? variant->topology.value : QStringLiteral("(none)")));

    int applied = 0;
    auto frames_now = fincept::WindowRegistry::instance().frames();
    QSet<fincept::WindowFrame*> bound; // frames already paired with a FrameLayout this call

    for (const auto& fl : workspace.frames) {
        // Find an existing frame matching this window_id (and not already
        // claimed by an earlier FrameLayout in this same apply() call).
        fincept::WindowFrame* target = nullptr;
        bool spawned = false;
        for (auto* f : frames_now) {
            if (f && !bound.contains(f) && f->frame_uuid() == fl.window_id) {
                target = f;
                break;
            }
        }
        if (!target) {
            // Spawn a fresh frame and adopt the persisted WindowId via the
            // constructor — must happen before setup_docking_mode's default
            // navigate creates panels that capture frame_uuid(). Without
            // this, the variant geometry/screen lookups (keyed on
            // fl.window_id) would miss for every spawned frame.
            target = new fincept::WindowFrame(
                fincept::WindowFrame::next_window_id(),
                /*parent=*/nullptr,
                /*adopted_uuid=*/fl.window_id);
            target->setAttribute(Qt::WA_DeleteOnClose);
            target->show();
            frames_now.append(target);
            spawned = true;
        }
        bound.insert(target);

        // Apply geometry from the matched variant — but ONLY when we just
        // spawned the frame. For an already-live frame the user is looking
        // at, its position was set by the WindowFrame constructor's
        // QSettings restore (and the user may have moved it). Re-applying
        // the workspace's saved screen here teleports the window mid-
        // session — e.g. on PIN unlock, load_last_or_default() runs and
        // would yank the window from monitor 1 (where the lock screen sat)
        // to monitor 2 (where the snapshot says it lived last). The user
        // experiences this as the app "jumping" to another monitor.
        //
        // Freshly-spawned frames need the geometry hint because their
        // constructor had no QSettings row for the adopted UUID.
        if (variant && spawned) {
            const auto scr_it = variant->frame_screens.constFind(fl.window_id);
            if (scr_it != variant->frame_screens.constEnd()) {
                if (QScreen* saved_screen =
                        fincept::MonitorTopology::find_screen_by_stable_id(scr_it.value())) {
                    if (auto* win = target->windowHandle()) {
                        if (win->screen() != saved_screen)
                            win->setScreen(saved_screen);
                    } else {
                        // No QWindow yet (frame not shown). Move to the
                        // saved screen's available area as a best-effort
                        // hint; the geometry call below refines this.
                        target->move(saved_screen->availableGeometry().topLeft());
                    }
                }
            }
            const auto geo_it = variant->frame_geometries.constFind(fl.window_id);
            if (geo_it != variant->frame_geometries.constEnd())
                target->setGeometry(ensure_on_screen(geo_it.value()));
        }

        if (target->apply_layout(fl))
            ++applied;
    }

    LOG_INFO(kShellTag, QString("apply: %1/%2 frames restored cleanly").arg(applied).arg(workspace.frames.size()));

    // Restore SymbolContext if the workspace carried one. Empty / missing
    // is fine (older workspaces saved before Phase 5 shipped).
    const auto ctx_it = workspace.link_state.constFind(QStringLiteral("symbol_context"));
    if (ctx_it != workspace.link_state.constEnd() && !ctx_it.value().isEmpty()) {
        QJsonParseError err{};
        const auto ctx_doc = QJsonDocument::fromJson(ctx_it.value(), &err);
        if (err.error == QJsonParseError::NoError && ctx_doc.isObject())
            SymbolContext::instance().from_json(ctx_doc.object());
        else
            LOG_WARN(kShellTag,
                     QString("apply: symbol_context parse failed: %1").arg(err.errorString()));
    }

    // Update process-wide state so the title bar + toolbar see the current
    // layout. Pin non-auto layouts in LayoutCatalog so cold-boot can find
    // them next session. Auto snapshots are throwaway — never pinned.
    if (applied > 0) {
        g_current_name = workspace.name;
        g_current_id = workspace.id;
        if (workspace.kind != QStringLiteral("auto") && !workspace.id.is_null()) {
            auto pin_r = LayoutCatalog::instance().set_last_loaded_id(workspace.id);
            if (pin_r.is_err())
                LOG_WARN(kShellTag,
                         QString("Failed to pin last_loaded_id: %1")
                             .arg(QString::fromStdString(pin_r.error())));
        }
    }
    return applied;
}

int WorkspaceShell::load_last_or_default() {
    // Step 1: explicit last-loaded layout (set by apply() on every non-auto
    // workspace). If the UUID is dead (file deleted out from under us),
    // log + fall through.
    auto last = LayoutCatalog::instance().last_loaded_id();
    if (last.is_ok() && !last.value().is_null()) {
        auto wr = LayoutCatalog::instance().load_workspace(last.value());
        if (wr.is_ok()) {
            const int n = apply(wr.value());
            LOG_INFO(kShellTag,
                     QString("load_last_or_default: restored '%1' (%2 frames)")
                         .arg(wr.value().name).arg(n));
            return n;
        }
        LOG_WARN(kShellTag,
                 QString("load_last_or_default: last_loaded_uuid=%1 unreadable: %2")
                     .arg(last.value().to_string(), QString::fromStdString(wr.error())));
    }

    // Step 2: most recent kind='auto' snapshot from the ring buffer. The
    // shell owns the ring; if it's gone (early-init / shutdown teardown)
    // we can't fall through any further.
    auto* ring = TerminalShell::instance().snapshot_ring();
    if (!ring) {
        LOG_INFO(kShellTag, "load_last_or_default: snapshot ring unavailable; first run path");
        return 0;
    }
    auto snaps = ring->latest_auto(/*limit=*/1);
    if (snaps.is_ok() && !snaps.value().isEmpty()) {
        auto payload = ring->load_payload(snaps.value().first().snapshot_id);
        if (payload.is_ok() && !payload.value().isEmpty()) {
            QJsonParseError err{};
            const auto doc = QJsonDocument::fromJson(payload.value(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                Workspace w = Workspace::from_json(doc.object());
                // Force kind='auto' so apply() doesn't pin a snapshot as
                // last_loaded_id (the next user save should be the pin).
                w.kind = QStringLiteral("auto");
                const int n = apply(w);
                LOG_INFO(kShellTag,
                         QString("load_last_or_default: restored auto snapshot (%1 frames)")
                             .arg(n));
                return n;
            }
            LOG_WARN(kShellTag,
                     QString("load_last_or_default: snapshot parse failed: %1").arg(err.errorString()));
        }
    }

    LOG_INFO(kShellTag, "load_last_or_default: nothing to restore (first run / fresh profile)");
    return 0;
}

} // namespace fincept::layout
