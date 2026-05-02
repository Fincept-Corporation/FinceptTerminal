#pragma once
#include "core/result/Result.h"

#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <functional>

namespace fincept {

class TerminalShell;
class WindowFrame;
namespace ui { class PanelHandle; }

/// Context passed to every action handler. Phase-7 (command bar) and Phase-1
/// (shell) both want to invoke actions; the context carries everything an
/// action needs to make policy decisions without the registry knowing about
/// the concrete shell or frame types.
///
/// `focused_frame` and `focused_panel` are nullable — actions that require
/// either should declare it via their predicate (see `ActionDef::predicate`)
/// so surfaces (palette, command bar) grey out the action when the context
/// is wrong, rather than the handler defending itself with null checks.
struct CommandContext {
    TerminalShell* shell = nullptr;          ///< Always set when invoked through the registry.
    WindowFrame* focused_frame = nullptr;    ///< nullptr if focus is on no frame (e.g. Launchpad).
    ui::PanelHandle* focused_panel = nullptr;///< nullptr if focused frame has no panels.
    QVariantMap args;                        ///< Per-action parameter bag, keyed by ParameterSlot::name.
};

/// One declared parameter for a parameterised action.
///
/// The command bar's parser uses `name` + `type` to decide what to ask for
/// when the user hasn't supplied an argument inline. The optional
/// `suggestion_source` is a short tag the suggestion index recognises
/// ("symbol", "layout_name", "panel_id", etc.) and translates into the right
/// completion list.
struct ParameterSlot {
    QString name;                  ///< Used as key in CommandContext::args.
    QString display;               ///< Prompt shown in guided picker.
    QString type;                  ///< "string", "int", "symbol", "layout_id", ...
    bool required = true;
    QString suggestion_source;     ///< Empty = no suggestions (free-form input).
    QVariant default_value;
};

/// Predicate evaluated before showing / invoking an action.
///
/// Returning false hides the action from palette + greys it out in menus and
/// makes invocation a no-op (with a debug log). Predicates are pure — they
/// must not mutate state. Typical bodies:
///
///   [](const CommandContext& ctx) { return ctx.focused_frame != nullptr; }
///   [](const CommandContext& ctx) { return QGuiApplication::screens().size() > 1; }
///
/// Empty (default-constructed) predicate means "always available".
using ActionPredicate = std::function<bool(const CommandContext&)>;

/// Handler invoked when the action fires. Returns Result<void> so callers
/// can surface errors via the standard project pipeline (LOG_ERROR + toast).
/// Empty handler = no-op (used for placeholder/registered-but-not-yet-wired
/// actions during phased rollout).
using ActionHandler = std::function<Result<void>(const CommandContext&)>;

/// One entry in the ActionRegistry. Designed to be aggregate-initialised:
///
///   ActionRegistry::instance().register_action(ActionDef{
///       .id = "window.cycle_forward",
///       .display = "Cycle Windows Forward",
///       .category = "Window",
///       .aliases = {"cycle windows", "next window"},
///       .default_hotkey = QKeySequence("Ctrl+Alt+Tab"),
///       .predicate = [](const CommandContext& c) { return /* >1 frame */; },
///       .handler = &handle_cycle_forward,
///   });
struct ActionDef {
    QString id;                        ///< Stable identifier ("window.cycle_forward"). Persistence + bus key.
    QString display;                   ///< Human-readable label for menus / palette / help.
    QString category;                  ///< Top-level grouping ("Window", "Panel", "Layout", ...).
    QStringList aliases;               ///< Extra search/command-bar tokens.
    QKeySequence default_hotkey;       ///< Empty = no default hotkey. User can rebind.
    ActionPredicate predicate;         ///< Empty = always available.
    ActionHandler handler;             ///< Empty = no-op (placeholder).
    QList<ParameterSlot> parameter_slots;
};

} // namespace fincept
