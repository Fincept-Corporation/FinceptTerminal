// src/screens/portfolio/views/QuantStatsView.cpp
#include "screens/portfolio/views/QuantStatsView.h"

#include "core/logging/Logger.h"
#include "screens/portfolio/PortfolioSharedRun.h"
#include "services/portfolio/PortfolioAnalyticsService.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/FanChart.h"
#include "ui/charts/MonthlyReturnsHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/StatTile.h"

#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

using fincept::services::AnalyticsResult;
using fincept::services::PortfolioAnalyticsService;

namespace fincept::screens {

// ── Helpers ───────────────────────────────────────────────────────────────────

/// An automatic or user run that has not answered after this long shows "Timed out" + RETRY.
static constexpr int kQsTimeoutS = 120;

// Locale-aware (grouping, decimal separator, em dash for NaN) through ui/formatting.
static QString qs_pct_str(double v, int dp = 2) {
    return ui::formatting::pct(v, dp, true);
}

/// Unsigned percentage, for quantities that are not returns (volatility, downside
/// deviation, win/loss rate, probability of loss) — "+18.40%" read as a gain.
static QString qs_upct_str(double v, int dp = 2) {
    return ui::formatting::pct(v, dp, false);
}

static QString ratio_str(double v, int dp = 2) {
    return ui::formatting::number(v, dp);
}

/// A number from the script's JSON, scaled; nullopt for null / missing. The script reports
/// undefined statistics (Sharpe with zero deviation, Calmar with no drawdown, Profit Factor
/// with no losing day, ...) as null — they must read "--", not a real 0.00.
static std::optional<double> qs_num(const QJsonObject& o, const QString& key, double scale = 1.0) {
    const QJsonValue v = o.value(key);
    if (!v.isDouble())
        return std::nullopt;
    return v.toDouble() * scale;
}

static QString opt_pct_str(const std::optional<double>& v, int dp = 2) {
    return v ? qs_pct_str(*v, dp) : QStringLiteral("--");
}

static QString opt_upct_str(const std::optional<double>& v, int dp = 2) {
    return v ? qs_upct_str(*v, dp) : QStringLiteral("--");
}

static QString opt_ratio_str(const std::optional<double>& v, int dp = 2) {
    return v ? ratio_str(*v, dp) : QStringLiteral("--");
}

/// Direction of a signed figure (a return, a loss): +1 / -1, 0 when unknown.
static int qs_sign(const std::optional<double>& v) {
    if (!v)
        return 0;
    return *v >= 0.0 ? 1 : -1;
}

/// Tile tone for a signed figure; the value text carries the sign too (never colour alone).
static ui::StatTile::Tone qs_tone(const std::optional<double>& v) {
    const int s = qs_sign(v);
    return s > 0 ? ui::StatTile::Tone::Positive : s < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral;
}

/// The script's own plain sentence: a multi-line message (a traceback) is cut to its last line.
static QString qs_plain_error(const QString& raw, const QString& fallback) {
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString msg = lines.isEmpty() ? QString() : lines.last().trimmed();
    if (msg.isEmpty())
        msg = fallback;
    return msg.left(300);
}

// Table row height: `design_h` at the default font size, growing with it.
static int qs_row_height(int design_h) {
    return std::max(design_h, ui::fonts::font_px(-3) + design_h - 11);
}

static QTableWidgetItem* qs_make_item(const QString& text, Qt::Alignment align, const QColor& color) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setForeground(color);
    return item;
}

/// Colour of a value cell: the direction colour for a signed figure, text otherwise.
static QColor qs_value_color(int sign) {
    if (sign > 0)
        return QColor(ui::colors::POSITIVE());
    if (sign < 0)
        return QColor(ui::colors::NEGATIVE());
    return QColor(ui::colors::TEXT_PRIMARY());
}

// Apply standard dark styling to a QTableWidget.
static void qs_style_table(QTableWidget* t) {
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setShowGrid(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                             "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                             "QHeaderView::section { background:%4; color:%5; border:none;"
                             "  border-bottom:2px solid %6; padding:4px 8px; font-size:" + fincept::ui::fonts::px(-5) + "px;"
                             "  font-weight:700; letter-spacing:0.5px; }")
                         .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                              ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
}

/// Amber section title used across the result tabs.
static QString qs_title_style() {
    return QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; letter-spacing:1px;")
        .arg(ui::colors::AMBER());
}

/// A result page's own background, scoped to it (an unscoped rule would reach the
/// shared state widgets inside).
static void qs_scoped_background(QWidget* w, const QString& name) {
    w->setObjectName(name);
    w->setStyleSheet(QString("#%1 { background:%2; }").arg(name, ui::colors::BG_BASE()));
}

// The page stack inside `tabs` (QTabWidget's own child), so a tab page built there is not
// moved — and re-styled as a whole — by addTab(). `fallback` when it cannot be found.
static QWidget* qs_page_parent(QTabWidget* tabs, QWidget* fallback) {
    if (!tabs)
        return fallback;
    QWidget* stack = tabs->findChild<QStackedWidget*>(QStringLiteral("qt_tabwidget_stackedwidget"),
                                                     Qt::FindDirectChildrenOnly);
    return stack ? stack : fallback;
}

// One line on a QuantStats time-series chart. Values are fractions (×100 for display).
struct QsLine {
    QsLine(QString n, QJsonArray v, QColor c, bool dash = false, bool fill = false, double s = 100.0)
        : name(std::move(n)), values(std::move(v)), color(c), dashed(dash), fill_to_zero(fill), scale(s) {}
    QString name;
    QJsonArray values;
    QColor color;
    bool dashed = false;
    bool fill_to_zero = false; // shade between the line and 0 (drawdown)
    double scale = 100.0;
};

static double qs_date_ms(const QJsonValue& v) {
    const QDate d = QDate::fromString(v.toString().left(10), Qt::ISODate);
    return d.isValid() ? static_cast<double>(QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch())
                       : std::numeric_limits<double>::quiet_NaN();
}

// Date-axis chart for the series the script returns (cumulative, drawdown,
// rolling) on the shared TimeSeriesChart: crosshair + tooltip, wheel zoom, range
// presets, image / CSV export. Null points (rolling warm-up) are gaps. A y_fmt
// with "%%" formats values as percent, anything else as a plain number.
static QWidget* qs_line_chart(const QJsonArray& dates, const QVector<QsLine>& lines, const char* y_fmt,
                              QWidget* parent) {
    auto* chart = new ui::TimeSeriesChart(parent);
    chart->set_utc(true);
    chart->set_show_mode_buttons(false);
    chart->set_show_legend(lines.size() > 1);
    chart->set_range_presets({"3M", "6M", "YTD", "1Y", "ALL"});
    chart->set_time_formatter([](double x) { return ui::formatting::bar_date(static_cast<qint64>(x / 1000.0)); });
    const bool percent = QString::fromLatin1(y_fmt).contains(QStringLiteral("%%"));
    const ui::TimeSeriesChart::ValueFormatter fmt = percent ? ui::TimeSeriesChart::ValueFormatter(
                                                                  [](double v) { return ui::formatting::pct(v, 2); })
                                                            : ui::TimeSeriesChart::ValueFormatter(
                                                                  [](double v) { return ui::formatting::number(v, 2); });
    chart->set_value_formatter(ui::TimeSeriesChart::Axis::Left, fmt);
    for (const auto& ln : lines) {
        QVector<QPointF> pts;
        const int n = static_cast<int>(std::min(dates.size(), ln.values.size()));
        pts.reserve(n);
        for (int i = 0; i < n; ++i) {
            const double x = qs_date_ms(dates[i]);
            if (!std::isfinite(x))
                continue;
            pts.append({x, ln.values[i].isDouble() ? ln.values[i].toDouble() * ln.scale
                                                   : std::numeric_limits<double>::quiet_NaN()});
        }
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = ln.name;
        spec.points = pts;
        spec.color = ui::TimeSeriesChart::ColorRole::Custom;
        spec.custom_color = ln.color;
        spec.dashed = ln.dashed;
        spec.rebase = false;
        spec.formatter = fmt;
        if (ln.fill_to_zero) {
            spec.style = ui::TimeSeriesChart::Style::Area;
            spec.baseline = 0.0;
        }
        chart->add_series(spec);
    }
    return chart;
}

// ── Constructor ───────────────────────────────────────────────────────────────

QuantStatsView::QuantStatsView(QWidget* parent) : QWidget(parent) {
    build_ui();
    // Tables and built charts carry their colours: re-render once the new
    // appearance has settled (never inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &QuantStatsView::on_appearance_settled);
    // Currency / number format changed: the same path — every tab with a result is
    // re-rendered from its remembered payload (the visible one now, the others when
    // shown). Nothing re-runs.
    ui::formatting::on_format_changed(this, [this]() { on_appearance_settled(); });
}

QuantStatsView::~QuantStatsView() {
    // The panes' cancel callbacks use the request maps: drop the panes while the
    // members still exist (Qt would delete them only after the members are gone).
    for (const auto& r : std::as_const(qs_requests_))
        r.cancel();
    for (const auto& r : std::as_const(mc_requests_))
        r.cancel();
    delete tabs_;
    tabs_ = nullptr;
}

// ── build_ui ─────────────────────────────────────────────────────────────────

void QuantStatsView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header: title · as-of · period · RUN on one row, the note wraps below ──
    auto* header = new QWidget(this);
    header->setMinimumHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));

    auto* hv = new QVBoxLayout(header);
    hv->setContentsMargins(12, 6, 12, 6);
    hv->setSpacing(2);
    auto* h_lay = new QHBoxLayout;
    h_lay->setSpacing(8);

    title_lbl_ = new QLabel(tr("QUANTSTATS ANALYSIS"), header);
    title_lbl_->setStyleSheet(qs_title_style());
    h_lay->addWidget(title_lbl_);
    h_lay->addStretch(1);

    qs_status_ = new QLabel(header);
    qs_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    h_lay->addWidget(qs_status_);

    // History window of the analysis (was a fixed one year).
    qs_period_cb_ = new QComboBox(header);
    for (const auto& [key, label] : {std::pair{"6mo", tr("6 months")}, std::pair{"1y", tr("1 year")},
                                     std::pair{"2y", tr("2 years")}, std::pair{"3y", tr("3 years")},
                                     std::pair{"5y", tr("5 years")}, std::pair{"10y", tr("10 years")},
                                     std::pair{"ytd", tr("Year to date")}, std::pair{"max", tr("Max")}})
        qs_period_cb_->addItem(label, QString::fromLatin1(key));
    qs_period_cb_->setCurrentIndex(1);
    qs_period_cb_->setToolTip(tr("Price history the analysis uses (current weights, back-tested)"));
    qs_period_cb_->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:1px 6px;"
                                         "  font-size:" + fincept::ui::fonts::px(-4) + "px; }")
                                     .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    // A period the user picks is an explicit request: run it (a restored value is not).
    connect(qs_period_cb_, &QComboBox::activated, this, [this](int) { on_period_activated(); });
    h_lay->addWidget(qs_period_cb_);

    qs_run_btn_ = new QPushButton(tr("▶ RUN QUANTSTATS"), header);
    qs_run_btn_->setMinimumHeight(22);
    qs_run_btn_->setCursor(Qt::PointingHandCursor);
    qs_run_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%5; border:none;"
                "  padding:0 12px; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; }"
                "QPushButton:hover { background:%2; }"
                "QPushButton:disabled { background:%3; color:%4; }")
            .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::TEXT_ON_ACCENT()));
    connect(qs_run_btn_, &QPushButton::clicked, this, [this]() {
        if (ui::AutoLoadPane* p = current_qs_pane())
            p->run();
    });
    h_lay->addWidget(qs_run_btn_);
    hv->addLayout(h_lay);

    // Two analyses of the same back-test exist; say where the other one is and how they differ.
    sibling_lbl_ = new QLabel(tr("Back-test of today's weights · the ffn-based equivalent is under ⋯ › FFN Analysis; "
                                 "realised (time-weighted) performance is in PERF/RISK"),
                              header);
    sibling_lbl_->setWordWrap(true);
    sibling_lbl_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    hv->addWidget(sibling_lbl_);

    root->addWidget(header);

    // ── Tab widget ────────────────────────────────────────────────────────────
    tabs_ = new QTabWidget(this);
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(true); // 1280 px / large fonts: scroll instead of clipping
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));
    QWidget* pages = qs_page_parent(tabs_, this);

    // One quantstats run feeds METRICS, RETURNS, DRAWDOWN and ROLLING.
    qs_shared_ = new PortfolioSharedRun(this);
    qs_shared_->set_launch([this](quint64 run) { launch_quantstats(run); });
    qs_shared_->set_abort([this](quint64 run) { qs_requests_.take(run).cancel(); });

    auto new_qs_tab = [&](ResultTab& tab, ui::LoadingSkeleton::Shape shape,
                          const std::function<void(const QJsonValue&)>& render) {
        tab.stack = new QStackedWidget(pages);
        tab.pane = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, tab.stack);
        tab.stack->addWidget(tab.pane);
        configure_pane(tab, static_cast<int>(shape), render);
        tab.pane->set_cache_key(QStringLiteral("portfolio/quantstats"), kPortfolioResultTtlDays);
        qs_shared_->attach(tab.pane);
    };

    // ── METRICS tab ───────────────────────────────────────────────────────────
    {
        new_qs_tab(metrics_tab_, ui::LoadingSkeleton::Shape::Table, [this](const QJsonValue& p) {
            qs_data_ = p.toObject();
            update_metrics();
        });
        metrics_tab_.pane->skeleton()->set_row_count(14);
        metrics_tab_.empty_text = []() {
            return tr("Performance, risk and ratio metrics of the current weights appear once the portfolio holds a "
                      "position.");
        };
        auto* metrics_w = new QWidget(metrics_tab_.pane->content_parent());
        auto* ml = new QVBoxLayout(metrics_w);
        ml->setContentsMargins(12, 8, 12, 8);
        ml->setSpacing(4);

        metrics_section_lbl_ = new QLabel(tr("KEY PERFORMANCE INDICATORS"), metrics_w);
        metrics_section_lbl_->setStyleSheet(qs_title_style());
        ml->addWidget(metrics_section_lbl_);

        metrics_table_ = new QTableWidget(metrics_w);
        metrics_table_->setColumnCount(3);
        metrics_table_->setHorizontalHeaderLabels({tr("METRIC"), tr("VALUE"), tr("BENCHMARK")});
        metrics_table_->setColumnWidth(0, 220);
        metrics_table_->setColumnWidth(1, 130);
        qs_style_table(metrics_table_);
        ml->addWidget(metrics_table_, 1);
        metrics_tab_.pane->set_content(metrics_w);

        metrics_tab_index_ = tabs_->addTab(metrics_tab_.stack, tr("METRICS"));
    }

    auto make_scroll = [](ui::AutoLoadPane* pane) {
        auto* scroll = new QScrollArea(pane->content_parent());
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        pane->set_content(scroll);
        return scroll;
    };

    // ── RETURNS tab ───────────────────────────────────────────────────────────
    {
        new_qs_tab(returns_tab_, ui::LoadingSkeleton::Shape::Result, [this](const QJsonValue& p) {
            qs_data_ = p.toObject();
            update_returns();
        });
        returns_tab_.empty_text = []() {
            return tr("The cumulative return, the distribution of daily returns and the monthly returns appear once "
                      "the portfolio holds a position.");
        };
        returns_scroll_ = make_scroll(returns_tab_.pane);
        returns_tab_index_ = tabs_->addTab(returns_tab_.stack, tr("RETURNS"));
    }

    // ── DRAWDOWN tab ──────────────────────────────────────────────────────────
    {
        new_qs_tab(drawdown_tab_, ui::LoadingSkeleton::Shape::Result, [this](const QJsonValue& p) {
            qs_data_ = p.toObject();
            update_drawdown();
        });
        drawdown_tab_.empty_text = []() {
            return tr("The drawdown curve, the worst drawdown periods and the risk metrics appear once the portfolio "
                      "holds a position.");
        };
        drawdown_scroll_ = make_scroll(drawdown_tab_.pane);
        drawdown_tab_index_ = tabs_->addTab(drawdown_tab_.stack, tr("DRAWDOWN"));
    }

    // ── ROLLING tab ───────────────────────────────────────────────────────────
    {
        new_qs_tab(rolling_tab_, ui::LoadingSkeleton::Shape::Chart, [this](const QJsonValue& p) {
            qs_data_ = p.toObject();
            update_rolling();
        });
        rolling_tab_.empty_text = []() {
            return tr("Rolling Sharpe ratio, volatility and beta appear once the portfolio holds a position.");
        };
        rolling_scroll_ = make_scroll(rolling_tab_.pane);
        rolling_tab_index_ = tabs_->addTab(rolling_tab_.stack, tr("ROLLING"));
    }

    // ── MONTE CARLO tab ───────────────────────────────────────────────────────
    mc_tab_index_ = tabs_->addTab(build_mc_page(pages), tr("MONTE CARLO"));

    root->addWidget(tabs_, 1);

    // Inputs remembered per sub-tab: the period (QuantStats) and paths / method (Monte Carlo).
    qs_inputs_ = new ui::InputMemory(QStringLiteral("portfolio"), QStringLiteral("quantstats"), this);
    qs_inputs_->track(QStringLiteral("period"), qs_period_cb_);
    qs_inputs_->restore();
    mc_inputs_ = new ui::InputMemory(QStringLiteral("portfolio"), QStringLiteral("montecarlo"), this);
    mc_inputs_->track(QStringLiteral("paths"), mc_paths_spin_);
    mc_inputs_->track(QStringLiteral("method"), mc_method_cb_);
    mc_inputs_->restore();
    connect(mc_inputs_, &ui::InputMemory::changed, this, [this]() {
        mc_tab_.pane->set_loading_text(mc_loading_text());
        if (has_data_ && !empty_book_)
            mc_tab_.pane->set_signature(mc_signature()); // a result for other inputs shows "RUN AGAIN"
    });

    for (ResultTab* t : {&metrics_tab_, &returns_tab_, &drawdown_tab_, &rolling_tab_})
        t->pane->set_loading_text(qs_loading_text());
    mc_tab_.pane->set_loading_text(mc_loading_text());
    metrics_tab_.pane->set_status_label(qs_status_);
    header_pane_ = metrics_tab_.pane;
    connect(tabs_, &QTabWidget::currentChanged, this, &QuantStatsView::on_tab_changed);
}

QWidget* QuantStatsView::build_mc_page(QWidget* parent) {
    auto* mc_w = new QWidget(parent);
    qs_scoped_background(mc_w, QStringLiteral("qsMcPage"));
    auto* mcl = new QVBoxLayout(mc_w);
    mcl->setContentsMargins(16, 12, 16, 12);
    mcl->setSpacing(8);

    mc_title_ = new QLabel(tr("MONTE CARLO SIMULATION"), mc_w);
    mc_title_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    mcl->addWidget(mc_title_);

    mc_desc_ = new QLabel(tr("Simulate portfolio return paths — bootstrap of whole historical days (keeps fat "
                             "tails and co-movement) or geometric Brownian motion — to estimate the distribution "
                             "of future returns, drawdowns and terminal wealth."),
                          mc_w);
    mc_desc_->setWordWrap(true);
    mc_desc_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    mcl->addWidget(mc_desc_);

    // Inputs row
    auto* mc_btn_row = new QHBoxLayout;
    mc_btn_row->setSpacing(8);

    paths_lbl_ = new QLabel(tr("Paths:"), mc_w);
    paths_lbl_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_SECONDARY()));
    mc_btn_row->addWidget(paths_lbl_);
    mc_paths_spin_ = new QSpinBox(mc_w);
    mc_paths_spin_->setRange(100, 100000);
    mc_paths_spin_->setSingleStep(1000);
    mc_paths_spin_->setValue(5000);
    mc_paths_spin_->setGroupSeparatorShown(true);
    mc_paths_spin_->setToolTip(tr("More paths = steadier percentiles, slower run (100 – 100,000)"));
    mc_btn_row->addWidget(mc_paths_spin_);
    mc_method_cb_ = new QComboBox(mc_w);
    mc_method_cb_->addItem(tr("Bootstrap"), QStringLiteral("bootstrap"));
    mc_method_cb_->addItem(tr("GBM (normal)"), QStringLiteral("gbm"));
    mc_btn_row->addWidget(mc_method_cb_);
    for (QWidget* w : {static_cast<QWidget*>(mc_paths_spin_), static_cast<QWidget*>(mc_method_cb_)})
        w->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3; padding:1px 6px;")
                             .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));

    mc_run_btn_ = new QPushButton(tr("▶ RUN MONTE CARLO"), mc_w);
    mc_run_btn_->setMinimumHeight(28);
    mc_run_btn_->setCursor(Qt::PointingHandCursor);
    mc_run_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%5; border:none;"
                "  padding:0 16px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                "QPushButton:hover { background:%2; }"
                "QPushButton:disabled { background:%3; color:%4; }")
            .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::TEXT_ON_ACCENT()));
    mc_btn_row->addWidget(mc_run_btn_);

    mc_status_ = new QLabel(mc_w);
    mc_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    mc_btn_row->addWidget(mc_status_);
    mc_btn_row->addStretch();
    mcl->addLayout(mc_btn_row);

    // Result: its own run (a second Python process, only when this tab is shown).
    mc_tab_.stack = new QStackedWidget(mc_w);
    mc_tab_.pane = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, mc_tab_.stack);
    mc_tab_.stack->addWidget(mc_tab_.pane);
    configure_pane(mc_tab_, static_cast<int>(ui::LoadingSkeleton::Shape::Chart), [this](const QJsonValue& p) {
        mc_data_ = p.toObject();
        update_monte_carlo_chart();
    });
    mc_tab_.pane->set_cache_key(QStringLiteral("portfolio/montecarlo"), kPortfolioResultTtlDays);
    mc_tab_.pane->set_persist_limit(512 * 1024); // the payload carries 50 sample paths (~270 KB)
    mc_tab_.pane->set_status_label(mc_status_);
    mc_tab_.pane->set_start([this](quint64 id) { start_monte_carlo(id); });
    mc_tab_.pane->set_cancel([this](quint64 id) { mc_requests_.take(id).cancel(); });
    mc_tab_.empty_text = []() {
        return tr("A Monte Carlo simulation of the current weights appears once the portfolio holds a position.");
    };
    connect(mc_run_btn_, &QPushButton::clicked, mc_tab_.pane, &ui::AutoLoadPane::run);

    auto* content = new QWidget(mc_tab_.pane->content_parent());
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(8);

    mc_kpis_ = new ui::KpiStrip(content);
    mc_kpis_->add_tile(QStringLiteral("median"), tr("MEDIAN RETURN"));
    mc_kpis_->add_tile(QStringLiteral("p5"), tr("5TH PERCENTILE"));
    mc_kpis_->add_tile(QStringLiteral("p95"), tr("95TH PERCENTILE"));
    mc_kpis_->add_tile(QStringLiteral("prob_loss"), tr("PROB OF LOSS"));
    mc_kpis_->add_tile(QStringLiteral("max_dd"), tr("EXP MAX DRAWDOWN"));
    mc_kpis_->add_tile(QStringLiteral("cvar"), tr("WORST 5% AVG"));
    mc_kpis_->add_tile(QStringLiteral("loss_at_risk"), tr("≈ LOSS AT RISK"));
    cl->addWidget(mc_kpis_);

    mc_fan_ = new ui::FanChart(content);
    mc_fan_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1); });
    mc_fan_->set_x_formatter([](double x) { return QuantStatsView::tr("Day %1").arg(static_cast<int>(std::lround(x))); });
    mc_fan_->add_reference_line(0.0, tr("Break-even"));
    mc_fan_->setMinimumHeight(qs_row_height(220));
    cl->addWidget(mc_fan_, 1);

    // Distribution of the simulated one-year outcomes (shown when the payload has them).
    mc_dist_ = new ui::DistributionChart(content);
    mc_dist_->set_signed_colors(true);
    mc_dist_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1); });
    mc_dist_->setMinimumHeight(qs_row_height(160));
    mc_dist_->setVisible(false);
    cl->addWidget(mc_dist_);

    mc_caption_ = new QLabel(content);
    mc_caption_->setWordWrap(true);
    mc_caption_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; padding:4px 0;").arg(ui::colors::TEXT_TERTIARY()));
    cl->addWidget(mc_caption_);
    mc_tab_.pane->set_content(content);

    mcl->addWidget(mc_tab_.stack, 1);
    return mc_w;
}

void QuantStatsView::configure_pane(ResultTab& tab, int shape, const std::function<void(const QJsonValue&)>& render) {
    ui::AutoLoadPane* pane = tab.pane;
    tab.render = render;
    pane->skeleton()->set_shape(static_cast<ui::LoadingSkeleton::Shape>(shape));
    pane->set_max_age(kPortfolioScriptMaxAgeS);
    pane->set_timeout(kQsTimeoutS);
    pane->set_render([this, pane, render](const QJsonValue& payload, const QDateTime&, bool) {
        theme_stale_.remove(pane);
        render(payload);
    });
}

// ── Tabs / state ──────────────────────────────────────────────────────────────

QuantStatsView::ResultTab* QuantStatsView::tab_for_index(int index) {
    if (index == metrics_tab_index_)
        return &metrics_tab_;
    if (index == returns_tab_index_)
        return &returns_tab_;
    if (index == drawdown_tab_index_)
        return &drawdown_tab_;
    if (index == rolling_tab_index_)
        return &rolling_tab_;
    if (index == mc_tab_index_)
        return &mc_tab_;
    return nullptr;
}

ui::AutoLoadPane* QuantStatsView::current_qs_pane() {
    ResultTab* t = tabs_ ? tab_for_index(tabs_->currentIndex()) : nullptr;
    if (!t || t == &mc_tab_)
        return header_pane_ ? header_pane_ : metrics_tab_.pane; // the one the header's status shows
    return t->pane;
}

void QuantStatsView::on_tab_changed(int index) {
    if (ResultTab* t = tab_for_index(index))
        rerender_if_stale(*t);
    if (index == mc_tab_index_)
        return; // the header keeps showing the QuantStats run; MONTE CARLO has its own status
    // The header's "as of" follows the QuantStats tab on screen.
    ui::AutoLoadPane* current = current_qs_pane();
    for (ResultTab* t : {&metrics_tab_, &returns_tab_, &drawdown_tab_, &rolling_tab_})
        if (t->pane != current)
            t->pane->set_status_label(nullptr);
    current->set_status_label(qs_status_);
    header_pane_ = current;
}

void QuantStatsView::set_empty_book(bool empty) {
    if (empty == empty_book_)
        return;
    empty_book_ = empty;
    for (ResultTab* t : {&metrics_tab_, &returns_tab_, &drawdown_tab_, &rolling_tab_, &mc_tab_}) {
        if (!empty) {
            t->stack->setCurrentWidget(t->pane);
            continue;
        }
        if (!t->empty) {
            t->empty = new ui::EmptyState(t->stack);
            t->stack->addWidget(t->empty);
            connect(t->empty, &ui::EmptyState::primary_clicked, this, &QuantStatsView::add_position_requested);
        }
        t->empty->apply(ui::EmptyState::no_data_spec(tr("No positions to analyse"), t->empty_text(), tr("BUY")));
        t->stack->setCurrentWidget(t->empty); // the pane is hidden: nothing runs
    }
    if (empty)
        qs_status_->clear();
}

void QuantStatsView::on_appearance_settled() {
    for (ResultTab* t : {&metrics_tab_, &returns_tab_, &drawdown_tab_, &rolling_tab_, &mc_tab_})
        if (t->pane->has_result())
            theme_stale_.insert(t->pane);
    if (ResultTab* t = tabs_ ? tab_for_index(tabs_->currentIndex()) : nullptr)
        rerender_if_stale(*t);
}

void QuantStatsView::rerender_if_stale(ResultTab& tab) {
    if (!theme_stale_.contains(tab.pane))
        return;
    theme_stale_.remove(tab.pane);
    if (tab.pane->has_result() && tab.render)
        tab.render(tab.pane->payload());
}

// ── set_data ──────────────────────────────────────────────────────────────────

void QuantStatsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;

    // An empty book has nothing to analyse: the result tabs say so (with BUY) and nothing runs.
    const bool has_holdings = !summary_.holdings.isEmpty();
    const QString why = has_holdings ? QString() : tr("Add a position first");
    for (QPushButton* b : {qs_run_btn_, mc_run_btn_}) {
        b->setEnabled(has_holdings);
        b->setToolTip(why);
    }
    set_empty_book(!has_holdings);

    // Results are kept per portfolio and holdings: a switch or a trade selects
    // another remembered result (or runs the analysis once for it); a price tick
    // keeps the key, so nothing re-runs before the result's max age.
    if (has_holdings) {
        const QString key = portfolio_run_key(summary_);
        qs_shared_->set_view(key, qs_signature());
        mc_tab_.pane->set_view(key);
        mc_tab_.pane->set_signature(mc_signature());
    }

    // METRICS carries live rows of the book: they follow every refresh.
    if (metrics_tab_.pane->has_result()) {
        qs_data_ = metrics_tab_.pane->payload().toObject();
        update_metrics();
    }
}

// ── update_metrics ────────────────────────────────────────────────────────────

void QuantStatsView::update_metrics() {
    metrics_table_->clearContents();
    // clearContents() keeps the section rows' spans: after the run the rows shift, and a
    // stale span over a data row hid its value cells (Avg Daily Return, Downside
    // Deviation, Profit Factor showed no value at all).
    metrics_table_->clearSpans();

    // ── Build row definitions ─────────────────────────────────────────────────
    struct Row {
        QString section; // non-empty marks a section separator row
        QString name;
        QString value;
        QString benchmark = "--";
        int sign = 0; // direction of a signed figure (colours the value); 0 = neutral
        bool is_section = false;
    };

    QVector<Row> rows;

    auto add_section = [&](const QString& label) { rows.push_back({label, {}, {}, {}, 0, true}); };

    const bool have_qs = !qs_data_.isEmpty();

    // Helper lambdas for reading qs_data_ — nullopt when the script reported null / nothing,
    // rendered as "--" instead of a fake 0.
    auto qs_pct = [&](const QString& group, const QString& key) -> std::optional<double> {
        if (!have_qs)
            return std::nullopt;
        return qs_num(qs_data_.value(group).toObject(), key, 100.0);
    };
    auto qs_val = [&](const QString& group, const QString& key) -> std::optional<double> {
        if (!have_qs)
            return std::nullopt;
        return qs_num(qs_data_.value(group).toObject(), key);
    };
    auto qs_int = [&](const QString& group, const QString& key) -> int {
        if (!have_qs)
            return 0;
        return qs_data_[group].toObject()[key].toInt();
    };
    // Same statistics computed by the script for the benchmark index.
    const QJsonObject bench_obj = qs_data_.value("benchmark").toObject();
    auto b_val = [&](const QString& group, const QString& key, bool pct, int dp = 2) -> QString {
        const QJsonValue v = bench_obj.value(group).toObject().value(key);
        if (!v.isDouble())
            return QStringLiteral("--");
        return pct ? qs_pct_str(v.toDouble() * 100.0, dp) : ratio_str(v.toDouble(), dp);
    };
    // Benchmark figure for a quantity that is not a return (no "+" sign).
    auto b_uval = [&](const QString& group, const QString& key) -> QString {
        return opt_upct_str(qs_num(bench_obj.value(group).toObject(), key, 100.0));
    };

    // ── THE BOOK TODAY (live, from the portfolio summary) ────────────────────
    add_section(tr("PORTFOLIO TODAY"));
    {
        const double pnl_pct = summary_.total_unrealized_pnl_percent;
        const double day_pct = summary_.total_day_change_percent;
        rows.push_back({"", tr("Unrealized P&L %"), qs_pct_str(pnl_pct), "--", qs_sign(pnl_pct)});
        rows.push_back({"", tr("Day Change %"), qs_pct_str(day_pct), "--", qs_sign(day_pct)});
        rows.push_back({"", tr("Total Positions"), QString::number(summary_.total_positions), "--", 0});
        rows.push_back({"", tr("Gainers"), QString::number(summary_.gainers), "--", 0});
        rows.push_back({"", tr("Losers"), QString::number(summary_.losers), "--", 0});
    }

    if (have_qs) {
        // ── PERFORMANCE ───────────────────────────────────────────────────────
        add_section(tr("PERFORMANCE"));
        const auto total_ret = qs_pct("performance", "total_return");
        const auto ann_ret = qs_pct("performance", "annualized_return");
        const int t_days = qs_int("performance", "trading_days");
        const auto best_day = qs_pct("performance", "best_day");
        const auto worst_day = qs_pct("performance", "worst_day");
        const auto avg_daily = qs_pct("performance", "avg_daily_return");

        rows.push_back({"", tr("Total Return"), opt_pct_str(total_ret), b_val("performance", "total_return", true), qs_sign(total_ret)});
        rows.push_back({"", tr("Annualized Return"), opt_pct_str(ann_ret), b_val("performance", "annualized_return", true), qs_sign(ann_ret)});
        rows.push_back({"", tr("Trading Days"), QString::number(t_days), "--", 0});
        rows.push_back({"", tr("Best Day"), opt_pct_str(best_day), b_val("performance", "best_day", true), qs_sign(best_day)});
        rows.push_back({"", tr("Worst Day"), opt_pct_str(worst_day), b_val("performance", "worst_day", true), qs_sign(worst_day)});
        rows.push_back({"", tr("Avg Daily Return"), opt_pct_str(avg_daily, 4), "--", qs_sign(avg_daily)});

        // ── RISK ──────────────────────────────────────────────────────────────
        add_section(tr("RISK"));
        const auto ann_vol = qs_pct("risk", "annualized_volatility");
        const auto max_dd = qs_pct("risk", "max_drawdown");
        const auto var95 = qs_pct("risk", "var_95_daily");
        const auto cvar95 = qs_pct("risk", "cvar_95_daily");
        const auto down_dev = qs_pct("risk", "downside_deviation");

        rows.push_back({"", tr("Annualized Volatility"), opt_upct_str(ann_vol), b_uval("risk", "annualized_volatility"), 0});
        rows.push_back({"", tr("Max Drawdown"), opt_pct_str(max_dd), b_val("risk", "max_drawdown", true), qs_sign(max_dd)});
        rows.push_back({"", tr("VaR 95% (Daily)"), opt_pct_str(var95), b_val("risk", "var_95_daily", true), qs_sign(var95)});
        rows.push_back({"", tr("CVaR 95% (Daily)"), opt_pct_str(cvar95), b_val("risk", "cvar_95_daily", true), qs_sign(cvar95)});
        rows.push_back({"", tr("Downside Deviation"), opt_upct_str(down_dev), b_uval("risk", "downside_deviation"), 0});

        // ── RATIOS ────────────────────────────────────────────────────────────
        add_section(tr("RATIOS"));
        const auto sharpe = qs_val("ratios", "sharpe_ratio");
        const auto sortino = qs_val("ratios", "sortino_ratio");
        const auto calmar = qs_val("ratios", "calmar_ratio");
        const auto pf = qs_val("ratios", "profit_factor");

        rows.push_back({"", tr("Sharpe Ratio"), opt_ratio_str(sharpe), b_val("ratios", "sharpe_ratio", false), 0});
        rows.push_back({"", tr("Sortino Ratio"), opt_ratio_str(sortino), b_val("ratios", "sortino_ratio", false), 0});
        rows.push_back({"", tr("Calmar Ratio"), opt_ratio_str(calmar), b_val("ratios", "calmar_ratio", false), 0});
        rows.push_back({"", tr("Profit Factor"), opt_ratio_str(pf), "--", 0});

        // ── DISTRIBUTION ──────────────────────────────────────────────────────
        add_section(tr("DISTRIBUTION"));
        const auto skew = qs_val("distribution", "skewness");
        const auto kurt = qs_val("distribution", "kurtosis");
        const auto win_rate = qs_pct("distribution", "win_rate");
        const int win_days = qs_int("distribution", "win_days");
        const int los_days = qs_int("distribution", "loss_days");
        const auto avg_win = qs_pct("distribution", "avg_win");
        const auto avg_loss = qs_pct("distribution", "avg_loss");

        rows.push_back({"", tr("Skewness"), opt_ratio_str(skew, 3), b_val("distribution", "skewness", false, 3), 0});
        rows.push_back({"", tr("Excess Kurtosis"), opt_ratio_str(kurt, 3), b_val("distribution", "kurtosis", false, 3), 0});
        rows.push_back({"", tr("Win Rate"), opt_upct_str(win_rate), "--", 0});
        rows.push_back({"", tr("Win Days"), QString::number(win_days), "--", 0});
        rows.push_back({"", tr("Loss Days"), QString::number(los_days), "--", 0});
        rows.push_back({"", tr("Avg Win"), opt_pct_str(avg_win), "--", qs_sign(avg_win)});
        rows.push_back({"", tr("Avg Loss"), opt_pct_str(avg_loss), "--", qs_sign(avg_loss)});

        // RELATIVE TO BENCHMARK: how the back-tested mix behaves against the index — did it
        // earn its risk? (Appended last so the existing section order stays put.)
        const QJsonObject rel = qs_data_.value("relative").toObject();
        if (!rel.isEmpty()) {
            const QString bsym = bench_obj.value("symbol").toString();
            add_section(bsym.isEmpty() ? tr("RELATIVE TO BENCHMARK") : tr("RELATIVE TO %1").arg(bsym));
            auto num = [&rel](const char* key) { return rel.value(QLatin1String(key)); };
            auto add_ratio = [&](const QString& name, const char* key, int dp) {
                const QJsonValue v = num(key);
                if (v.isDouble())
                    rows.push_back({"", name, ratio_str(v.toDouble(), dp), "--", 0});
            };
            add_ratio(tr("Beta"), "beta", 2);
            if (num("alpha").isDouble()) {
                const double a = num("alpha").toDouble() * 100.0;
                rows.push_back({"", tr("Alpha (annualized)"), qs_pct_str(a), "--", qs_sign(a)});
            }
            add_ratio(tr("Correlation"), "correlation", 2);
            add_ratio(tr("R-Squared"), "r_squared", 2);
            if (num("tracking_error").isDouble())
                rows.push_back({"", tr("Tracking Error"), qs_upct_str(num("tracking_error").toDouble() * 100.0), "--", 0});
            add_ratio(tr("Information Ratio"), "information_ratio", 2);
            // Capture ratios: above 1 on the way up and below 1 on the way down is the
            // asymmetry an active book wants.
            add_ratio(tr("Up Capture"), "up_capture", 2);
            add_ratio(tr("Down Capture"), "down_capture", 2);
        }
    }

    // ── Populate table ────────────────────────────────────────────────────────
    metrics_table_->setUpdatesEnabled(false);
    metrics_table_->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        metrics_table_->setRowHeight(r, qs_row_height(26));
        const auto& row = rows[r];

        if (row.is_section) {
            // Section separator spanning all columns
            auto* item = new QTableWidgetItem(row.section);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setForeground(QColor(ui::colors::AMBER()));
            item->setBackground(QColor(ui::colors::BG_RAISED()));
            QFont f = item->font();
            f.setPixelSize(ui::fonts::font_px(-3));
            f.setBold(true);
            item->setFont(f);
            metrics_table_->setItem(r, 0, item);
            metrics_table_->setSpan(r, 0, 1, 3);
            continue;
        }

        metrics_table_->setItem(
            r, 0, qs_make_item(row.name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        const bool blank = row.value.isEmpty() || row.value == QLatin1String("--");
        metrics_table_->setItem(r, 1,
                                qs_make_item(blank ? QStringLiteral("--") : row.value, Qt::AlignRight | Qt::AlignVCenter,
                                          blank ? QColor(ui::colors::TEXT_TERTIARY()) : qs_value_color(row.sign)));
        metrics_table_->setItem(
            r, 2, qs_make_item(row.benchmark, Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_TERTIARY())));
    }
    metrics_table_->setUpdatesEnabled(true);
}

// ── update_returns ────────────────────────────────────────────────────────────

void QuantStatsView::update_returns() {
    if (qs_data_.isEmpty() || !returns_scroll_)
        return;

    auto* content = new QWidget; // handed to the scroll area below (which drops the previous one)
    qs_scoped_background(content, QStringLiteral("qsReturnsContent"));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    // Cumulative return of today's mix against the benchmark, same dates.
    const QJsonObject series = qs_data_["series"].toObject();
    if (!series.isEmpty()) {
        auto* cum_title = new QLabel(tr("CUMULATIVE RETURN — CURRENT WEIGHTS, BACK-TESTED"), content);
        cum_title->setStyleSheet(qs_title_style());
        cl->addWidget(cum_title);
        QVector<QsLine> lines;
        lines.append({tr("Portfolio"), series["cumulative"].toArray(), QColor(ui::colors::POSITIVE())});
        if (series.contains("benchmark_cumulative")) {
            const QString bsym = qs_data_["benchmark"].toObject()["symbol"].toString();
            lines.append({bsym.isEmpty() ? tr("Benchmark") : bsym, series["benchmark_cumulative"].toArray(),
                          QColor(ui::colors::CYAN()), true});
        }
        auto* cum_chart = qs_line_chart(series["dates"].toArray(), lines, "%.0f%%", content);
        cum_chart->setMinimumHeight(240);
        cl->addWidget(cum_chart);
    }

    auto* title = new QLabel(tr("RETURN DISTRIBUTION"), content);
    title->setStyleSheet(qs_title_style());
    cl->addWidget(title);

    // Win / loss headline figures.
    const QJsonObject dist = qs_data_["distribution"].toObject();
    const auto win_rate = qs_num(dist, "win_rate", 100.0);
    const int win_days = dist["win_days"].toInt();
    const int loss_days = dist["loss_days"].toInt();
    const auto avg_win = qs_num(dist, "avg_win", 100.0);
    const auto avg_loss = qs_num(dist, "avg_loss", 100.0);
    const auto skew = qs_num(dist, "skewness");
    const auto kurt = qs_num(dist, "kurtosis");
    // LOSS RATE only when the win rate is known — a missing win rate used to read "+100.00%".
    const std::optional<double> loss_rate =
        win_rate ? std::optional<double>(100.0 - *win_rate) : std::nullopt;

    auto* kpis = new ui::KpiStrip(content);
    kpis->add_tile(tr("WIN RATE"))->set_value(opt_upct_str(win_rate));
    kpis->add_tile(tr("LOSS RATE"))->set_value(opt_upct_str(loss_rate));
    kpis->add_tile(tr("WIN DAYS"))->set_value(QString::number(win_days));
    kpis->add_tile(tr("LOSS DAYS"))->set_value(QString::number(loss_days));
    {
        auto* t = kpis->add_tile(tr("AVG WIN"));
        t->set_value(opt_pct_str(avg_win));
        t->set_tone(qs_tone(avg_win));
    }
    {
        auto* t = kpis->add_tile(tr("AVG LOSS"));
        t->set_value(opt_pct_str(avg_loss));
        t->set_tone(qs_tone(avg_loss));
    }
    kpis->add_tile(tr("SKEWNESS"))->set_value(opt_ratio_str(skew, 3));
    kpis->add_tile(tr("EXCESS KURTOSIS"))->set_value(opt_ratio_str(kurt, 3));
    cl->addWidget(kpis);

    // The distribution itself: a histogram of the daily returns with a fitted
    // normal curve, the mean and the 5th percentile (historical 1-day VaR 95 %).
    {
        QVector<double> samples;
        for (const auto v : qs_data_["returns"].toArray())
            if (v.isDouble())
                samples.append(v.toDouble() * 100.0);
        if (samples.size() >= 2) {
            auto* hist = new ui::DistributionChart(content);
            hist->set_signed_colors(true);
            hist->set_normal_overlay(true);
            hist->set_value_formatter([](double v) { return ui::formatting::pct(v, 2); });
            hist->set_samples(samples);
            hist->add_marker(hist->mean(), tr("Mean"), ui::DistributionChart::MarkerRole::Mean);
            hist->add_marker(hist->quantile(0.05), tr("5th pct (VaR 95%)"), ui::DistributionChart::MarkerRole::VaR);
            hist->setMinimumHeight(qs_row_height(240));
            hist->setToolTip(tr("%n daily return(s) of the current weights over the analysed period.", "",
                                static_cast<int>(samples.size())));
            cl->addWidget(hist);
        }
    }

    auto* dist_table = new QTableWidget(content);
    dist_table->setColumnCount(2);
    dist_table->setHorizontalHeaderLabels({tr("METRIC"), tr("VALUE")});
    dist_table->setColumnWidth(0, 200);
    qs_style_table(dist_table);

    const QJsonObject perf = qs_data_["performance"].toObject();
    const auto best_day = qs_num(perf, "best_day", 100.0);
    const auto worst_day = qs_num(perf, "worst_day", 100.0);
    const auto avg_daily = qs_num(perf, "avg_daily_return", 100.0);
    const auto profit_factor = qs_num(qs_data_["ratios"].toObject(), "profit_factor");

    struct TableRow {
        QString name;
        QString val;
        int sign;
    };
    const QVector<TableRow> trows = {
        {tr("Best Day"), opt_pct_str(best_day), qs_sign(best_day)},
        {tr("Worst Day"), opt_pct_str(worst_day), qs_sign(worst_day)},
        {tr("Avg Daily Return"), opt_pct_str(avg_daily, 4), qs_sign(avg_daily)},
        {tr("Profit Factor"), opt_ratio_str(profit_factor), 0},
    };
    dist_table->setRowCount(trows.size());
    for (int i = 0; i < trows.size(); ++i) {
        dist_table->setRowHeight(i, qs_row_height(26));
        dist_table->setItem(
            i, 0, qs_make_item(trows[i].name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        dist_table->setItem(i, 1,
                            qs_make_item(trows[i].val, Qt::AlignRight | Qt::AlignVCenter, qs_value_color(trows[i].sign)));
    }
    dist_table->setMinimumHeight(qs_row_height(26) * (static_cast<int>(trows.size()) + 1) + 4);
    cl->addWidget(dist_table);

    // Monthly returns grid (years × months + the year compounded) — the classic
    // QuantStats heatmap: seasonality and the bad months at a glance.
    const QJsonObject monthly = qs_data_["monthly"].toObject();
    if (!monthly.isEmpty()) {
        auto* m_title = new QLabel(tr("MONTHLY RETURNS"), content);
        m_title->setStyleSheet(qs_title_style());
        cl->addWidget(m_title);

        // Every month of the analysed period (not just one year), on the shared
        // heatmap: diverging theme scale with a robust clamp and a compounded year column.
        ui::MonthlyReturnsHeatmap::MonthlyReturns by_year; // year -> month -> return %
        for (auto it = monthly.begin(); it != monthly.end(); ++it) {
            if (!it.value().isDouble())
                continue;
            const QStringList ym = it.key().split(QLatin1Char('-'));
            if (ym.size() != 2)
                continue;
            by_year[ym[0].toInt()][ym[1].toInt()] = it.value().toDouble() * 100.0;
        }
        auto* mt = new ui::MonthlyReturnsHeatmap(content);
        mt->set_monthly_returns(by_year);
        mt->setMinimumHeight(qs_row_height(28) * (static_cast<int>(by_year.size()) + 2));
        cl->addWidget(mt);
    }
    cl->addStretch();

    returns_scroll_->setWidget(content);
}

// ── update_drawdown ───────────────────────────────────────────────────────────

void QuantStatsView::update_drawdown() {
    if (qs_data_.isEmpty() || !drawdown_scroll_)
        return;

    auto* content = new QWidget; // handed to the scroll area below (which drops the previous one)
    qs_scoped_background(content, QStringLiteral("qsDrawdownContent"));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    auto* title = new QLabel(tr("DRAWDOWN & RISK METRICS"), content);
    title->setStyleSheet(qs_title_style());
    cl->addWidget(title);

    // Headline risk figures (were a hero card and a repeated table).
    const QJsonObject risk = qs_data_["risk"].toObject();
    const auto max_dd = qs_num(risk, "max_drawdown", 100.0);
    const auto var95 = qs_num(risk, "var_95_daily", 100.0);
    const auto cvar95 = qs_num(risk, "cvar_95_daily", 100.0);
    const auto ann_vol = qs_num(risk, "annualized_volatility", 100.0);
    const auto down_dev = qs_num(risk, "downside_deviation", 100.0);

    auto* kpis = new ui::KpiStrip(content);
    {
        auto* t = kpis->add_tile(tr("MAX DRAWDOWN"));
        t->set_value(opt_pct_str(max_dd));
        t->set_tone(qs_tone(max_dd));
        t->set_tooltip_text(tr("Worst peak-to-trough fall of the current weights over the analysed period."));
    }
    kpis->add_tile(tr("ANN. VOLATILITY"))->set_value(opt_upct_str(ann_vol));
    {
        auto* t = kpis->add_tile(tr("VAR 95% (DAILY)"));
        t->set_value(opt_pct_str(var95));
        t->set_tone(qs_tone(var95));
    }
    {
        auto* t = kpis->add_tile(tr("CVAR 95% (DAILY)"));
        t->set_value(opt_pct_str(cvar95));
        t->set_tone(qs_tone(cvar95));
    }
    kpis->add_tile(tr("DOWNSIDE DEVIATION"))->set_value(opt_upct_str(down_dev));
    cl->addWidget(kpis);

    // Underwater curve: how far below its running peak the mix was, every day.
    const QJsonObject series = qs_data_["series"].toObject();
    if (!series.isEmpty()) {
        auto* uw = qs_line_chart(
            series["dates"].toArray(),
            {QsLine(tr("Drawdown"), series["drawdown"].toArray(), QColor(ui::colors::NEGATIVE()), false, true)},
            "%.0f%%", content);
        uw->setMinimumHeight(200);
        cl->addWidget(uw);
    }

    // The worst episodes: when they started, bottomed and recovered — depth alone says
    // nothing about how long the money was under water.
    const QJsonArray episodes = qs_data_["drawdowns"].toArray();
    if (!episodes.isEmpty()) {
        auto* ep_title = new QLabel(tr("WORST DRAWDOWN PERIODS"), content);
        ep_title->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.5px;")
                                    .arg(ui::colors::TEXT_SECONDARY()));
        cl->addWidget(ep_title);
        auto* ep = new QTableWidget(static_cast<int>(episodes.size()), 5, content);
        ep->setHorizontalHeaderLabels({tr("PEAK"), tr("TROUGH"), tr("RECOVERED"), tr("DEPTH"), tr("DAYS")});
        qs_style_table(ep);
        ep->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (int i = 0; i < episodes.size(); ++i) {
            const QJsonObject e = episodes[i].toObject();
            ep->setRowHeight(i, qs_row_height(24));
            const auto left = Qt::AlignLeft | Qt::AlignVCenter;
            const auto right = Qt::AlignRight | Qt::AlignVCenter;
            ep->setItem(i, 0, qs_make_item(e["start"].toString(), left, QColor(ui::colors::TEXT_SECONDARY())));
            ep->setItem(i, 1, qs_make_item(e["trough"].toString(), left, QColor(ui::colors::TEXT_SECONDARY())));
            const bool open = e["end"].isNull();
            ep->setItem(i, 2,
                        qs_make_item(open ? tr("not yet") : e["end"].toString(), left,
                                  QColor(open ? ui::colors::WARNING() : ui::colors::TEXT_SECONDARY())));
            ep->setItem(i, 3, qs_make_item(qs_pct_str(e["depth"].toDouble() * 100.0), right, QColor(ui::colors::NEGATIVE())));
            ep->setItem(i, 4, qs_make_item(QString::number(e["days"].toInt()), right, QColor(ui::colors::TEXT_PRIMARY())));
        }
        ep->setMinimumHeight(qs_row_height(24) * (static_cast<int>(episodes.size()) + 1) + 6);
        cl->addWidget(ep);
    }
    cl->addStretch();

    drawdown_scroll_->setWidget(content);
}

// ── update_rolling ────────────────────────────────────────────────────────────

void QuantStatsView::update_rolling() {
    if (qs_data_.isEmpty() || !rolling_scroll_)
        return;

    auto* content = new QWidget; // handed to the scroll area below (which drops the previous one)
    qs_scoped_background(content, QStringLiteral("qsRollingContent"));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    // Rolling statistics over a 63-day window: whether the risk-adjusted return and
    // the volatility are stable, or the full-period figure hides a regime change.
    // (This tab used to repeat the RATIOS rows of the METRICS tab under a ROLLING title.)
    const QJsonObject rolling = qs_data_["rolling"].toObject();
    const int window = rolling["window"].toInt(63);
    auto* title = new QLabel(tr("ROLLING %1-DAY METRICS — CURRENT WEIGHTS, BACK-TESTED").arg(window), content);
    title->setStyleSheet(qs_title_style());
    cl->addWidget(title);

    if (rolling.isEmpty()) {
        auto* es = ui::EmptyState::create(
            ui::EmptyState::no_data_spec(tr("Not enough history yet"),
                                         tr("A %1-day rolling window needs more daily history than the analysed "
                                            "period has. Pick a longer period in the header.")
                                             .arg(window)),
            content);
        es->set_compact(true);
        cl->addWidget(es, 1);
    } else {
        const QJsonArray dates = rolling["dates"].toArray();
        auto add_chart = [&](const QString& caption, const QVector<QsLine>& lines, const char* fmt) {
            auto* cap = new QLabel(caption, content);
            cap->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.5px;")
                                   .arg(ui::colors::TEXT_SECONDARY()));
            cl->addWidget(cap);
            auto* view = qs_line_chart(dates, lines, fmt, content);
            view->setMinimumHeight(190);
            cl->addWidget(view);
        };
        add_chart(tr("ROLLING SHARPE RATIO (annualised)"),
                  {QsLine(tr("Sharpe"), rolling["sharpe"].toArray(), QColor(ui::colors::CYAN()), false, false, 1.0)},
                  "%.1f");
        add_chart(tr("ROLLING VOLATILITY (annualised)"),
                  {QsLine(tr("Volatility"), rolling["volatility"].toArray(), QColor(ui::colors::AMBER()))}, "%.0f%%");
        if (rolling.contains("beta")) {
            const QString bsym = qs_data_["benchmark"].toObject()["symbol"].toString();
            add_chart(tr("ROLLING BETA vs %1").arg(bsym.isEmpty() ? tr("benchmark") : bsym),
                      {QsLine(tr("Beta"), rolling["beta"].toArray(), QColor(ui::colors::WARNING()), false, false, 1.0)},
                      "%.2f");
        }
        cl->addStretch();
    }

    rolling_scroll_->setWidget(content);
}

// ── update_monte_carlo_chart ──────────────────────────────────────────────────

void QuantStatsView::update_monte_carlo_chart() {
    if (mc_data_.isEmpty() || !mc_kpis_)
        return;
    const QJsonObject& d = mc_data_;

    // ── Headline figures (percent values from the script) ─────────────────────
    auto num = [&d](const char* key) -> std::optional<double> {
        const QJsonValue v = d.value(QLatin1String(key));
        return v.isDouble() ? std::optional<double>(v.toDouble()) : std::nullopt;
    };
    auto set_tile = [this](const char* key, const QString& value, ui::StatTile::Tone tone) {
        if (ui::StatTile* t = mc_kpis_->tile(QLatin1String(key))) {
            t->set_value(value);
            t->set_tone(tone);
        }
    };
    const auto median = num("median_return");
    const auto p5 = num("percentile_5");
    const auto p95 = num("percentile_95");
    const auto prob_loss = num("prob_loss");
    const auto max_dd = num("expected_max_dd");
    const auto cvar5 = num("cvar_5");
    set_tile("median", opt_pct_str(median), qs_tone(median));
    set_tile("p5", opt_pct_str(p5), qs_tone(p5));
    set_tile("p95", opt_pct_str(p95), qs_tone(p95));
    set_tile("prob_loss", opt_upct_str(prob_loss), ui::StatTile::Tone::Warning);
    set_tile("max_dd", opt_pct_str(max_dd), qs_tone(max_dd));
    set_tile("cvar", opt_pct_str(cvar5), qs_tone(cvar5));
    // Tail in money: the average of the worst 5% of outcomes, on today's NAV.
    if (cvar5 && summary_.total_market_value > 0) {
        const double loss = summary_.total_market_value * *cvar5 / 100.0;
        set_tile("loss_at_risk", ui::formatting::signed_money(loss, currency_, ui::formatting::Compact::None, 0),
                 qs_tone(loss));
    } else {
        set_tile("loss_at_risk", QStringLiteral("--"), ui::StatTile::Tone::Neutral);
    }

    // ── Percentile fan over all simulations (the script computes it per step) ──
    auto to_vec = [](const QJsonArray& a) {
        QVector<double> v;
        v.reserve(a.size());
        for (const auto& x : a)
            v.append(x.isDouble() ? x.toDouble() : std::numeric_limits<double>::quiet_NaN());
        return v;
    };
    const QJsonObject bands = d["percentile_paths"].toObject();
    const QVector<double> b5 = to_vec(bands["p5"].toArray());
    const QVector<double> b25 = to_vec(bands["p25"].toArray());
    const QVector<double> b50 = to_vec(bands["p50"].toArray());
    const QVector<double> b75 = to_vec(bands["p75"].toArray());
    const QVector<double> b95 = to_vec(bands["p95"].toArray());
    const int n_steps = static_cast<int>(b50.size());
    if (n_steps >= 2) {
        QVector<double> x(n_steps);
        for (int t = 0; t < n_steps; ++t)
            x[t] = t + 1;
        QVector<ui::FanBand> fan;
        if (b5.size() == n_steps && b95.size() == n_steps)
            fan.append(ui::FanBand{5, 95, b5, b95});
        if (b25.size() == n_steps && b75.size() == n_steps)
            fan.append(ui::FanBand{25, 75, b25, b75});
        mc_fan_->set_fan(x, fan, b50);
        mc_fan_->set_state(ui::PaintedChart::Ready);
    } else {
        // Older payloads: build the fan from the sample paths.
        QVector<QVector<double>> paths;
        for (const auto& p : d["paths"].toArray())
            paths.append(to_vec(p.toArray()));
        const int n = paths.isEmpty() ? 0 : static_cast<int>(paths.first().size());
        if (n >= 2) {
            QVector<double> x(n);
            for (int t = 0; t < n; ++t)
                x[t] = t + 1;
            mc_fan_->set_paths(x, paths, {{5, 95}, {25, 75}}, 20);
            mc_fan_->set_state(ui::PaintedChart::Ready);
        } else {
            mc_fan_->clear();
            mc_fan_->set_state(ui::PaintedChart::Empty, tr("The simulation returned no paths."));
        }
    }

    // ── Terminal outcomes (a sample of all simulations, percent) ──────────────
    {
        QVector<double> terminal;
        for (const auto& v : d["terminal_returns"].toArray())
            if (v.isDouble())
                terminal.append(v.toDouble());
        if (terminal.size() >= 2) {
            mc_dist_->clear_markers();
            mc_dist_->set_samples(terminal);
            mc_dist_->add_marker(mc_dist_->quantile(0.5), tr("Median"), ui::DistributionChart::MarkerRole::Median);
            mc_dist_->add_marker(mc_dist_->quantile(0.05), tr("5th pct"), ui::DistributionChart::MarkerRole::VaR);
        }
        mc_dist_->setVisible(terminal.size() >= 2);
    }

    // ── Caption: what was simulated ───────────────────────────────────────────
    const QString method = d["method"].toString();
    const QString method_label = method == QLatin1String("gbm") ? tr("geometric Brownian motion")
                                 : method.isEmpty()             ? tr("bootstrap")
                                                                : tr("bootstrap of historical days");
    QString caption = tr("%1 simulated paths over %2 trading days (%3). Shaded bands: 5th–95th and 25th–75th "
                         "percentile over all paths; line = median.")
                          .arg(ui::formatting::number(d["num_simulations"].toDouble(), 0))
                          .arg(d["horizon_days"].toInt(252))
                          .arg(method_label);
    if (d.contains("annual_drift"))
        caption += QLatin1Char(' ') +
                   tr("Calibrated to the mix's history: drift %1%/yr, volatility %2%/yr — a projection of the past, "
                      "not a forecast.")
                       .arg(ui::formatting::number(d["annual_drift"].toDouble(), 1),
                            ui::formatting::number(d["annual_volatility"].toDouble(), 1));
    if (d["seed"].isDouble())
        caption += QLatin1Char(' ') + tr("Seed %1.").arg(d["seed"].toInteger());
    mc_caption_->setText(caption);

    QStringList warns;
    for (const auto& w : d["warnings"].toArray())
        warns << w.toString();
    if (warns.isEmpty())
        mc_tab_.pane->clear_notice();
    else
        mc_tab_.pane->set_notice(warns.join(QStringLiteral("; ")), ui::EmptyState::Tone::Warning);
}

// ── Runs ──────────────────────────────────────────────────────────────────────

QString QuantStatsView::period_key() const {
    return qs_period_cb_ ? qs_period_cb_->currentData().toString() : QStringLiteral("1y");
}

QString QuantStatsView::qs_loading_text() const {
    const QString key = period_key();
    if (key == QLatin1String("ytd"))
        return tr("Downloading this year's prices…");
    if (key == QLatin1String("max"))
        return tr("Downloading the full price history…");
    return tr("Downloading %1 of prices…").arg(qs_period_cb_ ? qs_period_cb_->currentText() : tr("1 year"));
}

QString QuantStatsView::mc_loading_text() const {
    return tr("Simulating %1 paths…").arg(ui::formatting::number(mc_paths_spin_ ? mc_paths_spin_->value() : 5000, 0));
}

QString QuantStatsView::qs_signature() const {
    return ui::autoload::signature_of(QVariantMap{{QStringLiteral("period"), period_key()}});
}

QString QuantStatsView::mc_signature() const {
    return ui::autoload::signature_of(QVariantMap{
        {QStringLiteral("paths"), mc_paths_spin_ ? mc_paths_spin_->value() : 5000},
        {QStringLiteral("method"), mc_method_cb_ ? mc_method_cb_->currentData().toString() : QStringLiteral("bootstrap")},
        {QStringLiteral("period"), period_key()}});
}

void QuantStatsView::on_period_activated() {
    for (ResultTab* t : {&metrics_tab_, &returns_tab_, &drawdown_tab_, &rolling_tab_})
        t->pane->set_loading_text(qs_loading_text());
    if (!has_data_ || empty_book_)
        return;
    qs_shared_->set_view(qs_shared_->view(), qs_signature());
    mc_tab_.pane->set_signature(mc_signature()); // the simulation uses the same window: "RUN AGAIN" there
    if (ui::AutoLoadPane* p = current_qs_pane())
        p->run();
}

void QuantStatsView::launch_quantstats(quint64 run_id) {
    // Before the first set_data() there is no book yet: the run stays open and is
    // dropped when set_data() switches the panes to the portfolio's key.
    if (!has_data_)
        return;

    // Valued holdings only: an unpriced position has no weight (NaN), and sending it
    // as 0 / NaN would silently change the book being analysed.
    QStringList symbols;
    QList<double> weights;
    for (const auto& h : summary_.holdings) {
        if (!h.valued() || !std::isfinite(h.weight))
            continue;
        symbols.append(h.symbol);
        weights.append(h.weight / 100.0);
    }
    if (symbols.isEmpty()) {
        qs_shared_->fail(run_id, tr("No position can be valued yet — there is nothing to analyse."));
        return;
    }

    // The portfolio's own benchmark and risk-free rate (base currency); an unknown
    // rate is sent as absent so the script reports it instead of inventing one.
    auto& psvc = services::PortfolioService::instance();
    const QString bench = services::PortfolioService::benchmark_for(summary_.portfolio);
    const auto rf = psvc.risk_free_for(summary_.portfolio);
    const QJsonObject context = psvc.analytics_context(summary_);

    QPointer<QuantStatsView> self = this;
    const services::AnalyticsRequest req = PortfolioAnalyticsService::instance().run_quantstats(
        symbols, weights, rf.known ? rf.rate : std::numeric_limits<double>::quiet_NaN(), bench,
        [self, run_id](const AnalyticsResult& r) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, run_id, r]() {
                    if (!self)
                        return;
                    self->qs_requests_.remove(run_id);
                    if (r.cancelled)
                        return; // the panes left it; nothing to show
                    if (!r.success) {
                        self->qs_shared_->fail(run_id,
                                               qs_plain_error(r.error, QuantStatsView::tr("The QuantStats analysis failed.")));
                        return;
                    }
                    self->qs_shared_->finish(run_id, r.data);
                },
                Qt::QueuedConnection);
        },
        period_key(), context);
    qs_requests_.insert(run_id, req);
}

void QuantStatsView::start_monte_carlo(quint64 request_id) {
    if (!has_data_)
        return; // dropped when set_data() switches the pane to the portfolio's key

    QStringList symbols;
    QList<double> weights;
    for (const auto& h : summary_.holdings) {
        if (!h.valued() || !std::isfinite(h.weight))
            continue;
        symbols.append(h.symbol);
        weights.append(h.weight / 100.0);
    }
    if (symbols.isEmpty()) {
        mc_tab_.pane->fail(request_id, tr("No position can be valued yet — there is nothing to simulate."));
        return;
    }
    const int paths = mc_paths_spin_ ? mc_paths_spin_->value() : 5000;
    QJsonObject context = services::PortfolioService::instance().analytics_context(summary_);
    if (mc_method_cb_)
        context.insert(QStringLiteral("method"), mc_method_cb_->currentData().toString());
    context.insert(QStringLiteral("period"), period_key());

    QPointer<QuantStatsView> self = this;
    const services::AnalyticsRequest req = PortfolioAnalyticsService::instance().run_monte_carlo(
        symbols, weights, paths,
        [self, request_id](const AnalyticsResult& r) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, request_id, r]() {
                    if (!self)
                        return;
                    self->mc_requests_.remove(request_id);
                    if (r.cancelled)
                        return;
                    if (!r.success) {
                        self->mc_tab_.pane->fail(
                            request_id, qs_plain_error(r.error, QuantStatsView::tr("The Monte Carlo simulation failed.")));
                        return;
                    }
                    self->mc_tab_.pane->deliver(request_id, r.data); // a stale id is ignored by the pane
                },
                Qt::QueuedConnection);
        },
        context);
    mc_requests_.insert(request_id, req);
}

// ── Language ──────────────────────────────────────────────────────────────────

void QuantStatsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void QuantStatsView::retranslateUi() {
    if (tabs_) {
        if (metrics_tab_index_ >= 0)
            tabs_->setTabText(metrics_tab_index_, tr("METRICS"));
        if (returns_tab_index_ >= 0)
            tabs_->setTabText(returns_tab_index_, tr("RETURNS"));
        if (drawdown_tab_index_ >= 0)
            tabs_->setTabText(drawdown_tab_index_, tr("DRAWDOWN"));
        if (rolling_tab_index_ >= 0)
            tabs_->setTabText(rolling_tab_index_, tr("ROLLING"));
        if (mc_tab_index_ >= 0)
            tabs_->setTabText(mc_tab_index_, tr("MONTE CARLO"));
    }
    if (title_lbl_)
        title_lbl_->setText(tr("QUANTSTATS ANALYSIS"));
    if (sibling_lbl_)
        sibling_lbl_->setText(tr("Back-test of today's weights · the ffn-based equivalent is under ⋯ › FFN Analysis; "
                                 "realised (time-weighted) performance is in PERF/RISK"));
    if (qs_run_btn_)
        qs_run_btn_->setText(tr("▶ RUN QUANTSTATS"));
    if (metrics_section_lbl_)
        metrics_section_lbl_->setText(tr("KEY PERFORMANCE INDICATORS"));
    if (mc_title_)
        mc_title_->setText(tr("MONTE CARLO SIMULATION"));
    if (mc_desc_)
        mc_desc_->setText(tr("Simulate portfolio return paths — bootstrap of whole historical days (keeps fat "
                             "tails and co-movement) or geometric Brownian motion — to estimate the distribution "
                             "of future returns, drawdowns and terminal wealth."));
    if (paths_lbl_)
        paths_lbl_->setText(tr("Paths:"));
    if (mc_run_btn_)
        mc_run_btn_->setText(tr("▶ RUN MONTE CARLO"));
    if (metrics_table_)
        metrics_table_->setHorizontalHeaderLabels({tr("METRIC"), tr("VALUE"), tr("BENCHMARK")});
    if (mc_kpis_) {
        const std::pair<const char*, QString> labels[] = {
            {"median", tr("MEDIAN RETURN")},   {"p5", tr("5TH PERCENTILE")},     {"p95", tr("95TH PERCENTILE")},
            {"prob_loss", tr("PROB OF LOSS")}, {"max_dd", tr("EXP MAX DRAWDOWN")}, {"cvar", tr("WORST 5% AVG")},
            {"loss_at_risk", tr("≈ LOSS AT RISK")}};
        for (const auto& [key, label] : labels)
            if (ui::StatTile* t = mc_kpis_->tile(QLatin1String(key)))
                t->set_label(label);
    }

    for (ResultTab* t : {&metrics_tab_, &returns_tab_, &drawdown_tab_, &rolling_tab_})
        t->pane->set_loading_text(qs_loading_text());
    mc_tab_.pane->set_loading_text(mc_loading_text());
    for (ResultTab* t : {&metrics_tab_, &returns_tab_, &drawdown_tab_, &rolling_tab_, &mc_tab_})
        if (t->empty)
            t->empty->apply(ui::EmptyState::no_data_spec(tr("No positions to analyse"), t->empty_text(), tr("BUY")));

    // The rendered results carry translated row names and titles: render them again
    // (the visible tab now, the others on their next show).
    on_appearance_settled();
}

} // namespace fincept::screens
