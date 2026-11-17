#pragma once
// UiRisk.h — what an agent's UI action would commit, judged from words and
// from markers the screens set. UiActRisk.cpp gathers the element being
// operated and asks here.
//
// Risk is decided PER ACTION, at the moment it would run: a click on a button
// that places an order needs the user's approval whether it arrives as "click
// ref", "click name", a click at x/y, Enter / Space / a shortcut / a mnemonic,
// a table cell, a web control, or a step in a batch.
//
// ── Markers (names in src/core/agent/AgentRiskMarkers.h) ─────────────────────
//
//  fincept_agent_risk = <kind>            on a QWidget, a QAction or a QShortcut.
//  Kinds (case-insensitive):
//    order  (alias: trade)  places, changes or cancels an order, closes positions, starts / stops live
//                           trading. Approval every time; NEVER carried over to the app's own
//                           confirmation; one per ui_act call; refused outright in a PAPER run.
//    money                  moves money (withdraw, transfer, deposit, bridge, stake …). As `order`.
//    spend                  spends credits, model quota or a paid API (an AI run, a billed fetch).
//                           Approval; refused in a PAPER run. A timed grant may cover it, only for the
//                           same client + run + kind + screen (mcp/server/ApprovalPolicy.h).
//    delete (alias: destructive)  deletes or resets the user's data. Approval.
//    account                changes account / security state (keys, PIN, sign-in, permissions, tokens,
//                           approval settings, data folder). Approval.
//    code                   runs user or agent code on this computer (run / run all / restart kernel,
//                           notebook cells, "Open with default app"). Approval.
//    file                   writes a file outside the terminal's own data. Approval.
//    none                   no effect (documentation only).
//    anything else          an unknown kind still needs approval (the generic reason).
//  A marker only ever ADDS risk: nothing a screen sets makes a risky control safe.
//
//  fincept_agent_risk_inherit = true      on a marked CONTAINER: its kind also covers every unmarked
//                           control inside it (a billed tile, a ticket). Without it a container's marker
//                           covers only the container itself — markers are per control.
//
//  fincept_agent_risk_keys = "Ctrl+Return=code;Delete=delete;F5=spend"
//                           keys a widget handles in keyPressEvent (invisible to the engine otherwise):
//                           a key press on that widget, or on a focused widget inside it, with one of
//                           these sequences is judged as that kind. Also a QVariantMap {sequence: kind}
//                           or a QStringList of "sequence=kind". Sequences in Qt's portable text.
//                           A marker on a QLineEdit / text edit itself means "submitting this field
//                           (Enter, or the engine's submit:true) is that kind".
//
//  fincept_agent_risk_columns = "3=order;Action=delete"   on an item view: cells of those columns
//                           (logical column number or header text) are that kind for click with a
//                           column, double click, activation (Enter), type / set_value / clear.
//  model role kAgentRiskRole (data(index, kAgentRiskRole) = kind) — the same for one cell.
//
// Keys: Enter on a field judges the window's default button and the field's own marker; any key that is
// a button's own shortcut (any modifiers, e.g. the crypto ticket's Ctrl+Return) judges that button; a key
// that fires a QAction / QShortcut judges its words and its marker (or its parent action / button's).
//
// Dialogs: a message box / input / file dialog that a marked or risky step opens inherits that step's kind
// for its accept-role buttons (Yes, OK, Save, Open …). The step's approval covers that one confirmation —
// except for order and money kinds, whose confirmation the user always approves again.

#include <QString>

class QAbstractItemView;
class QKeySequence;
class QModelIndex;
class QObject;
class QWidget;

namespace fincept::mcp::core::ui {

struct Risk {
    bool risky = false;
    bool money = false; ///< orders, money moves, spends — refused in PAPER runs (grants: ApprovalPolicy.h)
    QString why;        ///< "places, changes or cancels an order, or moves money"
    QString kind;       ///< order, money, spend, destructive, account, code, file, generic ("" when safe)
};

/// The Risk of a marker kind ("order", "spend", "code" …; unknown → generic approval; "none"/"" → none).
Risk risk_for_kind(const QString& kind);

/// Order and money kinds: never carried over to a confirmation, one per call.
bool is_order_kind(const Risk& r);

/// The words of an element (lower case): what clicking / confirming it would do.
Risk classify_words(const QString& words_lower);

/// A control inside a table row: `action_lower` is the control's own words
/// ("cancel", "exit", "modify"), `context_lower` the row's cells, the column
/// header and the table's name. A row action on an order / position / holding
/// table is an order action even when the button only says "Cancel".
Risk classify_row_action(const QString& action_lower, const QString& context_lower);

/// A bare refusal ("Cancel", "No", "Close (Esc)") — never a risk in a dialog.
/// Only the WHOLE name counts ("CANCEL ALL ORDERS" is not a refusal).
bool is_negative_choice(const QString& name);

/// The `fincept_agent_risk` marker of `o` itself, or of an ancestor that sets
/// `fincept_agent_risk_inherit` (up to its window). Risk{} when none.
Risk marker_risk(const QObject* o);

/// The marker on `o` itself only (no ancestors).
Risk own_marker_risk(const QObject* o);

/// `fincept_agent_risk_keys` of `w` or an ancestor (up to its window) for this key sequence.
Risk keys_risk(const QWidget* w, const QKeySequence& seq);

/// A cell's kind: the model's kAgentRiskRole, else the view's `fincept_agent_risk_columns`.
Risk cell_risk(const QAbstractItemView* view, const QModelIndex& index);

/// The stronger of two judgements (money > other risk > none).
Risk stronger(const Risk& a, const Risk& b);

} // namespace fincept::mcp::core::ui
