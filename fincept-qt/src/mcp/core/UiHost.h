#pragma once
// UiHost.h — the seam between the UI engine (Ui*.cpp) and the terminal around it.
//
// The engine works on plain Qt widgets. What only the terminal knows — which
// top-level widgets are terminal windows, which widgets are panels (dock
// widgets) and what screen they host, whether the terminal is PIN-locked, how
// many Python scripts are running — is asked through these functions. The
// terminal implements them in UiHostTerminal.cpp (WindowFrame, the dock
// manager, DockScreenRouter, AuthManager …); the unit tests implement them over
// ordinary widgets (tests/stubs/mcp_ui_host_fake.cpp), so the whole engine is
// testable without the application.
//
// A "frame" is a terminal window (a QWidget); a "panel" is a docked screen (a
// QWidget — an ads::CDockWidget in the terminal). GUI THREAD ONLY.

#include <QList>
#include <QString>

#include <functional>

class QObject;
class QWidget;

namespace fincept::mcp::core::ui::host {

// ── Windows ─────────────────────────────────────────────────────────────────
/// The terminal's windows, in the order the terminal keeps them.
QList<QWidget*> frames();
/// True for a terminal window.
bool is_frame(const QObject* o);
/// The widget holding a window's panels (left out of the window's chrome), or nullptr.
QWidget* panel_area(QWidget* frame);
/// The window's id (ui_workspace's `window`), or -1.
int window_id(QWidget* frame);
/// The window shows its PIN / lock screen.
bool frame_locked(QWidget* frame);

// ── Panels ──────────────────────────────────────────────────────────────────
/// A window's open panels, in the order the window keeps them.
QList<QWidget*> panels(QWidget* frame);
/// True for a panel (a dock widget).
bool is_panel(const QObject* o);
/// The panel was closed (it may still exist, hidden).
bool panel_closed(QWidget* panel);
/// Screen id ("markets", "news#dup2").
QString panel_id(QWidget* panel);
/// What its tab says (falls back to the screen's title).
QString panel_title(QWidget* panel);
/// The panel's content widget (what its tab shows).
QWidget* panel_content(QWidget* panel);
/// The screen the panel hosts (the panel may wrap it); panel_content() when unknown.
QWidget* panel_screen(QWidget* panel);
/// The panel the window's router calls current, or nullptr.
QWidget* current_panel(QWidget* frame);
/// Bring the panel to the front of its area and focus its content.
void focus_panel(QWidget* panel);
/// Close the panel (its tab), as its close button does.
void close_panel(QWidget* panel);
/// A screen id's display title ("equity_research" → "Equity Research").
QString title_for_id(const QString& screen_id);

/// The instrument a symbol-linked panel shows (empty when none / not linked).
struct SymbolInfo {
    QString symbol;
    QString asset_class;
    QString exchange;
};
SymbolInfo panel_symbol(QWidget* panel);

// ── Terminal state ──────────────────────────────────────────────────────────
struct TerminalStatus {
    QString version;
    bool signed_in = false;
    QString plan;   ///< the signed-in plan's label
    QString layout; ///< the current layout's name, "" when unsaved
    bool locked = false;
};
TerminalStatus terminal_status();
/// The terminal is PIN-locked: nothing may be read or operated.
bool terminal_locked();
/// Python scripts running or queued.
int python_pending();

// ── Widgets with their own completion signal ────────────────────────────────
/// When `field` is the text field of a symbol search box (the terminal's
/// ui::SymbolSearchBox): call `done` once with what the box resolved — "AAPL ·
/// Apple Inc · NASDAQ" — or "unresolved: <its message>", and return true.
/// `done` is never called after `timeout_ms`; the caller keeps its own deadline.
/// Returns false (and never calls `done`) for any other widget.
bool watch_symbol_box(QWidget* field, int timeout_ms, std::function<void(const QString& outcome)> done);

} // namespace fincept::mcp::core::ui::host
