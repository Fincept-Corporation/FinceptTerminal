#pragma once
#include "core/layout/LayoutTypes.h"

namespace fincept::layout {

/// Phase 6 final: shell-level Workspace capture/apply helpers.
///
/// These walk `WindowRegistry` (every live `WindowFrame`) and either
/// gather their `FrameLayout`s into a `Workspace`, or take a saved
/// `Workspace` and apply each of its FrameLayouts to existing or
/// freshly-spawned frames.
///
/// `capture` also captures the current monitor topology and stores
/// per-frame geometry into a `WorkspaceVariant`, so the workspace
/// becomes self-contained for the desk-vs-laptop variant matcher
/// (decision 4.5).
///
/// `apply` consults `WorkspaceMatcher::match` to pick the variant that
/// best fits the current monitor topology, then applies frame
/// geometries from that variant after each frame's `apply_layout`
/// finishes restoring its dock contents.
class WorkspaceShell {
  public:
    /// Capture the current shell state into a `Workspace`. The returned
    /// Workspace gets a fresh UUID + updated_at timestamp and a
    /// `WorkspaceVariant` for the current monitor topology.
    ///
    /// `name` and `kind` populate the Workspace metadata. If `previous`
    /// is provided, its UUID + variants list are preserved (so adding
    /// a new variant for a different monitor topology doesn't lose the
    /// existing variants).
    static Workspace capture(const QString& name, const QString& kind = "user",
                             const Workspace* previous = nullptr);

    /// Apply a saved Workspace to the current shell. Walks the workspace's
    /// frames, reuses existing frames where window UUIDs match, spawns
    /// new frames otherwise. Each frame's apply_layout restores dock
    /// contents. The matched WorkspaceVariant (via WorkspaceMatcher)
    /// supplies frame geometries.
    ///
    /// Returns the number of frames successfully applied.
    static int apply(const Workspace& workspace);
};

} // namespace fincept::layout
