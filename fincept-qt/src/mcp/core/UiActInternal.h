#pragma once
// UiActInternal.h — PRIVATE to ui_act (UiAct*.cpp). One step of a ui_act call
// goes: parse (UiActTarget) → resolve the element and aim at a point
// (UiActTarget) → refuse what a person could not do and build the closure that
// performs it (UiActEdit for widgets, UiActWeb for web pages) → judge the risk
// and ask the user (UiActRisk) → perform it posted on the event loop, wait for
// it to settle and report what changed (UiActDiff, run in UiAct.cpp).

#include "mcp/core/UiEngine.h"
#include "mcp/core/UiRisk.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QKeySequence>
#include <QPoint>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

class QAbstractButton;
class QAbstractItemView;
class QComboBox;
class QPushButton;

namespace fincept::mcp::core::ui::act {

inline constexpr int kMaxSteps = 12;
inline constexpr int kMinSettleMs = 150;
inline constexpr int kSettleMs = 400;
inline constexpr int kSymbolResolveMs = 4000; // a symbol box resolving what was typed
inline constexpr int kCarryOverMs = 45000;
inline constexpr int kWebLocateMs = 5000;

struct Step {
    QString action;
    QString ref;
    QString name;
    QString role;
    QString target;
    QString text;
    QJsonValue value;
    bool append = false;
    bool submit = false;
    int amount = 0;
    bool has_xy = false;
    int x = 0;
    int y = 0;
    QString column;  // row refs: the cell to act on (header words or a column number)
    QJsonObject raw; // the step as given (wait / read steps pass it on)
};

Step parse_step(const QJsonObject& o, const QString& default_target);
/// Actions that operate a control.
const QStringList& known_actions();
/// Actions a batch may also hold: they observe only (wait, read).
bool is_observe_action(const QString& action);

/// What a step acts on.
struct Target {
    QPointer<QWidget> widget;
    QPointer<QAction> action;
    QPointer<QWidget> dock; // a panel ref
    int sub = -1;
    int row = -1;
    int listed_row = -1;  // the row number the caller used, when the row has moved since it was listed
    int column = -1;      // a cell's model column (from `column`, or a click at x/y on a row)
    bool has_hit = false; // the step aims at a point: `hit_pos`, in the target widget's coordinates
    QPoint hit_pos;
    QString ref;
    QString role;
    QString name;
    // ── Inside a web page (then `widget` is the web view) ──
    QPointer<QWidget> web_view;
    int web = -1;           // control N of the page (a ":w" ref)
    QString web_key;        // that control as it was listed
    bool web_point = false; // a point in the page (x/y)
    bool web_focus = false; // keys for whatever has focus in the page
    bool web_located = false;
    QJsonObject web_info; // what the page script found there (role, name, states, words, …)
};

// ── UiActTarget.cpp ──────────────────────────────────────────────────────────
QString describe_target(const Target& t);
QWidget* host_widget(const Target& t);
void classify_target(Target& t);
/// Find the step's element (ref, words, focus or a point). `*error` carries a code.
std::optional<Target> resolve_step_target(const Step& s, QString* error);
/// A step at x/y: aim at what is really under that point (see the .cpp).
bool aim_at_point(Target& t, const Step& s, QString* error);
/// Before a click / typing on a plain control: scroll it into view inside its
/// scroll areas, and refuse when something covers it (a loading overlay, a
/// panel drawn over it). Empty when it can be operated, else an error text.
QString ensure_operable(QWidget* w, const QString& what);

// ── UiActEdit.cpp ────────────────────────────────────────────────────────────
struct KeyChord {
    int key = 0;
    Qt::KeyboardModifiers mods;
    QString text;
    QKeySequence seq;
};
std::optional<KeyChord> parse_key(const QString& raw);

using Perform = std::function<void()>;
struct Prepared {
    Perform perform;
    QString note;            // what will be reported for the step
    QString expect_text;     // typing into a line edit: what it must hold afterwards ("" = no check)
    QPointer<QWidget> field; // that line edit
    bool symbol_box = false; // the field is a symbol search box: wait for what it resolves
    /// Set by `perform` when the widget refused the change (a table's setData said no).
    std::shared_ptr<QString> failed;
};
/// Validate a step on a native widget and build what performs it.
std::optional<Prepared> prepare(const Step& s, const Target& t, QString* error);

void send_mouse(QWidget* w, QPoint pos, Qt::MouseButton button, bool dbl);
void send_key(QWidget* w, int key, Qt::KeyboardModifiers mods, const QString& text);

/// Option / tab / menu item / radio named by the caller: exact (case-insensitive)
/// first, then a UNIQUE prefix, then a UNIQUE contained match. Several equally
/// good candidates → -1 / nullptr and `*ambiguous` lists them.
int find_option(QComboBox* cb, const QString& wanted, QStringList* ambiguous = nullptr);

// ── UiActWeb.cpp ─────────────────────────────────────────────────────────────
std::optional<Prepared> prepare_web(const Step& s, const Target& t, QString* error);
using Located = std::function<void(std::optional<Target> target, const QString& error)>;
/// Ask the page what the step's element is now.
void locate_web(const Step& s, const Target& t, Located done);

// ── UiActRisk.cpp (the order-path safety lives here) ─────────────────────────
Risk assess(const Step& requested, const Target& t);
QString dialog_context(QWidget* w);
/// The screen's own LIVE / PAPER indicators near `w`: "LIVE", "PAPER", "LIVE PAPER" or "".
QString trading_mode_words(QWidget* w);
QString row_fingerprint(const Target& t);
/// After the user approves a risky step, one answer in the confirmation dialog
/// that step opens may follow without a second approval — never for money.
bool carry_covers(const QString& origin, QWidget* w, const Risk& risk);
void arm_carry(const QString& origin, const Risk& approved);
/// After a step settles: the dialogs it opened (not in `before`) take the step's
/// risk for their accept-role buttons (Yes / OK / Save / Open …).
void adopt_opened_dialogs(const Risk& opener, const QList<QPointer<QWidget>>& before);
/// The risk a button inherits from the step that opened its dialog (accept-role buttons only).
Risk opened_dialog_risk(QWidget* w);

// ── UiActDiff.cpp ────────────────────────────────────────────────────────────
struct ViewCount {
    QPointer<QAbstractItemView> view;
    int rows = 0;
    QString ref;
};
struct UiState {
    QList<QPointer<QWidget>> dialogs;
    QPointer<QWidget> popup;
    QPointer<QWidget> focused;
    QStringList panel_ids;
    QList<ViewCount> views;
    QSet<QString> texts;
    QSet<QString> toasts;
    QString value;
};
QWidget* change_scope(QWidget* w);
UiState capture(QWidget* scope, QWidget* target);
QStringList diff(const UiState& before, const UiState& after, QWidget* target, const QString& target_label);

} // namespace fincept::mcp::core::ui::act
