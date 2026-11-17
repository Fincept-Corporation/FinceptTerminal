// src/ui/widgets/algo/BacktestReportPanel.cpp
#include "ui/widgets/algo/BacktestReportPanel.h"

#include "core/market/MarketSessions.h"
#include "ui/charts/CandleChart.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/MonthlyReturnsHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/LiveTableModel.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KeySortProxy.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QKeySequence>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTableView>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::ui::algo {

namespace fmt = fincept::ui::formatting;

namespace {

double rp_num(const QJsonObject& o, const char* key) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? v.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

QVector<double> rp_doubles(const QJsonArray& a) {
    QVector<double> out;
    out.reserve(a.size());
    for (const auto& v : a)
        out.append(v.isDouble() ? v.toDouble() : std::numeric_limits<double>::quiet_NaN());
    return out;
}

// At most `n` points of a long series for tile sparklines (keeps the last one).
QVector<double> rp_thin(const QVector<double>& v, int n = 120) {
    if (v.size() <= n)
        return v;
    QVector<double> out;
    const int step = static_cast<int>(v.size()) / n;
    for (int i = 0; i < v.size(); i += step)
        out.append(v[i]);
    out.append(v.last());
    return out;
}

StatTile::Tone rp_sign_tone(double v) {
    if (!std::isfinite(v) || v == 0)
        return StatTile::Tone::Neutral;
    return v > 0 ? StatTile::Tone::Positive : StatTile::Tone::Negative;
}

void rp_set_tone(QWidget* w, const char* tone) {
    const QString key = QString::fromLatin1(tone);
    if (!w || w->property("tone").toString() == key)
        return;
    w->setProperty("tone", key);
    w->style()->unpolish(w);
    w->style()->polish(w);
}

/// One closed trade, as the table shows it (raw numbers; text at data() time).
struct BrpTrade {
    int index = 0;
    bool is_long = true;
    double entry_ms = 0, exit_ms = 0;
    double entry_px = 0, exit_px = 0, qty = 0;
    double gross = 0, costs = 0, net = 0, net_pct = 0, mae = 0, mfe = 0;
    int bars = 0;
    QString reason;
};

using BrpModel = fincept::ui::LiveTableModel<BrpTrade>;

enum BrpCol { kBrpIdx, kBrpSide, kBrpEntry, kBrpExit, kBrpEntryPx, kBrpExitPx, kBrpQty, kBrpGross, kBrpCosts, kBrpNet,
              kBrpNetPct, kBrpMae, kBrpMfe, kBrpBars, kBrpReason, kBrpColCount };

// Pages of the stack, in the order the constructor adds them.
enum RpPage { kRpPageState = 0, kRpPageReport = 1, kRpPageIdle = 2 };
// Result tabs.
enum RpTab { kRpTabTrades, kRpTabMonthly, kRpTabCosts, kRpTabAssumptions, kRpTabPrice, kRpTabDistribution, kRpTabCount };

/// Key / value rows into a 2 x `columns` grid of labels (#algoKey / #algoValue); rebuilt each render.
struct BrpKv {
    QString key;
    QString value;
    const char* tone = "";
    QString tip;
};

void brp_fill_kv(QWidget* host, const QVector<BrpKv>& rows, int columns) {
    auto* grid = qobject_cast<QGridLayout*>(host->layout());
    if (!grid)
        return;
    while (QLayoutItem* it = grid->takeAt(0)) {
        if (QWidget* w = it->widget())
            w->deleteLater();
        delete it;
    }
    columns = std::max(1, columns);
    for (int i = 0; i < rows.size(); ++i) {
        const int r = i / columns;
        const int c = (i % columns) * 2;
        auto* k = new QLabel(rows[i].key, host);
        k->setObjectName(QStringLiteral("algoKey"));
        auto* v = new QLabel(rows[i].value, host);
        v->setObjectName(QStringLiteral("algoValue"));
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (rows[i].tone && *rows[i].tone)
            v->setProperty("tone", QString::fromLatin1(rows[i].tone));
        if (!rows[i].tip.isEmpty()) {
            k->setToolTip(rows[i].tip);
            v->setToolTip(rows[i].tip);
        }
        grid->addWidget(k, r, c);
        grid->addWidget(v, r, c + 1);
    }
    for (int c = 0; c < columns * 2; ++c)
        grid->setColumnStretch(c, c % 2 == 1 ? 1 : 0);
    grid->setRowStretch(grid->rowCount(), 1);
}

QWidget* brp_scroll_page(QWidget* parent, QWidget** content_out) {
    auto* scroll = new QScrollArea(parent);
    scroll->setObjectName(QStringLiteral("algoScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    scroll->setWidget(content);
    *content_out = content;
    return scroll;
}

} // namespace

BacktestReportPanel::BacktestReportPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("backtestReportPanel"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    stack_ = new QStackedWidget(this);
    stack_->addWidget(build_state_page());  // kRpPageState
    stack_->addWidget(build_report_page()); // kRpPageReport
    stack_->addWidget(build_idle_page());   // kRpPageIdle
    root->addWidget(stack_, 1);

    retranslateUi();
    clear();

    // Charts and tiles paint from the live theme; the table's colours come from data() at paint
    // time. Only sizes follow the font, after the appearance has settled.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { fit_to_font(); });
    // Currency / number / date format changed: re-render the report from the payload on
    // screen (nothing is re-run). A run in progress (has_result_ false) is left alone.
    fmt::on_format_changed(this, [this]() {
        if (has_result_)
            render_result();
    });
}

// ── Pages ────────────────────────────────────────────────────────────────────

QWidget* BacktestReportPanel::build_state_page() {
    auto* page = new QWidget(this);
    page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored); // the stack sizes by the report
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(24, 24, 24, 24);
    l->setSpacing(10);
    l->addStretch();
    state_title_ = new QLabel(page);
    state_title_->setObjectName(QStringLiteral("algoValue"));
    state_title_->setAlignment(Qt::AlignCenter);
    state_title_->setWordWrap(true);
    l->addWidget(state_title_);
    state_detail_ = new QLabel(page);
    state_detail_->setObjectName(QStringLiteral("algoNote"));
    state_detail_->setAlignment(Qt::AlignCenter);
    state_detail_->setWordWrap(true);
    state_detail_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(state_detail_);
    progress_ = new QProgressBar(page);
    progress_->setObjectName(QStringLiteral("algoProgress"));
    progress_->setRange(0, 100);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(4);
    progress_->setAccessibleName(tr("Backtest progress"));
    l->addWidget(progress_);
    auto* row = new QHBoxLayout();
    row->addStretch();
    cancel_btn_ = new QPushButton(page);
    cancel_btn_->setObjectName(QStringLiteral("algoToolButton"));
    cancel_btn_->setCursor(Qt::PointingHandCursor);
    connect(cancel_btn_, &QPushButton::clicked, this, &BacktestReportPanel::cancel_requested);
    row->addWidget(cancel_btn_);
    row->addStretch();
    l->addLayout(row);
    l->addStretch();
    return page;
}

QWidget* BacktestReportPanel::build_idle_page() {
    idle_ = new EmptyState(this);
    idle_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    connect(idle_, &EmptyState::primary_clicked, this, &BacktestReportPanel::run_requested);
    return idle_;
}

QWidget* BacktestReportPanel::build_report_page() {
    auto* page = new QWidget(this);
    auto* l = new QVBoxLayout(page);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    // Run header: what ran (left), when / how long (dim), warnings chip (right).
    auto* head = new QWidget(page);
    head->setObjectName(QStringLiteral("algoStrip"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(8, 4, 8, 4);
    hl->setSpacing(10);
    header_ = new QLabel(head);
    header_->setObjectName(QStringLiteral("algoValue"));
    header_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header_->setMinimumWidth(1); // elided by width (full text in the tooltip)
    header_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    hl->addWidget(header_, 3);
    run_info_ = new QLabel(head);
    run_info_->setObjectName(QStringLiteral("algoNote"));
    run_info_->setMinimumWidth(1);
    run_info_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    run_info_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hl->addWidget(run_info_, 2);
    warnings_btn_ = new QPushButton(head);
    warnings_btn_->setObjectName(QStringLiteral("algoToolButton"));
    warnings_btn_->setProperty("tone", QStringLiteral("warn"));
    warnings_btn_->setCursor(Qt::PointingHandCursor);
    connect(warnings_btn_, &QPushButton::clicked, this, [this]() { show_tab(kRpTabAssumptions); });
    hl->addWidget(warnings_btn_);
    l->addWidget(head);

    // KPI ribbon.
    auto* ribbon = new QWidget(page);
    ribbon->setObjectName(QStringLiteral("algoRibbon"));
    ribbon->setAttribute(Qt::WA_StyledBackground, true);
    auto* rl = new QVBoxLayout(ribbon);
    rl->setContentsMargins(6, 4, 6, 4);
    kpis_ = new KpiStrip(ribbon);
    kpis_->set_spacing(6);
    kpis_->set_max_columns(9);
    struct T {
        const char* key;
        int tab;
    };
    const T tiles[] = {{"ret", kRpTabMonthly},     {"cagr", kRpTabMonthly},        {"sharpe", kRpTabAssumptions},
                       {"maxdd", kRpTabAssumptions}, {"win", kRpTabTrades},         {"pf", kRpTabTrades},
                       {"trades", kRpTabTrades},    {"expo", kRpTabAssumptions},    {"vs", kRpTabCosts}};
    for (const T& t : tiles) {
        StatTile* tile = kpis_->add_tile(QLatin1String(t.key), QString());
        tile->set_compact(true);
        tile->set_clickable(true);
        const int tab = t.tab;
        connect(tile, &StatTile::clicked, this, [this, tab]() { show_tab(tab); });
    }
    kpis_->tile(QStringLiteral("maxdd"))->set_sparkline_color(StatTile::SparkColor::Neutral);
    rl->addWidget(kpis_);
    l->addWidget(ribbon);

    // Chart over tabs.
    split_ = new QSplitter(Qt::Vertical, page);
    split_->setObjectName(QStringLiteral("algoSplitter"));
    split_->setChildrenCollapsible(false);
    split_->setHandleWidth(1);
    equity_chart_ = new TimeSeriesChart(split_);
    equity_chart_->set_range_presets({QStringLiteral("1M"), QStringLiteral("3M"), QStringLiteral("6M"),
                                      QStringLiteral("YTD"), QStringLiteral("1Y"), QStringLiteral("ALL")});
    equity_chart_->set_crosshair_enabled(true);
    equity_chart_->set_zoom_enabled(true);
    equity_chart_->set_transparent_background(true);
    equity_chart_->set_export_name(QStringLiteral("backtest_equity"));
    equity_chart_->setMinimumHeight(QFontMetrics(font()).height() * 9);
    split_->addWidget(equity_chart_);

    auto* lower = new QWidget(split_);
    auto* ll = new QVBoxLayout(lower);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    tabs_ = new QTabBar(lower);
    tabs_->setObjectName(QStringLiteral("algoTabs"));
    tabs_->setExpanding(false);
    tabs_->setUsesScrollButtons(true);
    tabs_->setDrawBase(false);
    tabs_->setElideMode(Qt::ElideNone);
    for (int i = 0; i < kRpTabCount; ++i)
        tabs_->addTab(QString());
    ll->addWidget(tabs_);
    pages_ = new QStackedWidget(lower);
    pages_->addWidget(build_trades_page(pages_));                 // kRpTabTrades
    heatmap_ = new MonthlyReturnsHeatmap(pages_);                  // kRpTabMonthly
    heatmap_->set_value_formatter([](double v) { return fmt::pct(v, 1, true); });
    pages_->addWidget(heatmap_);
    pages_->addWidget(build_costs_page(pages_));                  // kRpTabCosts
    pages_->addWidget(build_assumptions_page(pages_));            // kRpTabAssumptions
    price_chart_ = new CandleChart(pages_);                       // kRpTabPrice
    price_chart_->set_export_name(QStringLiteral("backtest_price"));
    pages_->addWidget(price_chart_);
    pages_->addWidget(build_distribution_page(pages_));           // kRpTabDistribution
    // Every page may shrink to the splitter's share: the stack must not take the tallest page.
    for (int i = 0; i < pages_->count(); ++i)
        pages_->widget(i)->setMinimumHeight(0);
    pages_->setMinimumHeight(QFontMetrics(font()).height() * 6);
    ll->addWidget(pages_, 1);
    connect(tabs_, &QTabBar::currentChanged, pages_, &QStackedWidget::setCurrentIndex);
    split_->addWidget(lower);
    split_->setStretchFactor(0, 5);
    split_->setStretchFactor(1, 4);
    l->addWidget(split_, 1);
    return page;
}

QWidget* BacktestReportPanel::build_trades_page(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    auto* bar = new QWidget(w);
    bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(8, 3, 8, 3);
    trades_count_ = new QLabel(bar);
    trades_count_->setObjectName(QStringLiteral("algoNote"));
    bl->addWidget(trades_count_, 1);
    export_btn_ = new QPushButton(bar);
    export_btn_->setObjectName(QStringLiteral("algoToolButton"));
    export_btn_->setCursor(Qt::PointingHandCursor);
    connect(export_btn_, &QPushButton::clicked, this, &BacktestReportPanel::export_trades_csv);
    bl->addWidget(export_btn_);
    l->addWidget(bar);

    auto* model = new BrpModel(w);
    model->set_key([](const BrpTrade& t) { return QString::number(t.index); });
    trades_model_ = model;
    trades_proxy_ = new KeySortProxy(w);
    trades_proxy_->setSourceModel(model);
    trades_proxy_->setSortRole(BrpModel::SortRole);
    trades_ = new QTableView(w);
    trades_->setObjectName(QStringLiteral("algoTable"));
    trades_->setModel(trades_proxy_);
    trades_->verticalHeader()->hide();
    trades_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    trades_->setAlternatingRowColors(true);
    trades_->setSelectionBehavior(QAbstractItemView::SelectRows);
    trades_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    trades_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trades_->setShowGrid(false);
    trades_->setWordWrap(false);
    trades_->setTextElideMode(Qt::ElideRight);
    trades_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    trades_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    trades_->horizontalHeader()->setStretchLastSection(true);
    trades_->horizontalHeader()->setHighlightSections(false);
    trades_->setSortingEnabled(true);
    trades_->setAccessibleName(tr("Closed trades"));
    {
        // Right-click export: the same machine-readable CSV as the EXPORT button.
        ChartExport::MenuSpec spec;
        spec.suggested_name = QStringLiteral("backtest_trades");
        spec.csv = [this]() {
            ChartExport::Table t;
            trades_export(&t.headers, &t.rows);
            return t;
        };
        ChartExport::install_context_menu(trades_, spec);
    }
    auto* copy = new QShortcut(QKeySequence::Copy, trades_);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copy, &QShortcut::activated, trades_, [this]() {
        QStringList h;
        QVector<QStringList> rows;
        trades_export(&h, &rows);
        QStringList lines{h.join(QLatin1Char('\t'))};
        const QModelIndexList sel = trades_->selectionModel() ? trades_->selectionModel()->selectedRows() : QModelIndexList();
        for (const QModelIndex& pi : sel) {
            const int r = trades_proxy_->mapToSource(pi).row();
            if (r >= 0 && r < rows.size())
                lines << rows[r].join(QLatin1Char('\t'));
        }
        if (lines.size() > 1)
            QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    });
    l->addWidget(trades_, 1);
    fit_to_font();
    return w;
}

QWidget* BacktestReportPanel::build_costs_page(QWidget* parent) {
    QWidget* content = nullptr;
    QWidget* page = brp_scroll_page(parent, &content);
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(10, 8, 10, 8);
    costs_grid_ = new QWidget(content);
    auto* g = new QGridLayout(costs_grid_);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(14);
    g->setVerticalSpacing(3);
    v->addWidget(costs_grid_);
    v->addStretch();
    return page;
}

QWidget* BacktestReportPanel::build_assumptions_page(QWidget* parent) {
    QWidget* content = nullptr;
    QWidget* page = brp_scroll_page(parent, &content);
    auto* v = new QVBoxLayout(content);
    v->setContentsMargins(10, 8, 10, 8);
    v->setSpacing(8);
    warnings_list_ = new QLabel(content);
    warnings_list_->setObjectName(QStringLiteral("algoNote"));
    warnings_list_->setProperty("tone", QStringLiteral("warn"));
    warnings_list_->setWordWrap(true);
    warnings_list_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(warnings_list_);
    assumptions_note_ = new QLabel(content);
    assumptions_note_->setObjectName(QStringLiteral("algoNote"));
    assumptions_note_->setWordWrap(true);
    assumptions_note_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(assumptions_note_);
    assumptions_grid_ = new QWidget(content);
    auto* g = new QGridLayout(assumptions_grid_);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(14);
    g->setVerticalSpacing(3);
    v->addWidget(assumptions_grid_);
    v->addStretch();
    return page;
}

QWidget* BacktestReportPanel::build_distribution_page(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* dl = new QVBoxLayout(page);
    dl->setContentsMargins(8, 4, 8, 4);
    dl->setSpacing(4);
    dist_caption_ = new QLabel(page);
    dist_caption_->setObjectName(QStringLiteral("algoNote"));
    dl->addWidget(dist_caption_);
    dist_ = new DistributionChart(page);
    dist_->set_normal_overlay(true);
    dist_->set_signed_colors(true);
    dist_->set_value_formatter([](double v) { return fmt::pct(v, 2, true); });
    dl->addWidget(dist_, 1);
    return page;
}

void BacktestReportPanel::fit_to_font() {
    if (!trades_)
        return;
    const QFontMetrics fm(trades_->font());
    trades_->verticalHeader()->setDefaultSectionSize(std::max(18, fm.height() + 6));
    const int avg = std::max(5, fm.averageCharWidth());
    const int chars[kBrpColCount] = {4, 6, 16, 16, 10, 10, 8, 12, 10, 12, 8, 8, 8, 6, 12};
    for (int c = 0; c < kBrpColCount; ++c)
        trades_->setColumnWidth(c, chars[c] * avg + 14);
}

void BacktestReportPanel::show_tab(int index) {
    if (tabs_ && index >= 0 && index < tabs_->count())
        tabs_->setCurrentIndex(index);
}

void BacktestReportPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!shown_once_ && split_) {
        shown_once_ = true;
        const int h = std::max(1, split_->height());
        split_->setSizes({h * 55 / 100, h * 45 / 100});
    }
}

// ── States ───────────────────────────────────────────────────────────────────

void BacktestReportPanel::show_state(const QString& title, const QString& detail, bool busy, bool error) {
    state_is_error_ = error;
    busy_ = busy;
    state_title_->setText(title);
    rp_set_tone(state_title_, error ? "danger" : "");
    state_detail_->setText(detail);
    state_detail_->setVisible(!detail.isEmpty());
    progress_->setVisible(busy);
    cancel_btn_->setVisible(busy);
    stack_->setCurrentIndex(kRpPageState);
}

void BacktestReportPanel::apply_idle_spec() {
    idle_->apply(EmptyState::no_data_spec(
        tr("No backtest yet"),
        tr("Run a backtest to see net-of-cost performance, the equity curve against buy & hold, trades and monthly "
           "returns."),
        tr("RUN BACKTEST")));
}

void BacktestReportPanel::clear() {
    has_result_ = false;
    busy_ = false;
    last_payload_ = QJsonObject();
    ran_at_ = QDateTime();
    run_duration_ms_ = -1;
    remembered_ = false;
    run_clock_.invalidate();
    apply_idle_spec();
    stack_->setCurrentIndex(kRpPageIdle);
}

void BacktestReportPanel::set_queued(const QString& what) {
    // Not started yet: no bar (it would read as progress) and no clock.
    has_result_ = false;
    run_clock_.invalidate();
    show_state(tr("Queued"), what, false, false);
}

void BacktestReportPanel::set_loading(const QString& what) {
    // The previous report must not stay on screen as if it belonged to this run.
    has_result_ = false;
    run_clock_.start();
    // Indeterminate until the first progress report above 0 (fetching the bars has none).
    determinate_ = false;
    progress_->setRange(0, 0);
    show_state(what, QString(), true, false);
}

void BacktestReportPanel::set_progress(int percent, const QString& stage) {
    if (stack_->currentIndex() != kRpPageState || !busy_)
        return;
    const int pct = std::clamp(percent, 0, 100);
    if (pct > 0 && !determinate_) {
        determinate_ = true;
        progress_->setRange(0, 100);
    }
    if (determinate_)
        progress_->setValue(pct);
    const QString text = determinate_ ? tr("%1 % · %2").arg(pct).arg(stage) : stage;
    state_detail_->setText(text);
    state_detail_->setVisible(!text.isEmpty());
}

void BacktestReportPanel::set_error(const QString& message) {
    has_result_ = false;
    run_clock_.invalidate();
    show_state(tr("Backtest failed"), message, false, true);
}

void BacktestReportPanel::set_cancelled() {
    has_result_ = false;
    run_clock_.invalidate();
    show_state(tr("Backtest cancelled."), QString(), false, false);
}

// ── Formatting helpers ──────────────────────────────────────────────────────

QString BacktestReportPanel::money(double v, bool signed_value) const {
    if (!std::isfinite(v))
        return fmt::placeholder();
    // The run's quote currency; unknown -> no unit (never the display currency).
    return signed_value ? fmt::signed_money(v, currency_) : fmt::money(v, currency_);
}

QString BacktestReportPanel::price_text(double v) const {
    // A bare price in the instrument's decimals (the column header names the currency).
    return fmt::price(v, fmt::instrument_for_symbol(last_payload_.value(QStringLiteral("symbol")).toString(), currency_));
}

QString BacktestReportPanel::time_text(double epoch_ms) const {
    if (!(epoch_ms > 0))
        return fmt::placeholder();
    const auto* venue = core::market::venue_by_code(last_payload_.value(QStringLiteral("venue")).toString());
    const QTimeZone tz = venue ? core::market::venue_timezone(*venue) : QTimeZone(QTimeZone::UTC);
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(epoch_ms), tz);
    // Daily bars: the exchange's calendar date; intraday: date + time in the exchange's zone.
    if (daily_bars_)
        return fmt::calendar_date(dt.date(), fmt::DateStyle::Short);
    return fmt::calendar_date(dt.date(), fmt::DateStyle::Short) + QLatin1Char(' ') + dt.time().toString(QStringLiteral("HH:mm"));
}

QString BacktestReportPanel::reason_text(const QString& r) const {
    if (r == QLatin1String("exit_signal"))
        return tr("Exit rule");
    if (r == QLatin1String("stop_loss"))
        return tr("Stop-loss");
    if (r == QLatin1String("trailing_stop"))
        return tr("Trailing stop");
    if (r == QLatin1String("take_profit"))
        return tr("Take-profit");
    if (r == QLatin1String("end_of_data"))
        return tr("End of data");
    return r;
}

// ── Result ───────────────────────────────────────────────────────────────────

void BacktestReportPanel::set_result(const QJsonObject& payload) {
    if (&payload == &last_payload_) {
        render_result(); // re-render of the result on screen: keep its run time
        return;
    }
    last_payload_ = payload;
    ran_at_ = QDateTime::currentDateTime();
    run_duration_ms_ = run_clock_.isValid() ? run_clock_.elapsed() : -1;
    run_clock_.invalidate();
    remembered_ = false;
    render_result();
}

void BacktestReportPanel::set_remembered_result(const QJsonObject& payload, const QDateTime& ran_at,
                                                qint64 duration_ms) {
    last_payload_ = payload;
    ran_at_ = ran_at;
    run_duration_ms_ = duration_ms >= 0 ? duration_ms : -1;
    run_clock_.invalidate();
    remembered_ = true;
    render_result();
}

void BacktestReportPanel::render_result() {
    has_result_ = true;
    busy_ = false;
    // The engine reports the listing venue's currency; a pair the venue table does not know
    // ("BTC-USD", "ETH/USDT") is in its quote asset.
    currency_ = last_payload_.value(QStringLiteral("currency")).toString();
    if (currency_.isEmpty())
        currency_ = fmt::instrument_for_symbol(last_payload_.value(QStringLiteral("symbol")).toString()).currency;
    daily_bars_ = last_payload_.value(QStringLiteral("bar_seconds")).toInt(86400) >= 86400;

    populate_header();
    populate_kpis();
    populate_equity();
    populate_trades();
    populate_monthly();
    populate_costs();
    populate_assumptions();
    populate_price();
    populate_distribution();
    stack_->setCurrentIndex(kRpPageReport);
}

QString BacktestReportPanel::run_info_text() const {
    if (!ran_at_.isValid())
        return QString();
    const QString when = fmt::date_time(ran_at_, fmt::DateStyle::Medium, false);
    QString took;
    if (run_duration_ms_ >= 0) {
        const double secs = static_cast<double>(run_duration_ms_) / 1000.0;
        if (secs < 60.0) {
            took = tr("took %1 s").arg(fmt::number(secs, secs < 10.0 ? 1 : 0));
        } else {
            const qint64 whole = run_duration_ms_ / 1000;
            took = tr("took %1 min %2 s").arg(whole / 60).arg(whole % 60);
        }
    }
    const QString lead = remembered_ ? tr("As of %1 (remembered — RUN BACKTEST refreshes it)").arg(when)
                                     : tr("Ran %1").arg(when);
    return took.isEmpty() ? lead : lead + QStringLiteral(" · ") + took;
}

void BacktestReportPanel::populate_header() {
    const QJsonObject& p = last_payload_;
    const QString side = p.value(QStringLiteral("side")).toString() == QLatin1String("SHORT") ? tr("short") : tr("long");
    const QString venue = p.value(QStringLiteral("venue")).toString();
    const QString costs = p.value(QStringLiteral("costs_applied")).toBool()
                              ? tr("net of costs (%1)").arg(
                                    p.value(QStringLiteral("cost_model")).toObject().value(QStringLiteral("preset")).toString())
                              : tr("GROSS — no costs");
    const QString range = QStringLiteral("%1 → %2").arg(time_text(rp_num(p, "start_time")), time_text(rp_num(p, "end_time")));
    const QString head = QStringList{p.value(QStringLiteral("symbol")).toString(),
                                     venue.isEmpty() ? tr("venue unknown") : venue,
                                     p.value(QStringLiteral("timeframe")).toString(),
                                     side,
                                     range,
                                     tr("%n bar(s)", nullptr, p.value(QStringLiteral("bars")).toInt()),
                                     costs}
                             .join(QStringLiteral(" · "));
    header_->setText(head);
    header_->setToolTip(head);
    const QString info = run_info_text();
    run_info_->setText(info);
    run_info_->setToolTip(info);
    rp_set_tone(run_info_, remembered_ ? "warn" : "");
    const int warnings = p.value(QStringLiteral("warnings")).toArray().size();
    warnings_btn_->setText(tr("⚠ %n WARNING(S)", nullptr, warnings));
    warnings_btn_->setAccessibleName(warnings_btn_->text());
    warnings_btn_->setToolTip(tr("Warnings and assumptions of this result"));
    warnings_btn_->setVisible(warnings > 0);
}

void BacktestReportPanel::populate_kpis() {
    const QJsonObject& p = last_payload_;
    const QString dash = fmt::placeholder();
    auto pct_or_dash = [&dash](double v, int dec = 2, bool sign = true) {
        return std::isfinite(v) ? fmt::pct(v, dec, sign) : dash;
    };
    auto num_or_dash = [&dash](double v, int dec = 2) { return std::isfinite(v) ? fmt::number(v, dec) : dash; };
    auto set = [this](const char* key, const QString& value, StatTile::Tone tone, const QString& tip,
                      const QString& sub = {}) {
        StatTile* t = kpis_->tile(QLatin1String(key));
        if (!t)
            return t;
        t->set_state(StatTile::State::Normal);
        t->set_value(value);
        t->set_tone(tone);
        t->set_tooltip_text(tip);
        t->set_sub_label(sub);
        return t;
    };
    const QVector<double> equity = rp_doubles(p.value(QStringLiteral("equity_curve")).toArray());
    const QVector<double> dd = rp_doubles(p.value(QStringLiteral("drawdown_curve")).toArray());
    const double init = rp_num(p, "initial_capital");

    const double ret = rp_num(p, "total_return");
    if (StatTile* t = set("ret", pct_or_dash(ret), rp_sign_tone(ret),
                          tr("Final equity over initial capital, after commission, taxes and slippage."),
                          tr("final %1").arg(money(rp_num(p, "final_value"))))) {
        if (!equity.isEmpty())
            t->set_sparkline(rp_thin(equity), init);
        else
            t->clear_sparkline();
    }
    const double cagr = rp_num(p, "cagr");
    set("cagr", pct_or_dash(cagr), rp_sign_tone(cagr), tr("Compound annual growth over the tested calendar period."),
        p.value(QStringLiteral("cagr_extrapolated")).toBool() ? tr("short period — extrapolated") : QString());
    const double sharpe = rp_num(p, "sharpe_ratio");
    const bool daily = p.value(QStringLiteral("ratio_basis")).toString(QStringLiteral("bar")) == QLatin1String("daily") ||
                       daily_bars_;
    set("sharpe", num_or_dash(sharpe), sharpe < 0 ? StatTile::Tone::Warning : StatTile::Tone::Neutral,
        tr("Excess return over the risk-free rate per unit of volatility, annualised (%1).")
            .arg(daily ? tr("daily returns × √%1").arg(num_or_dash(rp_num(p, "ratio_periods_per_year") > 0
                                                                       ? rp_num(p, "ratio_periods_per_year")
                                                                       : rp_num(p, "periods_per_year"),
                                                                   0))
                       : tr("bar returns × √%1 bars a year").arg(num_or_dash(rp_num(p, "periods_per_year"), 0))),
        tr("rf %1").arg(pct_or_dash(rp_num(p, "risk_free_rate"), 2, false)));
    const double mdd = rp_num(p, "max_drawdown");
    if (StatTile* t = set("maxdd", std::isfinite(mdd) ? fmt::pct(-std::abs(mdd), 2, true) : dash,
                          std::abs(mdd) >= 25 ? StatTile::Tone::Danger
                                              : (std::abs(mdd) >= 10 ? StatTile::Tone::Warning : StatTile::Tone::Neutral),
                          tr("Largest fall from a running equity peak, on every bar."),
                          money(-std::abs(rp_num(p, "max_drawdown_abs"))))) {
        if (!dd.isEmpty())
            t->set_sparkline(rp_thin(dd), 0.0);
        else
            t->clear_sparkline();
    }
    const int n = p.value(QStringLiteral("total_trades")).toInt();
    set("win", n > 0 ? pct_or_dash(rp_num(p, "win_rate"), 1, false) : dash, StatTile::Tone::Neutral,
        tr("Share of closed trades with a positive net P&L."),
        tr("%1 W / %2 L").arg(p.value(QStringLiteral("winning_trades")).toInt()).arg(p.value(QStringLiteral("losing_trades")).toInt()));
    const bool pf_inf = p.value(QStringLiteral("profit_factor_infinite")).toBool();
    const double pf = rp_num(p, "profit_factor");
    set("pf", pf_inf ? QStringLiteral("∞") : (n > 0 ? num_or_dash(pf) : dash),
        (!pf_inf && std::isfinite(pf) && pf < 1.0 && n > 0) ? StatTile::Tone::Warning : StatTile::Tone::Neutral,
        tr("Gross profit over gross loss of net trade P&L (below 1 loses money)."));
    set("trades", QString::number(n), StatTile::Tone::Neutral, tr("Closed round trips."),
        tr("avg %1 bars").arg(num_or_dash(rp_num(p, "avg_bars_held"), 1)));
    set("expo", pct_or_dash(rp_num(p, "exposure_pct"), 1, false), StatTile::Tone::Neutral,
        tr("Share of bars with an open position."),
        tr("turnover %1×").arg(num_or_dash(rp_num(p, "turnover"), 1)));
    const double bh = rp_num(p, "benchmark_return");
    const double excess = rp_num(p, "excess_return");
    set("vs", std::isfinite(excess) ? fmt::pp(excess) : dash, rp_sign_tone(excess),
        tr("Strategy return minus buying at the first evaluated bar and holding to the end, with the same costs."),
        tr("buy & hold %1").arg(pct_or_dash(bh)));
}

void BacktestReportPanel::populate_equity() {
    using TS = TimeSeriesChart;
    const QJsonObject& p = last_payload_;
    equity_chart_->clear();
    while (equity_chart_->pane_count() < 2)
        equity_chart_->add_pane(0.32);
    const QVector<double> t = rp_doubles(p.value(QStringLiteral("equity_time")).toArray());
    const QVector<double> eq = rp_doubles(p.value(QStringLiteral("equity_curve")).toArray());
    const QVector<double> bh = rp_doubles(p.value(QStringLiteral("benchmark_curve")).toArray());
    const QVector<double> dd = rp_doubles(p.value(QStringLiteral("drawdown_curve")).toArray());
    if (t.size() < 2 || eq.size() != t.size()) {
        equity_chart_->set_state(TS::State::Empty, tr("No equity curve"));
        return;
    }
    equity_chart_->set_state(TS::State::Ready);
    equity_chart_->set_title(tr("Equity (net) vs buy & hold"));

    TS::SeriesSpec strat;
    strat.name = tr("Strategy (net)");
    strat.xs = t;
    strat.ys = eq;
    strat.color = ChartColorRole::Accent;
    strat.line_width = 1.75;
    strat.id = QStringLiteral("equity");
    const int eq_series = equity_chart_->add_series(strat);

    if (bh.size() == t.size()) {
        TS::SeriesSpec b;
        b.name = tr("Buy & hold");
        b.xs = t;
        b.ys = bh;
        b.color = ChartColorRole::Muted;
        b.dashed = true;
        b.id = QStringLiteral("benchmark");
        equity_chart_->add_series(b);
    }
    if (dd.size() == t.size()) {
        TS::SeriesSpec u;
        u.name = tr("Drawdown");
        u.xs = t;
        u.ys = dd;
        u.style = TS::Style::Area;
        u.pane = 1;
        u.color = ChartColorRole::Negative;
        u.baseline = 0.0;
        u.rebase = false;
        u.id = QStringLiteral("drawdown");
        u.formatter = [](double v) { return fmt::pct(v, 2, true); };
        equity_chart_->add_series(u);
        equity_chart_->set_pane_title(1, tr("Underwater %"));
        equity_chart_->set_value_formatter(TS::Axis::Left, [](double v) { return fmt::pct(v, 1, true); }, 1);
    }
    // Equity in the run's currency, abbreviated per the user's scheme (K/M/B or lakh / crore).
    const QString ccy = currency_;
    equity_chart_->set_value_formatter(
        TS::Axis::Left, [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); }, 0);
    const double init = rp_num(p, "initial_capital");
    if (std::isfinite(init))
        equity_chart_->add_hline(init, tr("Capital"), ChartColorRole::Muted);
    // The deepest drawdown, shaded from its peak to its trough.
    const double dd_from = rp_num(p, "max_dd_peak_time");
    const double dd_to = rp_num(p, "max_dd_trough_time");
    if (dd_from > 0 && dd_to > dd_from)
        equity_chart_->add_band(dd_from, dd_to, ChartColorRole::Negative, tr("Max drawdown"));
    // Trade markers on the equity line (a few hundred at most: beyond that they are noise).
    const QJsonArray trades = p.value(QStringLiteral("trades")).toArray();
    if (trades.size() <= 300) {
        for (const auto& tv : trades) {
            const QJsonObject tr_obj = tv.toObject();
            const bool lng = tr_obj.value(QStringLiteral("side")).toString() != QLatin1String("SHORT");
            const double pnl = tr_obj.value(QStringLiteral("pnl")).toDouble();
            equity_chart_->add_marker(tr_obj.value(QStringLiteral("entry_time")).toDouble(), eq_series,
                                      lng ? ChartMarkerKind::Buy : ChartMarkerKind::Sell, QString(),
                                      tr("Entry %1 @ %2").arg(lng ? tr("long") : tr("short"),
                                                              price_text(tr_obj.value(QStringLiteral("entry_price")).toDouble())));
            equity_chart_->add_marker(tr_obj.value(QStringLiteral("exit_time")).toDouble(), eq_series,
                                      lng ? ChartMarkerKind::Sell : ChartMarkerKind::Buy, QString(),
                                      tr("Exit (%1) @ %2 · net %3")
                                          .arg(reason_text(tr_obj.value(QStringLiteral("reason")).toString()),
                                               price_text(tr_obj.value(QStringLiteral("exit_price")).toDouble()),
                                               money(pnl, true)));
        }
    }
    equity_chart_->reset_x_range();
}

void BacktestReportPanel::populate_price() {
    const QJsonObject& p = last_payload_;
    price_chart_->clear();
    price_chart_->clear_annotations();
    const QJsonObject bars = p.value(QStringLiteral("price_bars")).toObject();
    const QVector<double> t = rp_doubles(bars.value(QStringLiteral("t")).toArray());
    const QVector<double> o = rp_doubles(bars.value(QStringLiteral("o")).toArray());
    const QVector<double> h = rp_doubles(bars.value(QStringLiteral("h")).toArray());
    const QVector<double> l = rp_doubles(bars.value(QStringLiteral("l")).toArray());
    const QVector<double> c = rp_doubles(bars.value(QStringLiteral("c")).toArray());
    const QVector<double> v = rp_doubles(bars.value(QStringLiteral("v")).toArray());
    if (t.isEmpty() || o.size() != t.size() || c.size() != t.size()) {
        price_chart_->set_state(CandleChart::State::Empty, tr("No price data in the result"));
        return;
    }
    QVector<CandleChart::Bar> out;
    out.reserve(t.size());
    for (int i = 0; i < t.size(); ++i) {
        CandleChart::Bar b;
        b.time_ms = t[i];
        b.open = o[i];
        b.high = i < h.size() ? h[i] : o[i];
        b.low = i < l.size() ? l[i] : o[i];
        b.close = c[i];
        b.volume = i < v.size() ? v[i] : 0.0;
        out.append(b);
    }
    price_chart_->set_state(CandleChart::State::Ready);
    price_chart_->set_title(p.value(QStringLiteral("symbol")).toString());
    price_chart_->set_bars(out);
    price_chart_->set_default_visible_bars(std::min<int>(static_cast<int>(out.size()), 300));
    // Axis: bare prices in the instrument's decimals; marker tooltips name the currency.
    const fmt::InstrumentFmt inst = fmt::instrument_for_symbol(p.value(QStringLiteral("symbol")).toString(), currency_);
    price_chart_->set_price_formatter([inst](double x) { return fmt::price(x, inst); });
    for (const auto& tv : p.value(QStringLiteral("trades")).toArray()) {
        const QJsonObject tr_obj = tv.toObject();
        const bool lng = tr_obj.value(QStringLiteral("side")).toString() != QLatin1String("SHORT");
        const double qty = tr_obj.value(QStringLiteral("shares")).toDouble();
        const double en = tr_obj.value(QStringLiteral("entry_price")).toDouble();
        const double ex = tr_obj.value(QStringLiteral("exit_price")).toDouble();
        const double pnl = tr_obj.value(QStringLiteral("pnl")).toDouble();
        price_chart_->add_trade_marker(tr_obj.value(QStringLiteral("entry_time")).toDouble(), en,
                                       lng ? CandleChart::TradeSide::Buy : CandleChart::TradeSide::Sell,
                                       lng ? tr("B") : tr("S"),
                                       tr("%1 entry: %2 @ %3")
                                           .arg(lng ? tr("Long") : tr("Short"), fmt::quantity(qty),
                                                fmt::price_with_symbol(en, inst)));
        price_chart_->add_trade_marker(tr_obj.value(QStringLiteral("exit_time")).toDouble(), ex,
                                       lng ? CandleChart::TradeSide::Sell : CandleChart::TradeSide::Buy, QString(),
                                       tr("Exit (%1): %2 @ %3 · net %4")
                                           .arg(reason_text(tr_obj.value(QStringLiteral("reason")).toString()),
                                                fmt::quantity(qty), fmt::price_with_symbol(ex, inst), money(pnl, true)));
    }
    price_chart_->reset_view();
}

void BacktestReportPanel::populate_monthly() {
    MonthlyReturnsHeatmap::MonthlyReturns data;
    for (const auto& v : last_payload_.value(QStringLiteral("monthly_returns")).toArray()) {
        const QString ym = v.toObject().value(QStringLiteral("month")).toString(); // "yyyy-MM" (exchange time)
        const int y = ym.left(4).toInt();
        const int m = ym.mid(5, 2).toInt();
        if (y > 0 && m >= 1 && m <= 12)
            data[y][m] = v.toObject().value(QStringLiteral("return")).toDouble();
    }
    if (data.isEmpty())
        heatmap_->clear();
    else
        heatmap_->set_monthly_returns(data);
}

void BacktestReportPanel::populate_distribution() {
    const QVector<double> s = rp_doubles(last_payload_.value(QStringLiteral("return_samples")).toArray());
    dist_->clear_markers();
    QVector<double> finite;
    for (double x : s)
        if (std::isfinite(x))
            finite.append(x);
    const bool daily = last_payload_.value(QStringLiteral("return_samples_kind")).toString() == QLatin1String("daily");
    dist_caption_->setText(daily ? tr("Distribution of daily returns (%1 days)").arg(finite.size())
                                 : tr("Distribution of bar returns (%1 bars)").arg(finite.size()));
    if (finite.size() < 5) {
        dist_->clear();
        return;
    }
    dist_->set_samples(finite);
    dist_->add_marker(dist_->mean(), tr("Mean"), DistributionChart::MarkerRole::Mean);
    const double var95 = dist_->quantile(0.05);
    if (std::isfinite(var95))
        dist_->add_marker(var95, tr("5th pct"), DistributionChart::MarkerRole::VaR);
}

void BacktestReportPanel::populate_trades() {
    auto* model = static_cast<BrpModel*>(trades_model_);
    // One run = one currency: the price columns name it in the header; P&L / cost cells carry it.
    const QString px_entry = currency_.isEmpty() ? tr("ENTRY PX") : fmt::header(tr("ENTRY PX"), currency_);
    const QString px_exit = currency_.isEmpty() ? tr("EXIT PX") : fmt::header(tr("EXIT PX"), currency_);
    model->set_headers({tr("#"), tr("SIDE"), tr("ENTRY"), tr("EXIT"), px_entry, px_exit, tr("QTY"), tr("GROSS P&L"),
                        tr("COSTS"), tr("NET P&L"), tr("NET %"), tr("MAE %"), tr("MFE %"), tr("BARS"), tr("EXIT REASON")});
    QPointer<BacktestReportPanel> self(this);
    model->set_cell([self](const BrpTrade& t, int col, int role) -> QVariant {
        if (!self)
            return {};
        if (role == Qt::DisplayRole) {
            switch (col) {
                case kBrpIdx:
                    return t.index;
                case kBrpSide:
                    return t.is_long ? tr("LONG") : tr("SHORT");
                case kBrpEntry:
                    return self->time_text(t.entry_ms);
                case kBrpExit:
                    return self->time_text(t.exit_ms);
                case kBrpEntryPx:
                    return self->price_text(t.entry_px);
                case kBrpExitPx:
                    return self->price_text(t.exit_px);
                case kBrpQty:
                    return fmt::quantity(t.qty);
                case kBrpGross:
                    return self->money(t.gross, true);
                case kBrpCosts:
                    return self->money(t.costs);
                case kBrpNet:
                    return self->money(t.net, true);
                case kBrpNetPct:
                    return fmt::pct(t.net_pct, 2, true);
                case kBrpMae:
                    return fmt::pct(t.mae, 2, true);
                case kBrpMfe:
                    return fmt::pct(t.mfe, 2, true);
                case kBrpBars:
                    return t.bars;
                case kBrpReason:
                    return self->reason_text(t.reason);
                default:
                    break;
            }
            return {};
        }
        if (role == BrpModel::SortRole) {
            switch (col) {
                case kBrpIdx:
                    return t.index;
                case kBrpSide:
                    return t.is_long ? 0 : 1;
                case kBrpEntry:
                    return t.entry_ms;
                case kBrpExit:
                    return t.exit_ms;
                case kBrpEntryPx:
                    return t.entry_px;
                case kBrpExitPx:
                    return t.exit_px;
                case kBrpQty:
                    return t.qty;
                case kBrpGross:
                    return t.gross;
                case kBrpCosts:
                    return t.costs;
                case kBrpNet:
                    return t.net;
                case kBrpNetPct:
                    return t.net_pct;
                case kBrpMae:
                    return t.mae;
                case kBrpMfe:
                    return t.mfe;
                case kBrpBars:
                    return t.bars;
                case kBrpReason:
                    return t.reason;
                default:
                    break;
            }
            return {};
        }
        if (role == Qt::TextAlignmentRole)
            return (col == kBrpSide || col == kBrpReason || col == kBrpEntry || col == kBrpExit)
                       ? QVariant(int(Qt::AlignLeft | Qt::AlignVCenter))
                       : QVariant(int(Qt::AlignRight | Qt::AlignVCenter));
        if (role == Qt::ForegroundRole) {
            // Direction only, with the sign in the text as well.
            double v = 0;
            if (col == kBrpGross)
                v = t.gross;
            else if (col == kBrpNet)
                v = t.net;
            else if (col == kBrpNetPct)
                v = t.net_pct;
            else
                return {};
            if (v > 0)
                return QColor(colors::POSITIVE());
            if (v < 0)
                return QColor(colors::NEGATIVE());
            return {};
        }
        return {};
    });
    QVector<BrpTrade> rows;
    const QJsonArray trades = last_payload_.value(QStringLiteral("trades")).toArray();
    rows.reserve(trades.size());
    for (int i = 0; i < trades.size(); ++i) {
        const QJsonObject t = trades[i].toObject();
        BrpTrade r;
        r.index = i + 1;
        r.is_long = t.value(QStringLiteral("side")).toString() != QLatin1String("SHORT");
        r.entry_ms = t.value(QStringLiteral("entry_time")).toDouble();
        r.exit_ms = t.value(QStringLiteral("exit_time")).toDouble();
        r.entry_px = t.value(QStringLiteral("entry_price")).toDouble();
        r.exit_px = t.value(QStringLiteral("exit_price")).toDouble();
        r.qty = t.value(QStringLiteral("shares")).toDouble();
        r.gross = t.value(QStringLiteral("gross_pnl")).toDouble();
        r.costs = t.value(QStringLiteral("fees")).toDouble() + t.value(QStringLiteral("slippage")).toDouble() +
                  t.value(QStringLiteral("borrow")).toDouble();
        r.net = t.value(QStringLiteral("pnl")).toDouble();
        r.net_pct = t.value(QStringLiteral("pnl_pct")).toDouble();
        r.mae = t.value(QStringLiteral("mae_pct")).toDouble();
        r.mfe = t.value(QStringLiteral("mfe_pct")).toDouble();
        r.bars = t.value(QStringLiteral("bars_held")).toInt();
        r.reason = t.value(QStringLiteral("reason")).toString();
        rows.append(r);
    }
    // A new result is a new row set (same index keys would otherwise keep the old text).
    model->clear();
    model->upsert(rows);
    fit_to_font();
    trades_count_->setText(trades.isEmpty() ? tr("No closed trades.") : tr("%n closed trade(s)", nullptr, trades.size()));
    export_btn_->setEnabled(!trades.isEmpty());
}

void BacktestReportPanel::populate_costs() {
    const QJsonObject& p = last_payload_;
    const QString dash = fmt::placeholder();
    auto pct = [&dash](double v, bool sign = true) { return std::isfinite(v) ? fmt::pct(v, 2, sign) : dash; };
    const double gross = rp_num(p, "gross_return");
    const double net = rp_num(p, "total_return");
    const QJsonObject cm = p.value(QStringLiteral("cost_model")).toObject();
    auto dir = [](double v) { return v > 0 ? "pos" : (v < 0 ? "neg" : ""); };
    QVector<BrpKv> rows = {
        {tr("Gross return"), pct(gross), dir(gross), tr("Before commission, taxes, slippage and borrow fees.")},
        {tr("Net return"), pct(net), dir(net), {}},
        {tr("Cost drag"), std::isfinite(net - gross) ? fmt::pp(net - gross) : dash, "", tr("Net minus gross return.")},
        {tr("Total costs"), money(rp_num(p, "total_costs")), "", tr("Commission + taxes + slippage + borrow over every fill.")},
        {tr("Fees & taxes"), money(rp_num(p, "total_fees")), "", {}},
        {tr("Slippage"), money(rp_num(p, "total_slippage")), "", tr("Price given up against the reference price on market fills.")},
        {tr("Borrow"), money(rp_num(p, "total_borrow")), "", tr("Stock-borrow fee on shorts.")},
        {tr("Turnover"), tr("%1×").arg(std::isfinite(rp_num(p, "turnover")) ? fmt::number(rp_num(p, "turnover"), 1) : dash), "",
         tr("Traded value over average equity.")},
        {tr("Turnover / year"), tr("%1×").arg(std::isfinite(rp_num(p, "turnover_annual")) ? fmt::number(rp_num(p, "turnover_annual"), 1) : dash),
         "", {}},
        {tr("Buy & hold"), pct(rp_num(p, "benchmark_return")), dir(rp_num(p, "benchmark_return")),
         tr("Bought at the first evaluated bar's open with the same costs, sold at the end.")},
        {tr("Buy & hold max DD"), std::isfinite(rp_num(p, "benchmark_max_drawdown")) ? fmt::pct(-std::abs(rp_num(p, "benchmark_max_drawdown")), 2, true) : dash,
         "", {}},
        {tr("Cost preset"), cm.value(QStringLiteral("preset")).toString(QStringLiteral("none")), "",
         tr("The cost model of this run (an approximation of a typical retail schedule).")},
        {tr("Commission"), tr("%1 % · %2 / unit · %3 / order").arg(fmt::number(cm.value(QStringLiteral("commission_pct")).toDouble(), 4),
                                                                 fmt::number(cm.value(QStringLiteral("commission_per_unit")).toDouble(), 4),
                                                                 fmt::number(cm.value(QStringLiteral("commission_per_order")).toDouble(), 2)),
         "", {}},
        {tr("Taxes buy / sell"), tr("%1 % / %2 %").arg(fmt::number(cm.value(QStringLiteral("buy_tax_pct")).toDouble(), 4),
                                                      fmt::number(cm.value(QStringLiteral("sell_tax_pct")).toDouble(), 4)),
         "", {}},
        {tr("Slippage model"), tr("%1 bp + %2 × ATR").arg(fmt::number(cm.value(QStringLiteral("slippage_bps")).toDouble(), 1),
                                                        fmt::number(cm.value(QStringLiteral("slippage_atr_frac")).toDouble(), 3)),
         "", {}},
        {tr("Lot size"), fmt::number(cm.value(QStringLiteral("lot_size")).toDouble(1), 4), "", {}},
    };
    if (!p.value(QStringLiteral("costs_applied")).toBool())
        rows.prepend({tr("Costs"), tr("NONE — gross result"), "warn", tr("No commission, taxes or slippage were applied.")});
    brp_fill_kv(costs_grid_, rows, width() > QFontMetrics(font()).averageCharWidth() * 110 ? 2 : 1);
}

void BacktestReportPanel::populate_assumptions() {
    const QJsonObject& p = last_payload_;
    const QString dash = fmt::placeholder();
    auto num = [&dash](double v, int dec = 2) { return std::isfinite(v) ? fmt::number(v, dec) : dash; };
    QStringList w;
    for (const auto& v : p.value(QStringLiteral("warnings")).toArray())
        w << QStringLiteral("⚠ ") + v.toString();
    warnings_list_->setText(w.join(QLatin1Char('\n')));
    warnings_list_->setVisible(!w.isEmpty());
    assumptions_note_->setText(
        tr("Fill model: signals on the close of a bar fill at the next bar's open, moved adversely by the slippage; "
           "stop-loss / trailing stops are resting orders against each bar's range (a gap fills at the open), "
           "take-profit is a limit; a bar that touches both counts as a stop. Equity is marked on every close; a "
           "position still open at the end is closed at the last close."));
    const bool daily_basis = p.value(QStringLiteral("ratio_basis")).toString() == QLatin1String("daily");
    QVector<BrpKv> rows = {
        {tr("Bars"), QString::number(p.value(QStringLiteral("bars")).toInt()), "", {}},
        {tr("Timeframe"), p.value(QStringLiteral("timeframe")).toString(), "", {}},
        {tr("Venue / currency"), QStringLiteral("%1 / %2").arg(p.value(QStringLiteral("venue")).toString(dash), currency_.isEmpty() ? dash : currency_),
         "", {}},
        {tr("Bars per year"), num(rp_num(p, "periods_per_year"), 0), "", tr("From the exchange calendar.")},
        {tr("Sharpe / Sortino basis"),
         daily_basis ? tr("daily closes × √%1").arg(num(rp_num(p, "ratio_periods_per_year"), 0))
                     : tr("bar returns × √%1").arg(num(rp_num(p, "ratio_periods_per_year") > 0 ? rp_num(p, "ratio_periods_per_year")
                                                                                              : rp_num(p, "periods_per_year"),
                                                      0)),
         "", tr("Intraday runs of 20+ trading days are measured on their daily closes, like every other Sharpe in the terminal.")},
        {tr("Risk-free"), std::isfinite(rp_num(p, "risk_free_rate")) ? fmt::pct(rp_num(p, "risk_free_rate"), 2, false) : dash, "", {}},
        {tr("Sharpe"), num(rp_num(p, "sharpe_ratio")), "", {}},
        {tr("Sortino"), num(rp_num(p, "sortino")), "", {}},
        {tr("Calmar"), num(rp_num(p, "calmar")), "", {}},
        {tr("Volatility (ann.)"), std::isfinite(rp_num(p, "ann_volatility")) ? fmt::pct(rp_num(p, "ann_volatility"), 2, false) : dash, "", {}},
        {tr("Longest drawdown"), tr("%1 d · %2 bars").arg(num(rp_num(p, "max_dd_duration_days"), 0)).arg(p.value(QStringLiteral("max_dd_duration_bars")).toInt()),
         "", {}},
        {tr("Expectancy"), money(rp_num(p, "expectancy"), true), rp_num(p, "expectancy") > 0 ? "pos" : (rp_num(p, "expectancy") < 0 ? "neg" : ""),
         tr("Average net P&L per closed trade.")},
        {tr("Payoff ratio"), num(rp_num(p, "payoff_ratio")), "", tr("Average win over average loss.")},
        {tr("Avg MAE / MFE"), QStringLiteral("%1 / %2").arg(std::isfinite(rp_num(p, "avg_mae_pct")) ? fmt::pct(rp_num(p, "avg_mae_pct"), 2, true) : dash,
                                                            std::isfinite(rp_num(p, "avg_mfe_pct")) ? fmt::pct(rp_num(p, "avg_mfe_pct"), 2, true) : dash),
         "", tr("Maximum adverse / favourable excursion while in a trade, % of the entry price.")},
        {tr("Skipped entries"), QString::number(p.value(QStringLiteral("skipped_entries")).toInt()), "", {}},
    };
    brp_fill_kv(assumptions_grid_, rows, width() > QFontMetrics(font()).averageCharWidth() * 110 ? 2 : 1);
}

void BacktestReportPanel::export_trades_csv() {
    QStringList headers;
    QVector<QStringList> rows;
    trades_export(&headers, &rows);
    const QString name = QStringLiteral("%1_backtest_trades")
                             .arg(last_payload_.value(QStringLiteral("symbol")).toString().replace(QLatin1Char('.'), QLatin1Char('_')));
    ChartExport::save_csv(headers, rows, name, this);
}

void BacktestReportPanel::trades_export(QStringList* headers, QVector<QStringList>* rows) const {
    // Machine-readable: C-locale numbers without grouping or symbol, ISO 8601 times in the
    // venue's zone, and the ISO code of the price / P&L columns in its own column.
    *headers = {QStringLiteral("trade"),     QStringLiteral("side"),        QStringLiteral("entry_time"),
                QStringLiteral("exit_time"), QStringLiteral("entry_price"), QStringLiteral("exit_price"),
                QStringLiteral("quantity"),  QStringLiteral("gross_pnl"),   QStringLiteral("costs"),
                QStringLiteral("net_pnl"),   QStringLiteral("currency"),    QStringLiteral("net_pct"),
                QStringLiteral("mae_pct"),   QStringLiteral("mfe_pct"),     QStringLiteral("bars_held"),
                QStringLiteral("exit_reason")};
    rows->clear();
    const auto* venue = core::market::venue_by_code(last_payload_.value(QStringLiteral("venue")).toString());
    const QTimeZone tz = venue ? core::market::venue_timezone(*venue) : QTimeZone(QTimeZone::UTC);
    auto iso = [&tz](double ms) {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ms), tz).toString(Qt::ISODate) : QString();
    };
    const QString ccy = fmt::export_money(0, currency_).currency;
    const QJsonArray trades = last_payload_.value(QStringLiteral("trades")).toArray();
    rows->reserve(trades.size());
    for (int i = 0; i < trades.size(); ++i) {
        const QJsonObject t = trades[i].toObject();
        auto num = [&t](const char* key) { return fmt::export_number(rp_num(t, key)); };
        const double costs = t.value(QStringLiteral("fees")).toDouble() + t.value(QStringLiteral("slippage")).toDouble() +
                             t.value(QStringLiteral("borrow")).toDouble();
        rows->append({QString::number(i + 1),
                      t.value(QStringLiteral("side")).toString() == QLatin1String("SHORT") ? QStringLiteral("SHORT")
                                                                                           : QStringLiteral("LONG"),
                      iso(t.value(QStringLiteral("entry_time")).toDouble()),
                      iso(t.value(QStringLiteral("exit_time")).toDouble()), num("entry_price"), num("exit_price"),
                      num("shares"), num("gross_pnl"), fmt::export_number(costs), num("pnl"), ccy, num("pnl_pct"),
                      num("mae_pct"), num("mfe_pct"), QString::number(t.value(QStringLiteral("bars_held")).toInt()),
                      t.value(QStringLiteral("reason")).toString()});
    }
}

// ── Language ─────────────────────────────────────────────────────────────────

void BacktestReportPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        if (has_result_)
            render_result();
    }
    QWidget::changeEvent(event);
}

void BacktestReportPanel::retranslateUi() {
    cancel_btn_->setText(tr("CANCEL"));
    cancel_btn_->setAccessibleName(tr("Cancel the backtest"));
    export_btn_->setText(tr("EXPORT CSV…"));
    export_btn_->setAccessibleName(tr("Export the trades as CSV"));
    export_btn_->setToolTip(tr("Plain numbers, ISO times in the exchange's zone and a currency column"));
    const QString tabs[kRpTabCount] = {tr("TRADES"), tr("MONTHLY"), tr("COSTS"), tr("ASSUMPTIONS / WARNINGS"),
                                       tr("PRICE && TRADES"), tr("DISTRIBUTION")};
    for (int i = 0; i < kRpTabCount && i < tabs_->count(); ++i)
        tabs_->setTabText(i, tabs[i]);
    const QString labels[] = {tr("NET RETURN"), tr("CAGR"),     tr("SHARPE"),   tr("MAX DRAWDOWN"),
                              tr("WIN RATE"),   tr("PROFIT FACTOR"), tr("TRADES"), tr("EXPOSURE"),
                              tr("VS BUY & HOLD")};
    for (int i = 0; i < kpis_->count() && i < 9; ++i)
        if (StatTile* t = kpis_->tile(i))
            t->set_label(labels[i]);
    if (stack_->currentIndex() == kRpPageIdle)
        apply_idle_spec();
}

} // namespace fincept::ui::algo
