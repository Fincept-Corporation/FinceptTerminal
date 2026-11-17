// src/screens/backtesting/BtPerformanceView.cpp
//
// PERFORMANCE sub-tab (spec 2.6): monthly heat map [STRATEGY | BENCHMARK | EXCESS] · yearly returns ·
// rolling Sharpe / volatility / beta · return distribution [DAILY | WEEKLY | MONTHLY] · period
// statistics (strategy | benchmark | difference). A 2 x 2 grid in a vertical scroll area that becomes
// one column when the view is narrow.
#include "screens/backtesting/BtRunViews.h"

#include "ui/charts/BarChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/MonthlyReturnsHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/PanelHeader.h"

#include <QDate>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::bt {

using services::backtest::BtMonthRow;

namespace {

using PfTs = ui::TimeSeriesChart;

constexpr double kPfDayMs = 86400000.0;
constexpr qint64 kPfUnixJulian = 2440588;
constexpr int kPfTwoColumnChars = 110; // view width (average characters) for the 2 x 2 grid

enum PfKind { kPfReturn, kPfShare, kPfBars, kPfCapture };

struct PfStatRow {
    QString label;
    double s = kBtNaN; ///< strategy
    double b = kBtNaN; ///< benchmark
    int kind = kPfReturn;
    QString tip;
};
using PfStatModel = ui::LiveTableModel<PfStatRow>;

struct PfStats {
    double best_day = kBtNaN, worst_day = kBtNaN, best_week = kBtNaN, worst_week = kBtNaN;
    double best_month = kBtNaN, worst_month = kBtNaN, best_year = kBtNaN, worst_year = kBtNaN;
    double pos_days = kBtNaN, pos_months = kBtNaN, avg_up = kBtNaN, avg_down = kBtNaN;
    double win_streak = kBtNaN, loss_streak = kBtNaN;
};

QVariant pf_right() {
    return QVariant((Qt::AlignRight | Qt::AlignVCenter).toInt());
}

double pf_month_start(int year, int month) {
    return static_cast<double>(QDate(year, month, 1).toJulianDay() - kPfUnixJulian) * kPfDayMs;
}

void pf_period_window(int year, int month, double* from, double* to) {
    if (month <= 0) {
        *from = pf_month_start(year, 1);
        *to = pf_month_start(year + 1, 1) - 1.0;
        return;
    }
    *from = pf_month_start(year, month);
    *to = (month == 12 ? pf_month_start(year + 1, 1) : pf_month_start(year, month + 1)) - 1.0;
}

/// Calendar key of a UTC epoch-ms stamp: 1 = Monday-based week, 2 = year * 12 + month, 3 = year.
qint64 pf_period_key(double t_ms, int kind) {
    const qint64 day = static_cast<qint64>(std::floor(t_ms / kPfDayMs));
    if (kind == 1)
        return static_cast<qint64>(std::floor(static_cast<double>(day + 3) / 7.0));
    const QDate d = QDate::fromJulianDay(day + kPfUnixJulian);
    return kind == 2 ? static_cast<qint64>(d.year()) * 12 + d.month() - 1 : static_cast<qint64>(d.year());
}

/// Per-bar returns as given (kind 0) or compounded per calendar week (1), month (2) or year (3).
QVector<double> pf_aggregate(const QVector<double>& t, const QVector<double>& r, int kind) {
    QVector<double> out;
    const int n = static_cast<int>(std::min(t.size(), r.size()));
    if (kind == 0) {
        out.reserve(n);
        for (int i = 0; i < n; ++i)
            if (std::isfinite(t[i]) && std::isfinite(r[i]))
                out.append(r[i]);
        return out;
    }
    qint64 key = std::numeric_limits<qint64>::min();
    double acc = 1.0;
    bool open = false;
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(t[i]) || !std::isfinite(r[i]))
            continue;
        const qint64 k = pf_period_key(t[i], kind);
        if (open && k != key) {
            out.append(acc - 1.0);
            acc = 1.0;
        }
        key = k;
        open = true;
        acc *= 1.0 + r[i];
    }
    if (open)
        out.append(acc - 1.0);
    return out;
}

/// The strategy's per-bar returns without the first bar (0 by construction).
void pf_strategy_returns(const BtRun& r, QVector<double>* t, QVector<double>* ret) {
    const int n = static_cast<int>(std::min(r.t.size(), r.returns.size()));
    t->reserve(n);
    ret->reserve(n);
    for (int i = 1; i < n; ++i) {
        if (std::isfinite(r.t[i]) && std::isfinite(r.returns[i])) {
            t->append(r.t[i]);
            ret->append(r.returns[i]);
        }
    }
}

/// The benchmark's per-bar returns (consecutive finite levels) and, in `aligned`, one value per bar
/// of the run (NaN where unknown).
void pf_bench_returns(const BtRun& r, QVector<double>* t, QVector<double>* ret, QVector<double>* aligned) {
    const int n = static_cast<int>(std::min(r.t.size(), r.benchmark.size()));
    aligned->fill(kBtNaN, static_cast<int>(r.t.size()));
    double prev = kBtNaN;
    for (int i = 0; i < n; ++i) {
        const double b = r.benchmark[i];
        if (!std::isfinite(b) || !std::isfinite(r.t[i]))
            continue;
        if (std::isfinite(prev) && prev != 0.0) {
            const double x = b / prev - 1.0;
            t->append(r.t[i]);
            ret->append(x);
            (*aligned)[i] = x;
        }
        prev = b;
    }
}

PfStats pf_period_stats(const QVector<double>& t, const QVector<double>& r) {
    PfStats s;
    const QVector<double> bars_r = pf_aggregate(t, r, 0);
    if (bars_r.isEmpty())
        return s;
    int pos = 0, up_n = 0, dn_n = 0, win = 0, loss = 0, win_max = 0, loss_max = 0;
    double up_sum = 0, dn_sum = 0;
    s.best_day = s.worst_day = bars_r.first();
    for (double x : bars_r) {
        s.best_day = std::max(s.best_day, x);
        s.worst_day = std::min(s.worst_day, x);
        if (x > 0) {
            ++pos;
            ++up_n;
            up_sum += x;
            ++win;
            loss = 0;
        } else if (x < 0) {
            ++dn_n;
            dn_sum += x;
            ++loss;
            win = 0;
        } else {
            win = loss = 0;
        }
        win_max = std::max(win_max, win);
        loss_max = std::max(loss_max, loss);
    }
    const int nb = static_cast<int>(bars_r.size());
    s.pos_days = static_cast<double>(pos) / nb;
    s.avg_up = up_n > 0 ? up_sum / up_n : kBtNaN;
    s.avg_down = dn_n > 0 ? dn_sum / dn_n : kBtNaN;
    s.win_streak = win_max;
    s.loss_streak = loss_max;
    const auto extremes = [](const QVector<double>& v, double* best, double* worst) {
        if (v.isEmpty())
            return;
        const auto mm = std::minmax_element(v.begin(), v.end());
        *worst = *mm.first;
        *best = *mm.second;
    };
    extremes(pf_aggregate(t, r, 1), &s.best_week, &s.worst_week);
    const QVector<double> months = pf_aggregate(t, r, 2);
    extremes(months, &s.best_month, &s.worst_month);
    if (!months.isEmpty())
        s.pos_months = static_cast<double>(std::count_if(months.begin(), months.end(), [](double x) { return x > 0; })) /
                       static_cast<double>(months.size());
    extremes(pf_aggregate(t, r, 3), &s.best_year, &s.worst_year);
    return s;
}

/// Rolling Sharpe (mean / sd x sqrt(ppy), risk-free 0) and annualised volatility over `w` values.
void pf_rolling(const QVector<double>& t, const QVector<double>& r, int w, double ppy, QVector<double>* xs,
                QVector<double>* sharpe, QVector<double>* vol) {
    const int n = static_cast<int>(std::min(t.size(), r.size()));
    if (w < 3 || n < w)
        return;
    const double ann = std::sqrt(ppy);
    double sum = 0, sq = 0;
    for (int i = 0; i < n; ++i) {
        sum += r[i];
        sq += r[i] * r[i];
        if (i >= w) {
            sum -= r[i - w];
            sq -= r[i - w] * r[i - w];
        }
        if (i < w - 1)
            continue;
        const double mean = sum / w;
        const double var = std::max(0.0, (sq - sum * sum / w) / (w - 1));
        const double sd = std::sqrt(var);
        xs->append(t[i]);
        vol->append(sd * ann);
        sharpe->append(sd > 1e-12 ? mean / sd * ann : kBtNaN);
    }
}

/// Rolling beta of `r` on `b` (same length; pairs with a NaN skipped) over `w` pairs.
void pf_rolling_beta(const QVector<double>& t, const QVector<double>& r, const QVector<double>& b, int w,
                     QVector<double>* xs, QVector<double>* beta) {
    QVector<double> tt, rr, bb;
    const int n = static_cast<int>(std::min({t.size(), r.size(), b.size()}));
    for (int i = 0; i < n; ++i) {
        if (std::isfinite(t[i]) && std::isfinite(r[i]) && std::isfinite(b[i])) {
            tt.append(t[i]);
            rr.append(r[i]);
            bb.append(b[i]);
        }
    }
    const int m = static_cast<int>(tt.size());
    if (w < 3 || m < w)
        return;
    double sr = 0, sb = 0, sbb = 0, srb = 0;
    for (int i = 0; i < m; ++i) {
        sr += rr[i];
        sb += bb[i];
        sbb += bb[i] * bb[i];
        srb += rr[i] * bb[i];
        if (i >= w) {
            sr -= rr[i - w];
            sb -= bb[i - w];
            sbb -= bb[i - w] * bb[i - w];
            srb -= rr[i - w] * bb[i - w];
        }
        if (i < w - 1)
            continue;
        const double vb = sbb - sb * sb / w;
        xs->append(tt[i]);
        beta->append(vb > 1e-16 ? (srb - sr * sb / w) / vb : kBtNaN);
    }
}

QString pf_value_text(double v, int kind) {
    switch (kind) {
        case kPfReturn:
            return pct(v, 2, true);
        case kPfShare:
        case kPfCapture:
            return pct(v, 1, false);
        case kPfBars:
            return bars(v);
        default:
            break;
    }
    return num(v, 2);
}

QString pf_diff_text(double s, double b, int kind) {
    if (!std::isfinite(s) || !std::isfinite(b))
        return dash();
    if (kind == kPfBars) {
        const double d = std::round(s - b);
        return d > 0 ? QStringLiteral("+") + count(d) : (d < 0 ? QStringLiteral("−") + count(-d) : count(0));
    }
    return pp(s - b, 2);
}

QVariant pf_stat_cell(const PfStatRow& row, int col, int role) {
    switch (role) {
        case Qt::DisplayRole:
            switch (col) {
                case 0:
                    return row.label;
                case 1:
                    return pf_value_text(row.s, row.kind);
                case 2:
                    return pf_value_text(row.b, row.kind);
                case 3:
                    return pf_diff_text(row.s, row.b, row.kind);
                default:
                    break;
            }
            break;
        case Qt::ForegroundRole: {
            double v = kBtNaN;
            if (row.kind == kPfReturn && (col == 1 || col == 2))
                v = col == 1 ? row.s : row.b;
            else if (col == 3 && row.kind != kPfBars && std::isfinite(row.s) && std::isfinite(row.b))
                v = row.s - row.b;
            const Tone tn = sign_tone(v);
            if (tn != Tone::None)
                return tone_color(tn);
            if (col == 0)
                return tone_color(Tone::Dim);
            break;
        }
        case Qt::TextAlignmentRole:
            if (col > 0)
                return pf_right();
            break;
        case Qt::ToolTipRole:
            return row.tip;
        default:
            break;
    }
    return {};
}

double pf_pick(const BtRun& r, const char* path, double computed) {
    const double v = r.metric(QString::fromLatin1(path));
    return std::isfinite(v) ? v : computed;
}

} // namespace

// ── Construction ────────────────────────────────────────────────────────────

BtPerformanceView::BtPerformanceView(BtSelection* selection, QWidget* parent) : BtRunView(selection, parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("btScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll_);
    grid_ = new QGridLayout(content);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(6);
    grid_->setVerticalSpacing(6);
    scroll_->setWidget(content);
    root->addWidget(scroll_);

    // ── Monthly returns ──
    heat_panel_ = make_panel(tr("MONTHLY RETURNS"), content, 4);
    heat_seg_ = make_segmented({tr("STRATEGY"), tr("BENCHMARK"), tr("EXCESS")}, 0,
                               [this](int i) {
                                   heat_mode_ = i;
                                   render_heatmap();
                               },
                               heat_panel_.header);
    heat_panel_.header->add_control(heat_seg_);
    heat_ = new ui::MonthlyReturnsHeatmap(heat_panel_.body_widget);
    heat_->set_show_year_total(true);
    heat_->set_fixed_clamp(10.0);
    heat_->setToolTip(tr("Colour scale clamped at ±10 % a month. Click a month: the charts zoom to it and TRADES is "
                         "filtered to it"));
    heat_panel_.body->addWidget(heat_, 1);
    connect(heat_, &ui::MonthlyReturnsHeatmap::cell_clicked, this, [this](int y, int m) { open_period(y, m, false); });

    // ── Yearly returns ──
    year_panel_ = make_panel(tr("YEARLY RETURNS"), content, 4);
    years_chart_ = new ui::BarChart(year_panel_.body_widget);
    years_chart_->set_mode(ui::BarChart::Mode::Grouped);
    years_chart_->set_show_values(true);
    years_chart_->set_value_formatter([](double v) { return pct(v, 1, true); });
    years_chart_->setToolTip(tr("Click a year: OVERVIEW zooms to it and TRADES is filtered to it"));
    year_panel_.body->addWidget(years_chart_, 1);
    connect(years_chart_, &ui::BarChart::bar_clicked, this, [this](int, int category) {
        if (category >= 0 && category < years_.size())
            open_period(years_.at(category), 0, true);
    });

    // ── Rolling ──
    roll_panel_ = make_panel(tr("ROLLING"), content, 4);
    rolling_ = make_time_chart(roll_panel_.body_widget, QStringLiteral("rolling_metrics"));
    rolling_->set_range_presets({QStringLiteral("1Y"), QStringLiteral("3Y"), QStringLiteral("5Y"), QStringLiteral("ALL")});
    roll_vol_pane_ = rolling_->add_pane(0.8);
    roll_third_pane_ = rolling_->add_pane(0.8);
    rolling_->set_pane_title(0, tr("Sharpe"));
    rolling_->set_pane_title(roll_vol_pane_, tr("Volatility (annualised)"));
    rolling_->set_value_formatter(PfTs::Axis::Left, number_formatter(1), 0);
    rolling_->set_value_formatter(PfTs::Axis::Left, fraction_formatter(0), roll_vol_pane_);
    rolling_->set_state(ui::ChartState::Empty, tr("No backtest yet"));
    roll_panel_.body->addWidget(rolling_, 1);
    roll_note_ = make_note(QString(), roll_panel_.body_widget);
    roll_panel_.body->addWidget(roll_note_);
    connect(rolling_, &PfTs::crosshair_moved, this, [this](double x) {
        if (selection_)
            emit selection_->crosshair_moved(x, this);
    });

    // ── Return distribution ──
    dist_panel_ = make_panel(tr("RETURN DISTRIBUTION"), content, 4);
    dist_seg_ = make_segmented({tr("DAILY"), tr("WEEKLY"), tr("MONTHLY")}, 0,
                               [this](int i) {
                                   dist_mode_ = i;
                                   render_distribution();
                               },
                               dist_panel_.header);
    dist_panel_.header->add_control(dist_seg_);
    dist_ = new ui::DistributionChart(dist_panel_.body_widget);
    dist_->set_normal_overlay(true);
    dist_->set_signed_colors(true);
    dist_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1, true); });
    dist_panel_.body->addWidget(dist_, 1);
    dist_note_ = make_note(QString(), dist_panel_.body_widget);
    dist_panel_.body->addWidget(dist_note_);

    // ── Period statistics ──
    stats_panel_ = make_panel(tr("PERIOD STATISTICS"), content, 0);
    stats_table_ = new QTableView(stats_panel_.body_widget);
    setup_table(stats_table_, QStringLiteral("period_statistics"));
    stats_table_->setSortingEnabled(false);
    stats_table_->horizontalHeader()->setSortIndicatorShown(false);
    auto* sm = new PfStatModel(this);
    sm->set_headers({tr("STATISTIC"), tr("STRATEGY"), tr("BENCHMARK"), tr("DIFFERENCE")});
    sm->set_cell([](const PfStatRow& row, int c, int role) { return pf_stat_cell(row, c, role); });
    stats_model_ = sm;
    stats_table_->setModel(sm);
    set_column_chars(stats_table_, {28, 14, 14, 14});
    ui::formatting::refresh_model_on_format_change(sm);
    stats_panel_.body->addWidget(stats_table_, 1);

    // ── Cross-highlight ──
    if (selection_) {
        connect(selection_, &BtSelection::time_range_selected, this, [this](double a, double b, QObject* origin) {
            if (bt_view_owns_origin(this, origin))
                return;
            if (std::isfinite(a) && std::isfinite(b))
                rolling_->set_x_range(a, b);
            else
                rolling_->reset_x_range();
        });
        connect(selection_, &BtSelection::crosshair_moved, this, [this](double x, QObject* origin) {
            if (!bt_view_owns_origin(this, origin))
                rolling_->set_linked_crosshair(x);
        });
    }

    // Export menus of the painted charts (the time chart has its own).
    {
        ui::ChartExport::MenuSpec heat_spec;
        heat_spec.suggested_name = QStringLiteral("monthly_returns");
        heat_spec.csv = [this]() {
            ui::ChartExport::Table t;
            t.headers << tr("Year");
            for (int m = 1; m <= 12; ++m)
                t.headers << QDate(2000, m, 1).toString(QStringLiteral("MMM"));
            t.headers << tr("Year total");
            const auto& data = heat_->monthly_returns();
            for (auto it = data.cbegin(); it != data.cend(); ++it) {
                QStringList row{QString::number(it.key())};
                for (int m = 1; m <= 12; ++m)
                    row << (it.value().contains(m) ? ui::formatting::export_number(it.value().value(m), 4) : QString());
                row << ui::formatting::export_number(ui::MonthlyReturnsHeatmap::compound_pct(it.value()), 4);
                t.rows.append(row);
            }
            return t;
        };
        ui::ChartExport::install_context_menu(heat_, heat_spec);

        ui::ChartExport::MenuSpec year_spec;
        year_spec.suggested_name = QStringLiteral("yearly_returns");
        year_spec.csv = [this]() {
            ui::ChartExport::Table t;
            t.headers = QStringList{tr("Year"), tr("Strategy"), tr("Benchmark"), tr("Excess")};
            if (run_)
                for (const auto& y : run_->yearly)
                    t.rows.append(QStringList{QString::number(y.year), ui::formatting::export_number(y.strategy, 6),
                                              ui::formatting::export_number(y.benchmark, 6),
                                              ui::formatting::export_number(y.excess, 6)});
            return t;
        };
        ui::ChartExport::install_context_menu(years_chart_, year_spec);

        ui::ChartExport::MenuSpec dist_spec;
        dist_spec.suggested_name = QStringLiteral("return_distribution");
        ui::ChartExport::install_context_menu(dist_, dist_spec);
    }

    // A format change re-renders the text; the rolling chart keeps the user's zoom.
    ui::formatting::on_format_changed(this, [this]() {
        render_heatmap();
        render_yearly();
        render_distribution();
        render_stats();
        rolling_->set_value_formatter(PfTs::Axis::Left, number_formatter(1), 0);
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        fit_to_font();
        relayout(width(), true);
    });
    fit_to_font();
    relayout(width(), true);
}

void BtPerformanceView::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout(e->size().width(), false);
}

void BtPerformanceView::relayout(int width, bool force) {
    const QFontMetrics fm(ui::fonts::font(0));
    const int cols = width >= kPfTwoColumnChars * std::max(5, fm.averageCharWidth()) ? 2 : 1;
    if (!force && cols == grid_cols_)
        return;
    grid_cols_ = cols;
    const QVector<QFrame*> cells = {heat_panel_.frame, year_panel_.frame, roll_panel_.frame, dist_panel_.frame};
    for (QFrame* f : cells)
        grid_->removeWidget(f);
    grid_->removeWidget(stats_panel_.frame);
    for (int i = 0; i < 6; ++i) {
        grid_->setRowStretch(i, 0);
        grid_->setColumnStretch(i, 0);
    }
    for (int i = 0; i < cells.size(); ++i)
        grid_->addWidget(cells.at(i), i / cols, i % cols);
    const int stats_row = (static_cast<int>(cells.size()) + cols - 1) / cols;
    grid_->addWidget(stats_panel_.frame, stats_row, 0, 1, cols);
    for (int c = 0; c < cols; ++c)
        grid_->setColumnStretch(c, 1);
    for (int r = 0; r < stats_row; ++r)
        grid_->setRowStretch(r, 3);
    grid_->setRowStretch(stats_row, 2);
}

void BtPerformanceView::fit_to_font() {
    const QFontMetrics fm(ui::fonts::font(-2));
    const int cell_h = std::max(260, fm.height() * 17);
    for (QFrame* f : {heat_panel_.frame, year_panel_.frame, roll_panel_.frame, dist_panel_.frame})
        f->setMinimumHeight(cell_h);
    const int row_h = stats_table_->verticalHeader()->defaultSectionSize();
    // All 16 statistics without an inner scroll bar (the page scrolls).
    stats_table_->setMinimumHeight(stats_table_->horizontalHeader()->sizeHint().height() + row_h * 16 + 4);
}

// ── Rendering ───────────────────────────────────────────────────────────────

void BtPerformanceView::show_run(const BtRunPtr& rp) {
    if (rp && rp == run_)
        return;
    run_ = rp;
    for (int i = 0; i < 3; ++i) {
        dist_samples_[i].clear();
        dist_ready_[i] = false;
    }
    render();
}

void BtPerformanceView::render() {
    render_heatmap();
    render_yearly();
    render_rolling();
    render_distribution();
    render_stats();
}

void BtPerformanceView::render_heatmap() {
    const BtRun* r = run_.get();
    const bool has_b = r && !r->benchmark_monthly.isEmpty();
    const auto buttons = heat_seg_->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 1; i < buttons.size(); ++i) {
        buttons.at(i)->setEnabled(has_b);
        buttons.at(i)->setToolTip(has_b ? QString() : tr("This run has no benchmark monthly returns"));
    }
    if (!has_b && heat_mode_ != 0) {
        heat_mode_ = 0;
        set_segment(heat_seg_, 0);
    }
    if (!r || r->monthly.isEmpty()) {
        heat_->clear();
        heat_->set_state(ui::PaintedChart::Empty, r ? tr("No monthly returns in this run") : tr("No backtest yet"));
        heat_panel_.header->set_badge(QString());
        return;
    }
    ui::MonthlyReturnsHeatmap::MonthlyReturns data;
    const auto put = [&data](const QVector<BtMonthRow>& rows) {
        for (const BtMonthRow& m : rows)
            for (int i = 0; i < 12; ++i)
                if (std::isfinite(m.months[i]))
                    data[m.year][i + 1] = m.months[i] * 100.0;
    };
    if (heat_mode_ == 1) {
        put(r->benchmark_monthly);
    } else if (heat_mode_ == 2) {
        // Excess = strategy − benchmark, only where both are known.
        QHash<int, const BtMonthRow*> bench;
        for (const BtMonthRow& b : r->benchmark_monthly)
            bench.insert(b.year, &b);
        for (const BtMonthRow& m : r->monthly) {
            const BtMonthRow* b = bench.value(m.year, nullptr);
            if (!b)
                continue;
            for (int i = 0; i < 12; ++i)
                if (std::isfinite(m.months[i]) && std::isfinite(b->months[i]))
                    data[m.year][i + 1] = (m.months[i] - b->months[i]) * 100.0;
        }
    } else {
        put(r->monthly);
    }
    if (data.isEmpty()) {
        heat_->clear();
        heat_->set_state(ui::PaintedChart::Empty, tr("No overlapping months"));
    } else {
        heat_->set_state(ui::PaintedChart::Ready);
        heat_->set_monthly_returns(data);
    }
    const QString what = heat_mode_ == 1   ? (r->benchmark_symbol.isEmpty() ? tr("benchmark") : r->benchmark_symbol)
                         : heat_mode_ == 2 ? tr("excess over %1").arg(r->benchmark_symbol.isEmpty()
                                                                          ? tr("benchmark")
                                                                          : r->benchmark_symbol)
                                           : tr("strategy");
    heat_panel_.header->set_badge(tr("%1 · %2 y").arg(what).arg(data.size()));
}

void BtPerformanceView::render_yearly() {
    years_.clear();
    const BtRun* r = run_.get();
    if (!r || r->yearly.isEmpty()) {
        years_chart_->clear();
        years_chart_->set_state(ui::PaintedChart::Empty, r ? tr("No calendar-year returns in this run") : tr("No backtest yet"));
        year_panel_.header->set_badge(QString());
        return;
    }
    QStringList cats;
    ui::BarSeries strat{tr("Strategy"), {}, {}};
    ui::BarSeries bench{r->benchmark_symbol.isEmpty() ? tr("Benchmark") : r->benchmark_symbol.toUpper(), {}, {}};
    bool any_bench = false;
    for (const auto& y : r->yearly) {
        years_.append(y.year);
        cats << QString::number(y.year);
        strat.values << y.strategy;
        bench.values << y.benchmark;
        any_bench = any_bench || std::isfinite(y.benchmark);
    }
    years_chart_->set_state(ui::PaintedChart::Ready);
    years_chart_->set_categories(cats);
    if (any_bench)
        years_chart_->set_series({strat, bench});
    else
        years_chart_->set_series({strat});
    year_panel_.header->set_badge(r->yearly_computed ? tr("compounded from the months") : QString());
}

void BtPerformanceView::render_rolling() {
    rolling_->clear();
    const BtRun* r = run_.get();
    if (!r || r->t.size() < 3) {
        rolling_->set_state(ui::ChartState::Empty, r ? tr("Too few bars for rolling statistics") : tr("No backtest yet"));
        roll_note_->setText(QString());
        roll_panel_.header->set_badge(QString());
        return;
    }
    const double ppy = std::isfinite(r->periods_per_year) && r->periods_per_year > 0 ? r->periods_per_year : 252.0;
    QVector<double> st, sr;
    pf_strategy_returns(*r, &st, &sr);
    const int n = static_cast<int>(st.size());
    int w = r->rolling_window > 0 ? r->rolling_window : 63;
    const bool engine = !r->rolling_sharpe.empty() || !r->rolling_volatility.empty();
    if (!engine)
        w = std::min(w, std::max(10, n / 3));

    QVector<QPointF> sharpe, vol, third;
    bool third_is_beta = false;
    bool third_engine = false;
    if (engine) {
        sharpe = to_points(r->rolling_sharpe.t, r->rolling_sharpe.v);
        vol = to_points(r->rolling_volatility.t, r->rolling_volatility.v);
    } else {
        QVector<double> xs, sh, vo;
        pf_rolling(st, sr, w, ppy, &xs, &sh, &vo);
        sharpe = to_points(xs, sh);
        vol = to_points(xs, vo);
    }
    if (!r->rolling_beta.empty()) {
        third = to_points(r->rolling_beta.t, r->rolling_beta.v);
        third_is_beta = true;
        third_engine = true;
    } else if (r->has_benchmark()) {
        QVector<double> bt, br, aligned;
        pf_bench_returns(*r, &bt, &br, &aligned);
        QVector<double> rr(static_cast<int>(r->t.size()), kBtNaN);
        const int m = static_cast<int>(std::min(r->t.size(), r->returns.size()));
        for (int i = 1; i < m; ++i)
            rr[i] = r->returns[i];
        QVector<double> xs, beta;
        pf_rolling_beta(r->t, rr, aligned, w, &xs, &beta);
        third = to_points(xs, beta);
        third_is_beta = !third.isEmpty();
    }
    if (!third_is_beta) {
        if (!r->rolling_drawdown.empty()) {
            third = to_points(r->rolling_drawdown.t, r->rolling_drawdown.v);
            third_engine = true;
        } else {
            third = to_points(r->t, r->drawdown);
        }
    }
    if (sharpe.isEmpty() && vol.isEmpty()) {
        rolling_->set_state(ui::ChartState::Empty, tr("Too few bars for a %1-bar window").arg(w));
        roll_note_->setText(QString());
        roll_panel_.header->set_badge(QString());
        return;
    }
    rolling_->set_state(ui::ChartState::Ready);

    PfTs::SeriesSpec s1;
    s1.name = tr("Rolling Sharpe");
    s1.points = sharpe;
    s1.color = ui::ChartColorRole::Accent;
    s1.rebase = false;
    s1.formatter = number_formatter(2);
    rolling_->add_series(s1);
    rolling_->add_hline(0.0, QString(), ui::ChartColorRole::Muted, 0);

    PfTs::SeriesSpec s2;
    s2.name = tr("Rolling volatility");
    s2.points = vol;
    s2.pane = roll_vol_pane_;
    s2.color = ui::ChartColorRole::Info;
    s2.rebase = false;
    s2.formatter = fraction_formatter(1);
    rolling_->add_series(s2);

    PfTs::SeriesSpec s3;
    s3.points = third;
    s3.pane = roll_third_pane_;
    s3.rebase = false;
    if (third_is_beta) {
        s3.name = tr("Rolling beta");
        s3.color = ui::ChartColorRole::Cyan;
        s3.formatter = number_formatter(2);
        rolling_->set_pane_title(roll_third_pane_, tr("Beta vs %1").arg(r->benchmark_symbol.isEmpty()
                                                                            ? tr("benchmark")
                                                                            : r->benchmark_symbol.toUpper()));
        rolling_->set_value_formatter(PfTs::Axis::Left, number_formatter(1), roll_third_pane_);
        rolling_->add_series(s3);
        rolling_->add_hline(1.0, QString(), ui::ChartColorRole::Muted, roll_third_pane_);
    } else {
        s3.name = third_engine ? tr("Rolling drawdown") : tr("Drawdown");
        s3.style = PfTs::Style::Area;
        s3.baseline = 0.0;
        s3.color = ui::ChartColorRole::Warning;
        s3.formatter = fraction_formatter(1);
        rolling_->set_pane_title(roll_third_pane_, third_engine ? tr("Rolling drawdown") : tr("Drawdown (no benchmark)"));
        rolling_->set_value_formatter(PfTs::Axis::Left, fraction_formatter(0), roll_third_pane_);
        rolling_->add_series(s3);
    }
    // An engine that reports only some of the rolling series: its empty panes give their space to the
    // others instead of showing a blank strip. Show first, then hide: hiding the only shown pane is refused.
    const int roll_panes[3] = {0, roll_vol_pane_, roll_third_pane_};
    const bool roll_shown[3] = {!sharpe.isEmpty(), !vol.isEmpty(), !third.isEmpty()};
    for (int i = 0; i < 3; ++i)
        if (roll_shown[i])
            rolling_->set_pane_visible(roll_panes[i], true);
    for (int i = 0; i < 3; ++i)
        if (!roll_shown[i])
            rolling_->set_pane_visible(roll_panes[i], false);
    roll_panel_.header->set_badge(tr("%1-bar window").arg(w));
    roll_note_->setText(engine ? tr("Window %1 bars, as computed by the engine.").arg(w)
                               : tr("Window %1 bars, computed here from the bar returns (risk-free rate 0, "
                                    "%2 periods a year): the engine did not report rolling metrics.")
                                     .arg(w)
                                     .arg(num(ppy, 0)));
}

void BtPerformanceView::render_distribution() {
    const BtRun* r = run_.get();
    const auto buttons = dist_seg_->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    if (!buttons.isEmpty())
        buttons.first()->setText(r && r->intraday ? tr("BAR") : tr("DAILY"));
    dist_->clear_markers();
    if (!r) {
        dist_->clear();
        dist_->set_state(ui::PaintedChart::Empty, tr("No backtest yet"));
        dist_note_->setText(QString());
        dist_panel_.header->set_badge(QString());
        return;
    }
    const int mode = std::clamp(dist_mode_, 0, 2);
    if (!dist_ready_[mode]) {
        QVector<double> st, sr;
        pf_strategy_returns(*r, &st, &sr);
        QVector<double> v = pf_aggregate(st, sr, mode);
        for (double& x : v)
            x *= 100.0;
        dist_samples_[mode] = v;
        dist_ready_[mode] = true;
    }
    const QVector<double>& s = dist_samples_[mode];
    if (s.size() < 2) {
        dist_->clear();
        dist_->set_state(ui::PaintedChart::Empty, tr("Too few returns for a distribution"));
        dist_note_->setText(QString());
        dist_panel_.header->set_badge(QString());
        return;
    }
    dist_->set_state(ui::PaintedChart::Ready);
    dist_->set_samples(s);
    const double mean = dist_->mean();
    const double var95 = dist_->quantile(0.05);
    QVector<double> sorted = s;
    std::sort(sorted.begin(), sorted.end());
    double tail = 0;
    int tail_n = 0;
    for (double x : sorted) {
        if (x > var95)
            break;
        tail += x;
        ++tail_n;
    }
    const double cvar95 = tail_n > 0 ? tail / tail_n : kBtNaN;
    dist_->add_marker(mean, tr("Mean"), ui::DistributionChart::MarkerRole::Mean);
    dist_->add_marker(dist_->quantile(0.5), tr("Median"), ui::DistributionChart::MarkerRole::Median);
    dist_->add_marker(var95, tr("VaR 95"), ui::DistributionChart::MarkerRole::VaR);
    if (std::isfinite(cvar95))
        dist_->add_marker(cvar95, tr("CVaR 95"), ui::DistributionChart::MarkerRole::CVaR);

    // Moments for the note.
    const int n = static_cast<int>(s.size());
    double m2 = 0, m3 = 0;
    for (double x : s) {
        const double d = x - mean;
        m2 += d * d;
        m3 += d * d * d;
    }
    m2 /= n;
    m3 /= n;
    const double skew = m2 > 0 ? m3 / std::pow(m2, 1.5) : kBtNaN;
    const QString what = mode == 0 ? (r->intraday ? tr("bar") : tr("daily")) : (mode == 1 ? tr("weekly") : tr("monthly"));
    dist_panel_.header->set_badge(tr("%1 %2 returns").arg(count(n), what));
    dist_note_->setText(tr("Mean %1 · σ %2 · skew %3 · VaR 95 %4 · CVaR 95 %5 (historical, not a forecast)")
                            .arg(ui::formatting::pct(mean, 2, true), ui::formatting::pct(dist_->stdev(), 2, false),
                                 ratio(skew, 2), ui::formatting::pct(var95, 2, true),
                                 std::isfinite(cvar95) ? ui::formatting::pct(cvar95, 2, true) : dash()));
}

void BtPerformanceView::render_stats() {
    auto* sm = static_cast<PfStatModel*>(stats_model_);
    const BtRun* r = run_.get();
    if (!r) {
        sm->upsert({});
        stats_panel_.header->set_badge(QString());
        return;
    }
    QVector<double> st, sr;
    pf_strategy_returns(*r, &st, &sr);
    const PfStats cs = pf_period_stats(st, sr);
    PfStats bs;
    if (r->has_benchmark()) {
        QVector<double> bt, br, aligned;
        pf_bench_returns(*r, &bt, &br, &aligned);
        bs = pf_period_stats(bt, br);
    }
    const bool bar = r->intraday;
    const QString nb = r->has_benchmark() ? QString() : tr("No benchmark in this run");
    QVector<PfStatRow> rows;
    const auto add = [&rows, &nb](const QString& label, double s, double b, int kind, const QString& tip) {
        rows.append(PfStatRow{label, s, b, kind, tip.isEmpty() ? nb : tip});
    };
    add(bar ? tr("Best bar") : tr("Best day"), pf_pick(*r, "periodStats.bestDay", cs.best_day), bs.best_day, kPfReturn, {});
    add(bar ? tr("Worst bar") : tr("Worst day"), pf_pick(*r, "periodStats.worstDay", cs.worst_day), bs.worst_day,
        kPfReturn, {});
    add(tr("Best week"), pf_pick(*r, "periodStats.bestWeek", cs.best_week), bs.best_week, kPfReturn, {});
    add(tr("Worst week"), pf_pick(*r, "periodStats.worstWeek", cs.worst_week), bs.worst_week, kPfReturn, {});
    add(tr("Best month"), pf_pick(*r, "periodStats.bestMonth", cs.best_month), bs.best_month, kPfReturn, {});
    add(tr("Worst month"), pf_pick(*r, "periodStats.worstMonth", cs.worst_month), bs.worst_month, kPfReturn, {});
    add(tr("Best year"), pf_pick(*r, "periodStats.bestYear", cs.best_year), bs.best_year, kPfReturn,
        tr("Calendar years; the first and last may be partial"));
    add(tr("Worst year"), pf_pick(*r, "periodStats.worstYear", cs.worst_year), bs.worst_year, kPfReturn,
        tr("Calendar years; the first and last may be partial"));
    add(bar ? tr("Positive bars") : tr("Positive days"), pf_pick(*r, "periodStats.positiveDays", cs.pos_days),
        bs.pos_days, kPfShare, {});
    add(tr("Positive months"), pf_pick(*r, "periodStats.positiveMonths", cs.pos_months), bs.pos_months, kPfShare, {});
    add(bar ? tr("Avg up bar") : tr("Avg up day"), pf_pick(*r, "periodStats.avgUpDay", cs.avg_up), bs.avg_up, kPfReturn,
        {});
    add(bar ? tr("Avg down bar") : tr("Avg down day"), pf_pick(*r, "periodStats.avgDownDay", cs.avg_down), bs.avg_down,
        kPfReturn, {});
    add(tr("Longest win streak"), pf_pick(*r, "periodStats.longestWinStreak", cs.win_streak), bs.win_streak, kPfBars,
        tr("Consecutive bars with a positive return"));
    add(tr("Longest loss streak"), pf_pick(*r, "periodStats.longestLossStreak", cs.loss_streak), bs.loss_streak,
        kPfBars, tr("Consecutive bars with a negative return"));
    const double upc = r->metric(QStringLiteral("advancedMetrics.upCapture"));
    const double dnc = r->metric(QStringLiteral("advancedMetrics.downCapture"));
    add(tr("Up capture"), upc, r->has_benchmark() ? 1.0 : kBtNaN, kPfCapture,
        std::isfinite(upc) ? tr("Strategy return on the benchmark's up bars, relative to the benchmark (100 % = the same)")
                           : r->why_blank(QStringLiteral("advancedMetrics.upCapture")));
    add(tr("Down capture"), dnc, r->has_benchmark() ? 1.0 : kBtNaN, kPfCapture,
        std::isfinite(dnc) ? tr("Strategy return on the benchmark's down bars, relative to the benchmark (lower is better)")
                           : r->why_blank(QStringLiteral("advancedMetrics.downCapture")));
    sm->upsert(rows);
    stats_panel_.header->set_badge(r->has_benchmark()
                                       ? tr("vs %1").arg(r->benchmark_symbol.isEmpty() ? tr("benchmark")
                                                                                        : r->benchmark_symbol.toUpper())
                                       : tr("no benchmark"));
}

void BtPerformanceView::open_period(int year, int month, bool to_overview) {
    if (year <= 0)
        return;
    double from = 0, to = 0;
    pf_period_window(year, month, &from, &to);
    rolling_->set_x_range(from, to);
    if (!selection_)
        return;
    const QString label = month <= 0 ? QString::number(year)
                                     : QStringLiteral("%1-%2").arg(year).arg(month, 2, 10, QLatin1Char('0'));
    emit selection_->month_selected(year, month, this);
    emit selection_->time_range_selected(from, to, this);
    emit selection_->trades_filter_requested(from, to, tr("in %1").arg(label), this);
    if (to_overview)
        emit selection_->tab_requested(QStringLiteral("overview"));
}

} // namespace fincept::screens::bt
