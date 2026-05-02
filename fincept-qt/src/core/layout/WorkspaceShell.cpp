#include "core/layout/WorkspaceShell.h"

#include "app/WindowFrame.h"
#include "core/identity/Uuid.h"
#include "core/layout/WorkspaceMatcher.h"
#include "core/logging/Logger.h"
#include "core/screen/MonitorTopology.h"
#include "core/window/WindowRegistry.h"

#include <QApplication>
#include <QDateTime>

namespace fincept::layout {

namespace {
constexpr const char* kShellTag = "WorkspaceShell";
} // namespace

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

    for (const auto& fl : workspace.frames) {
        // Find an existing frame matching this window_id; else spawn new.
        fincept::WindowFrame* target = nullptr;
        for (auto* f : frames_now) {
            if (f && f->frame_uuid() == fl.window_id) {
                target = f;
                break;
            }
        }
        if (!target) {
            // Spawn a fresh frame. The constructor's frame_uuid_ is
            // freshly generated; we can't easily override it because
            // WindowId is process-stable per construction. Instead, reuse
            // an unused existing frame if available; else create a new one.
            // For v1 we accept that the new frame's UUID differs from the
            // saved one — the FrameLayout's panel UUIDs are what really
            // matter for state restoration.
            target = new fincept::WindowFrame(fincept::WindowFrame::next_window_id());
            target->setAttribute(Qt::WA_DeleteOnClose);
            target->show();
            frames_now.append(target);
        }

        // Apply geometry from the matched variant (if any).
        if (variant) {
            const auto geo_it = variant->frame_geometries.constFind(fl.window_id);
            if (geo_it != variant->frame_geometries.constEnd())
                target->setGeometry(geo_it.value());
        }

        if (target->apply_layout(fl))
            ++applied;
    }

    LOG_INFO(kShellTag, QString("apply: %1/%2 frames restored cleanly").arg(applied).arg(workspace.frames.size()));
    return applied;
}

} // namespace fincept::layout
