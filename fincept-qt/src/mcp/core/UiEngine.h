#pragma once
// UiEngine.h — PRIVATE header of the core UI tools (src/mcp/core/Ui*.cpp).
//
// The engine sees the terminal the way a person does: top-level windows
// (WindowFrame), the panels docked in them (ads::CDockWidget, one per screen),
// dialogs and popup menus, and inside each the ordinary Qt widgets. It turns
// those into compact "elements" (role, name, value, states) with short refs
// (w1 window, p3 panel, d1 dialog, m1 menu, e12 element, e12:r5 a row, e12:w3 a
// control on the web page e12 shows), and
// operates them by ref or by the words on them.
//
// GUI THREAD ONLY. Every function here touches widgets. The tool handlers in
// CoreTools.cpp post onto the GUI thread before calling in, and every entry
// point answers through a Reply that runs on the GUI thread.

#include "mcp/McpTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>

class QAction;
class QAbstractItemView;
class QMenu;

namespace ads {
class CDockWidget;
}

namespace fincept {
class WindowFrame;
}

namespace fincept::mcp::core::ui {

/// Answer for one tool call. Called exactly once, on the GUI thread.
using Reply = std::function<void(ToolResult)>;
/// True once the caller gave up (job cancelled / tool watchdog).
using CancelCheck = std::function<bool()>;

// ── Entry points (CoreTools.cpp dispatches here) ─────────────────────────────
void ui_overview(const QJsonObject& args, Reply reply);
void ui_open(const QJsonObject& args, Reply reply);
void ui_snapshot(const QJsonObject& args, Reply reply);
/// `cancelled`: stop before the next step / after an approval once the caller gave up.
void ui_act(const QJsonObject& args, const QString& origin, CancelCheck cancelled, Reply reply);
/// For callers with no cancellation (self-tests).
inline void ui_act(const QJsonObject& args, const QString& origin, Reply reply) {
    ui_act(args, origin, CancelCheck{}, std::move(reply));
}
void ui_read(const QJsonObject& args, Reply reply);
void ui_wait(const QJsonObject& args, CancelCheck cancelled, Reply reply);
void ui_screenshot(const QJsonObject& args, Reply reply);
void ui_workspace(const QJsonObject& args, const QString& origin, Reply reply);
void ui_invoke(const QJsonObject& args, const QString& origin, Reply reply);

// ═════════════════════════════════════════════════════════════════════════════
// Model (UiModel.cpp)
// ═════════════════════════════════════════════════════════════════════════════

enum class RefKind : char { Window = 'w', Panel = 'p', Dialog = 'd', Menu = 'm', Element = 'e' };

/// A ref resolved back to live objects.
struct Resolved {
    QString ref; ///< canonical ("e12", "e12:r5")
    RefKind kind = RefKind::Element;
    QPointer<QWidget> widget;        ///< the element / panel content / dialog / window
    QPointer<QAction> action;        ///< menu and toolbar items
    QPointer<ads::CDockWidget> dock; ///< for panels
    int sub = -1;                    ///< tab index for tab refs
    int row = -1;                    ///< ":r<N>" suffix — a row of a table / list / tree (where it is NOW)
    int listed_row = -1;             ///< the row number the caller used, when the row has moved since
    int web = -1;                    ///< ":w<N>" suffix — control N of the web page the element shows
    QString web_key;                 ///< that control as it was listed ("button|Indicators ▾")
};

/// Ref registry. Stable: the same object keeps the same ref for as long as it
/// lives, so refs from an earlier snapshot keep working.
class Refs {
  public:
    static Refs& instance();
    QString ref_for(QObject* obj, RefKind kind, int sub = -1);
    /// nullopt when the ref is unknown or its object is gone; `*why` (optional)
    /// then says which — and, for a row ref, that the row left the list.
    std::optional<Resolved> resolve(const QString& ref, QString* why = nullptr);
    /// True when this ref (without any ":r" suffix) was ever handed out.
    bool was_issued(const QString& ref) const;

    /// Remember which row "<base_ref>:r<row>" showed when it was listed, so the
    /// ref keeps meaning THAT row after the view re-sorts, inserts above it or
    /// rebuilds its model: resolve() follows it, or fails when it is gone —
    /// it never silently lands on whatever row now has that number.
    void anchor_row(QAbstractItemView* view, const QString& base_ref, int row, const QModelIndex& first);
    /// Remember web control "<base_ref>:w<index>" as listed (its web_control_key),
    /// so acting on it later finds that control again or says it is gone.
    void anchor_web(const QString& base_ref, int index, const QString& key);

  private:
    struct Entry {
        QPointer<QObject> obj;
        RefKind kind = RefKind::Element;
        int sub = -1;
    };
    struct RowAnchor {
        QPersistentModelIndex idx;
        QString key; // the row's identifying text (see row_key)
    };
    void sweep();
    QHash<QString, QHash<int, RowAnchor>> anchors_;   // base ref → row → anchor
    QHash<QString, QHash<int, QString>> web_anchors_; // base ref → web control index → key
    QHash<QString, Entry> by_ref_;
    QHash<QString, QString> by_key_; // "<kind><ptr>:<sub>" → ref
    QHash<char, quint64> next_;
    int allocations_since_sweep_ = 0;
};

enum class Detail { Brief, Normal, Full };

/// One element as the model sees it.
struct Node {
    QPointer<QWidget> widget;
    QPointer<QAction> action;
    int sub = -1;
    QString ref;  // empty for plain text
    QString role; // button, textbox, combobox, checkbox, radio, tab, table, list, tree, text, heading,
                  // link, menuitem, spinbutton, slider, date, progressbar, chart, webview, group, clickable, image
    QString name;
    QString value;
    QStringList states;
    int depth = 0;
    bool interactive = false;
    // tables / lists / trees
    int rows = -1;
    int cols = -1;
    QStringList headers;
    QStringList row_lines;                                // preview rows already formatted ("r0: a | b | c")
    QStringList matched_rows;                             // query mode: rows holding the query words (first few)
    int matched_total = 0;                                // query mode: every matching row found
    bool scan_capped = false;                             // query mode: the row scan stopped before the end
    bool rows_capped = false;                             // `rows` is a lower bound (a huge tree)
    QList<QPair<int, QPersistentModelIndex>> row_anchors; // rows shown in row_lines / matched_rows
    QPointer<QWidget> label_widget;                       // the QLabel this element took its name from
};

/// Walk options.
struct WalkOptions {
    Detail detail = Detail::Normal;
    int max_nodes = 250;
    QStringList query;      // lower-case words; empty = everything
    QSet<QWidget*> exclude; // subtrees not to enter (e.g. a window's dock area)
    int base_depth = 0;     // indentation added to every node
    /// Also list the controls inside table / list cells (buttons placed in rows),
    /// named "<name> · r<row>". On for name search and query snapshots.
    bool cell_widgets = false;
};

/// Elements of `root` in reading order. Sets `*truncated` when max_nodes cut it.
QList<Node> collect_nodes(QWidget* root, const WalkOptions& opt, bool* truncated);

/// One element line ("e5 button \"Refresh\" [disabled]") plus its table rows.
QStringList format_node(const Node& n, Detail detail);

/// Classify a single widget (nullopt when it is not an element on its own).
std::optional<Node> describe_widget(QWidget* w, Detail detail);

/// Visible text a widget shows (label text, button text, field value unless
/// secret, …). Used by text search and waits (long documents capped at 20k).
QString widget_text(QWidget* w);
/// Like widget_text, but a document's whole text — for ui_read's pages.
QString widget_full_text(QWidget* w);

/// Password-echo inputs and widgets tagged `fincept_secret`, or inside one.
bool is_secret(const QWidget* w);

/// Never operated or listed: the approval dialog (an agent must not be able to
/// approve its own request) and the dock framework's internals.
bool is_hidden_from_agents(const QWidget* w);

// ── Windows, panels, dialogs ────────────────────────────────────────────────

QList<WindowFrame*> frames();
WindowFrame* active_frame();
WindowFrame* frame_of(QWidget* w);

struct PanelInfo {
    QPointer<ads::CDockWidget> dock;
    QString screen_id; ///< dock objectName ("markets", "news#dup2")
    QString title;
    bool visible = false; ///< false = a background tab
    bool focused = false;
};
QList<PanelInfo> panels_of(WindowFrame* frame);
ads::CDockWidget* dock_of(QWidget* w);
/// The panel the user (or agent) is in: focus widget's dock, else the router's current.
ads::CDockWidget* focused_dock(WindowFrame* frame);
/// The screen a panel hosts (the dock may wrap it, e.g. with a link-group badge).
QWidget* screen_widget_of(ads::CDockWidget* dock);

/// Visible dialogs (QDialog / message boxes / tool windows), topmost first;
/// excludes the approval dialog.
QList<QWidget*> open_dialogs();
/// The modal dialog that blocks input elsewhere, if any.
QWidget* blocking_dialog();
/// The open popup menu / list, if any.
QWidget* open_popup();

QString window_title_of(QWidget* top);

/// Resolve a `target` argument: "" / "window" (active window), "dialog", a ref,
/// a screen id or title. `*label` describes it ("p3 Markets"). On failure
/// returns nullptr and sets `*error`.
QWidget* resolve_scope(const QString& target, QString* label, QString* error);

/// Screen id / title / alias → the open panel's dock widget (active window
/// first). nullptr when that screen is not open.
ads::CDockWidget* find_open_panel(const QString& screen);

/// Describe where a widget lives: "p3 Markets" / "d1 Confirm order" / "w1".
QString location_label(QWidget* w);

// ── Element search by words ─────────────────────────────────────────────────

struct NameMatch {
    QPointer<QWidget> widget;
    QPointer<QAction> action;
    int sub = -1;
    int row = -1;
    int score = 0; // higher is better
    QString ref;
    QString role;
    QString name;
};
/// Interactive elements of `scope` whose words match `name` (exact > prefix >
/// contains), optionally restricted to `role`. Best first.
QList<NameMatch> find_by_name(QWidget* scope, const QString& name, const QString& role);

// ── Busy state ──────────────────────────────────────────────────────────────

/// "Still loading?" as a person would judge it: a loading overlay, a busy bar or
/// a "Loading…" label showing in the scope, or a Python script still running.
/// (DataHub's app-wide in-flight count is deliberately not used: background
/// feeds refresh constantly, so it never reads idle.)
struct BusyState {
    int python = 0;     ///< running + queued scripts
    int indicators = 0; ///< visible loading overlays / busy bars / "Loading…" labels in scope
    bool busy() const { return python > 0 || indicators > 0; }
    QString describe() const;
};
BusyState busy_state(QWidget* scope);

// ── Item views ──────────────────────────────────────────────────────────────

/// Rows past this are not counted in a tree (the count is then a lower bound).
inline constexpr int kMaxTreeRows = 200000;
/// Rows a text search scans before it stops and says so.
inline constexpr int kMaxRowScan = 50000;
/// Columns shown per row unless the caller asks for others (columns / col_offset).
inline constexpr int kPreviewCols = 8;

/// Visible columns of a view, as model column numbers (all of them — callers
/// that show fewer say so).
QList<int> visible_columns(QAbstractItemView* view);
/// Header texts for those columns.
QStringList column_headers(QAbstractItemView* view, const QList<int>& cols);
/// Number of rows the reader sees: rows hidden by a filter are not counted;
/// trees count expanded items. `*capped` is set when a tree passed kMaxTreeRows.
int view_row_count(QAbstractItemView* view, bool* capped = nullptr);
/// Rows hidden by a filter (tables / lists; 0 for trees).
int view_hidden_rows(QAbstractItemView* view);
/// Visit rows in reading order from row number `from`, ONE pass (trees walk the
/// expanded items once — never index-by-number in a loop, which is quadratic).
/// Hidden rows are skipped; row numbers stay the model's (trees: flattened
/// position). `fn(row, first_cell)` returns false to stop. Returns rows visited.
int for_each_view_row(QAbstractItemView* view, int from,
                      const std::function<bool(int row, const QModelIndex& first)>& fn);
/// Index of flattened row `row`, column `col` (trees walk expanded items).
QModelIndex view_index(QAbstractItemView* view, int row, int col);
/// The row number an index sits at now, or -1 (hidden / not in this view).
int row_of_index(QAbstractItemView* view, const QModelIndex& idx);
/// "a | b | c" for one row, cells clipped to `cell_chars`. With `cell_refs`,
/// controls placed in cells are shown with their refs: [e45 button "Cancel"].
QString index_row_text(QAbstractItemView* view, const QModelIndex& first, const QList<int>& cols, int cell_chars,
                       bool cell_refs = false);
QString view_row_text(QAbstractItemView* view, int row, const QList<int>& cols, int cell_chars);
/// What identifies a row to a person (its name / symbol cells, not its
/// ticking numbers) — used to recognise the row again after the view changes.
QString row_key(QAbstractItemView* view, const QModelIndex& first);
/// A cell's control text: its buttons / labels, with refs when `with_refs`.
QString cell_widget_text(QWidget* cell, bool with_refs);
/// When `w` lives in a table / list cell (an index widget or inside one): the
/// view and the cell's index. nullptr otherwise.
QAbstractItemView* cell_widget_owner(QWidget* w, QModelIndex* idx);
/// "sorted by Price ↓" when the view shows a sort indicator, else empty.
QString view_sort_state(QAbstractItemView* view);
/// First row whose text holds every word of `text`, from `from`; -1 if none.
/// Scans at most kMaxRowScan rows; `*scan_capped` says it stopped early.
int view_find_row(QAbstractItemView* view, const QString& text, int from = 0, bool* scan_capped = nullptr);

// ── Web pages (QWebEngineView) ──────────────────────────────────────────────
//
// A web view's content is HTML in another process, not widgets: it is read and
// located by page scripts, run asynchronously. Controls on a page get refs
// "e12:w3" (control 3 of the page element e12) when ui_read lists them.

/// The web view `w` is, hosts (a KLineChart wraps one) or sits inside (its
/// render child). nullptr when there is none, or the build has no WebEngine.
QWidget* web_view_of(QWidget* w);
/// The widget that takes a web view's mouse and key input (its render child).
QWidget* web_input_widget(QWidget* view);
/// CSS pixels → the view's pixels (the page zoom).
double web_zoom(QWidget* view);
/// Run `script` (an expression that returns a JSON string) in the page and parse
/// its answer. `page_world`: the page's own JavaScript world, to reach its
/// globals; otherwise an isolated world that sees only the DOM. `done` runs once
/// on the GUI thread: the object, or `error` set (page gone, no answer in time).
void run_web_script(QWidget* view, const QString& script, bool page_world, int timeout_ms,
                    std::function<void(const QJsonObject& result, const QString& error)> done);
/// Script: the page's title, url, size, scroll, visible text and its controls
/// ({i, r role, n name, v value, s states, x, y, w, h — CSS px, centre}).
QString web_read_script();
/// Script: the page's visible text only ({text}) — cheap enough to poll.
QString web_text_script();
/// Script: find one control — {"mode":"control","i":N,"k":key}, the element at a
/// point {"mode":"point","x":,"y":} (CSS px) or the focused one {"mode":"focus"}
/// — scroll it into view (`reveal`), and optionally act on it through the page
/// (`op`: "click", "focus", "select" with `text`, "scroll" with `dy`). Answers its
/// role, name, value, states, centre, `words` (for risk checks), `secret`,
/// `editable`, `covered`, `options` of a select, or `error`.
QString web_target_script(const QJsonObject& spec);
/// Script (page world): a KLineChart's data — count, range, last candle,
/// indicators with their latest values and `limit` candles from `offset`
/// (offset < 0: the newest). Empty answer when the page has no such chart.
QString web_chart_script(int offset, int limit);
/// "role|name" — what recognises a listed web control again.
QString web_control_key(const QJsonObject& control);

// ── Small utilities ─────────────────────────────────────────────────────────

QString plain_text(const QString& maybe_rich);
QStringList query_words(const QString& query);
bool words_match(const QString& haystack_lower, const QStringList& words);
QString humanize_object_name(const QString& object_name);
int clamp_int(const QJsonValue& v, int def, int lo, int hi);

} // namespace fincept::mcp::core::ui
