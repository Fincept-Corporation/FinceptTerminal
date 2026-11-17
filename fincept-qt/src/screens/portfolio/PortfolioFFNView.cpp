// src/screens/portfolio/PortfolioFFNView.cpp
#include "screens/portfolio/PortfolioFFNView.h"

#include "core/logging/Logger.h"
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "screens/portfolio/PortfolioSharedRun.h"
#include "services/portfolio/PortfolioAnalyticsService.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/StatTile.h"

#include <QDate>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QKeySequence>
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

/// A run that has not answered after this long shows "Timed out" + RETRY.
static constexpr int kFfnTimeoutS = 120;
/// Inputs of the run besides the holdings (in the view key): fixed today.
static const char* const kFfnSignature = "ffn-1y";

// ── Palette for multi-series charts ──────────────────────────────────────────
// The theme's six series colours, then two more hues of the portfolio category
// palette (violet, fuchsia) so up to eight lines stay distinct.
static QColor series_color(int index) {
    const int i = index % 8;
    if (i < 6)
        return ui::colors::series(i);
    return PortfolioSectorPanel::sector_color(i == 6 ? 10 : 15);
}

// Table row height: 28px at the default font size, growing with it.
static int table_row_height() {
    return std::max(28, ui::fonts::font_px(-3) + 17);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static QString ffn_pct_str(double v, int dp = 2) {
    return ui::formatting::fraction_pct(v, dp, true);
}
/// Unsigned percentage of a magnitude (volatility, a weight).
static QString ffn_upct_str(double v, int dp = 2) {
    return ui::formatting::fraction_pct(v, dp, false);
}
static QString fmt(double v, int dp = 2) {
    return ui::formatting::number(v, dp);
}

static QTableWidgetItem* ffn_make_item(const QString& text, Qt::Alignment align, const QColor& color) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setForeground(color);
    return item;
}

/// Direction colour of a signed figure (the text carries the sign too).
static QColor ffn_sign_color(double v) {
    return QColor(v >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
}

static ui::StatTile::Tone ffn_tone(const std::optional<double>& v) {
    if (!v)
        return ui::StatTile::Tone::Neutral;
    return *v >= 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative;
}

/// The script's own plain sentence: a multi-line message (a traceback) is cut to its last line.
static QString ffn_plain_error(const QString& raw, const QString& fallback) {
    const QStringList lines = raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString msg = lines.isEmpty() ? QString() : lines.last().trimmed();
    if (msg.isEmpty())
        msg = fallback;
    return msg.left(300);
}

static QString ffn_title_style() {
    return QString("color:%1; font-size:" + fincept::ui::fonts::px(-3) + "px; font-weight:700; letter-spacing:1px;")
        .arg(ui::colors::AMBER());
}

static QString ffn_subtitle_style() {
    return QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700;")
        .arg(ui::colors::TEXT_SECONDARY());
}

/// A page's own background, scoped to it (an unscoped rule would reach the shared
/// state widgets inside).
static void ffn_scoped_background(QWidget* w, const QString& name) {
    w->setObjectName(name);
    w->setStyleSheet(QString("#%1 { background:%2; }").arg(name, ui::colors::BG_BASE()));
}

// The page stack inside `tabs` (QTabWidget's own child), so a tab page built there is not
// moved — and re-styled as a whole — by addTab(). `fallback` when it cannot be found.
static QWidget* ffn_page_parent(QTabWidget* tabs, QWidget* fallback) {
    if (!tabs)
        return fallback;
    QWidget* stack = tabs->findChild<QStackedWidget*>(QStringLiteral("qt_tabwidget_stackedwidget"),
                                                     Qt::FindDirectChildrenOnly);
    return stack ? stack : fallback;
}

static double ffn_date_ms(const QString& d) {
    const QDate day = QDate::fromString(d.left(10), Qt::ISODate);
    return day.isValid() ? static_cast<double>(QDateTime(day, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch())
                         : std::numeric_limits<double>::quiet_NaN();
}

/// A {"YYYY-MM-DD": value} map as chart points (sorted by the chart), value × `scale`.
static QVector<QPointF> ffn_points(const QJsonObject& date_map, double scale) {
    QVector<QPointF> pts;
    pts.reserve(date_map.size());
    for (auto it = date_map.begin(); it != date_map.end(); ++it) {
        const double x = ffn_date_ms(it.key());
        if (!std::isfinite(x) || !it.value().isDouble())
            continue;
        pts.append({x, it.value().toDouble() * scale});
    }
    return pts;
}

static ui::TimeSeriesChart* ffn_time_chart(QWidget* parent, const QString& export_name) {
    auto* chart = new ui::TimeSeriesChart(parent);
    chart->set_utc(true);
    chart->set_range_presets({"3M", "6M", "YTD", "1Y", "ALL"});
    chart->set_time_formatter([](double x) { return ui::formatting::bar_date(static_cast<qint64>(x / 1000.0)); });
    chart->set_export_name(export_name);
    return chart;
}

// ── Benchmark stats ───────────────────────────────────────────────────────────
// Computed natively from the benchmark daily closes routed in via
// set_benchmark() (same 1-year series the perf-chart overlay uses). All values
// are fractions. Sharpe here is (CAGR − rf) / vol, the same definition
// ffn_analysis.py uses for the portfolio column, so the two are comparable.
struct BenchStats {
    bool valid = false;
    double total_return = 0, cagr = 0, volatility = 0, sharpe = 0, max_drawdown = 0;
};

static BenchStats compute_bench_stats(const QVector<double>& closes, double rf_rate) {
    BenchStats s;
    if (closes.size() < 3 || closes.first() <= 0.0 || closes.last() <= 0.0)
        return s;

    const int n = closes.size() - 1; // daily return periods
    s.total_return = closes.last() / closes.first() - 1.0;
    s.cagr = std::pow(closes.last() / closes.first(), 252.0 / n) - 1.0;

    // Annualized volatility from daily simple returns (sample stdev × √252)
    QVector<double> rets;
    rets.reserve(n);
    double mean = 0;
    for (int i = 1; i < closes.size(); ++i) {
        if (closes[i - 1] <= 0.0)
            continue;
        const double r = closes[i] / closes[i - 1] - 1.0;
        rets.append(r);
        mean += r;
    }
    if (rets.size() < 2)
        return s;
    mean /= rets.size();
    double var = 0;
    for (double r : rets)
        var += (r - mean) * (r - mean);
    var /= (rets.size() - 1);
    s.volatility = std::sqrt(var) * std::sqrt(252.0);

    s.sharpe = s.volatility > 1e-9 ? (s.cagr - rf_rate) / s.volatility : 0.0;

    // Max drawdown: worst peak-to-trough decline
    double peak = closes.first();
    for (double c : closes) {
        peak = std::max(peak, c);
        if (peak > 0.0)
            s.max_drawdown = std::min(s.max_drawdown, c / peak - 1.0);
    }

    s.valid = true;
    return s;
}

// ── Constructor / build_ui ────────────────────────────────────────────────────

PortfolioFFNView::PortfolioFFNView(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &PortfolioFFNView::on_appearance_settled);
    // Currency / number format changed: the same path — tabs with a result re-render
    // from their remembered payload (the visible one now, the others when shown).
    ui::formatting::on_format_changed(this, [this]() { on_appearance_settled(); });
}

PortfolioFFNView::~PortfolioFFNView() {
    // Stop scripts still running for this view, and drop the panes while the
    // members their callbacks use still exist.
    for (const auto& r : std::as_const(requests_))
        r.cancel();
    delete tabs_;
    tabs_ = nullptr;
}

void PortfolioFFNView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setMinimumHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));

    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(8);

    back_btn_ = new QPushButton(tr("← BACK"), header);
    back_btn_->setMinimumHeight(24);
    back_btn_->setCursor(Qt::PointingHandCursor);
    back_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                     "  padding:0 10px; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; }"
                                     "QPushButton:hover { background:%1; color:%2; }")
                                 .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT()));
    // Esc is the expected "leave this full-screen view" key. The shortcut is
    // owned by the button, so it is inert while this page is not the visible
    // one in PortfolioScreen's stack.
    back_btn_->setShortcut(QKeySequence(Qt::Key_Escape));
    back_btn_->setToolTip(tr("Back to the portfolio workspace  (Esc)"));
    back_btn_->setAccessibleName(tr("Back to the portfolio workspace"));
    connect(back_btn_, &QPushButton::clicked, this, &PortfolioFFNView::back_requested);
    h_layout->addWidget(back_btn_);

    auto* sep = new QWidget(header);
    sep->setFixedSize(1, 20);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_MED()));
    h_layout->addWidget(sep);

    title_label_ = new QLabel(tr("FFN ANALYTICS"), header);
    title_label_->setStyleSheet(ffn_title_style());
    h_layout->addWidget(title_label_);

    h_layout->addStretch();

    status_label_ = new QLabel(header);
    status_label_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    h_layout->addWidget(status_label_);

    run_btn_ = new QPushButton(tr("RUN FFN ANALYSIS"), header);
    run_btn_->setMinimumHeight(24);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%4; border:none;"
                "  padding:0 12px; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton:hover { background:%5; }"
                "QPushButton:disabled { background:%2; color:%3; }")
            .arg(ui::colors::AMBER(), ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::TEXT_ON_ACCENT(), ui::colors::WARNING()));
    connect(run_btn_, &QPushButton::clicked, this, [this]() {
        if (ui::AutoLoadPane* p = current_pane())
            p->run();
    });
    h_layout->addWidget(run_btn_);

    layout->addWidget(header);

    // ── Tabs ──────────────────────────────────────────────────────────────────
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
    QWidget* pages = ffn_page_parent(tabs_, this);

    // One ffn run feeds the six tabs.
    shared_ = new PortfolioSharedRun(this);
    shared_->set_launch([this](quint64 run) { launch_ffn(run); });
    shared_->set_abort([this](quint64 run) { requests_.take(run).cancel(); });

    const QString table_ss = QString("QTableWidget { background:%1; color:%2; border:none; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                                     "QTableWidget::item { padding:3px 8px; border-bottom:1px solid %3; }"
                                     "QHeaderView::section { background:%4; color:%5; border:none;"
                                     "  border-bottom:2px solid %6; padding:3px 8px;"
                                     "  font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; }")
                                 .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                      ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER());

    // helper to create a configured QTableWidget
    auto make_table = [&](QWidget* parent, int cols, const QStringList& headers) -> QTableWidget* {
        auto* t = new QTableWidget(parent);
        t->setColumnCount(cols);
        t->setHorizontalHeaderLabels(headers);
        t->setSelectionMode(QAbstractItemView::NoSelection);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setShowGrid(false);
        t->verticalHeader()->setVisible(false);
        t->horizontalHeader()->setStretchLastSection(true);
        t->setStyleSheet(table_ss);
        return t;
    };

    // ── OVERVIEW tab ─────────────────────────────────────────────────────────
    {
        new_tab(overview_tab_, pages, static_cast<int>(ui::LoadingSkeleton::Shape::Result),
                [this]() { update_overview(); });
        overview_tab_.empty_text = []() {
            return tr("Annualised return, volatility, Sharpe ratio and drawdown of the current weights appear once "
                      "the portfolio holds a position.");
        };
        auto* w = new QWidget(overview_tab_.pane->content_parent());
        ffn_scoped_background(w, QStringLiteral("ffnOverview"));
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(8);

        overview_hdr_ = new QLabel(tr("PORTFOLIO METRICS OVERVIEW"), w);
        overview_hdr_->setStyleSheet(ffn_title_style());
        vl->addWidget(overview_hdr_);

        overview_kpis_ = new ui::KpiStrip(w);
        overview_kpis_->add_tile(QStringLiteral("ret"), tr("ANN. RETURN"));
        overview_kpis_->add_tile(QStringLiteral("vol"), tr("VOLATILITY"));
        overview_kpis_->add_tile(QStringLiteral("sharpe"), tr("SHARPE"));
        overview_kpis_->add_tile(QStringLiteral("dd"), tr("MAX DRAWDOWN"));
        overview_kpis_->tile(QStringLiteral("vol"))->set_delta_inverted(true);
        vl->addWidget(overview_kpis_);

        overview_table_ = make_table(w, 2, {tr("METRIC"), tr("PORTFOLIO")});
        overview_table_->setColumnWidth(0, 240);
        vl->addWidget(overview_table_, 1);
        overview_tab_.pane->set_content(w);

        tabs_->addTab(overview_tab_.stack, tr("OVERVIEW"));
    }

    // ── BENCHMARK tab ────────────────────────────────────────────────────────
    {
        auto* page = new QWidget(pages);
        ffn_scoped_background(page, QStringLiteral("ffnBenchmarkPage"));
        auto* vl = new QVBoxLayout(page);
        vl->setContentsMargins(16, 12, 16, 12);
        vl->setSpacing(10);

        benchmark_hdr_ = new QLabel(tr("BENCHMARK COMPARISON"), page);
        benchmark_hdr_->setStyleSheet(ffn_title_style());
        vl->addWidget(benchmark_hdr_);

        // The benchmark's own figures need no run: shown at once from the closes
        // the screen already holds (set_benchmark).
        bench_kpis_ = new ui::KpiStrip(page);
        bench_kpis_->add_tile(QStringLiteral("total"), tr("TOTAL RETURN"));
        bench_kpis_->add_tile(QStringLiteral("cagr"), tr("CAGR"));
        bench_kpis_->add_tile(QStringLiteral("vol"), tr("VOLATILITY"));
        bench_kpis_->add_tile(QStringLiteral("sharpe"), tr("SHARPE"));
        bench_kpis_->add_tile(QStringLiteral("dd"), tr("MAX DRAWDOWN"));
        vl->addWidget(bench_kpis_);

        new_tab(benchmark_tab_, page, static_cast<int>(ui::LoadingSkeleton::Shape::Table),
                [this]() { update_benchmark(); });
        benchmark_tab_.pane->skeleton()->set_row_count(5);
        benchmark_tab_.empty_text = []() {
            return tr("The current weights' return, volatility and Sharpe ratio next to the benchmark appear once the "
                      "portfolio holds a position.");
        };
        benchmark_table_ = make_table(benchmark_tab_.pane->content_parent(), 3,
                                      {tr("METRIC"), tr("PORTFOLIO"), tr("BENCHMARK")});
        benchmark_table_->setColumnWidth(0, 200);
        benchmark_table_->setColumnWidth(1, 150);
        benchmark_tab_.pane->set_content(benchmark_table_);
        vl->addWidget(benchmark_tab_.stack, 1);

        benchmark_info_label_ = new QLabel(benchmark_info_text(), page);
        benchmark_info_label_->setWordWrap(true);
        benchmark_info_label_->setStyleSheet(
            QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; padding:6px 0;").arg(ui::colors::TEXT_TERTIARY()));
        vl->addWidget(benchmark_info_label_);

        tabs_->addTab(page, tr("BENCHMARK"));
        update_bench_strip();
    }

    // ── OPTIMISATION tab ─────────────────────────────────────────────────────
    {
        new_tab(optimization_tab_, pages, static_cast<int>(ui::LoadingSkeleton::Shape::Result),
                [this]() { update_optimization(); });
        optimization_tab_.empty_text = []() {
            return tr("Equal-risk, inverse-volatility and equal weights compared with the current mix appear once the "
                      "portfolio holds a position.");
        };
        auto* scroll = new QScrollArea(optimization_tab_.pane->content_parent());
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* w = new QWidget(scroll->viewport());
        ffn_scoped_background(w, QStringLiteral("ffnOptimisation"));
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(10);

        optimization_hdr_ = new QLabel(tr("PORTFOLIO OPTIMISATION — WEIGHT COMPARISON"), w);
        optimization_hdr_->setStyleSheet(ffn_title_style());
        vl->addWidget(optimization_hdr_);

        opt_bars_ = new ui::BarChart(w);
        opt_bars_->set_mode(ui::BarChart::Mode::Grouped);
        opt_bars_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1, false); });
        opt_bars_->setMinimumHeight(std::max(200, ui::fonts::font_px(0) * 14));
        vl->addWidget(opt_bars_);

        weights_hdr_ = new QLabel(tr("ALLOCATION WEIGHTS BY STRATEGY"), w);
        weights_hdr_->setStyleSheet(ffn_subtitle_style());
        vl->addWidget(weights_hdr_);

        opt_weights_table_ = make_table(w, 5, {tr("SYMBOL"), tr("CURRENT"), tr("ERC"), tr("INV-VOL"), tr("EQUAL")});
        opt_weights_table_->setColumnWidth(0, 100);
        vl->addWidget(opt_weights_table_);

        stats_hdr_ = new QLabel(tr("STRATEGY PERFORMANCE STATS"), w);
        stats_hdr_->setStyleSheet(ffn_subtitle_style());
        vl->addWidget(stats_hdr_);

        opt_stats_table_ =
            make_table(w, 5, {tr("STRATEGY"), tr("TOTAL RETURN"), tr("VOLATILITY"), tr("SHARPE"), tr("MAX DRAWDOWN")});
        opt_stats_table_->setColumnWidth(0, 120);
        vl->addWidget(opt_stats_table_);
        vl->addStretch();

        scroll->setWidget(w);
        optimization_tab_.pane->set_content(scroll);
        tabs_->addTab(optimization_tab_.stack, tr("OPTIMISATION"));
    }

    // ── REBASED tab ───────────────────────────────────────────────────────────
    {
        new_tab(rebased_tab_, pages, static_cast<int>(ui::LoadingSkeleton::Shape::Chart),
                [this]() { update_rebased(); });
        rebased_tab_.empty_text = []() {
            return tr("Every holding on a common base of 100 appears once the portfolio holds a position.");
        };
        rebased_chart_ = ffn_time_chart(rebased_tab_.pane->content_parent(), QStringLiteral("ffn_rebased"));
        rebased_chart_->set_title(tr("Rebased performance (base = 100)"));
        rebased_chart_->set_value_mode(ui::TimeSeriesChart::ValueMode::Rebased100); // re-bases as you zoom
        rebased_chart_->set_show_mode_buttons(true);
        rebased_tab_.pane->set_content(rebased_chart_);
        tabs_->addTab(rebased_tab_.stack, tr("REBASED"));
    }

    // ── DRAWDOWNS tab ─────────────────────────────────────────────────────────
    {
        new_tab(drawdowns_tab_, pages, static_cast<int>(ui::LoadingSkeleton::Shape::Chart),
                [this]() { update_drawdowns(); });
        drawdowns_tab_.empty_text = []() {
            return tr("The historical drawdowns of each holding appear once the portfolio holds a position.");
        };
        drawdowns_chart_ = ffn_time_chart(drawdowns_tab_.pane->content_parent(), QStringLiteral("ffn_drawdowns"));
        drawdowns_chart_->set_title(tr("Drawdown from the running peak"));
        drawdowns_chart_->set_show_mode_buttons(false);
        drawdowns_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                              [](double v) { return ui::formatting::pct(v, 1); });
        drawdowns_tab_.pane->set_content(drawdowns_chart_);
        tabs_->addTab(drawdowns_tab_.stack, tr("DRAWDOWNS"));
    }

    // ── ROLLING CORRELATIONS tab ──────────────────────────────────────────────
    {
        new_tab(rolling_tab_, pages, static_cast<int>(ui::LoadingSkeleton::Shape::Chart),
                [this]() { update_rolling(); });
        rolling_tab_.empty_text = []() {
            return tr("60-day rolling correlations between the holdings appear once the portfolio holds two or more "
                      "positions.");
        };
        rolling_content_ = new QStackedWidget(rolling_tab_.pane->content_parent());
        rolling_chart_ = ffn_time_chart(rolling_content_, QStringLiteral("ffn_rolling_correlation"));
        rolling_chart_->set_title(tr("Rolling 60-day correlations"));
        rolling_chart_->set_show_mode_buttons(false);
        rolling_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                            [](double v) { return ui::formatting::number(v, 2); });
        rolling_chart_->set_y_range(0, ui::TimeSeriesChart::Axis::Left, -1.0, 1.0);
        rolling_content_->addWidget(rolling_chart_);
        rolling_tab_.pane->set_content(rolling_content_);
        tabs_->addTab(rolling_tab_.stack, tr("ROLLING"));
    }

    layout->addWidget(tabs_, 1);

    overview_tab_.pane->set_status_label(status_label_);
    connect(tabs_, &QTabWidget::currentChanged, this, &PortfolioFFNView::on_tab_changed);
}

PortfolioFFNView::ResultTab& PortfolioFFNView::new_tab(ResultTab& tab, QWidget* parent, int shape,
                                                       std::function<void()> render) {
    tab.stack = new QStackedWidget(parent);
    tab.pane = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, tab.stack);
    tab.stack->addWidget(tab.pane);
    tab.render = std::move(render);
    ui::AutoLoadPane* pane = tab.pane;
    pane->skeleton()->set_shape(static_cast<ui::LoadingSkeleton::Shape>(shape));
    pane->set_loading_text(tr("Downloading 1 year of prices…"));
    pane->set_cache_key(QStringLiteral("portfolio/ffn"), kPortfolioResultTtlDays);
    pane->set_persist_limit(1024 * 1024); // per-symbol stats + rebased / drawdown / rolling series (~480 KB)
    pane->set_max_age(kPortfolioScriptMaxAgeS);
    pane->set_timeout(kFfnTimeoutS);
    ResultTab* t = &tab;
    pane->set_render([this, pane, t](const QJsonValue& payload, const QDateTime&, bool) {
        theme_stale_.remove(pane);
        ffn_data_ = payload.toObject();
        if (t->render)
            t->render();
    });
    shared_->attach(pane);
    return tab;
}

// ── Tabs / state ──────────────────────────────────────────────────────────────

PortfolioFFNView::ResultTab* PortfolioFFNView::tab_for_index(int index) {
    QWidget* page = tabs_ ? tabs_->widget(index) : nullptr;
    if (!page)
        return nullptr;
    for (ResultTab* t : {&overview_tab_, &benchmark_tab_, &optimization_tab_, &rebased_tab_, &drawdowns_tab_,
                         &rolling_tab_}) {
        if (t->stack == page || t->stack->parentWidget() == page)
            return t;
    }
    return nullptr;
}

ui::AutoLoadPane* PortfolioFFNView::current_pane() {
    ResultTab* t = tabs_ ? tab_for_index(tabs_->currentIndex()) : nullptr;
    return t ? t->pane : overview_tab_.pane;
}

void PortfolioFFNView::on_tab_changed(int index) {
    ResultTab* current = tab_for_index(index);
    if (!current)
        return;
    rerender_if_stale(*current);
    // The header's "as of" follows the tab on screen.
    for (ResultTab* t : {&overview_tab_, &benchmark_tab_, &optimization_tab_, &rebased_tab_, &drawdowns_tab_,
                         &rolling_tab_})
        if (t != current)
            t->pane->set_status_label(nullptr);
    current->pane->set_status_label(status_label_);
}

void PortfolioFFNView::set_empty_book(bool empty) {
    if (empty == empty_book_)
        return;
    empty_book_ = empty;
    for (ResultTab* t : {&overview_tab_, &benchmark_tab_, &optimization_tab_, &rebased_tab_, &drawdowns_tab_,
                         &rolling_tab_}) {
        if (!empty) {
            t->stack->setCurrentWidget(t->pane);
            continue;
        }
        if (!t->empty) {
            t->empty = new ui::EmptyState(t->stack);
            t->stack->addWidget(t->empty);
            connect(t->empty, &ui::EmptyState::primary_clicked, this, &PortfolioFFNView::add_position_requested);
        }
        t->empty->apply(ui::EmptyState::no_data_spec(tr("No positions to analyse"), t->empty_text(), tr("BUY")));
        t->stack->setCurrentWidget(t->empty); // the pane is hidden: nothing runs
    }
    if (empty)
        status_label_->clear();
}

void PortfolioFFNView::on_appearance_settled() {
    for (ResultTab* t : {&overview_tab_, &benchmark_tab_, &optimization_tab_, &rebased_tab_, &drawdowns_tab_,
                         &rolling_tab_})
        if (t->pane->has_result())
            theme_stale_.insert(t->pane);
    if (ResultTab* t = tabs_ ? tab_for_index(tabs_->currentIndex()) : nullptr)
        rerender_if_stale(*t);
}

void PortfolioFFNView::rerender_if_stale(ResultTab& tab) {
    if (!theme_stale_.contains(tab.pane))
        return;
    theme_stale_.remove(tab.pane);
    if (tab.pane->has_result() && tab.render) {
        ffn_data_ = tab.pane->payload().toObject();
        tab.render();
    }
}

QString PortfolioFFNView::benchmark_info_text() const {
    return tr("Portfolio column: the CURRENT weights applied to 1-year daily price history (a back-test of today's "
              "mix, not your realised performance).\nBenchmark: %1, 1-year daily closes. "
              "Sharpe = (CAGR − risk-free %2%) / volatility.")
        .arg(benchmark_symbol_, ui::formatting::number(risk_free_rate_ * 100.0, 2));
}

// ── set_data ──────────────────────────────────────────────────────────────────

void PortfolioFFNView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;

    // An empty book has nothing to analyse: the tabs say so (with BUY) and nothing runs.
    const bool has_holdings = !summary_.holdings.isEmpty();
    run_btn_->setEnabled(has_holdings);
    run_btn_->setToolTip(has_holdings ? QString() : tr("Add a position first"));
    set_empty_book(!has_holdings);

    // Results are kept per portfolio and holdings: a switch or a trade selects
    // another remembered result (or runs ffn once for it); a price tick keeps the
    // key, so nothing re-runs before the result's max age.
    if (has_holdings)
        shared_->set_view(portfolio_run_key(summary_), QString::fromLatin1(kFfnSignature));

    // The OVERVIEW carries live rows of the book (value, cost, P&L): they follow every refresh.
    if (overview_tab_.pane->has_result()) {
        ffn_data_ = overview_tab_.pane->payload().toObject();
        update_overview();
    }
}

void PortfolioFFNView::set_benchmark(const QString& symbol, const QVector<double>& closes, double risk_free_rate) {
    benchmark_closes_ = closes;
    if (!symbol.isEmpty())
        benchmark_symbol_ = symbol;
    if (std::isfinite(risk_free_rate))
        risk_free_rate_ = risk_free_rate;
    if (benchmark_info_label_)
        benchmark_info_label_->setText(benchmark_info_text());
    update_bench_strip();
    // Refresh the result tabs that compare with the benchmark.
    for (ResultTab* t : {&overview_tab_, &benchmark_tab_}) {
        if (t->pane->has_result() && t->render) {
            ffn_data_ = t->pane->payload().toObject();
            t->render();
        }
    }
}

// ── update_bench_strip ────────────────────────────────────────────────────────

void PortfolioFFNView::update_bench_strip() {
    if (!bench_kpis_)
        return;
    const BenchStats b = compute_bench_stats(benchmark_closes_, risk_free_rate_);
    struct T {
        const char* key;
        QString value;
        std::optional<double> signed_value;
    };
    const T tiles[] = {
        {"total", b.valid ? ffn_pct_str(b.total_return) : QStringLiteral("--"),
         b.valid ? std::optional<double>(b.total_return) : std::nullopt},
        {"cagr", b.valid ? ffn_pct_str(b.cagr) : QStringLiteral("--"), b.valid ? std::optional<double>(b.cagr) : std::nullopt},
        {"vol", b.valid ? ffn_upct_str(b.volatility) : QStringLiteral("--"), std::nullopt},
        {"sharpe", b.valid ? fmt(b.sharpe) : QStringLiteral("--"), std::nullopt},
        {"dd", b.valid ? ffn_pct_str(b.max_drawdown) : QStringLiteral("--"),
         b.valid ? std::optional<double>(b.max_drawdown) : std::nullopt},
    };
    const QString sub = b.valid ? tr("%1 · 1 year").arg(benchmark_symbol_)
                                : tr("%1 · history arrives with the risk data").arg(benchmark_symbol_);
    for (const auto& t : tiles) {
        if (ui::StatTile* tile = bench_kpis_->tile(QLatin1String(t.key))) {
            tile->set_value(t.value);
            tile->set_tone(ffn_tone(t.signed_value));
            tile->set_sub_label(sub);
        }
    }
}

// ── update_overview ───────────────────────────────────────────────────────────

void PortfolioFFNView::update_overview() {
    if (!overview_table_ || !overview_kpis_)
        return;

    // Portfolio-level return/vol/Sharpe/drawdown come ONLY from the optimiser's
    // "current" block (a real weighted return series). The old fallback summed
    // per-symbol weighted vols, Sharpes and drawdowns — none of which are linear
    // in the weights — and summed every holding's up/down day counts into
    // meaningless totals.
    std::optional<double> total_ann_ret, total_ann_vol, total_sharpe, total_max_dd;
    double total_best_day = 0, total_worst_day = 0;
    QString best_sym, worst_sym;
    double best_ret = -1e9, worst_ret = 1e9;

    for (const auto& h : summary_.holdings) {
        if (!ffn_data_.contains(h.symbol))
            continue;
        auto s = ffn_data_[h.symbol].toObject();
        total_best_day = std::max(total_best_day, s["best_day"].toDouble());
        total_worst_day = std::min(total_worst_day, s["worst_day"].toDouble());
        double sym_ret = s["total_return"].toDouble();
        if (sym_ret > best_ret) {
            best_ret = sym_ret;
            best_sym = h.symbol;
        }
        if (sym_ret < worst_ret) {
            worst_ret = sym_ret;
            worst_sym = h.symbol;
        }
    }
    const auto cur_stats = ffn_data_["optimization"].toObject()["stats"].toObject()["current"].toObject();
    auto num = [&cur_stats](const char* k) -> std::optional<double> {
        const QJsonValue v = cur_stats.value(QLatin1String(k));
        return v.isDouble() ? std::optional<double>(v.toDouble()) : std::nullopt;
    };
    total_ann_ret = num("cagr");
    total_ann_vol = num("volatility");
    total_sharpe = num("sharpe");
    total_max_dd = num("max_drawdown");

    // ── Headline tiles, each against the benchmark ────────────────────────────
    const BenchStats bench = compute_bench_stats(benchmark_closes_, risk_free_rate_);
    const QString vs = tr("1Y, current weights");
    auto fill = [&](const char* key, const std::optional<double>& v, const QString& text, bool signed_tone,
                    double bench_value, bool as_points) {
        ui::StatTile* t = overview_kpis_->tile(QLatin1String(key));
        if (!t)
            return;
        t->set_value(v ? text : QStringLiteral("--"));
        t->set_tone(signed_tone ? ffn_tone(v) : ui::StatTile::Tone::Neutral);
        t->set_sub_label(vs);
        if (v && bench.valid) {
            const double diff = as_points ? (*v - bench_value) * 100.0 : *v - bench_value;
            const QString d = as_points ? ui::formatting::pp(diff, 1) : ui::formatting::signed_number(diff, 2);
            t->set_delta(diff, tr("%1 vs %2").arg(d, benchmark_symbol_));
        } else {
            t->clear_delta();
        }
    };
    fill("ret", total_ann_ret, total_ann_ret ? ffn_pct_str(*total_ann_ret) : QString(), true, bench.cagr, true);
    fill("vol", total_ann_vol, total_ann_vol ? ffn_upct_str(*total_ann_vol) : QString(), false, bench.volatility, true);
    fill("sharpe", total_sharpe, total_sharpe ? fmt(*total_sharpe) : QString(), false, bench.sharpe, false);
    fill("dd", total_max_dd, total_max_dd ? ffn_pct_str(*total_max_dd) : QString(), true, bench.max_drawdown, true);

    // ── The rest ──────────────────────────────────────────────────────────────
    const double pnl_pct = summary_.total_unrealized_pnl_percent;
    const double win_rate = summary_.total_positions > 0 ? summary_.gainers * 100.0 / summary_.total_positions : 0.0;

    struct Row {
        QString name;
        QString value;
        QColor color;
    };
    const QColor neutral(ui::colors::TEXT_PRIMARY());
    const QVector<Row> rows = {
        {tr("Best Day (any holding)"), ffn_pct_str(total_best_day), ffn_sign_color(total_best_day)},
        {tr("Worst Day (any holding)"), ffn_pct_str(total_worst_day), ffn_sign_color(total_worst_day)},
        {tr("Best Holding (1Y)"), best_sym.isEmpty() ? "--" : best_sym + " (" + ffn_pct_str(best_ret) + ")",
         best_sym.isEmpty() ? neutral : ffn_sign_color(best_ret)},
        {tr("Worst Holding (1Y)"), worst_sym.isEmpty() ? "--" : worst_sym + " (" + ffn_pct_str(worst_ret) + ")",
         worst_sym.isEmpty() ? neutral : ffn_sign_color(worst_ret)},
        {tr("Unrealized P&L (vs cost)"), ffn_pct_str(pnl_pct / 100.0), ffn_sign_color(pnl_pct)},
        {tr("Holdings in Profit"), fmt(win_rate) + "%", neutral},
        {tr("Positions"), QString::number(summary_.total_positions), neutral},
        {tr("Total Value"), ui::formatting::money(summary_.total_market_value, currency_), neutral},
        {tr("Cost Basis"), ui::formatting::money(summary_.total_cost_basis, currency_), neutral},
    };

    overview_table_->setUpdatesEnabled(false);
    overview_table_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        overview_table_->setRowHeight(r, table_row_height());
        overview_table_->setItem(
            r, 0, ffn_make_item(row.name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        overview_table_->setItem(r, 1, ffn_make_item(row.value, Qt::AlignRight | Qt::AlignVCenter, row.color));
    }
    overview_table_->setUpdatesEnabled(true);
}

// ── update_benchmark ──────────────────────────────────────────────────────────

void PortfolioFFNView::update_benchmark() {
    if (!benchmark_table_)
        return;
    // Populate from optimization.stats.current which gives real portfolio perf.
    const auto cur_stats = ffn_data_["optimization"].toObject()["stats"].toObject()["current"].toObject();

    // Benchmark column — "--" until set_benchmark() has delivered closes.
    const BenchStats bench = compute_bench_stats(benchmark_closes_, risk_free_rate_);
    const QStringList bench_vals =
        bench.valid ? QStringList{ffn_pct_str(bench.total_return), ffn_pct_str(bench.cagr), ffn_upct_str(bench.volatility),
                                  fmt(bench.sharpe), ffn_pct_str(bench.max_drawdown)}
                    : QStringList{"--", "--", "--", "--", "--"};

    auto cell = [&cur_stats](const char* k, int kind) {
        const QJsonValue v = cur_stats.value(QLatin1String(k));
        if (!v.isDouble())
            return QStringLiteral("--");
        return kind == 0 ? ffn_pct_str(v.toDouble()) : kind == 1 ? ffn_upct_str(v.toDouble()) : fmt(v.toDouble());
    };
    struct BRow {
        QString metric;
        QString portfolio;
    };
    const QVector<BRow> rows = {
        {tr("Total Return"), cell("total_return", 0)}, {tr("CAGR"), cell("cagr", 0)},
        {tr("Volatility"), cell("volatility", 1)},     {tr("Sharpe Ratio"), cell("sharpe", 2)},
        {tr("Max Drawdown"), cell("max_drawdown", 0)},
    };

    benchmark_table_->setHorizontalHeaderLabels({tr("METRIC"), tr("PORTFOLIO"), tr("BENCHMARK (%1)").arg(benchmark_symbol_)});
    benchmark_table_->setUpdatesEnabled(false);
    benchmark_table_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        benchmark_table_->setRowHeight(r, table_row_height());
        benchmark_table_->setItem(
            r, 0, ffn_make_item(row.metric, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        benchmark_table_->setItem(
            r, 1, ffn_make_item(row.portfolio, Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_PRIMARY())));
        benchmark_table_->setItem(
            r, 2,
            ffn_make_item(bench_vals.value(r, QStringLiteral("--")), Qt::AlignRight | Qt::AlignVCenter,
                      QColor(bench.valid ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY())));
    }
    benchmark_table_->setUpdatesEnabled(true);
}

// ── update_optimization ───────────────────────────────────────────────────────

void PortfolioFFNView::update_optimization() {
    if (!opt_weights_table_ || !opt_stats_table_)
        return;
    const auto opt_obj = ffn_data_["optimization"].toObject();
    const auto erc_obj = opt_obj["erc"].toObject();
    const auto inv_obj = opt_obj["inv_vol"].toObject();
    const auto equal_obj = opt_obj["equal"].toObject();
    const auto current_obj = opt_obj["current"].toObject();
    const auto stats_obj = opt_obj["stats"].toObject();

    // Collect symbols from the current weights object
    QStringList syms = current_obj.keys();
    syms.sort();

    // ── Grouped bars: the four allocations side by side per holding ───────────
    {
        QVector<ui::BarSeries> series;
        const std::pair<QString, const QJsonObject*> strategies[] = {
            {tr("Current"), &current_obj}, {tr("ERC"), &erc_obj}, {tr("Inverse vol"), &inv_obj}, {tr("Equal"), &equal_obj}};
        for (const auto& [name, obj] : strategies) {
            ui::BarSeries s;
            s.name = name;
            s.values.reserve(syms.size());
            for (const QString& sym : syms)
                s.values.append(obj->value(sym).isDouble() ? obj->value(sym).toDouble() * 100.0
                                                           : std::numeric_limits<double>::quiet_NaN());
            series.append(s);
        }
        opt_bars_->set_categories(syms);
        opt_bars_->set_series(series);
        if (syms.isEmpty())
            opt_bars_->set_state(ui::PaintedChart::Empty, tr("The run returned no weights."));
        else
            opt_bars_->set_state(ui::PaintedChart::Ready);
    }

    // ── Weights table ─────────────────────────────────────────────────────────
    opt_weights_table_->setUpdatesEnabled(false);
    opt_weights_table_->setRowCount(syms.size());
    auto wcell = [](const QJsonObject& o, const QString& sym) {
        return o.value(sym).isDouble() ? ffn_upct_str(o.value(sym).toDouble(), 1) : QStringLiteral("--");
    };
    for (int r = 0; r < syms.size(); ++r) {
        const QString& sym = syms[r];
        opt_weights_table_->setRowHeight(r, table_row_height());
        opt_weights_table_->setItem(r, 0,
                                    ffn_make_item(sym, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::AMBER())));
        opt_weights_table_->setItem(
            r, 1, ffn_make_item(wcell(current_obj, sym), Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::CYAN())));
        opt_weights_table_->setItem(r, 2,
                                    ffn_make_item(wcell(erc_obj, sym), Qt::AlignRight | Qt::AlignVCenter,
                                              QColor(ui::colors::TEXT_PRIMARY())));
        opt_weights_table_->setItem(r, 3,
                                    ffn_make_item(wcell(inv_obj, sym), Qt::AlignRight | Qt::AlignVCenter,
                                              QColor(ui::colors::TEXT_PRIMARY())));
        opt_weights_table_->setItem(r, 4,
                                    ffn_make_item(wcell(equal_obj, sym), Qt::AlignRight | Qt::AlignVCenter,
                                              QColor(ui::colors::TEXT_PRIMARY())));
    }
    opt_weights_table_->setMinimumHeight(table_row_height() * (static_cast<int>(syms.size()) + 1) + 6);
    opt_weights_table_->setUpdatesEnabled(true);

    // ── Stats table ───────────────────────────────────────────────────────────
    const QStringList strategy_keys = {"current", "erc", "inv_vol", "equal"};
    const QStringList strategy_names = {tr("CURRENT"), tr("ERC"), tr("INV-VOL"), tr("EQUAL")};
    opt_stats_table_->setUpdatesEnabled(false);
    opt_stats_table_->setRowCount(strategy_keys.size());

    for (int r = 0; r < strategy_keys.size(); ++r) {
        const QString& key = strategy_keys[r];
        const QString& name = strategy_names[r];
        auto s = stats_obj[key].toObject();
        opt_stats_table_->setRowHeight(r, table_row_height());

        // Named `total_ret`, not `tr` — a local called `tr` shadows QObject::tr()
        // for the rest of the scope, so any future tr("…") added here would fail
        // to compile with a baffling error.
        if (s.isEmpty() || !s["volatility"].isDouble()) {
            // The script omits a strategy whose stats it could not compute
            // (it used to report a row of fake zeros).
            opt_stats_table_->setItem(r, 0, ffn_make_item(name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::AMBER())));
            for (int c = 1; c <= 4; ++c)
                opt_stats_table_->setItem(r, c, ffn_make_item(QStringLiteral("--"), Qt::AlignRight | Qt::AlignVCenter,
                                                          QColor(ui::colors::TEXT_TERTIARY())));
            continue;
        }
        const double total_ret = s["total_return"].toDouble();
        const double vol = s["volatility"].toDouble();
        const double sh = s["sharpe"].toDouble();
        const double dd = s["max_drawdown"].toDouble();

        opt_stats_table_->setItem(r, 0, ffn_make_item(name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::AMBER())));
        opt_stats_table_->setItem(r, 1,
                                  ffn_make_item(ffn_pct_str(total_ret), Qt::AlignRight | Qt::AlignVCenter,
                                            ffn_sign_color(total_ret)));
        opt_stats_table_->setItem(
            r, 2, ffn_make_item(ffn_upct_str(vol), Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_PRIMARY())));
        opt_stats_table_->setItem(r, 3,
                                  ffn_make_item(fmt(sh), Qt::AlignRight | Qt::AlignVCenter,
                                            QColor(ui::colors::TEXT_PRIMARY())));
        opt_stats_table_->setItem(
            r, 4, ffn_make_item(ffn_pct_str(dd), Qt::AlignRight | Qt::AlignVCenter, ffn_sign_color(dd)));
    }
    opt_stats_table_->setMinimumHeight(table_row_height() * (static_cast<int>(strategy_keys.size()) + 1) + 6);
    opt_stats_table_->setUpdatesEnabled(true);
}

// ── update_rebased ────────────────────────────────────────────────────────────

void PortfolioFFNView::update_rebased() {
    if (!rebased_chart_)
        return;
    rebased_chart_->clear();
    const auto rebased_obj = ffn_data_["rebased"].toObject();
    QStringList syms = rebased_obj.keys();
    syms.sort();
    int color_idx = 0;
    for (const QString& sym : syms) {
        const QVector<QPointF> pts = ffn_points(rebased_obj[sym].toObject(), 1.0);
        if (pts.isEmpty())
            continue;
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = sym;
        spec.points = pts;
        spec.color = ui::TimeSeriesChart::ColorRole::Custom;
        spec.custom_color = series_color(color_idx++);
        spec.rebase = true;
        rebased_chart_->add_series(spec);
    }
    if (color_idx == 0)
        rebased_chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No price history came back for these holdings."));
    else
        rebased_chart_->set_state(ui::TimeSeriesChart::State::Ready);
    rebased_chart_->reset_x_range();
}

// ── update_drawdowns ──────────────────────────────────────────────────────────

void PortfolioFFNView::update_drawdowns() {
    if (!drawdowns_chart_)
        return;
    drawdowns_chart_->clear();
    const auto dd_obj = ffn_data_["drawdown_series"].toObject();
    QStringList syms = dd_obj.keys();
    syms.sort();
    const bool single = syms.size() == 1;
    int color_idx = 0;
    for (const QString& sym : syms) {
        const QVector<QPointF> pts = ffn_points(dd_obj[sym].toObject(), 100.0); // fraction -> percent
        if (pts.isEmpty())
            continue;
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = sym;
        spec.points = pts;
        spec.rebase = false;
        spec.formatter = [](double v) { return ui::formatting::pct(v, 1); };
        if (single) {
            // One holding: the underwater area in the loss colour.
            spec.style = ui::TimeSeriesChart::Style::Area;
            spec.color = ui::TimeSeriesChart::ColorRole::Negative;
            spec.baseline = 0.0;
        } else {
            // Several: one line each, told apart by colour and by the legend / tooltip.
            spec.color = ui::TimeSeriesChart::ColorRole::Custom;
            spec.custom_color = series_color(color_idx);
        }
        ++color_idx;
        drawdowns_chart_->add_series(spec);
    }
    drawdowns_chart_->set_show_legend(color_idx > 1);
    if (color_idx == 0) {
        drawdowns_chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No price history came back for these holdings."));
    } else {
        drawdowns_chart_->add_hline(0.0, QString(), ui::TimeSeriesChart::ColorRole::Muted);
        drawdowns_chart_->set_state(ui::TimeSeriesChart::State::Ready);
    }
    drawdowns_chart_->reset_x_range();
}

// ── update_rolling ────────────────────────────────────────────────────────────

void PortfolioFFNView::update_rolling() {
    if (!rolling_chart_ || !rolling_content_)
        return;
    const auto rc_obj = ffn_data_["rolling_corr"].toObject();
    if (rc_obj.isEmpty()) {
        // A single holding has no pair to correlate.
        if (!rolling_empty_) {
            rolling_empty_ = new ui::EmptyState(rolling_content_);
            rolling_content_->addWidget(rolling_empty_);
        }
        rolling_empty_->apply(ui::EmptyState::no_data_spec(
            tr("Rolling correlation needs two or more holdings"),
            tr("Correlations are measured between pairs of holdings over a 60-day window. Add another position to "
               "see how they move together.")));
        rolling_content_->setCurrentWidget(rolling_empty_);
        return;
    }

    rolling_chart_->clear();
    QStringList pairs = rc_obj.keys();
    pairs.sort();
    int color_idx = 0;
    for (const QString& pair : pairs) {
        const QVector<QPointF> pts = ffn_points(rc_obj[pair].toObject(), 1.0);
        if (pts.isEmpty())
            continue;
        ui::TimeSeriesChart::SeriesSpec spec;
        spec.name = QString(pair).replace(QLatin1Char('_'), QStringLiteral(" / "));
        spec.points = pts;
        spec.color = ui::TimeSeriesChart::ColorRole::Custom;
        spec.custom_color = series_color(color_idx++);
        spec.rebase = false;
        rolling_chart_->add_series(spec);
    }
    // Many pairs (n holdings give n·(n−1)/2): the tooltip names them; a legend would crowd the plot.
    rolling_chart_->set_show_legend(color_idx <= 12);
    rolling_chart_->add_hline(0.0, QString(), ui::TimeSeriesChart::ColorRole::Muted);
    rolling_chart_->set_state(ui::TimeSeriesChart::State::Ready);
    rolling_chart_->reset_x_range();
    rolling_content_->setCurrentWidget(rolling_chart_);
}

// ── launch_ffn ────────────────────────────────────────────────────────────────

void PortfolioFFNView::launch_ffn(quint64 run_id) {
    // Before the first set_data() there is no book yet: the run stays open and is
    // dropped when set_data() switches the panes to the portfolio's key.
    if (!has_data_)
        return;

    // Valued holdings only: an unpriced position has no weight to analyse.
    QStringList symbols;
    QJsonObject weights_obj;
    for (const auto& h : summary_.holdings) {
        if (!h.valued() || !std::isfinite(h.weight))
            continue;
        symbols.append(h.symbol);
        weights_obj[h.symbol] = h.weight / 100.0;
    }
    if (symbols.isEmpty()) {
        shared_->fail(run_id, tr("No position can be valued yet — there is nothing to analyse."));
        return;
    }
    // Same base currency, quoted currencies, benchmark calendar as the other analytics.
    const QJsonObject context = services::PortfolioService::instance().analytics_context(summary_);

    QPointer<PortfolioFFNView> self = this;
    const services::AnalyticsRequest req = PortfolioAnalyticsService::instance().run_ffn(
        symbols, weights_obj, risk_free_rate_,
        [self, run_id](const AnalyticsResult& r) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, run_id, r]() {
                    if (!self)
                        return;
                    self->requests_.remove(run_id);
                    if (r.cancelled)
                        return; // every tab left it; nothing to show
                    if (!r.success) {
                        LOG_ERROR("FFNView", "FFN script failed: " + r.error.left(300));
                        self->shared_->fail(run_id, ffn_plain_error(r.error, PortfolioFFNView::tr(
                                                                                 "The FFN analysis failed.")));
                        return;
                    }

                    // Count only per-symbol keys (exclude the section keys)
                    static const QStringList k_section_keys = {"rebased", "drawdown_series", "rolling_corr",
                                                               "optimization", "meta", "error"};
                    int sym_count = 0;
                    for (const auto& k : r.data.keys())
                        if (!k_section_keys.contains(k))
                            ++sym_count;

                    // No usable price history for any holding: say so instead of
                    // six tabs of empty tables.
                    if (sym_count == 0) {
                        LOG_WARN("FFNView", "FFN returned no per-symbol results");
                        self->shared_->fail(run_id, PortfolioFFNView::tr("No price history was found for these "
                                                                         "holdings."));
                        return;
                    }

                    LOG_DEBUG("FFNView", QString("FFN analysis complete for %1 symbol(s)").arg(sym_count));
                    self->shared_->finish(run_id, r.data);
                },
                Qt::QueuedConnection);
        },
        context);
    requests_.insert(run_id, req);
}

// ── Language ──────────────────────────────────────────────────────────────────

void PortfolioFFNView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioFFNView::retranslateUi() {
    if (back_btn_) {
        back_btn_->setText(tr("← BACK"));
        back_btn_->setToolTip(tr("Back to the portfolio workspace  (Esc)"));
        back_btn_->setAccessibleName(tr("Back to the portfolio workspace"));
    }
    if (title_label_)
        title_label_->setText(tr("FFN ANALYTICS"));
    if (run_btn_)
        run_btn_->setText(tr("RUN FFN ANALYSIS"));

    if (overview_hdr_)
        overview_hdr_->setText(tr("PORTFOLIO METRICS OVERVIEW"));
    if (benchmark_hdr_)
        benchmark_hdr_->setText(tr("BENCHMARK COMPARISON"));
    if (optimization_hdr_)
        optimization_hdr_->setText(tr("PORTFOLIO OPTIMISATION — WEIGHT COMPARISON"));
    if (weights_hdr_)
        weights_hdr_->setText(tr("ALLOCATION WEIGHTS BY STRATEGY"));
    if (stats_hdr_)
        stats_hdr_->setText(tr("STRATEGY PERFORMANCE STATS"));
    if (benchmark_info_label_)
        benchmark_info_label_->setText(benchmark_info_text());

    // Tabs were added in known order in build_ui(): OVERVIEW, BENCHMARK,
    // OPTIMISATION, REBASED, DRAWDOWNS, ROLLING.
    if (tabs_) {
        const QString names[] = {tr("OVERVIEW"), tr("BENCHMARK"), tr("OPTIMISATION"),
                                 tr("REBASED"),  tr("DRAWDOWNS"), tr("ROLLING")};
        for (int i = 0; i < tabs_->count() && i < 6; ++i)
            tabs_->setTabText(i, names[i]);
    }

    if (overview_kpis_) {
        const std::pair<const char*, QString> labels[] = {
            {"ret", tr("ANN. RETURN")}, {"vol", tr("VOLATILITY")}, {"sharpe", tr("SHARPE")}, {"dd", tr("MAX DRAWDOWN")}};
        for (const auto& [key, label] : labels)
            if (ui::StatTile* t = overview_kpis_->tile(QLatin1String(key)))
                t->set_label(label);
    }
    if (bench_kpis_) {
        const std::pair<const char*, QString> labels[] = {{"total", tr("TOTAL RETURN")},
                                                          {"cagr", tr("CAGR")},
                                                          {"vol", tr("VOLATILITY")},
                                                          {"sharpe", tr("SHARPE")},
                                                          {"dd", tr("MAX DRAWDOWN")}};
        for (const auto& [key, label] : labels)
            if (ui::StatTile* t = bench_kpis_->tile(QLatin1String(key)))
                t->set_label(label);
        update_bench_strip();
    }

    // Re-set table column headers.
    if (overview_table_)
        overview_table_->setHorizontalHeaderLabels({tr("METRIC"), tr("PORTFOLIO")});
    if (benchmark_table_)
        benchmark_table_->setHorizontalHeaderLabels(
            {tr("METRIC"), tr("PORTFOLIO"), tr("BENCHMARK (%1)").arg(benchmark_symbol_)});
    if (opt_weights_table_)
        opt_weights_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("CURRENT"), tr("ERC"), tr("INV-VOL"), tr("EQUAL")});
    if (opt_stats_table_)
        opt_stats_table_->setHorizontalHeaderLabels(
            {tr("STRATEGY"), tr("TOTAL RETURN"), tr("VOLATILITY"), tr("SHARPE"), tr("MAX DRAWDOWN")});

    if (rebased_chart_)
        rebased_chart_->set_title(tr("Rebased performance (base = 100)"));
    if (drawdowns_chart_)
        drawdowns_chart_->set_title(tr("Drawdown from the running peak"));
    if (rolling_chart_)
        rolling_chart_->set_title(tr("Rolling 60-day correlations"));

    for (ResultTab* t : {&overview_tab_, &benchmark_tab_, &optimization_tab_, &rebased_tab_, &drawdowns_tab_,
                         &rolling_tab_}) {
        t->pane->set_loading_text(tr("Downloading 1 year of prices…"));
        if (t->empty)
            t->empty->apply(ui::EmptyState::no_data_spec(tr("No positions to analyse"), t->empty_text(), tr("BUY")));
    }

    // The rendered results carry translated row names: render them again (the
    // visible tab now, the others on their next show).
    on_appearance_settled();
}

} // namespace fincept::screens
