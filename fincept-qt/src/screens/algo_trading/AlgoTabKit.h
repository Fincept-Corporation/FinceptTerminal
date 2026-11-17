// src/screens/algo_trading/AlgoTabKit.h
//
// Shared furniture of the Algo Trading tab's six sub-tabs (namespace fincept::screens::algo_kit):
// the per-sub-tab command bar (title, the six sub-tab segments, the deployment badge) and status
// bar, panels with a ui::PanelHeader, input strips (ui::FlowLayout), field groups, chips, notes,
// tones, dense model/view tables and splitters — the Backtesting / Markets look.
//
// LAYOUT OF A SUB-TAB:  make_tab_bar()  →  input strip(s) / KPI ribbon  →  panels  →  make_status_bar()
//
// STYLING CONTRACT. Nothing built with this kit calls setStyleSheet: AlgoTradingScreen sets ONE root
// stylesheet (root_stylesheet(), built from theme tokens, before any panel exists) that styles these
// object names and dynamic properties (the central re-style pass keeps it in step with the theme):
//   QFrame#algoPanel            a panel (BG_SURFACE, 1 px BORDER_DIM); body #algoPanelBody
//   QWidget#algoStrip           an input strip band (BG_SURFACE, hairline below)
//   QWidget#algoRibbon          a KPI ribbon band (BG_BASE, hairline below)
//   QLabel#algoFieldLabel       a field caption (dim, bold, small); [error="true"] danger
//   QLabel#algoNote             small secondary text      QLabel#algoSentence  a rule in plain language
//   QLabel#algoChip             a framed chip (state / count / badge)
//   QLabel#algoValue            a value in a key / value line (primary, data font)
//   QPushButton#algoToolButton  a small outlined button; [active="true"] accent
//   QPushButton#algoSeg         a segment of make_segmented() (checkable; checked = accent fill)
//   QPushButton#algoPrimary     accent fill (TEXT_ON_ACCENT); [tone="danger"] danger outline
//   QPushButton#algoTemplate    a one-click template chip
//   QLineEdit / QComboBox / QAbstractSpinBox / QDateEdit #algoField   compact inputs
//   QTableView#algoTable (+ header), QTabBar#algoTabs, QSplitter#algoSplitter, QScrollArea#algoScroll
//   QFrame#algoRuleRow [invalid="true"] a rule row with a problem (danger tint); #algoRuleGroup head
//   QProgressBar#algoProgress (thin accent bar)   QLabel#algoHero (big number)   QLabel#algoKey (dim key)
//   QPlainTextEdit / QTextEdit / QTextBrowser #algoLog (monospace log pane)   QListView / QListWidget #algoList
//   QCheckBox#algoCheck   QFrame#algoHairline (1 px rule)   QWidget#algoDetail (detail pane background)
//   property tone = pos | neg | warn | dim | accent | ok | danger | info  — QLabel / QPushButton text
//   colour (on #algoChip also the border). pos / neg are for P&L / return DIRECTION only (always with a
//   sign or an arrow as well); ok / danger / warn / info for state.
// Charts, ui::StatTile / KpiStrip and the shared bars paint from the live theme and need nothing.
//
// PERFORMANCE CONTRACT. Tables are model/view (ui::LiveTableModel + ui::KeySortProxy, sort role
// ui::LiveTableModel<…>::SortRole); widths come from set_column_chars() once (never
// resizeColumnsToContents on a fill); row heights / widths are re-fitted on
// ThemeManager::appearance_settled, never in theme_changed.
#pragma once

#include <QColor>
#include <QFrame>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <initializer_list>

class QLabel;
class QPushButton;
class QSplitter;
class QTableView;
class QVBoxLayout;

namespace fincept::ui {
class FlowLayout;
class PanelHeader;
class ScreenCommandBar;
class ScreenStatusBar;
} // namespace fincept::ui

namespace fincept::screens::algo_kit {

// ── Sub-tabs ────────────────────────────────────────────────────────────────
enum Tab { TabBuilder = 0, TabStrategies, TabScanner, TabAlerts, TabDashboard, TabUniverse, TabCount };

/// Segment key of a sub-tab ("builder" …) and back (-1 for an unknown key).
QString tab_key(int tab);
int tab_from_key(const QString& key);

/// The sub-tab's command bar (object name "algoTabBar"): accent title "ALGO", the six sub-tab
/// segments (Alt+1 … Alt+6; `active` checked) at priority 95, and the deployment badge
/// (QLabel#algoDeployBadge, kept by AlgoTradingScreen) in the right area. The panel adds its own
/// actions (Primary role for its main action, key hints) and a overflow text. AlgoTradingScreen
/// connects segment_selected("tab", key) when it adopts the panel.
ui::ScreenCommandBar* make_tab_bar(QWidget* panel, Tab active);
/// Segment labels / tooltips again (LanguageChange).
void retranslate_tab_bar(ui::ScreenCommandBar* bar);

/// The sub-tab's status bar (object name "algoStatusBar") with the items every sub-tab fills:
///   "engine"  Left, accent    "ENGINE  NATIVE C++"            (preset)
///   "data"    Left            data source ("Yahoo Finance · delayed", "Zerodha")
///   "info"    Centre, dim     what is on screen ("12 strategies · 3 deployed")
///   "asof"    Right, dim      "as of 14:02"
///   "state"   Right, chip     READY / RUNNING / QUEUED / FAILED (set_state())
///   "deploy"  Right, chip     "2 RUNNING · 1 LIVE" (kept by AlgoTradingScreen)
/// plus the clock (started / stopped by the panel's show / hide events).
ui::ScreenStatusBar* make_status_bar(QWidget* panel);
enum class RunState { Ready, Running, Queued, Failed, Cancelled, Idle };
/// The "state" chip: text + tone (Ready ok, Running accent, Queued info, Failed danger …).
void set_state(ui::ScreenStatusBar* bar, RunState state, const QString& text = {});

/// The screen's one root stylesheet (see the contract above).
QString root_stylesheet();

// ── Text ────────────────────────────────────────────────────────────────────
/// The shared missing-value placeholder ("—").
QString dash();

// ── Tones ───────────────────────────────────────────────────────────────────
enum class Tone { None, Pos, Neg, Warn, Dim, Accent, Ok, Danger, Info };
const char* tone_key(Tone tone);
/// Pos for > 0, Neg for < 0, None otherwise (direction only).
Tone sign_tone(double v);
/// Set the "tone" property and re-polish only when it changed.
void set_tone(QWidget* w, Tone tone);
/// The theme colour of a tone (item-model ForegroundRole, painting), read now.
QColor tone_color(Tone tone);

// ── Panels and strips ───────────────────────────────────────────────────────
struct Panel {
    QFrame* frame = nullptr;          ///< #algoPanel
    ui::PanelHeader* header = nullptr;
    QWidget* body_widget = nullptr;   ///< #algoPanelBody
    QVBoxLayout* body = nullptr;      ///< margins `body_margin`, spacing 6
};
/// A panel: header strip + body. Controls go in panel.header->add_control(w).
Panel make_panel(const QString& title, QWidget* parent, int body_margin = 6);

/// An input strip band (#algoStrip) with a wrapping ui::FlowLayout (margins 8/4, spacing from the font).
QWidget* make_strip(QWidget* parent, ui::FlowLayout** flow_out = nullptr);
/// A caption (#algoFieldLabel) + widgets in one row, kept together when the strip wraps. The
/// widgets are re-parented into the group; `tooltip` goes on the caption.
QWidget* field_group(QWidget* parent, const QString& caption, std::initializer_list<QWidget*> widgets,
                     const QString& tooltip = {});
/// Object name #algoField + accessible name (+ tooltip) on an input.
void mark_field(QWidget* input, const QString& accessible_name, const QString& tooltip = {});

QLabel* make_note(const QString& text, QWidget* parent, Tone tone = Tone::Dim);
QLabel* make_chip(const QString& text, Tone tone, QWidget* parent);
/// A small outlined button (#algoToolButton) with an accessible name and tooltip.
QPushButton* make_tool_button(const QString& text, const QString& tooltip, QWidget* parent);
/// The accent-filled action button inside a panel (#algoPrimary).
QPushButton* make_primary_button(const QString& text, const QString& tooltip, QWidget* parent);
/// Checkable #algoSeg buttons, exactly one checked; `on_change(index)` on a user click. The
/// container's buttons are its direct children in order.
QWidget* make_segmented(const QStringList& labels, int checked, std::function<void(int)> on_change, QWidget* parent);
/// Check segment `index` (no on_change); index < 0 = none checked.
void set_segment(QWidget* segmented, int index);

/// A splitter (#algoSplitter: 1 px hairline handle, children not collapsible).
QSplitter* make_splitter(Qt::Orientation orientation, QWidget* parent);

// ── Tables ──────────────────────────────────────────────────────────────────
/// The dense table: #algoTable, row height from the font, no vertical header, alternating rows,
/// whole-row single selection, interactive header with the last section stretched, sorting on,
/// the copy / PNG / CSV context menu (ChartExport) under `export_name`, Ctrl+C copies the selected
/// rows. Row height and the column widths of set_column_chars() follow the font on
/// appearance_settled.
void setup_table(QTableView* view, const QString& export_name);
/// Column widths from average-character counts (0 = leave); set once, the user may resize.
void set_column_chars(QTableView* view, const QVector<int>& chars);

/// Unfocused spin boxes / combos inside `scroll_owner` ignore the mouse wheel (it scrolls the
/// view instead) — no value changes by accident while scrolling a form.
void guard_wheel(QWidget* input);

} // namespace fincept::screens::algo_kit
