#pragma once
#include "core/keys/KeyActions.h"

#include <QString>

namespace fincept::actions {

/// Register the built-in actions with `ActionRegistry::instance()`.
///
/// Called exactly once at process startup from `TerminalShell::initialise()`.
/// Subsequent calls are no-ops because the registry's `register_action()` is
/// idempotent on the action id.
///
/// The actions registered here are the canonical replacement for the ~30
/// hard-coded `connect(...)` calls that used to live in MainWindow's
/// constructor (lines 393–543 pre-Phase 4). Each action carries:
///
///   - id: e.g. "window.cycle_forward", "panel.refresh", "terminal.lock_now"
///   - default_hotkey: synced from KeyConfigManager so user-bound keys win
///   - predicate: gate that hides the action in surfaces when its context
///     isn't satisfied (e.g. "requires_more_than_one_monitor")
///   - handler: takes CommandContext and invokes the appropriate API on
///     the focused frame, the shell, or the keyboard policy
///
/// The hotkey binding layer in WindowFrame's constructor walks
/// `ActionRegistry::all_ids()` to install per-frame QActions that route
/// `triggered` signals through `ActionRegistry::invoke(id, ctx)`. The
/// `ctx.focused_frame` is resolved live at invoke time, fixing the
/// pre-Phase 4 latent bug where multi-window setups could route an
/// action to a non-focused frame whose `this` was captured in the
/// original lambda.
void register_builtins();

/// Map a KeyConfigManager `KeyAction` enum value to its canonical
/// `ActionRegistry` action id. Returns an empty string for KeyActions that
/// aren't (yet) modelled as ActionRegistry actions — the per-screen ones
/// (NavNext, NewsOpen, RunCell, etc.) which still belong to specific
/// screens, not the global registry.
///
/// WindowFrame's keyboard-binding loop uses this to wire each
/// KeyConfigManager-owned QAction's `triggered` signal to the corresponding
/// registry handler, preserving live re-binding when the user changes a
/// hotkey in Settings.
QString action_id_for(KeyAction a);

} // namespace fincept::actions
