// AltFundAnalyticsPanel.cpp — see AltFundAnalyticsPanel.h.
#include "screens/alt_investments/AltFundAnalyticsPanel.h"

#include "core/instrument/Instrument.h"
#include "screens/alt_investments/AltPanels.h"
#include "services/alt_investments/AltInvestmentsService.h"
#include "services/instruments/InstrumentResolver.h"
#include "storage/cache/CacheManager.h"
#include "ui/charts/ChartExport.h"
#include "ui/charts/DistributionChart.h"
#include "ui/charts/MonthlyReturnsHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/DataGridEditor.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/report/AddToReportButton.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::alt {

using fincept::services::alt::AltCall;
using fincept::services::alt::AltInvestmentsService;
using fincept::services::alt::AltReply;

namespace {

const QString kAltFaSeriesCacheKey = QStringLiteral("alt_investments/fund_analytics_series");

/// Illustrative 36 monthly returns (percent) the panel opens with, labelled as an example on screen.
QJsonArray alt_fa_example_series() {
    static const double kRet[] = {1.2, -0.4, 2.1, 0.8,  -1.5, 0.6, 1.7, -2.2, 0.9, 0.4,  1.3, -0.6,
                                  1.1, 1.9,  -3.1, 0.7, 0.2,  1.5, -0.9, 1.0, 0.5, 1.8,  -1.2, 0.3,
                                  0.8, -0.2, 1.4, 0.6,  -1.8, 2.1, 0.4, 0.9,  -0.7, 1.2, 1.6, 0.1};
    QJsonArray out;
    QDate d(2023, 1, 31);
    for (double r : kRet) {
        out.append(QJsonObject{{QStringLiteral("date"), d.toString(Qt::ISODate)}, {QStringLiteral("value"), r}});
        d = d.addMonths(1);
        d = QDate(d.year(), d.month(), d.daysInMonth());
    }
    return out;
}

void alt_fa_set(ui::KpiStrip* strip, const char* key, const QString& value, const QString& sub = QString()) {
    if (ui::StatTile* t = strip->tile(QString::fromLatin1(key))) {
        t->set_value(value);
        t->set_sub_label(sub);
    }
}

} // namespace

QWidget* make_fund_analytics_panel(QWidget* parent) {
    return new AltFundAnalyticsPanel(parent);
}

AltFundAnalyticsPanel::AltFundAnalyticsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("altWbRoot"));
    setStyleSheet(alt_wb_style()); // before any child exists
    build_ui();
}

AltFundAnalyticsPanel::~AltFundAnalyticsPanel() {
    for (auto it = service_ids_.cbegin(); it != service_ids_.cend(); ++it)
        AltInvestmentsService::instance().cancel(it.value());
    if (pane_)
        pane_->set_cancel({});
}

void AltFundAnalyticsPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName(QStringLiteral("altWbSplit"));
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);
    root->addWidget(split, 1);

    // ── Inputs ──
    auto* left = new QWidget(split);
    left->setObjectName(QStringLiteral("altWbForm"));
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(10, 8, 10, 8);
    ll->setSpacing(6);
    auto* head = new QWidget(left);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->addWidget(alt_wb_label(tr("RETURN SERIES"), "altWbSection", head), 1);
    run_ = alt_wb_button(tr("RUN"), true, head);
    hl->addWidget(run_);
    ll->addWidget(head);

    auto* form = new QWidget(left);
    auto* g = new QGridLayout(form);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(3);
    source_ = new QComboBox(form);
    source_->addItem(tr("Returns I enter or paste"), QStringLiteral("series"));
    source_->addItem(tr("A ticker (Yahoo Finance)"), QStringLiteral("ticker"));
    benchmark_ = new ui::SymbolSearchBox(form);
    benchmark_->setObjectName(QStringLiteral("alt_fa_benchmark"));
    services::ResolveOptions bopts;
    bopts.ctx.allowed_classes = {core::instrument::AssetClass::Etf, core::instrument::AssetClass::Index,
                                 core::instrument::AssetClass::Fund, core::instrument::AssetClass::Equity};
    benchmark_->set_options(bopts);
    benchmark_->set_compact(true);
    benchmark_->set_clear_on_pick(false);
    benchmark_->set_recent_key(QStringLiteral("alt_fa_benchmark"));
    benchmark_->set_placeholder(tr("Optional: SPY, AGG, ACWI… (downloads prices)"));
    rf_ = new QDoubleSpinBox(form);
    rf_->setRange(0, 20);
    rf_->setDecimals(2);
    rf_->setSuffix(QStringLiteral("%"));
    rf_->setValue(0);
    rf_->setToolTip(tr("Annual risk-free rate for Sharpe, Sortino and alpha"));
    window_ = new QSpinBox(form);
    window_->setRange(3, 60);
    window_->setValue(12);
    window_->setSuffix(tr(" months"));
    unsmooth_ = new QCheckBox(tr("Illiquid series: also show unsmoothed figures"), form);
    unsmooth_->setToolTip(tr("Geltner first-order unsmoothing for appraisal-based or lagged NAVs (private real "
                             "estate, private credit, some hedge funds): removes the autocorrelation that makes "
                             "reported volatility look low"));
    g->addWidget(alt_wb_label(tr("SOURCE"), "altWbFormLabel", form), 0, 0);
    g->addWidget(source_, 1, 0);
    g->addWidget(alt_wb_label(tr("BENCHMARK"), "altWbFormLabel", form), 0, 1);
    g->addWidget(benchmark_, 1, 1);
    g->addWidget(alt_wb_label(tr("RISK-FREE"), "altWbFormLabel", form), 2, 0);
    g->addWidget(rf_, 3, 0);
    g->addWidget(alt_wb_label(tr("ROLLING WINDOW"), "altWbFormLabel", form), 2, 1);
    g->addWidget(window_, 3, 1);
    g->addWidget(unsmooth_, 4, 0, 1, 2);
    ll->addWidget(form);

    ticker_box_ = new QWidget(left);
    auto* tb = new QGridLayout(ticker_box_);
    tb->setContentsMargins(0, 0, 0, 0);
    tb->setHorizontalSpacing(8);
    tb->setVerticalSpacing(3);
    ticker_ = new ui::SymbolSearchBox(ticker_box_);
    ticker_->setObjectName(QStringLiteral("alt_fa_ticker"));
    services::ResolveOptions topts;
    topts.ctx.allowed_classes = {core::instrument::AssetClass::Etf, core::instrument::AssetClass::Fund,
                                 core::instrument::AssetClass::Equity, core::instrument::AssetClass::Index};
    ticker_->set_options(topts);
    ticker_->set_compact(true);
    ticker_->set_clear_on_pick(false);
    ticker_->set_recent_key(QStringLiteral("alt_fa_ticker"));
    ticker_->set_placeholder(tr("Fund or ETF: QAI, DBMF, HDG…"));
    ticker_->set_text(QStringLiteral("QAI"));
    period_ = new QComboBox(ticker_box_);
    for (const char* p : {"3y", "5y", "10y", "max"})
        period_->addItem(QString::fromLatin1(p).toUpper(), QString::fromLatin1(p));
    period_->setCurrentIndex(1);
    tb->addWidget(alt_wb_label(tr("TICKER"), "altWbFormLabel", ticker_box_), 0, 0);
    tb->addWidget(ticker_, 1, 0);
    tb->addWidget(alt_wb_label(tr("PERIOD"), "altWbFormLabel", ticker_box_), 0, 1);
    tb->addWidget(period_, 1, 1);
    ll->addWidget(ticker_box_);

    series_box_ = new QWidget(left);
    auto* sb = new QVBoxLayout(series_box_);
    sb->setContentsMargins(0, 0, 0, 0);
    sb->setSpacing(3);
    auto* unit_row = new QWidget(series_box_);
    auto* ul = new QHBoxLayout(unit_row);
    ul->setContentsMargins(0, 0, 0, 0);
    ul->addWidget(alt_wb_label(tr("MONTHLY RETURNS IN"), "altWbFormLabel", unit_row));
    unit_ = new QComboBox(unit_row);
    unit_->addItem(tr("percent (1.5 = +1.5%)"), QStringLiteral("percent"));
    unit_->addItem(tr("decimals (0.015)"), QStringLiteral("decimal"));
    ul->addWidget(unit_, 1);
    sb->addWidget(unit_row);
    using ui::DataGridColumn;
    grid_ = new ui::DataGridEditor({DataGridColumn::date(QStringLiteral("date"), tr("MONTH (optional)"), false),
                                    DataGridColumn::number(QStringLiteral("value"), tr("RETURN"), 2)},
                                   series_box_);
    grid_->set_visible_rows(10);
    grid_->set_hint(tr("One row per month. Paste a column of returns, or date + return."));
    sb->addWidget(grid_, 1);
    series_note_ = alt_wb_label(QString(), "altWbHint", series_box_);
    series_note_->setWordWrap(true);
    sb->addWidget(series_note_);
    ll->addWidget(series_box_, 1);

    const int cw = QFontMetrics(ui::fonts::font(-1)).averageCharWidth();
    left->setMinimumWidth(std::max(320, cw * 46));
    left->setMaximumWidth(std::max(480, cw * 66));

    // Remembered settings (not the series: that stays in the local cache, below).
    memory_ = new ui::InputMemory(QStringLiteral("alt_investments"), QStringLiteral("fund_analytics"), left);
    memory_->track(QStringLiteral("source"), source_);
    memory_->track(QStringLiteral("period"), period_);
    memory_->track(QStringLiteral("rf"), rf_);
    memory_->track(QStringLiteral("window"), window_);
    memory_->track(QStringLiteral("unit"), unit_);
    memory_->track(QStringLiteral("unsmooth"), unsmooth_);
    memory_->track(QStringLiteral("ticker"), ticker_->line_edit());
    memory_->track(QStringLiteral("benchmark"), benchmark_->line_edit());
    restoring_ = true;
    memory_->restore();
    const QVariant saved = CacheManager::instance().get(kAltFaSeriesCacheKey);
    const QJsonDocument sdoc = QJsonDocument::fromJson(saved.toString().toUtf8());
    const bool have_saved = sdoc.isArray() && !sdoc.array().isEmpty();
    grid_->from_json(have_saved ? sdoc.array() : alt_fa_example_series());
    series_note_->setText(have_saved ? QString()
                                     : tr("Illustrative example returns - replace them with your fund's monthly returns."));
    restoring_ = false;

    // ── Result ──
    auto* right = new QWidget(split);
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    actions_ = alt_wb_make_actions(right, tr("FUND ANALYTICS"));
    rl->addWidget(actions_.row);
    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, right);
    rl->addWidget(pane_, 1);
    auto* scroll = new QScrollArea(pane_->content_parent());
    scroll->setObjectName(QStringLiteral("altWbScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    body->setObjectName(QStringLiteral("altWbScrollBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(8);
    kpis_ = new ui::KpiStrip(body);
    for (const auto& t : QList<QPair<const char*, QString>>{{"cagr", tr("CAGR")},
                                                            {"vol", tr("Volatility")},
                                                            {"sharpe", tr("Sharpe")},
                                                            {"sortino", tr("Sortino")},
                                                            {"mdd", tr("Max drawdown")},
                                                            {"var", tr("VaR 95% (1 month)")},
                                                            {"beta", tr("Beta")},
                                                            {"alpha", tr("Alpha (annual)")},
                                                            {"up", tr("Up capture")},
                                                            {"down", tr("Down capture")}})
        kpis_->add_tile(QString::fromLatin1(t.first), t.second);
    bl->addWidget(kpis_);
    notes_ = alt_wb_label(QString(), "altWbNote", body);
    notes_->setWordWrap(true);
    bl->addWidget(notes_);
    bl->addWidget(alt_wb_label(tr("GROWTH OF 100 AND DRAWDOWN"), "altWbSection", body));
    growth_ = new ui::TimeSeriesChart(body);
    growth_->set_show_legend(true);
    growth_->set_show_mode_buttons(false);
    growth_->set_export_name(QStringLiteral("fund_growth"));
    growth_->setMinimumHeight(300);
    bl->addWidget(growth_);
    bl->addWidget(alt_wb_label(tr("ROLLING RETURN AND VOLATILITY"), "altWbSection", body));
    rolling_ = new ui::TimeSeriesChart(body);
    rolling_->set_show_legend(true);
    rolling_->set_show_mode_buttons(false);
    rolling_->set_export_name(QStringLiteral("fund_rolling"));
    rolling_->setMinimumHeight(200);
    bl->addWidget(rolling_);
    auto* pair = new QWidget(body);
    auto* pl = new QHBoxLayout(pair);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(10);
    auto* gbox = new QWidget(pair);
    auto* gl = new QVBoxLayout(gbox);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->addWidget(alt_wb_label(tr("MONTHLY RETURNS (%)"), "altWbSection", gbox));
    grid_map_ = new ui::MonthlyReturnsHeatmap(gbox);
    grid_map_->set_show_year_total(true);
    grid_map_->setMinimumHeight(180);
    gl->addWidget(grid_map_);
    auto* dbox = new QWidget(pair);
    auto* dl = new QVBoxLayout(dbox);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->addWidget(alt_wb_label(tr("DISTRIBUTION OF MONTHLY RETURNS (%)"), "altWbSection", dbox));
    dist_ = new ui::DistributionChart(dbox);
    dist_->setMinimumHeight(180);
    dl->addWidget(dist_);
    pl->addWidget(gbox, 3);
    pl->addWidget(dbox, 2);
    bl->addWidget(pair);
    stats_model_ = new AltWbRowsModel(this);
    stats_table_ = alt_wb_make_table(body, stats_model_, QStringLiteral("fund_statistics"));
    bl->addWidget(stats_table_);
    bl->addStretch(1);
    scroll->setWidget(body);
    pane_->set_content(scroll);

    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({std::max(340, cw * 50), 4000});

    pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    pane_->set_loading_text(tr("Computing the return statistics…"));
    pane_->set_cache_key(QStringLiteral("alt_investments/fund_analytics"), 60);
    pane_->set_status_label(actions_.status);
    pane_->set_timeout(120);
    pane_->set_auto_delay(200);
    pane_->set_start([this](quint64 id) { start(id); });
    pane_->set_cancel([this](quint64 id) {
        const auto it = service_ids_.find(id);
        if (it != service_ids_.end()) {
            AltInvestmentsService::instance().cancel(it.value());
            service_ids_.erase(it);
        }
    });
    pane_->set_render([this](const QJsonValue& p, const QDateTime&, bool) { render(p); });
    // A number-format change re-renders the shown result from its payload (nothing is fetched).
    ui::formatting::on_format_changed(this, [this]() {
        if (pane_->has_result())
            render(pane_->payload());
    });
    update_view();

    auto on_input = [this]() {
        if (!restoring_)
            update_view();
    };
    connect(memory_, &ui::InputMemory::changed, this, on_input);
    connect(source_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, on_input);
    connect(grid_, &ui::DataGridEditor::changed, this, [this]() {
        if (restoring_)
            return;
        series_note_->setText(QString());
        save_series();
        update_view();
    });
    connect(ticker_, &ui::SymbolSearchBox::instrument_picked, this, [this](const core::instrument::Instrument& i) {
        if (!i.symbol.isEmpty())
            ticker_->set_text(i.symbol);
        update_view();
    });
    connect(benchmark_, &ui::SymbolSearchBox::instrument_picked, this, [this](const core::instrument::Instrument& i) {
        if (!i.symbol.isEmpty())
            benchmark_->set_text(i.symbol);
        update_view();
    });
    connect(run_, &QPushButton::clicked, this, [this]() {
        update_view();
        pane_->run();
    });

    connect(actions_.copy, &QPushButton::clicked, this, [this]() { alt_wb_copy(export_); });
    connect(actions_.csv, &QPushButton::clicked, this,
            [this]() { alt_wb_save_csv(export_, QStringLiteral("fund_analytics"), this); });
    connect(actions_.image, &QPushButton::clicked, this,
            [this]() { ui::ChartExport::save_png(pane_->content(), QStringLiteral("fund_analytics")); });
    actions_.report->set_payload_provider([this]() {
        return alt_wb_payload(export_, {{tr("Growth of 100"), growth_}, {tr("Monthly returns"), grid_map_}});
    });
}

bool AltFundAnalyticsPanel::uses_network() const {
    return source_->currentData().toString() == QLatin1String("ticker") || !benchmark_->text().trimmed().isEmpty();
}

QJsonObject AltFundAnalyticsPanel::input() const {
    QJsonObject in;
    const QString src = source_->currentData().toString();
    in.insert(QStringLiteral("source"), src);
    if (src == QLatin1String("ticker")) {
        in.insert(QStringLiteral("ticker"), ticker_->text().trimmed().toUpper());
        in.insert(QStringLiteral("period"), period_->currentData().toString());
    } else {
        in.insert(QStringLiteral("series"), grid_->to_json());
        in.insert(QStringLiteral("unit"), unit_->currentData().toString());
    }
    const QString b = benchmark_->text().trimmed().toUpper();
    if (!b.isEmpty())
        in.insert(QStringLiteral("benchmark"), b);
    in.insert(QStringLiteral("rf_annual"), rf_->value() / 100.0);
    in.insert(QStringLiteral("window"), window_->value());
    in.insert(QStringLiteral("unsmooth"), unsmooth_->isChecked());
    return in;
}

void AltFundAnalyticsPanel::save_series() {
    // Local cache only (a year): a fund's returns are the user's data and never go to settings or sync.
    CacheManager::instance().put(kAltFaSeriesCacheKey,
                                 QString::fromUtf8(QJsonDocument(grid_->raw_json()).toJson(QJsonDocument::Compact)),
                                 365 * 24 * 3600, QStringLiteral("alt_investments"));
}

void AltFundAnalyticsPanel::update_view() {
    const bool ticker = source_->currentData().toString() == QLatin1String("ticker");
    ticker_box_->setVisible(ticker);
    series_box_->setVisible(!ticker);
    const bool net = uses_network();
    const QJsonObject in = input();
    QString key = ticker ? QStringLiteral("fa/ticker/%1/%2").arg(in.value(QStringLiteral("ticker")).toString(),
                                                                 in.value(QStringLiteral("period")).toString())
                         : QStringLiteral("fa/series");
    if (!in.value(QStringLiteral("benchmark")).toString().isEmpty())
        key += QStringLiteral("/vs/") + in.value(QStringLiteral("benchmark")).toString();
    ui::EmptyState* pv = pane_->preview();
    pv->apply(ui::EmptyState::preview_spec(
        tr("Fund analytics"),
        tr("Return statistics, benchmark comparison (beta, alpha, capture), rolling figures and the monthly grid. "
           "Not run automatically: it downloads prices from Yahoo Finance."),
        tr("RUN"), tr("Yahoo Finance prices · no credits"), ui::EmptyState::Tone::Info,
        ui::LoadingSkeleton::Shape::Result));
    QList<QPair<QString, QString>> details;
    if (ticker)
        details.append({tr("Fund"), in.value(QStringLiteral("ticker")).toString()});
    else
        details.append({tr("Fund"), tr("%n month(s) entered", nullptr, grid_->valid_row_count())});
    if (!in.value(QStringLiteral("benchmark")).toString().isEmpty())
        details.append({tr("Benchmark"), in.value(QStringLiteral("benchmark")).toString()});
    pv->set_details(details, tr("WHAT WILL BE ANALYSED"));
    pane_->set_max_age(net ? ui::autoload::kDefaultMaxAgeSeconds : ui::autoload::kNeverRefresh);
    pane_->set_view(key, net ? ui::AutoLoadPane::Policy::Manual : ui::AutoLoadPane::Policy::AutoRun);
    pane_->set_signature(ui::autoload::signature_of(QJsonValue(in)));
}

void AltFundAnalyticsPanel::start(quint64 id) {
    AltCall c;
    c.script = services::alt::kFundAnalyticsScript;
    c.command = QStringLiteral("analyze");
    c.input = input();
    c.timeout_ms = 120 * 1000;
    c.source = uses_network() ? QStringLiteral("Yahoo Finance") : QString();
    QPointer<ui::AutoLoadPane> pane = pane_;
    const quint64 sid = AltInvestmentsService::instance().call(c, this, [this, id, pane](const AltReply& r) {
        service_ids_.remove(id);
        if (!pane || r.cancelled)
            return;
        r.ok ? pane->deliver(id, r.data) : pane->fail(id, r.error, tr("Could not analyse the series"));
    });
    service_ids_.insert(id, sid);
}

void AltFundAnalyticsPanel::render(const QJsonValue& payload) {
    const QJsonObject d = payload.toObject();
    const QJsonObject s = d.value(QStringLiteral("stats")).toObject();
    const QJsonObject rel = d.value(QStringLiteral("relative")).toObject();
    const QJsonObject us = d.value(QStringLiteral("unsmoothed")).toObject();
    auto sv = [&s](const char* k) { return alt_wb_num(s.value(QLatin1String(k))); };
    auto rv = [&rel](const char* k) { return alt_wb_num(rel.value(QLatin1String(k))); };
    const QString label = d.value(QStringLiteral("label")).toString();
    const QString bench = rel.value(QStringLiteral("benchmark")).toString();
    alt_fa_set(kpis_, "cagr", alt_wb_pct(sv("cagr"), 2, true),
               tr("%1 months").arg(d.value(QStringLiteral("months")).toInt()));
    alt_fa_set(kpis_, "vol", alt_wb_pct(sv("annualized_volatility")),
               us.isEmpty() ? QString()
                            : tr("unsmoothed %1").arg(alt_wb_pct(
                                  alt_wb_num(us.value(QStringLiteral("stats")).toObject().value(
                                      QStringLiteral("annualized_volatility"))))));
    alt_fa_set(kpis_, "sharpe", alt_wb_number(sv("sharpe_ratio"), 2));
    alt_fa_set(kpis_, "sortino", alt_wb_number(sv("sortino_ratio"), 2));
    alt_fa_set(kpis_, "mdd", alt_wb_pct(-sv("max_drawdown"), 1));
    alt_fa_set(kpis_, "var", alt_wb_pct(sv("var_95"), 1), tr("CVaR %1").arg(alt_wb_pct(sv("cvar_95"), 1)));
    const QString vs = bench.isEmpty() ? tr("no benchmark") : tr("vs %1").arg(bench);
    alt_fa_set(kpis_, "beta", alt_wb_number(rv("beta"), 2), vs);
    alt_fa_set(kpis_, "alpha", alt_wb_pct(rv("alpha_annual"), 2, true), vs);
    alt_fa_set(kpis_, "up", alt_wb_pct(rv("up_capture"), 0), vs);
    alt_fa_set(kpis_, "down", alt_wb_pct(rv("down_capture"), 0), vs);
    QStringList notes;
    for (const QJsonValue& n : d.value(QStringLiteral("notes")).toArray())
        notes << n.toString();
    if (!us.isEmpty())
        notes << tr("Unsmoothed with rho = %1 (lag-1 autocorrelation).")
                     .arg(alt_wb_number(alt_wb_num(us.value(QStringLiteral("rho"))), 2));
    notes_->setText(notes.join(QLatin1Char('\n')));
    notes_->setVisible(!notes.isEmpty());

    // x axis: month dates when the series has them, else month numbers.
    const QJsonArray dates = d.value(QStringLiteral("dates")).toArray();
    const bool dated = !dates.isEmpty() && dates.first().isString();
    QVector<double> xs;
    for (int i = 0; i < dates.size(); ++i)
        xs << (dated ? alt_wb_date_ms(dates.at(i).toString()) : double(i + 1));
    auto arr = [](const QJsonValue& v, double scale) {
        QVector<double> out;
        for (const QJsonValue& x : v.toArray())
            out << alt_wb_num(x) * scale;
        return out;
    };
    growth_->clear();
    growth_->set_x_is_time(dated);
    ui::TimeSeriesChart::SeriesSpec f;
    f.name = label;
    f.xs = xs;
    f.ys = arr(d.value(QStringLiteral("growth")), 1.0);
    f.rebase = false;
    growth_->add_series(f);
    if (!rel.isEmpty()) {
        const QJsonArray bd = d.value(QStringLiteral("benchmark_dates")).toArray();
        QVector<double> bx;
        for (int i = 0; i < bd.size(); ++i)
            bx << (bd.at(i).isString() ? alt_wb_date_ms(bd.at(i).toString()) : xs.value(xs.size() - bd.size() + i));
        ui::TimeSeriesChart::SeriesSpec ff;
        ff.name = tr("%1 (same months)").arg(label);
        ff.xs = bx;
        ff.ys = arr(d.value(QStringLiteral("fund_growth_aligned")), 1.0);
        ff.rebase = false;
        ff.visible = bx.size() != xs.size();
        ff.show_in_legend = ff.visible;
        growth_->add_series(ff);
        ui::TimeSeriesChart::SeriesSpec b;
        b.name = bench;
        b.xs = bx;
        b.ys = arr(d.value(QStringLiteral("benchmark_growth")), 1.0);
        b.rebase = false;
        b.dashed = true;
        growth_->add_series(b);
    }
    // clear() keeps panes: the drawdown pane is added on the first render only.
    const int dd_pane = growth_->pane_count() > 1 ? 1 : growth_->add_pane(0.3);
    growth_->set_pane_title(dd_pane, tr("Drawdown"));
    ui::TimeSeriesChart::SeriesSpec dd;
    dd.name = tr("Drawdown");
    dd.xs = xs;
    dd.ys = arr(d.value(QStringLiteral("drawdown")), 100.0);
    dd.pane = dd_pane;
    dd.style = ui::TimeSeriesChart::Style::Area;
    dd.baseline = 0.0;
    dd.color = ui::ChartColorRole::Warning;
    dd.rebase = false;
    growth_->add_series(dd);
    growth_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return alt_wb_number(v, 1); });
    growth_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                 [](double v) { return ui::formatting::pct(v, 1, false); }, dd_pane);

    rolling_->clear();
    rolling_->set_x_is_time(dated);
    QVector<double> rx, rret, rvol;
    for (const QJsonValue& v : d.value(QStringLiteral("rolling")).toArray()) {
        const QJsonObject o = v.toObject();
        const int idx = o.value(QStringLiteral("index")).toInt();
        rx << xs.value(idx, double(idx + 1));
        rret << alt_wb_num(o.value(QStringLiteral("return"))) * 100.0;
        rvol << alt_wb_num(o.value(QStringLiteral("volatility"))) * 100.0;
    }
    ui::TimeSeriesChart::SeriesSpec r1;
    r1.name = tr("Rolling %1-month return").arg(d.value(QStringLiteral("window")).toInt());
    r1.xs = rx;
    r1.ys = rret;
    r1.rebase = false;
    rolling_->add_series(r1);
    ui::TimeSeriesChart::SeriesSpec r2;
    r2.name = tr("Rolling volatility (annual)");
    r2.xs = rx;
    r2.ys = rvol;
    r2.rebase = false;
    r2.dashed = true;
    rolling_->add_series(r2);
    rolling_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return ui::formatting::pct(v, 1, false); });
    rolling_->setVisible(!rx.isEmpty());

    ui::MonthlyReturnsHeatmap::MonthlyReturns m;
    const QJsonObject grid = d.value(QStringLiteral("monthly_grid")).toObject();
    for (auto y = grid.constBegin(); y != grid.constEnd(); ++y) {
        const QJsonObject months = y.value().toObject();
        for (auto mo = months.constBegin(); mo != months.constEnd(); ++mo)
            m[y.key().toInt()][mo.key().toInt()] = mo.value().toDouble() * 100.0;
    }
    grid_map_->set_monthly_returns(m);
    grid_map_->setVisible(!m.isEmpty());

    dist_->clear_markers();
    dist_->set_samples(arr(d.value(QStringLiteral("returns")), 100.0));
    if (std::isfinite(sv("var_95")))
        dist_->add_marker(-sv("var_95") * 100.0, tr("VaR 95%"), ui::DistributionChart::MarkerRole::VaR);
    if (std::isfinite(sv("cvar_95")))
        dist_->add_marker(-sv("cvar_95") * 100.0, tr("CVaR 95%"), ui::DistributionChart::MarkerRole::CVaR);

    // Statistics table: fund, benchmark over the same months, unsmoothed fund.
    const QJsonObject bs = rel.value(QStringLiteral("benchmark_stats")).toObject();
    const QJsonObject uss = us.value(QStringLiteral("stats")).toObject();
    QStringList headers = {tr("Statistic"), label};
    if (!bs.isEmpty())
        headers << tr("%1 (same months)").arg(bench);
    if (!uss.isEmpty())
        headers << tr("%1 unsmoothed").arg(label);
    struct Row {
        const char* key;
        QString name;
        int kind; // 0 pct, 1 ratio, 2 signed pct
    };
    const QList<Row> rows_def = {{"cumulative_return", tr("Cumulative return"), 2},
                                 {"cagr", tr("CAGR"), 2},
                                 {"annualized_volatility", tr("Volatility (annual)"), 0},
                                 {"sharpe_ratio", tr("Sharpe"), 1},
                                 {"sortino_ratio", tr("Sortino"), 1},
                                 {"max_drawdown", tr("Max drawdown"), 0},
                                 {"best_month_return", tr("Best month"), 2},
                                 {"worst_month_return", tr("Worst month"), 2},
                                 {"positive_months_rate", tr("Positive months"), 0}};
    auto cell = [](const QJsonObject& o, const Row& r) {
        const double v = alt_wb_num(o.value(QLatin1String(r.key)));
        return r.kind == 1 ? alt_wb_number(v, 2) : alt_wb_pct(v, 2, r.kind == 2);
    };
    QVector<QStringList> rows;
    for (const Row& r : rows_def) {
        QStringList line{r.name, cell(s, r)};
        if (!bs.isEmpty())
            line << cell(bs, r);
        if (!uss.isEmpty())
            line << cell(uss, r);
        rows << line;
    }
    if (!rel.isEmpty()) {
        auto rel_row = [&](const QString& name, const QString& value) {
            QStringList line{name, value};
            while (line.size() < headers.size())
                line << QString();
            rows << line;
        };
        rel_row(tr("Beta vs %1").arg(bench), alt_wb_number(rv("beta"), 2));
        rel_row(tr("Alpha (annual)"), alt_wb_pct(rv("alpha_annual"), 2, true));
        rel_row(tr("Correlation"), alt_wb_number(rv("correlation"), 2));
        rel_row(tr("R squared"), alt_wb_number(rv("r_squared"), 2));
        rel_row(tr("Tracking error"), alt_wb_pct(rv("tracking_error")));
        rel_row(tr("Information ratio"), alt_wb_number(rv("information_ratio"), 2));
        rel_row(tr("Up capture"), alt_wb_pct(rv("up_capture"), 0));
        rel_row(tr("Down capture"), alt_wb_pct(rv("down_capture"), 0));
    }
    QVector<int> numeric;
    for (int i = 1; i < headers.size(); ++i)
        numeric << i;
    stats_model_->set_rows(headers, rows, numeric);
    alt_wb_fit_table(stats_table_, 20);

    export_ = AltWbExport{};
    export_.title = tr("Fund analytics: %1").arg(label);
    export_.figures = {{tr("CAGR"), alt_wb_pct(sv("cagr"), 2, true)},
                       {tr("Volatility"), alt_wb_pct(sv("annualized_volatility"))},
                       {tr("Sharpe"), alt_wb_number(sv("sharpe_ratio"), 2)},
                       {tr("Sortino"), alt_wb_number(sv("sortino_ratio"), 2)},
                       {tr("Max drawdown"), alt_wb_pct(-sv("max_drawdown"), 1)},
                       {tr("VaR 95% (1 month)"), alt_wb_pct(sv("var_95"), 1)}};
    if (!rel.isEmpty())
        export_.figures.append({{tr("Beta vs %1").arg(bench), alt_wb_number(rv("beta"), 2)},
                                {tr("Alpha (annual)"), alt_wb_pct(rv("alpha_annual"), 2, true)},
                                {tr("Up / down capture"),
                                 alt_wb_pct(rv("up_capture"), 0) + QStringLiteral(" / ") + alt_wb_pct(rv("down_capture"), 0)}});
    export_.inputs = {{tr("Series"), label},
                      {tr("Months"), QString::number(d.value(QStringLiteral("months")).toInt())},
                      {tr("From"), d.value(QStringLiteral("first_month")).toString()},
                      {tr("To"), d.value(QStringLiteral("last_month")).toString()},
                      {tr("Risk-free (annual)"), alt_wb_pct(d.value(QStringLiteral("rf_annual")).toDouble())}};
    export_.headers = headers;
    export_.rows = rows;
    export_.notes = notes.join(QLatin1Char(' '));
    alt_wb_set_actions_enabled(actions_, true);
}

} // namespace fincept::screens::alt
