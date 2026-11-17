// src/screens/backtesting/BtTradesView.cpp
//
// TRADES sub-tab (spec 2.6, 4.3, 4.4): filter bar · PRICE + TRADES (candles of the selected trade's
// symbol, or the equity curve with the trade banded when the run has no priceData) · P&L per trade ·
// MAE vs P&L scatter (QtCharts) · the trades table (model / view, fast key-based sort and filter
// proxy) with totals over the FILTERED rows · BY SYMBOL attribution · an EmptyState for zero trades.
#include "screens/backtesting/BtRunViews.h"

#include "screens/backtesting/BacktestViews.h"
#include "ui/charts/CandleChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/ChartFactory.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/PanelHeader.h"

#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QCursor>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLegend>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScatterSeries>
#include <QSet>
#include <QShortcut>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <functional>

namespace fincept::screens::bt {

using services::backtest::BtAttribution;
using services::backtest::BtTrade;

namespace {

using TvTs = ui::TimeSeriesChart;

enum TvCol {
    kTvNum,
    kTvSymbol,
    kTvSide,
    kTvEntryDate,
    kTvEntryPx,
    kTvExitDate,
    kTvExitPx,
    kTvQty,
    kTvPnl,
    kTvPnlPct,
    kTvHold,
    kTvMae,
    kTvMfe,
    kTvCosts,
    kTvReason,
    kTvCum,
};
enum TvAttrCol { kTaSymbol, kTaTrades, kTaWin, kTaPnl, kTaContrib, kTaCosts, kTaHold, kTaTime, kTaSpark };

constexpr int kTvHeatRole = Qt::UserRole + 20; // |MAE| / MFE for the heat delegates
constexpr int kTvScatterMax = 2000;
constexpr int kTvSparkPoints = 60;
constexpr int kTvWideChars = 120;

struct TvRow {
    const BtTrade* t = nullptr;
    int n = 0;          ///< 1-based number in run order
    double cum = kBtNaN; ///< cumulative realised P&L in exit order (closed trades)
};
using TvModel = ui::LiveTableModel<TvRow>;

struct TvAttrRow {
    BtAttribution a;
    QVector<double> spark; ///< cumulative realised P&L of the symbol, thinned
};
using TvAttrModel = ui::LiveTableModel<TvAttrRow>;

/// Sort and filter on keys read straight from the rows: QSortFilterProxyModel's default lessThan goes
/// through data(SortRole) per comparison, which costs tens of ms on 20,000 trades.
class TvTradeProxy final : public QSortFilterProxyModel {
  public:
    explicit TvTradeProxy(QObject* parent) : QSortFilterProxyModel(parent) {}
    std::function<bool(int, int, int)> less; ///< (column, left source row, right source row)
    QVector<char> accept;                    ///< per source row; empty = all
    void refilter() { invalidateRowsFilter(); }

  protected:
    bool lessThan(const QModelIndex& l, const QModelIndex& r) const override {
        return less ? less(l.column(), l.row(), r.row()) : QSortFilterProxyModel::lessThan(l, r);
    }
    bool filterAcceptsRow(int row, const QModelIndex&) const override {
        return accept.isEmpty() || (row >= 0 && row < accept.size() && accept.at(row) != 0);
    }
};

struct TvFilter {
    int outcome = 0;
    int side = 2;
    QString symbol;
    QString reason;
    QString text;
    double from = kBtNaN;
    double to = kBtNaN;
};

struct TvTotals {
    int shown = 0;
    int closed = 0;
    int wins = 0;
    int open = 0;
    double gross_profit = 0;
    double gross_loss = 0;
    double net = 0;
    double open_pnl = 0;
    double hold_sum = 0;
    int hold_n = 0;
    double costs = 0;
    bool any_costs = false;
};

QVariant tv_right() {
    return QVariant((Qt::AlignRight | Qt::AlignVCenter).toInt());
}

bool tv_is_short(const BtTrade& t) {
    return t.side == QLatin1String("short");
}

QVariant tv_cell(const TvRow& row, int col, int role, const QString& ccy) {
    if (!row.t)
        return {};
    const BtTrade& t = *row.t;
    switch (role) {
        case Qt::DisplayRole:
            switch (col) {
                case kTvNum:
                    return QString::number(row.n);
                case kTvSymbol:
                    return t.symbol;
                case kTvSide:
                    return tv_is_short(t) ? BtTradesView::tr("SHORT") : BtTradesView::tr("LONG");
                case kTvEntryDate:
                    return date_text(t.entry_date);
                case kTvEntryPx:
                    return price(t.entry_price, ccy);
                case kTvExitDate:
                    return t.open ? BtTradesView::tr("OPEN") : date_text(t.exit_date);
                case kTvExitPx:
                    return price(t.open ? t.mark_price : t.exit_price, ccy);
                case kTvQty:
                    return quantity(t.quantity);
                case kTvPnl:
                    return money(t.pnl, ccy, true);
                case kTvPnlPct:
                    return pct(t.pnl_pct, 2, true);
                case kTvHold:
                    return t.holding_bars >= 0 ? count(t.holding_bars) : dash();
                case kTvMae:
                    return pct(t.mae, 2, true);
                case kTvMfe:
                    return pct(t.mfe, 2, true);
                case kTvCosts:
                    return money(t.costs(), ccy);
                case kTvReason:
                    return exit_reason_text(t.exit_reason);
                case kTvCum:
                    return money(row.cum, ccy, true);
                default:
                    break;
            }
            break;
        case Qt::EditRole:
            if (col == kTvPnl && std::isfinite(t.pnl))
                return t.pnl;
            break;
        case kTvHeatRole:
            if (col == kTvMae && std::isfinite(t.mae))
                return std::fabs(t.mae);
            if (col == kTvMfe && std::isfinite(t.mfe))
                return t.mfe;
            break;
        case Qt::ForegroundRole: {
            if (col == kTvExitDate && t.open)
                return tone_color(Tone::Warn);
            if (col == kTvExitPx && t.open)
                return tone_color(Tone::Dim);
            double v = kBtNaN;
            if (col == kTvPnl)
                v = t.pnl;
            else if (col == kTvPnlPct)
                v = t.pnl_pct;
            else if (col == kTvCum)
                v = row.cum;
            const Tone tn = sign_tone(v);
            if (tn != Tone::None)
                return tone_color(tn);
            if (col == kTvNum)
                return tone_color(Tone::Dim);
            break;
        }
        case Qt::TextAlignmentRole:
            switch (col) {
                case kTvNum:
                case kTvEntryPx:
                case kTvExitPx:
                case kTvQty:
                case kTvPnl:
                case kTvPnlPct:
                case kTvHold:
                case kTvMae:
                case kTvMfe:
                case kTvCosts:
                case kTvCum:
                    return tv_right();
                default:
                    break;
            }
            break;
        case Qt::ToolTipRole:
            switch (col) {
                case kTvPnl:
                case kTvPnlPct:
                    if (t.open)
                        return BtTradesView::tr("Unrealised P&L: the position is still open, marked at %1")
                            .arg(price(t.mark_price, ccy));
                    return BtTradesView::tr("Net of costs. Gross %1 · costs %2")
                        .arg(money(t.gross_pnl, ccy, true), money(t.costs(), ccy));
                case kTvExitPx:
                    return t.open ? BtTradesView::tr("Mark price (the position is still open)") : QVariant();
                case kTvExitDate:
                    return t.open ? BtTradesView::tr("Open at the end of the run") : QVariant();
                case kTvHold:
                    return t.holding_days >= 0
                               ? BtTradesView::tr("%1 · %2 calendar days").arg(bars(t.holding_bars), count(t.holding_days))
                               : QVariant();
                case kTvMae:
                    return BtTradesView::tr("Maximum adverse excursion: the worst open loss during the trade");
                case kTvMfe:
                    return BtTradesView::tr("Maximum favourable excursion: the best open gain during the trade");
                case kTvCosts:
                    return BtTradesView::tr("Commission %1 · slippage %2 · borrow %3")
                        .arg(money(t.commission, ccy), money(t.slippage, ccy), money(t.borrow, ccy));
                case kTvCum:
                    return BtTradesView::tr("Realised P&L of all trades closed up to this one (exit order)");
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return {};
}

bool tv_num_less(double a, double b) {
    if (!std::isfinite(a))
        return false;
    if (!std::isfinite(b))
        return true;
    return a < b;
}

double tv_key(const TvRow& row, int col) {
    const BtTrade& t = *row.t;
    switch (col) {
        case kTvNum:
            return row.n;
        case kTvEntryDate:
            return t.entry_t;
        case kTvEntryPx:
            return t.entry_price;
        case kTvExitDate:
            return t.open ? kBtNaN : t.exit_t;
        case kTvExitPx:
            return t.open ? t.mark_price : t.exit_price;
        case kTvQty:
            return t.quantity;
        case kTvPnl:
            return t.pnl;
        case kTvPnlPct:
            return t.pnl_pct;
        case kTvHold:
            return t.holding_bars >= 0 ? static_cast<double>(t.holding_bars) : kBtNaN;
        case kTvMae:
            return t.mae;
        case kTvMfe:
            return t.mfe;
        case kTvCosts:
            return t.costs();
        case kTvCum:
            return row.cum;
        default:
            break;
    }
    return kBtNaN;
}

bool tv_less(const QVector<TvRow>& rows, int col, int a, int b) {
    if (a < 0 || b < 0 || a >= rows.size() || b >= rows.size())
        return a < b;
    const TvRow& x = rows.at(a);
    const TvRow& y = rows.at(b);
    switch (col) {
        case kTvSymbol: {
            const int c = QString::compare(x.t->symbol, y.t->symbol, Qt::CaseInsensitive);
            return c != 0 ? c < 0 : x.n < y.n;
        }
        case kTvSide:
            return x.t->side != y.t->side ? x.t->side < y.t->side : x.n < y.n;
        case kTvReason:
            return x.t->exit_reason != y.t->exit_reason ? x.t->exit_reason < y.t->exit_reason : x.n < y.n;
        default:
            break;
    }
    return tv_num_less(tv_key(x, col), tv_key(y, col));
}

QVector<char> tv_mask(const QVector<TvRow>& rows, const TvFilter& f, TvTotals* tot) {
    QVector<char> m(rows.size(), 0);
    const bool ext = std::isfinite(f.from) && std::isfinite(f.to);
    const int n = static_cast<int>(rows.size());
    for (int i = 0; i < n; ++i) {
        const BtTrade& t = *rows.at(i).t;
        bool ok = true;
        if (f.outcome == 1)
            ok = std::isfinite(t.pnl) && t.pnl > 0;
        else if (f.outcome == 2)
            ok = std::isfinite(t.pnl) && t.pnl < 0;
        if (ok && f.side != 2)
            ok = (f.side == 1) == tv_is_short(t);
        if (ok && !f.symbol.isEmpty())
            ok = t.symbol == f.symbol;
        if (ok && !f.reason.isEmpty())
            ok = t.exit_reason == f.reason;
        if (ok && ext)
            ok = std::isfinite(t.entry_t) && t.entry_t >= f.from && t.entry_t <= f.to;
        if (ok && !f.text.isEmpty()) {
            ok = t.symbol.contains(f.text, Qt::CaseInsensitive) || t.entry_date.contains(f.text) ||
                 t.exit_date.contains(f.text) || t.side.contains(f.text, Qt::CaseInsensitive) ||
                 t.id.contains(f.text, Qt::CaseInsensitive) || t.exit_reason.contains(f.text, Qt::CaseInsensitive) ||
                 exit_reason_text(t.exit_reason).contains(f.text, Qt::CaseInsensitive);
        }
        if (!ok)
            continue;
        m[i] = 1;
        ++tot->shown;
        if (t.open) {
            ++tot->open;
            if (std::isfinite(t.pnl))
                tot->open_pnl += t.pnl;
        } else {
            ++tot->closed;
            if (std::isfinite(t.pnl)) {
                tot->net += t.pnl;
                if (t.pnl > 0) {
                    ++tot->wins;
                    tot->gross_profit += t.pnl;
                } else if (t.pnl < 0) {
                    tot->gross_loss += t.pnl;
                }
            }
        }
        if (t.holding_bars >= 0) {
            tot->hold_sum += t.holding_bars;
            ++tot->hold_n;
        }
        const double c = t.costs();
        if (std::isfinite(c)) {
            tot->costs += c;
            tot->any_costs = true;
        }
    }
    return m;
}

QVector<BtKv> tv_totals_rows(const TvTotals& t, const QString& ccy) {
    QVector<BtKv> rows;
    rows << BtKv{BtTradesView::tr("Trades"),
                 t.open > 0 ? BtTradesView::tr("%1 (%2 open)").arg(count(t.shown), count(t.open)) : count(t.shown),
                 Tone::None, QString()};
    rows << BtKv{BtTradesView::tr("Win rate"), t.closed > 0 ? pct(static_cast<double>(t.wins) / t.closed, 1) : dash(),
                 Tone::None, BtTradesView::tr("Winning closed trades / closed trades")};
    rows << BtKv{BtTradesView::tr("Gross profit"), money(t.gross_profit, ccy, true), sign_tone(t.gross_profit), QString()};
    rows << BtKv{BtTradesView::tr("Gross loss"), money(t.gross_loss, ccy, true), sign_tone(t.gross_loss), QString()};
    rows << BtKv{BtTradesView::tr("Net P&L (closed)"), money(t.net, ccy, true), sign_tone(t.net), QString()};
    rows << BtKv{BtTradesView::tr("Open P&L"), t.open > 0 ? money(t.open_pnl, ccy, true) : dash(),
                 t.open > 0 ? sign_tone(t.open_pnl) : Tone::None, BtTradesView::tr("Unrealised, marked at the last bar")};
    rows << BtKv{BtTradesView::tr("Avg hold"), t.hold_n > 0 ? bars(t.hold_sum / t.hold_n) : dash(), Tone::None, QString()};
    rows << BtKv{BtTradesView::tr("Costs"), t.any_costs ? money(t.costs, ccy) : dash(), Tone::None, QString()};
    return rows;
}

QVariant tv_attr_cell(const TvAttrRow& row, int col, int role, const QString& ccy) {
    const BtAttribution& a = row.a;
    switch (role) {
        case Qt::DisplayRole:
            switch (col) {
                case kTaSymbol:
                    return a.symbol;
                case kTaTrades:
                    return count(a.trades);
                case kTaWin:
                    return pct(a.win_rate, 1);
                case kTaPnl:
                    return money(a.net_pnl, ccy, true);
                case kTaContrib:
                    return pp(a.contribution, 2);
                case kTaCosts:
                    return money(a.costs, ccy);
                case kTaHold:
                    return bars(a.avg_hold_bars);
                case kTaTime:
                    return pct(a.time_in_market, 1);
                default:
                    break;
            }
            break;
        case Qt::EditRole:
            if (col == kTaPnl && std::isfinite(a.net_pnl))
                return a.net_pnl;
            break;
        case TvAttrModel::SortRole:
            switch (col) {
                case kTaSymbol:
                    return a.symbol;
                case kTaTrades:
                    return a.trades;
                case kTaWin:
                    return std::isfinite(a.win_rate) ? a.win_rate : -1.0;
                case kTaPnl:
                    return std::isfinite(a.net_pnl) ? a.net_pnl : -1e300;
                case kTaContrib:
                    return std::isfinite(a.contribution) ? a.contribution : -1e300;
                case kTaCosts:
                    return std::isfinite(a.costs) ? a.costs : -1.0;
                case kTaHold:
                    return std::isfinite(a.avg_hold_bars) ? a.avg_hold_bars : -1.0;
                case kTaTime:
                    return std::isfinite(a.time_in_market) ? a.time_in_market : -1.0;
                case kTaSpark:
                    return row.spark.isEmpty() ? 0.0 : row.spark.last();
                default:
                    break;
            }
            break;
        case ui::SparklineDelegate::kDefaultRole:
            if (col == kTaSpark && row.spark.size() >= 2)
                return QVariant::fromValue(row.spark);
            break;
        case Qt::ForegroundRole: {
            const Tone tn = col == kTaPnl ? sign_tone(a.net_pnl) : (col == kTaContrib ? sign_tone(a.contribution) : Tone::None);
            if (tn != Tone::None)
                return tone_color(tn);
            break;
        }
        case Qt::TextAlignmentRole:
            if (col != kTaSymbol && col != kTaSpark)
                return tv_right();
            break;
        case Qt::ToolTipRole:
            if (col == kTaContrib)
                return BtTradesView::tr("Net P&L of %1 as a share of the initial capital (percentage points of the "
                                        "total return)")
                    .arg(a.symbol);
            if (col == kTaSpark)
                return BtTradesView::tr("Cumulative realised P&L of %1's closed trades").arg(a.symbol);
            return BtTradesView::tr("Click to show %1's trades").arg(a.symbol);
        default:
            break;
    }
    return {};
}

/// Index of the point of `pts` nearest to `p` (scaled by the point cloud's spread), -1 when empty.
int tv_nearest(const QList<QPointF>& pts, const QPointF& p) {
    int best = -1;
    double best_d = 0;
    for (int i = 0; i < pts.size(); ++i) {
        const double dx = pts.at(i).x() - p.x();
        const double dy = pts.at(i).y() - p.y();
        const double d = dx * dx + dy * dy;
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

QString tv_point_tip(const BtTrade& t, const QString& ccy, bool hold_x) {
    QStringList lines;
    lines << BtTradesView::tr("%1 · %2 · %3")
                 .arg(t.symbol, tv_is_short(t) ? BtTradesView::tr("SHORT") : BtTradesView::tr("LONG"),
                      date_text(t.entry_date));
    if (hold_x)
        lines << BtTradesView::tr("Held %1").arg(bars(t.holding_bars));
    else
        lines << BtTradesView::tr("MAE %1 · MFE %2").arg(pct(t.mae, 2, true), pct(t.mfe, 2, true));
    lines << BtTradesView::tr("P&L %1 (%2)").arg(money(t.pnl, ccy, true), pct(t.pnl_pct, 2, true));
    lines << BtTradesView::tr("Click to select the trade");
    return lines.join(QLatin1Char('\n'));
}

/// Every k-th value of `v` so at most `max_points` remain, the last kept.
QVector<double> tv_thin(const QVector<double>& v, int max_points) {
    const int n = static_cast<int>(v.size());
    if (n <= max_points)
        return v;
    QVector<double> out;
    const int step = (n + max_points - 1) / max_points;
    for (int i = 0; i < n; i += step)
        out.append(v.at(i));
    if ((n - 1) % step != 0)
        out.append(v.last());
    return out;
}

/// Closed trades' indexes in exit-time order (stable on entry order).
QVector<int> tv_exit_order(const QVector<BtTrade>& trades) {
    QVector<int> idx;
    idx.reserve(trades.size());
    for (int i = 0; i < trades.size(); ++i)
        if (!trades.at(i).open)
            idx.append(i);
    std::stable_sort(idx.begin(), idx.end(), [&trades](int a, int b) {
        return tv_num_less(trades.at(a).exit_t, trades.at(b).exit_t);
    });
    return idx;
}

} // namespace

// ── Static helpers ──────────────────────────────────────────────────────────

bool BtTradesView::trade_window(const BtRun& run, const BtTrade& trade, double* from_ms, double* to_ms) {
    if (!std::isfinite(trade.entry_t) || !from_ms || !to_ms)
        return false;
    double end = std::isfinite(trade.exit_t) && !trade.open ? trade.exit_t
                                                            : (run.t.isEmpty() ? trade.entry_t : run.t.last());
    if (!std::isfinite(end) || end < trade.entry_t)
        end = trade.entry_t;
    const int n = static_cast<int>(run.t.size());
    const double bar = n >= 2 ? (run.t.last() - run.t.first()) / (n - 1) : 86400000.0;
    const double margin = std::max(20.0 * bar, 0.5 * (end - trade.entry_t));
    *from_ms = trade.entry_t - margin;
    *to_ms = end + margin;
    return true;
}

// ── Construction ────────────────────────────────────────────────────────────

BtTradesView::BtTradesView(BtSelection* selection, QWidget* parent) : BtRunView(selection, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);
    build_filter_bar();
    root->addWidget(bar_);
    splitter_ = new QSplitter(Qt::Vertical, this);
    splitter_->setChildrenCollapsible(false);
    build_top();
    build_tables();
    splitter_->addWidget(top_);
    splitter_->addWidget(table_stack_);
    splitter_->setStretchFactor(0, 2);
    splitter_->setStretchFactor(1, 3);
    root->addWidget(splitter_, 1);

    auto* find = new QShortcut(QKeySequence::Find, this);
    find->setContext(Qt::WidgetWithChildrenShortcut);
    connect(find, &QShortcut::activated, this, &BtTradesView::focus_filter);

    if (selection_) {
        connect(selection_, &BtSelection::time_range_selected, this, [this](double a, double b, QObject* origin) {
            if (bt_view_owns_origin(this, origin))
                return;
            if (std::isfinite(a) && std::isfinite(b))
                fallback_->set_x_range(a, b);
            else
                fallback_->reset_x_range();
        });
        connect(selection_, &BtSelection::trade_selected, this, [this](const QString& id, QObject* origin) {
            if (bt_view_owns_origin(this, origin))
                return;
            if (id.isEmpty()) {
                sel_trade_.clear();
                const QSignalBlocker block(table_->selectionModel());
                table_->clearSelection();
                table_->viewport()->update();
                mark_selected_trade(false);
                return;
            }
            select_trade_by_id(id, true);
        });
        connect(selection_, &BtSelection::trades_filter_requested, this,
                [this](double a, double b, const QString& label, QObject* origin) {
                    if (bt_view_owns_origin(this, origin))
                        return;
                    const bool on = std::isfinite(a) && std::isfinite(b);
                    ext_from_ = on ? std::min(a, b) : kBtNaN;
                    ext_to_ = on ? std::max(a, b) : kBtNaN;
                    ext_label_ = on ? label : QString();
                    ext_chip_->setText(ext_label_.isEmpty() ? tr("in the selected range") : ext_label_);
                    ext_chip_->setVisible(on);
                    ext_clear_->setVisible(on);
                    refresh_filter();
                });
        connect(selection_, &BtSelection::crosshair_moved, this, [this](double x, QObject* origin) {
            if (!bt_view_owns_origin(this, origin))
                fallback_->set_linked_crosshair(x);
        });
    }

    // A format change re-renders the text; the equity fallback keeps the user's zoom.
    ui::formatting::on_format_changed(this, [this]() {
        refresh_filter();
        render_distribution();
        render_scatter();
        if (price_stack_->currentWidget() == fallback_)
            fallback_->set_value_formatter(TvTs::Axis::Left, money_formatter(ccy_), 0);
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        theme_scatter();
        fit_to_font();
        relayout(width(), true);
    });
    fit_to_font();
    relayout(width(), true);
}

void BtTradesView::build_filter_bar() {
    bar_ = new QWidget(this);
    auto* v = new QVBoxLayout(bar_);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(4);
    bar_row_a_ = new QHBoxLayout();
    bar_row_a_->setSpacing(6);
    bar_row_b_ = new QHBoxLayout();
    bar_row_b_->setSpacing(6);
    v->addLayout(bar_row_a_);
    v->addLayout(bar_row_b_);

    filter_edit_ = new QLineEdit(bar_);
    filter_edit_->setObjectName(QStringLiteral("btFilter"));
    filter_edit_->setPlaceholderText(tr("Filter: symbol, date, side, exit reason…  (Ctrl+F)"));
    filter_edit_->setClearButtonEnabled(true);
    filter_edit_->setAccessibleName(tr("Filter trades"));
    connect(filter_edit_, &QLineEdit::textChanged, this, [this]() { refresh_filter(); });

    outcome_seg_ = make_segmented({tr("ALL"), tr("WINNERS"), tr("LOSERS")}, 0,
                                  [this](int i) {
                                      outcome_ = i;
                                      refresh_filter();
                                  },
                                  bar_);
    side_seg_ = make_segmented({tr("LONG"), tr("SHORT"), tr("BOTH")}, 2,
                               [this](int i) {
                                   side_ = i;
                                   refresh_filter();
                               },
                               bar_);
    symbol_combo_ = new QComboBox(bar_);
    symbol_combo_->setObjectName(QStringLiteral("btCombo"));
    symbol_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    symbol_combo_->setMinimumContentsLength(9);
    symbol_combo_->setToolTip(tr("Only this symbol's trades"));
    symbol_combo_->addItem(tr("All symbols"), QString());
    connect(symbol_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { refresh_filter(); });
    reason_combo_ = new QComboBox(bar_);
    reason_combo_->setObjectName(QStringLiteral("btCombo"));
    reason_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    reason_combo_->setMinimumContentsLength(10);
    reason_combo_->setToolTip(tr("Only trades closed for this reason"));
    reason_combo_->addItem(tr("All exits"), QString());
    connect(reason_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { refresh_filter(); });

    bar_row_a_->addWidget(filter_edit_, 1);
    bar_row_a_->addWidget(outcome_seg_);
    bar_row_a_->addWidget(side_seg_);
    bar_row_a_->addWidget(symbol_combo_);
    bar_row_a_->addWidget(reason_combo_);

    ext_chip_ = make_chip(QString(), Tone::Accent, bar_);
    ext_chip_->setVisible(false);
    ext_clear_ = make_tool_button(tr("CLEAR"), tr("Remove the period filter set from another tab"), bar_);
    ext_clear_->setVisible(false);
    connect(ext_clear_, &QPushButton::clicked, this, [this]() {
        ext_from_ = ext_to_ = kBtNaN;
        ext_label_.clear();
        ext_chip_->setVisible(false);
        ext_clear_->setVisible(false);
        refresh_filter();
    });
    count_label_ = new QLabel(bar_);
    count_label_->setObjectName(QStringLiteral("btKvValue"));
    count_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    count_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    page_seg_ = make_segmented({tr("TRADES"), tr("BY SYMBOL")}, 0, [this](int i) { set_view_page(i); }, bar_);
    bar_row_b_->addWidget(ext_chip_);
    bar_row_b_->addWidget(ext_clear_);
    bar_row_b_->addWidget(count_label_, 1);
    bar_row_b_->addWidget(page_seg_);
}

void BtTradesView::build_top() {
    top_ = new QWidget(splitter_);
    top_grid_ = new QGridLayout(top_);
    top_grid_->setContentsMargins(0, 0, 0, 0);
    top_grid_->setHorizontalSpacing(6);
    top_grid_->setVerticalSpacing(6);

    // PRICE + TRADES
    price_panel_ = make_panel(tr("PRICE + TRADES"), top_, 4);
    price_combo_ = new QComboBox(price_panel_.header);
    price_combo_->setObjectName(QStringLiteral("btCombo"));
    price_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    price_combo_->setMinimumContentsLength(8);
    price_combo_->setToolTip(tr("Symbol shown on the price chart"));
    price_combo_->setVisible(false);
    price_panel_.header->add_control(price_combo_);
    connect(price_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!syncing_ && !price_combo_->currentText().isEmpty())
            request_price_chart(price_combo_->currentText());
    });
    price_stack_ = new QStackedWidget(price_panel_.body_widget);
    candle_ = new ui::CandleChart(price_stack_);
    candle_->set_utc(true);
    candle_->set_state(ui::ChartState::Empty, tr("No backtest yet"));
    fallback_ = make_time_chart(price_stack_, QStringLiteral("trade_equity"));
    fallback_->set_range_presets({QStringLiteral("3M"), QStringLiteral("1Y"), QStringLiteral("ALL")});
    fallback_->set_state(ui::ChartState::Empty, tr("No backtest yet"));
    price_stack_->addWidget(candle_);
    price_stack_->addWidget(fallback_);
    price_stack_->setCurrentWidget(fallback_);
    price_panel_.body->addWidget(price_stack_, 1);
    connect(fallback_, &TvTs::crosshair_moved, this, [this](double x) {
        if (selection_)
            emit selection_->crosshair_moved(x, this);
    });

    // P&L PER TRADE
    dist_panel_ = make_panel(tr("P&L PER TRADE"), top_, 4);
    pnl_dist_ = new ui::DistributionChart(dist_panel_.body_widget);
    pnl_dist_->set_signed_colors(true);
    pnl_dist_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1, true); });
    pnl_dist_->set_state(ui::PaintedChart::Empty, tr("No backtest yet"));
    {
        ui::ChartExport::MenuSpec spec;
        spec.suggested_name = QStringLiteral("pnl_per_trade");
        ui::ChartExport::install_context_menu(pnl_dist_, spec);
    }
    dist_panel_.body->addWidget(pnl_dist_, 1);

    // MAE vs P&L scatter (QtCharts)
    scatter_panel_ = make_panel(tr("MAE vs P&L"), top_, 2);
    scatter_stack_ = new QStackedWidget(scatter_panel_.body_widget);
    scatter_ = new QChart();
    scatter_->setAnimationOptions(QChart::NoAnimation);
    scatter_->setBackgroundRoundness(0); // square corners: nothing of the view shows through
    win_series_ = new QScatterSeries();
    win_series_->setName(tr("Winners"));
    win_series_->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    loss_series_ = new QScatterSeries();
    loss_series_->setName(tr("Losers"));
    loss_series_->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    scatter_->addSeries(win_series_);
    scatter_->addSeries(loss_series_);
    scatter_x_ = new QValueAxis();
    scatter_y_ = new QValueAxis();
    scatter_x_->setLabelFormat(QStringLiteral("%.1f%%"));
    scatter_y_->setLabelFormat(QStringLiteral("%.1f%%"));
    scatter_x_->setTickCount(5);
    scatter_y_->setTickCount(5);
    scatter_->addAxis(scatter_x_, Qt::AlignBottom);
    scatter_->addAxis(scatter_y_, Qt::AlignLeft);
    for (QScatterSeries* s : {win_series_, loss_series_}) {
        s->attachAxis(scatter_x_);
        s->attachAxis(scatter_y_);
    }
    scatter_view_ = new QChartView(scatter_, scatter_stack_);
    scatter_view_->setObjectName(QStringLiteral("btScatter"));
    scatter_view_->setRenderHint(QPainter::Antialiasing);
    scatter_view_->setFrameShape(QFrame::NoFrame);
    scatter_empty_ = make_note(tr("No closed trades to plot"), scatter_stack_);
    scatter_empty_->setAlignment(Qt::AlignCenter);
    scatter_stack_->addWidget(scatter_view_);
    scatter_stack_->addWidget(scatter_empty_);
    scatter_stack_->setCurrentWidget(scatter_empty_);
    scatter_panel_.body->addWidget(scatter_stack_, 1);
    {
        ui::ChartExport::MenuSpec spec;
        spec.suggested_name = QStringLiteral("mae_vs_pnl");
        spec.csv = [this]() {
            ui::ChartExport::Table t;
            t.headers = QStringList{tr("Trade"), tr("Symbol"), scatter_hold_x_ ? tr("Hold (bars)") : tr("MAE (%)"),
                                    tr("P&L (%)")};
            if (!run_)
                return t;
            const auto add = [this, &t](const QList<QPointF>& pts, const QVector<int>& rows) {
                for (int i = 0; i < pts.size() && i < rows.size(); ++i) {
                    const BtTrade& tr_ = run_->trades.at(rows.at(i));
                    t.rows.append(QStringList{tr_.id, tr_.symbol, ui::formatting::export_number(pts.at(i).x(), 4),
                                              ui::formatting::export_number(pts.at(i).y(), 4)});
                }
            };
            add(win_points_, win_rows_);
            add(loss_points_, loss_rows_);
            return t;
        };
        ui::ChartExport::install_context_menu(scatter_view_, spec);
    }
    theme_scatter();

    const auto hover = [this](const QList<QPointF>* pts, const QVector<int>* rows, const QPointF& p, bool on) {
        if (!on || !run_) {
            QToolTip::hideText();
            return;
        }
        const int i = tv_nearest(*pts, p);
        if (i >= 0 && i < rows->size() && rows->at(i) < run_->trades.size())
            QToolTip::showText(QCursor::pos(), tv_point_tip(run_->trades.at(rows->at(i)), ccy_, scatter_hold_x_),
                               scatter_view_);
    };
    const auto click = [this](const QList<QPointF>* pts, const QVector<int>* rows, const QPointF& p) {
        if (!run_)
            return;
        const int i = tv_nearest(*pts, p);
        if (i < 0 || i >= rows->size() || rows->at(i) >= run_->trades.size())
            return;
        const QString id = run_->trades.at(rows->at(i)).id;
        select_trade_by_id(id, true);
        if (selection_)
            emit selection_->trade_selected(id, this);
    };
    connect(win_series_, &QScatterSeries::hovered, this,
            [this, hover](const QPointF& p, bool on) { hover(&win_points_, &win_rows_, p, on); });
    connect(loss_series_, &QScatterSeries::hovered, this,
            [this, hover](const QPointF& p, bool on) { hover(&loss_points_, &loss_rows_, p, on); });
    connect(win_series_, &QScatterSeries::clicked, this,
            [this, click](const QPointF& p) { click(&win_points_, &win_rows_, p); });
    connect(loss_series_, &QScatterSeries::clicked, this,
            [this, click](const QPointF& p) { click(&loss_points_, &loss_rows_, p); });
}

void BtTradesView::build_tables() {
    table_stack_ = new QStackedWidget(splitter_);

    // Trades
    trades_page_ = new QWidget(table_stack_);
    auto* tv = new QVBoxLayout(trades_page_);
    tv->setContentsMargins(0, 0, 0, 0);
    tv->setSpacing(4);
    table_ = new QTableView(trades_page_);
    setup_table(table_, QStringLiteral("trades"));
    auto* model = new TvModel(this);
    model->set_headers({tr("#"), tr("SYMBOL"), tr("SIDE"), tr("ENTRY DATE"), tr("ENTRY PX"), tr("EXIT DATE"),
                        tr("EXIT PX"), tr("QTY"), tr("P&L"), tr("P&L %"), tr("HOLD"), tr("MAE"), tr("MFE"),
                        tr("COSTS"), tr("EXIT REASON"), tr("CUM P&L")});
    model->set_cell([this](const TvRow& row, int c, int role) { return tv_cell(row, c, role, ccy_); });
    trades_model_ = model;
    auto* proxy = new TvTradeProxy(this);
    proxy->setSourceModel(model);
    proxy->setDynamicSortFilter(false);
    proxy->less = [model](int c, int a, int b) { return tv_less(model->rows(), c, a, b); };
    proxy_ = proxy;
    table_->setModel(proxy);
    pnl_delegate_ = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, table_);
    pnl_delegate_->set_range(-1.0, 1.0);
    table_->setItemDelegateForColumn(kTvPnl, pnl_delegate_);
    mae_delegate_ = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, table_);
    mae_delegate_->set_heat_sequential(true);
    mae_delegate_->set_value_role(kTvHeatRole);
    mae_delegate_->set_range(0.0, 1.0);
    table_->setItemDelegateForColumn(kTvMae, mae_delegate_);
    mfe_delegate_ = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Heat, table_);
    mfe_delegate_->set_heat_sequential(true);
    mfe_delegate_->set_value_role(kTvHeatRole);
    mfe_delegate_->set_range(0.0, 1.0);
    table_->setItemDelegateForColumn(kTvMfe, mfe_delegate_);
    set_column_chars(table_, {5, 9, 6, 11, 10, 11, 10, 9, 13, 9, 7, 8, 8, 10, 13, 13});
    table_->sortByColumn(kTvEntryDate, Qt::DescendingOrder);
    ui::formatting::refresh_model_on_format_change(model);
    tv->addWidget(table_, 1);
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& cur, const QModelIndex&) {
                if (syncing_ || !cur.isValid())
                    return;
                on_current_trade(proxy_->mapToSource(cur).row());
            });
    connect(table_, &QTableView::activated, this, [this](const QModelIndex& idx) {
        if (idx.isValid())
            on_activated_trade(proxy_->mapToSource(idx).row());
    });

    auto* totals_frame = new QFrame(trades_page_);
    totals_frame->setObjectName(QStringLiteral("btPanel"));
    totals_frame->setAttribute(Qt::WA_StyledBackground, true);
    auto* tl = new QVBoxLayout(totals_frame);
    tl->setContentsMargins(8, 4, 8, 4);
    totals_ = new BtKvGrid(totals_frame, 4);
    totals_->setToolTip(tr("Totals over the trades shown (the filters apply)"));
    tl->addWidget(totals_);
    tv->addWidget(totals_frame);

    // By symbol
    attr_table_ = new QTableView(table_stack_);
    setup_table(attr_table_, QStringLiteral("attribution"));
    attr_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto* am = new TvAttrModel(this);
    am->set_headers({tr("SYMBOL"), tr("TRADES"), tr("WIN RATE"), tr("NET P&L"), tr("CONTRIBUTION"), tr("COSTS"),
                     tr("AVG HOLD"), tr("TIME IN MARKET"), tr("CUM P&L")});
    am->set_cell([this](const TvAttrRow& row, int c, int role) { return tv_attr_cell(row, c, role, ccy_); });
    attr_model_ = am;
    attr_proxy_ = make_sort_proxy(am, this);
    attr_table_->setModel(attr_proxy_);
    attr_pnl_delegate_ = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::SignedBar, attr_table_);
    attr_pnl_delegate_->set_range(-1.0, 1.0);
    attr_table_->setItemDelegateForColumn(kTaPnl, attr_pnl_delegate_);
    attr_table_->setItemDelegateForColumn(kTaSpark, new ui::SparklineDelegate(attr_table_));
    set_column_chars(attr_table_, {10, 8, 9, 14, 13, 11, 10, 14, 16});
    attr_table_->sortByColumn(kTaPnl, Qt::DescendingOrder);
    ui::formatting::refresh_model_on_format_change(am);
    const auto attr_open = [this](const QModelIndex& idx) {
        const auto* m = static_cast<TvAttrModel*>(attr_model_);
        const TvAttrRow* row = m->row_at(attr_proxy_->mapToSource(idx));
        if (!row)
            return;
        const int i = symbol_combo_->findData(row->a.symbol);
        if (i >= 0)
            symbol_combo_->setCurrentIndex(i); // refreshes the filter
        set_view_page(0);
    };
    connect(attr_table_, &QTableView::clicked, this, attr_open);
    connect(attr_table_, &QTableView::activated, this, attr_open);

    // Zero trades
    empty_ = new ui::EmptyState(table_stack_);
    empty_->apply(ui::EmptyState::no_data_spec(
        tr("No trades in this period"), tr("The strategy never signalled. Widen the period or loosen the parameters.")));

    table_stack_->addWidget(trades_page_);
    table_stack_->addWidget(attr_table_);
    table_stack_->addWidget(empty_);
}

// ── Layout ──────────────────────────────────────────────────────────────────

void BtTradesView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout(e->size().width(), false);
}

void BtTradesView::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (load_on_show_) {
        load_on_show_ = false;
        request_price_chart(pending_symbol_);
    }
}

void BtTradesView::relayout(int width, bool force) {
    const QFontMetrics fm(ui::fonts::font(0));
    const int ch = std::max(5, fm.averageCharWidth());

    const int cols = width >= kTvWideChars * ch ? 3 : 2;
    if (force || cols != top_cols_) {
        for (QFrame* f : {price_panel_.frame, dist_panel_.frame, scatter_panel_.frame})
            top_grid_->removeWidget(f);
        for (int i = 0; i < 3; ++i) {
            top_grid_->setRowStretch(i, 0);
            top_grid_->setColumnStretch(i, 0);
        }
        if (cols == 3) {
            top_grid_->addWidget(price_panel_.frame, 0, 0);
            top_grid_->addWidget(dist_panel_.frame, 0, 1);
            top_grid_->addWidget(scatter_panel_.frame, 0, 2);
            top_grid_->setColumnStretch(0, 5);
            top_grid_->setColumnStretch(1, 3);
            top_grid_->setColumnStretch(2, 3);
            top_grid_->setRowStretch(0, 1);
        } else {
            top_grid_->addWidget(price_panel_.frame, 0, 0, 1, 2);
            top_grid_->addWidget(dist_panel_.frame, 1, 0);
            top_grid_->addWidget(scatter_panel_.frame, 1, 1);
            top_grid_->setColumnStretch(0, 1);
            top_grid_->setColumnStretch(1, 1);
            top_grid_->setRowStretch(0, 3);
            top_grid_->setRowStretch(1, 2);
        }
        top_cols_ = cols;
    }

    // Filter bar: the outcome / side switches drop to the second row when the first is too narrow.
    const int need = ch * 18 + outcome_seg_->sizeHint().width() + side_seg_->sizeHint().width() +
                     symbol_combo_->sizeHint().width() + reason_combo_->sizeHint().width() + 4 * 6;
    const bool compact = width < need;
    if (force || compact != bar_compact_) {
        bar_row_a_->removeWidget(outcome_seg_);
        bar_row_a_->removeWidget(side_seg_);
        bar_row_b_->removeWidget(outcome_seg_);
        bar_row_b_->removeWidget(side_seg_);
        if (compact) {
            bar_row_b_->insertWidget(0, outcome_seg_);
            bar_row_b_->insertWidget(1, side_seg_);
        } else {
            bar_row_a_->insertWidget(1, outcome_seg_);
            bar_row_a_->insertWidget(2, side_seg_);
        }
        bar_compact_ = compact;
    }
    totals_->set_columns(width >= ch * 70 ? 4 : 2);
}

void BtTradesView::fit_to_font() {
    const QFontMetrics fm(ui::fonts::font(-2));
    const int h = fm.height() * 14;
    for (QFrame* f : {price_panel_.frame, dist_panel_.frame, scatter_panel_.frame})
        f->setMinimumHeight(h);
    const int row_h = table_->verticalHeader()->defaultSectionSize();
    table_->setMinimumHeight(table_->horizontalHeader()->sizeHint().height() + row_h * 6);
    attr_table_->setMinimumHeight(attr_table_->horizontalHeader()->sizeHint().height() + row_h * 6);
    const int marker = std::max(5, fm.height() / 2);
    win_series_->setMarkerSize(marker);
    loss_series_->setMarkerSize(marker);
    top_cols_ = -1;
}

void BtTradesView::theme_scatter() {
    ui::ChartFactory::apply_theme(scatter_);
    scatter_->setMargins(QMargins(2, 2, 2, 2));
    scatter_->legend()->setVisible(true);
    scatter_->legend()->setAlignment(Qt::AlignTop);
    scatter_->legend()->setMarkerShape(QLegend::MarkerShapeFromSeries);
    const QColor up(ui::colors::POSITIVE());
    const QColor down(ui::colors::NEGATIVE());
    win_series_->setColor(up);
    win_series_->setBorderColor(up);
    loss_series_->setColor(down);
    loss_series_->setBorderColor(down);
    const QFont f = ui::fonts::font(-3);
    for (QValueAxis* a : {scatter_x_, scatter_y_}) {
        a->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
        a->setTitleFont(f);
        a->setLabelsFont(f);
    }
}

// ── Rendering ───────────────────────────────────────────────────────────────

void BtTradesView::show_run(const BtRunPtr& rp) {
    if (rp && rp == run_)
        return;
    run_ = rp;
    ++gen_;
    ccy_ = rp ? rp->currency : QString();
    sel_trade_.clear();
    zoomed_trade_.clear();
    ext_from_ = ext_to_ = kBtNaN;
    ext_label_.clear();
    ext_chip_->setVisible(false);
    ext_clear_->setVisible(false);
    chart_symbol_.clear();
    candle_lines_.clear();
    candle_zoom_pending_ = false;
    fallback_band_ = -1;
    render();
}

void BtTradesView::render() {
    const BtRun* r = run_.get();
    auto* m = static_cast<TvModel*>(trades_model_);
    auto* p = static_cast<TvTradeProxy*>(proxy_);

    // Facet combos.
    {
        const QSignalBlocker b1(symbol_combo_);
        const QSignalBlocker b2(reason_combo_);
        symbol_combo_->clear();
        symbol_combo_->addItem(tr("All symbols"), QString());
        reason_combo_->clear();
        reason_combo_->addItem(tr("All exits"), QString());
        if (r) {
            QMap<QString, int> reasons;
            QSet<QString> seen;
            QStringList syms;
            for (const BtTrade& t : r->trades) {
                if (!t.exit_reason.isEmpty())
                    reasons[t.exit_reason] += 1;
                if (!t.symbol.isEmpty() && !seen.contains(t.symbol)) {
                    seen.insert(t.symbol);
                    syms << t.symbol;
                }
            }
            syms.sort(Qt::CaseInsensitive);
            for (const QString& s : syms)
                symbol_combo_->addItem(s, s);
            for (auto it = reasons.cbegin(); it != reasons.cend(); ++it)
                reason_combo_->addItem(tr("%1 (%2)").arg(exit_reason_text(it.key()), count(it.value())), it.key());
        }
        symbol_combo_->setVisible(symbol_combo_->count() > 2);
    }

    // Rows (pointers into the run, which run_ keeps alive) and cumulative realised P&L in exit order.
    QVector<TvRow> rows;
    double pnl_lo = 0, pnl_hi = 0, mae_hi = 0, mfe_hi = 0;
    if (r) {
        const int n = static_cast<int>(r->trades.size());
        rows.resize(n);
        for (int i = 0; i < n; ++i) {
            const BtTrade& t = r->trades.at(i);
            rows[i].t = &t;
            rows[i].n = i + 1;
            if (std::isfinite(t.pnl)) {
                pnl_lo = std::min(pnl_lo, t.pnl);
                pnl_hi = std::max(pnl_hi, t.pnl);
            }
            if (std::isfinite(t.mae))
                mae_hi = std::max(mae_hi, std::fabs(t.mae));
            if (std::isfinite(t.mfe))
                mfe_hi = std::max(mfe_hi, t.mfe);
        }
        double cum = 0;
        for (int i : tv_exit_order(r->trades)) {
            const double v = r->trades.at(i).pnl;
            if (std::isfinite(v))
                cum += v;
            rows[i].cum = cum;
        }
    }
    pnl_delegate_->set_range(pnl_lo < pnl_hi ? pnl_lo : -1.0, pnl_lo < pnl_hi ? pnl_hi : 1.0);
    mae_delegate_->set_range(0.0, mae_hi > 0 ? mae_hi : 1.0);
    mfe_delegate_->set_range(0.0, mfe_hi > 0 ? mfe_hi : 1.0);

    TvFilter f;
    f.outcome = outcome_;
    f.side = side_;
    f.text = filter_edit_->text().trimmed();
    f.from = ext_from_;
    f.to = ext_to_;
    TvTotals tot;
    p->accept = tv_mask(rows, f, &tot);
    syncing_ = true;
    m->upsert(rows);
    syncing_ = false;
    totals_->set_rows(tv_totals_rows(tot, ccy_));
    count_label_->setText(r ? tr("%1 of %2 trades  ·  net %3")
                                  .arg(count(tot.shown), count(static_cast<double>(r->trades.size())),
                                       money(tot.net + tot.open_pnl, ccy_, true))
                            : QString());

    const bool none = !r || r->trades.isEmpty();
    page_seg_->setEnabled(!none);
    set_view_page(page_);

    render_attribution();
    render_distribution();
    render_scatter();

    // Price chart: candles of the first traded symbol that has bars, else the equity curve.
    const QStringList psyms = r ? btview::price_symbols(r->payload) : QStringList();
    {
        const QSignalBlocker block(price_combo_);
        price_combo_->clear();
        price_combo_->addItems(psyms);
        price_combo_->setVisible(psyms.size() > 1);
    }
    if (!psyms.isEmpty()) {
        price_stack_->setCurrentWidget(candle_);
        price_panel_.header->set_title(tr("PRICE + TRADES"));
        QString first = psyms.first();
        if (r && !r->trades.isEmpty() && psyms.contains(r->trades.last().symbol))
            first = r->trades.last().symbol;
        {
            const QSignalBlocker block(price_combo_);
            price_combo_->setCurrentIndex(std::max(0, static_cast<int>(psyms.indexOf(first))));
        }
        request_price_chart(first);
    } else {
        price_stack_->setCurrentWidget(fallback_);
        price_panel_.header->set_title(tr("EQUITY · SELECTED TRADE"));
        render_price_fallback();
    }
}

void BtTradesView::refresh_filter() {
    auto* m = static_cast<TvModel*>(trades_model_);
    auto* p = static_cast<TvTradeProxy*>(proxy_);
    TvFilter f;
    f.outcome = outcome_;
    f.side = side_;
    f.symbol = symbol_combo_->currentData().toString();
    f.reason = reason_combo_->currentData().toString();
    f.text = filter_edit_->text().trimmed();
    f.from = ext_from_;
    f.to = ext_to_;
    TvTotals tot;
    p->accept = tv_mask(m->rows(), f, &tot);
    syncing_ = true;
    p->refilter();
    syncing_ = false;
    totals_->set_rows(tv_totals_rows(tot, ccy_));
    const BtRun* r = run_.get();
    count_label_->setText(r ? tr("%1 of %2 trades  ·  net %3")
                                  .arg(count(tot.shown), count(static_cast<double>(r->trades.size())),
                                       money(tot.net + tot.open_pnl, ccy_, true))
                            : QString());
}

void BtTradesView::render_attribution() {
    auto* am = static_cast<TvAttrModel*>(attr_model_);
    const BtRun* r = run_.get();
    if (!r) {
        am->upsert({});
        return;
    }
    QHash<QString, QVector<double>> curves;
    QHash<QString, double> cum;
    for (int i : tv_exit_order(r->trades)) {
        const BtTrade& t = r->trades.at(i);
        if (!std::isfinite(t.pnl))
            continue;
        double& c = cum[t.symbol];
        c += t.pnl;
        curves[t.symbol].append(c);
    }
    QVector<TvAttrRow> rows;
    rows.reserve(r->attribution.size());
    double lo = 0, hi = 0;
    for (const BtAttribution& a : r->attribution) {
        TvAttrRow row;
        row.a = a;
        QVector<double> curve = curves.value(a.symbol);
        if (!curve.isEmpty())
            curve.prepend(0.0);
        row.spark = tv_thin(curve, kTvSparkPoints);
        if (std::isfinite(a.net_pnl)) {
            lo = std::min(lo, a.net_pnl);
            hi = std::max(hi, a.net_pnl);
        }
        rows.append(row);
    }
    attr_pnl_delegate_->set_range(lo < hi ? lo : -1.0, lo < hi ? hi : 1.0);
    am->upsert(rows);
}

void BtTradesView::render_distribution() {
    const BtRun* r = run_.get();
    pnl_dist_->clear_markers();
    QVector<double> v;
    int wins = 0;
    if (r) {
        v.reserve(r->trades.size());
        for (const BtTrade& t : r->trades) {
            if (t.open || !std::isfinite(t.pnl_pct))
                continue;
            v.append(t.pnl_pct * 100.0);
            wins += t.pnl_pct > 0 ? 1 : 0;
        }
    }
    if (v.size() < 2) {
        pnl_dist_->clear();
        pnl_dist_->set_state(ui::PaintedChart::Empty, r ? tr("Fewer than two closed trades") : tr("No backtest yet"));
        dist_panel_.header->set_badge(r ? count(static_cast<double>(v.size())) : QString());
        return;
    }
    pnl_dist_->set_state(ui::PaintedChart::Ready);
    pnl_dist_->set_samples(v);
    pnl_dist_->add_marker(pnl_dist_->mean(), tr("Mean"), ui::DistributionChart::MarkerRole::Mean);
    pnl_dist_->add_marker(pnl_dist_->quantile(0.5), tr("Median"), ui::DistributionChart::MarkerRole::Median);
    dist_panel_.header->set_badge(tr("%1 closed · %2 won")
                                      .arg(count(static_cast<double>(v.size())),
                                           pct(static_cast<double>(wins) / static_cast<double>(v.size()), 0)));
}

void BtTradesView::render_scatter() {
    win_points_.clear();
    loss_points_.clear();
    win_rows_.clear();
    loss_rows_.clear();
    const BtRun* r = run_.get();
    int with_mae = 0, closed = 0;
    if (r) {
        for (const BtTrade& t : r->trades) {
            if (t.open || !std::isfinite(t.pnl_pct))
                continue;
            ++closed;
            with_mae += std::isfinite(t.mae) ? 1 : 0;
        }
    }
    scatter_hold_x_ = with_mae == 0;
    scatter_panel_.header->set_title(scatter_hold_x_ ? tr("HOLD vs P&L") : tr("MAE vs P&L"));
    const int eligible = scatter_hold_x_ ? closed : with_mae;
    if (!r || eligible == 0) {
        win_series_->clear();
        loss_series_->clear();
        scatter_empty_->setText(r ? tr("No closed trades to plot") : tr("No backtest yet"));
        scatter_stack_->setCurrentWidget(scatter_empty_);
        scatter_panel_.header->set_badge(QString());
        return;
    }
    const int step = std::max(1, (eligible + kTvScatterMax - 1) / kTvScatterMax);
    double x_lo = 0, x_hi = 0, y_lo = 0, y_hi = 0;
    int k = 0;
    const int n = static_cast<int>(r->trades.size());
    for (int i = 0; i < n; ++i) {
        const BtTrade& t = r->trades.at(i);
        if (t.open || !std::isfinite(t.pnl_pct))
            continue;
        double x = kBtNaN;
        if (scatter_hold_x_)
            x = t.holding_bars >= 0 ? static_cast<double>(t.holding_bars) : kBtNaN;
        else
            x = std::isfinite(t.mae) ? t.mae * 100.0 : kBtNaN;
        if (!std::isfinite(x))
            continue;
        if ((k++ % step) != 0)
            continue;
        const double y = t.pnl_pct * 100.0;
        x_lo = std::min(x_lo, x);
        x_hi = std::max(x_hi, x);
        y_lo = std::min(y_lo, y);
        y_hi = std::max(y_hi, y);
        if (y > 0) {
            win_points_.append(QPointF(x, y));
            win_rows_.append(i);
        } else {
            loss_points_.append(QPointF(x, y));
            loss_rows_.append(i);
        }
    }
    win_series_->replace(win_points_);
    loss_series_->replace(loss_points_);
    const double xp = std::max(0.5, (x_hi - x_lo) * 0.06);
    const double yp = std::max(0.5, (y_hi - y_lo) * 0.06);
    scatter_x_->setRange(x_lo - (scatter_hold_x_ ? 0.0 : xp), x_hi + xp);
    scatter_y_->setRange(y_lo - yp, y_hi + yp);
    scatter_x_->setLabelFormat(scatter_hold_x_ ? QStringLiteral("%.0f") : QStringLiteral("%.1f%%"));
    scatter_x_->setTitleText(scatter_hold_x_ ? tr("Hold (bars)") : tr("MAE (%)"));
    scatter_y_->setTitleText(tr("P&L (%)"));
    scatter_stack_->setCurrentWidget(scatter_view_);
    QString badge;
    if (step > 1)
        badge = tr("every %1th of %2").arg(step).arg(count(eligible));
    else if (scatter_hold_x_)
        badge = tr("MAE not reported");
    scatter_panel_.header->set_badge(badge);
    scatter_panel_.frame->setToolTip(scatter_hold_x_
                                         ? tr("The engine did not report MAE; holding period vs P&L is shown instead")
                                         : tr("Worst open loss during the trade vs its final P&L; ● winners, ■ losers"));
}

void BtTradesView::render_price_fallback() {
    fallback_->clear();
    fallback_band_ = -1;
    const BtRun* r = run_.get();
    if (!r || r->t.size() < 2) {
        fallback_->set_state(ui::ChartState::Empty, r ? tr("This run has no equity curve") : tr("No backtest yet"));
        price_panel_.header->set_badge(QString());
        return;
    }
    fallback_->set_state(ui::ChartState::Ready);
    fallback_->set_value_formatter(TvTs::Axis::Left, money_formatter(ccy_), 0);
    TvTs::SeriesSpec s;
    s.name = tr("Equity");
    s.points = to_points(r->t, r->equity);
    s.color = ui::ChartColorRole::Accent;
    const QString ccy = ccy_;
    s.formatter = [ccy](double v) { return money(v, ccy); };
    fallback_->add_series(s);
    price_panel_.header->set_badge(tr("no price data in this run"));
    mark_selected_trade(false);
}

void BtTradesView::request_price_chart(const QString& symbol) {
    if (symbol.isEmpty())
        return;
    pending_symbol_ = symbol;
    if (!isVisible()) {
        load_on_show_ = true;
        return;
    }
    if (load_pending_)
        return;
    load_pending_ = true;
    const quint64 gen = gen_;
    // Parsing priceData is the heaviest step of this tab: its own event, after show_run() returned.
    QTimer::singleShot(0, this, [this, gen]() {
        load_pending_ = false;
        if (gen == gen_)
            load_price_chart();
    });
}

void BtTradesView::load_price_chart() {
    if (!run_ || pending_symbol_.isEmpty() || pending_symbol_ == chart_symbol_)
        return;
    btview::load_trade_chart(candle_, run_->payload, pending_symbol_, btview::FmtCtx{ccy_});
    chart_symbol_ = pending_symbol_;
    candle_lines_.clear(); // load_trade_chart cleared the annotations
    price_panel_.header->set_badge(chart_symbol_);
    {
        const QSignalBlocker block(price_combo_);
        const int i = price_combo_->findText(chart_symbol_);
        if (i >= 0)
            price_combo_->setCurrentIndex(i);
    }
    mark_selected_trade(false);
}

void BtTradesView::mark_selected_trade(bool follow_symbol) {
    const BtRun* r = run_.get();
    const BtTrade* t = nullptr;
    if (r && !sel_trade_.isEmpty()) {
        for (const BtTrade& x : r->trades) {
            if (x.id == sel_trade_) {
                t = &x;
                break;
            }
        }
    }
    if (price_stack_->currentWidget() == candle_) {
        if (t && follow_symbol && t->symbol != chart_symbol_ && price_combo_->findText(t->symbol) >= 0) {
            candle_zoom_pending_ = true;
            request_price_chart(t->symbol); // marks (and zooms to) the trade once loaded
            return;
        }
        const bool zoom = follow_symbol || candle_zoom_pending_;
        candle_zoom_pending_ = false;
        for (int id : candle_lines_)
            candle_->remove_annotation(id); // removes a band too
        candle_lines_.clear();
        candle_->clear_bands();
        // The selected trade as a shaded span from entry to exit (its B / S / X markers carry the prices); a
        // selection zooms to it with the context trade_window() gives, as the equity fallback does.
        if (t && r && t->symbol == chart_symbol_ && std::isfinite(t->entry_t)) {
            const double end = std::isfinite(t->exit_t) && !t->open ? t->exit_t : (r->t.isEmpty() ? t->entry_t : r->t.last());
            const int band = candle_->add_band(t->entry_t, std::max(end, t->entry_t), ui::ChartColorRole::Accent,
                                               tr("#%1 %2").arg(t->id, tv_is_short(*t) ? tr("SHORT") : tr("LONG")));
            if (band >= 0)
                candle_lines_ << band;
            double from = 0, to = 0;
            if (zoom && trade_window(*r, *t, &from, &to))
                candle_->set_visible_range(from, to);
        }
        return;
    }
    if (fallback_band_ >= 0) {
        fallback_->remove_annotation(fallback_band_);
        fallback_band_ = -1;
    }
    if (t && r && std::isfinite(t->entry_t) && !r->t.isEmpty()) {
        const double end = std::isfinite(t->exit_t) && !t->open ? t->exit_t : r->t.last();
        fallback_band_ = fallback_->add_band(t->entry_t, std::max(end, t->entry_t + 1.0), ui::ChartColorRole::Accent,
                                             tr("%1 %2").arg(t->symbol, tv_is_short(*t) ? tr("SHORT") : tr("LONG")));
        if (follow_symbol) {
            double from = 0, to = 0;
            if (trade_window(*r, *t, &from, &to))
                fallback_->set_x_range(from, to);
        }
    }
}

// ── Selection ───────────────────────────────────────────────────────────────

int BtTradesView::source_row_of(const QString& trade_id) const {
    const auto* m = static_cast<const TvModel*>(trades_model_);
    const QVector<TvRow>& rows = m->rows();
    for (int i = 0; i < rows.size(); ++i)
        if (rows.at(i).t && rows.at(i).t->id == trade_id)
            return i;
    return -1;
}

void BtTradesView::on_current_trade(int source_row) {
    const auto* m = static_cast<const TvModel*>(trades_model_);
    if (source_row < 0 || source_row >= m->rows().size() || !m->rows().at(source_row).t)
        return;
    sel_trade_ = m->rows().at(source_row).t->id;
    mark_selected_trade(true);
    if (selection_)
        emit selection_->trade_selected(sel_trade_, this);
}

void BtTradesView::on_activated_trade(int source_row) {
    const auto* m = static_cast<const TvModel*>(trades_model_);
    if (!run_ || source_row < 0 || source_row >= m->rows().size() || !m->rows().at(source_row).t)
        return;
    const BtTrade& t = *m->rows().at(source_row).t;
    if (zoomed_trade_ == t.id) {
        // Second double-click / Enter on the same trade: back to the full range.
        zoomed_trade_.clear();
        fallback_->reset_x_range();
        if (price_stack_->currentWidget() == candle_)
            candle_->reset_view(); // the price chart follows the data again
        if (selection_)
            emit selection_->time_range_selected(kBtNaN, kBtNaN, this);
        return;
    }
    zoomed_trade_ = t.id;
    sel_trade_ = t.id;
    mark_selected_trade(true);
    double from = 0, to = 0;
    if (trade_window(*run_, t, &from, &to)) {
        fallback_->set_x_range(from, to);
        if (selection_)
            emit selection_->time_range_selected(from, to, this);
    }
    if (selection_)
        emit selection_->trade_selected(t.id, this);
}

void BtTradesView::select_trade_by_id(const QString& trade_id, bool scroll) {
    int row = source_row_of(trade_id);
    if (row < 0)
        return;
    auto* p = static_cast<TvTradeProxy*>(proxy_);
    if (row < p->accept.size() && p->accept.at(row) == 0)
        reset_filters(); // the trade was filtered out: show everything again
    const QModelIndex pi = proxy_->mapFromSource(trades_model_->index(row, 0));
    if (!pi.isValid())
        return;
    if (page_ != 0)
        set_view_page(0);
    syncing_ = true;
    table_->selectionModel()->setCurrentIndex(pi, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    syncing_ = false;
    if (scroll)
        table_->scrollTo(pi, QAbstractItemView::PositionAtCenter);
    sel_trade_ = trade_id;
    mark_selected_trade(true);
}

void BtTradesView::reset_filters() {
    {
        const QSignalBlocker b1(filter_edit_);
        const QSignalBlocker b2(symbol_combo_);
        const QSignalBlocker b3(reason_combo_);
        filter_edit_->clear();
        symbol_combo_->setCurrentIndex(0);
        reason_combo_->setCurrentIndex(0);
    }
    outcome_ = 0;
    side_ = 2;
    set_segment(outcome_seg_, 0);
    set_segment(side_seg_, 2);
    ext_from_ = ext_to_ = kBtNaN;
    ext_label_.clear();
    ext_chip_->setVisible(false);
    ext_clear_->setVisible(false);
    refresh_filter();
}

void BtTradesView::set_view_page(int page) {
    page_ = std::clamp(page, 0, 1);
    set_segment(page_seg_, page_);
    const bool none = !run_ || run_->trades.isEmpty();
    if (none && run_)
        table_stack_->setCurrentWidget(empty_);
    else
        table_stack_->setCurrentIndex(page_);
}

void BtTradesView::focus_filter() {
    if (page_ != 0)
        set_view_page(0);
    filter_edit_->setFocus(Qt::ShortcutFocusReason);
    filter_edit_->selectAll();
}

} // namespace fincept::screens::bt
