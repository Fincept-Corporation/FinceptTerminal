// src/screens/backtesting/BtTabKit.h
//
// Shared furniture of the Backtesting tab's sub-tabs (namespace fincept::screens::bt):
// number text with the "—" placeholder, tones, KPI-tile helpers, panels with a ui::PanelHeader,
// a key / value grid, table and chart set-up, a segmented switch, and the cross-highlight bus
// (BtSelection) shared by the run views.
//
// STYLING CONTRACT. Nothing here (and nothing in a sub-tab) calls setStyleSheet: the Backtesting
// screen sets ONE root stylesheet, built from theme tokens, that styles these object names and
// dynamic properties:
//   QFrame#btPanel            a panel (BG_SURFACE, 1 px BORDER_DIM); its body widget is #btPanelBody
//   QLabel#btKvKey / #btKvValue   key (dim, small) / value (primary, data font) of a BtKvGrid row
//   QLabel#btNote             small secondary text (word-wrapped by the caller when needed)
//   QLabel#btChip             a framed chip (state / count / badge)
//   QLabel#btHero             a large value (hero number)
//   QLabel#btSectionTitle     an upper-case dim caption inside a panel
//   QPushButton#btToolButton  a small outlined button; property active=true -> accent look
//   QPushButton#btSeg         a segment of make_segmented() (checkable; checked = accent)
//   QLineEdit#btFilter, QComboBox#btCombo, QTableView#btTable, QScrollArea#btScroll
//   property tone = pos | neg | warn | dim | accent | ok | danger  (QLabel / QPushButton text colour;
//   on #btChip also the border). pos / neg are for P&L / return DIRECTION only (always with a sign
//   or a word as well); ok / danger / warn for state.
// Charts read their colours from the theme at paint time and need nothing.
//
// PERFORMANCE CONTRACT. show_run() of a view runs on the GUI thread and must stay under 100 ms for
// a 20,000-trade / 20,000-bar run: model/view tables (ui::LiveTableModel + a sort proxy), no
// resizeColumnsToContents, series thinned to <= 5,000 points for display (to_points()).
#pragma once

#include "services/backtesting/BacktestResultModel.h"
#include "ui/widgets/StatTile.h"

#include <QFrame>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QAbstractItemModel;
class QGridLayout;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;
class QVBoxLayout;

namespace fincept::ui {
class PanelHeader;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::bt {

using services::backtest::BtRun;
using services::backtest::BtRunPtr;
using services::backtest::kBtNaN;

// ── Text ────────────────────────────────────────────────────────────────────
/// "—" (the shared missing-value placeholder).
QString dash();
/// 0.1234 -> "12.34%" ("+12.34%" when signed_); NaN -> "—".
QString pct(double fraction, int decimals = 2, bool signed_ = false);
/// A difference of two fractions in percentage points: 0.031 -> "+3.1 pp".
QString pp(double fraction_diff, int decimals = 1);
/// A plain ratio (Sharpe, beta, profit factor): "1.23"; NaN -> "—".
QString ratio(double v, int decimals = 2);
/// A grouped number: "1,234.57".
QString num(double v, int decimals = 2);
/// Money in `ccy` (ISO code; empty = no unit): "$1,234.50"; signed_ -> "+$1,234.50".
QString money(double v, const QString& ccy, bool signed_ = false);
/// Compact money: "$1.23M".
QString money_compact(double v, const QString& ccy, bool signed_ = false);
/// An instrument price with the instrument's precision.
QString price(double v, const QString& ccy);
/// A share / unit quantity ("148.59", "1,200").
QString quantity(double v);
/// An integer count, grouped; NaN / negative sentinel -> "—".
QString count(double v);
/// "21 bars".
QString bars(double v);
/// "signal" -> "Signal", "stop_loss" -> "Stop-loss", "open" -> "Open" ...
QString exit_reason_text(const QString& id);
/// "2023-04-14" (dates are shown as ISO in the dense tables; empty -> "—").
QString date_text(const QString& iso);

// ── Tones ───────────────────────────────────────────────────────────────────
enum class Tone { None, Pos, Neg, Warn, Dim, Accent, Ok, Danger };
/// The value of the dynamic property "tone" ("" for None).
const char* tone_key(Tone tone);
/// Pos for > 0, Neg for < 0, None otherwise (P&L / return direction only).
Tone sign_tone(double v);
/// Set the "tone" property and re-polish only when it changed.
void set_tone(QWidget* w, Tone tone);
/// The theme colour of a tone (for item-model ForegroundRole and painting), read now.
QColor tone_color(Tone tone);

// ── KPI tiles ───────────────────────────────────────────────────────────────
enum class Unit { Fraction, SignedFraction, PctPoints, Ratio, Money, SignedMoney, Count, Bars, Plain };
enum class Rule {
    None,     ///< neutral
    Sign,     ///< Positive / Negative by sign (returns, P&L)
    Drawdown, ///< magnitude of a (positive or negative) drawdown: >= 25 % Danger (state), >= 10 % Warning
    BelowOne, ///< < 1 Warning (profit factor)
    Negative, ///< < 0 Warning (Sharpe-like)
};
QString format_value(double v, Unit unit, const QString& ccy);
ui::StatTile::Tone tile_tone(double v, Rule rule);
/// Fill a tile: finite v -> its text and tone, State::Normal; NaN -> "—", State::Error with `why_blank`.
void set_tile(ui::StatTile* tile, double v, Unit unit, Rule rule, const QString& ccy, const QString& why_blank = {});
/// set_tile from run.metric(path) with run.why_blank(path) as the reason.
void set_tile_metric(ui::StatTile* tile, const BtRun& run, const QString& path, Unit unit, Rule rule);

// ── Panels ──────────────────────────────────────────────────────────────────
struct BtPanel {
    QFrame* frame = nullptr;          ///< #btPanel
    ui::PanelHeader* header = nullptr;
    QWidget* body_widget = nullptr;   ///< #btPanelBody
    QVBoxLayout* body = nullptr;      ///< layout of body_widget (margins `body_margin`, spacing 6)
};
/// A panel: header strip + body. Add controls with panel.header->add_control(w).
BtPanel make_panel(const QString& title, QWidget* parent, int body_margin = 6);

/// A dim word-wrapped note label (#btNote).
QLabel* make_note(const QString& text, QWidget* parent, Tone tone = Tone::Dim);
/// A chip label (#btChip) with a tone.
QLabel* make_chip(const QString& text, Tone tone, QWidget* parent);
/// A small outlined button (#btToolButton).
QPushButton* make_tool_button(const QString& text, const QString& tooltip, QWidget* parent);
/// A row of checkable #btSeg buttons, exactly one checked; `on_change(index)` on a user click.
/// Returns the container; its buttons are its children in order (findChildren<QPushButton*>()).
QWidget* make_segmented(const QStringList& labels, int checked, std::function<void(int)> on_change, QWidget* parent);
/// Check segment `index` of a make_segmented() container without calling on_change.
void set_segment(QWidget* segmented, int index);

/// One row of a key / value grid.
struct BtKv {
    QString key;
    QString value;
    Tone tone = Tone::None;
    QString tip{}; ///< tooltip of the value (e.g. why it is "—")
};

/// Key / value rows in `columns` column pairs (#btKvKey / #btKvValue labels, reused across
/// set_rows calls). Not a Q_OBJECT.
class BtKvGrid : public QWidget {
  public:
    explicit BtKvGrid(QWidget* parent = nullptr, int columns = 1);
    void set_rows(const QVector<BtKv>& rows);
    void set_columns(int columns);
    int columns() const { return columns_; }

  private:
    void relayout();
    QGridLayout* grid_ = nullptr;
    QVector<QLabel*> keys_;
    QVector<QLabel*> values_;
    int columns_ = 1;
    int shown_ = 0;
};

// ── Tables ──────────────────────────────────────────────────────────────────
/// Standard dense table: #btTable, row height from the font, no vertical header, alternating rows,
/// whole-row selection, interactive header with the last section stretched, sorting on, and the
/// ChartExport context menu (copy image / PNG / CSV) under `export_name`.
void setup_table(QTableView* view, const QString& export_name);
/// Column widths from average-character counts (set once; the user may resize). Call again after a
/// font change (the views do it on ThemeManager::appearance_settled).
void set_column_chars(QTableView* view, const QVector<int>& chars);
/// A QSortFilterProxyModel over `source` that sorts by ui::LiveTableModel<...>::SortRole.
QSortFilterProxyModel* make_sort_proxy(QAbstractItemModel* source, QObject* parent);

// ── Charts ──────────────────────────────────────────────────────────────────
/// A time chart with the tab's defaults: UTC dates (the payload's wall time), crosshair, wheel zoom,
/// transparent background, export name.
ui::TimeSeriesChart* make_time_chart(QWidget* parent, const QString& export_name);
/// (t, v * scale) points, NaN pairs dropped, thinned to <= max_points (first and last kept).
QVector<QPointF> to_points(const QVector<double>& t, const QVector<double>& v, double scale = 1.0,
                           int max_points = 5000);
/// Axis / tooltip formatter for fractions ("12%", "-3.5%").
std::function<QString(double)> fraction_formatter(int decimals = 1);
/// Axis / tooltip formatter for money in `ccy`.
std::function<QString(double)> money_formatter(const QString& ccy);
/// Axis / tooltip formatter for plain numbers with `decimals`.
std::function<QString(double)> number_formatter(int decimals = 2);

// ── Cross-highlight bus ─────────────────────────────────────────────────────
/// One per screen; every run view gets it. Signals carry the emitting object as `origin` so a view
/// ignores its own echo. NaN times = "reset".
class BtSelection : public QObject {
    Q_OBJECT
  public:
    explicit BtSelection(QObject* parent = nullptr) : QObject(parent) {}

  signals:
    /// Zoom time charts to [from, to] (epoch ms); NaN, NaN = full range.
    void time_range_selected(double from_ms, double to_ms, QObject* origin);
    /// A trade was selected (trade id = BtTrade::id); empty = none.
    void trade_selected(const QString& trade_id, QObject* origin);
    /// A drawdown was selected (rank 1..10); 0 = none.
    void drawdown_selected(int rank, QObject* origin);
    /// A heat-map month was clicked (month 1..12, 0 = whole year).
    void month_selected(int year, int month, QObject* origin);
    /// The crosshair of a time chart moved (NaN = left the chart).
    void crosshair_moved(double x_ms, QObject* origin);
    /// Filter the trades to entries inside [from, to] with a removable chip `label`; NaN = clear.
    void trades_filter_requested(double from_ms, double to_ms, const QString& label, QObject* origin);
    /// Switch the screen to a sub-tab: overview | performance | risk | trades | exposure | costs |
    /// assumptions.
    void tab_requested(const QString& tab_id);
};

/// Base of the six run views (OVERVIEW … COSTS). show_run() is called by the screen when the view
/// is visible and the active run changed (or once when it becomes visible); a null run clears.
class BtRunView : public QWidget {
    Q_OBJECT
  public:
    explicit BtRunView(BtSelection* selection, QWidget* parent = nullptr);
    virtual void show_run(const BtRunPtr& run) = 0;
    BtSelection* selection() const { return selection_; }
    const BtRunPtr& run() const { return run_; }

  protected:
    BtSelection* selection_ = nullptr;
    BtRunPtr run_;
};

} // namespace fincept::screens::bt
