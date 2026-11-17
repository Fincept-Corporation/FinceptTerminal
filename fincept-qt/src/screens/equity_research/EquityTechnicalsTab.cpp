// src/screens/equity_research/EquityTechnicalsTab.cpp
#include "screens/equity_research/EquityTechnicalsTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "services/equity/EquityResearchService.h"
#include "ui/charts/Gauge.h"
#include "ui/charts/RangeBar.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/ProAnalytics.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPointer>
#include <QScrollArea>
#include <QTableWidget>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace technicals_tab_detail {

namespace tt_uf = fincept::ui::formatting;
namespace tt_tm = fincept::services::equity::tmath;
using services::equity::TechSignal;

bool tt_ok(double v) {
    return std::isfinite(v);
}

QTableWidget* tt_table(QWidget* parent, int cols) {
    auto* t = new QTableWidget(0, cols, parent);
    t->setObjectName(QStringLiteral("erTable"));
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setAlternatingRowColors(true);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui::metrics::follow_row_height(t, 22);
    return t;
}

QTableWidgetItem* tt_cell(const QString& text, Qt::Alignment a = Qt::AlignRight | Qt::AlignVCenter) {
    auto* it = new QTableWidgetItem(text);
    it->setTextAlignment(a);
    return it;
}

QVector<ui::GaugeZone> tt_zones() {
    const char* ctx = "EquityTechnicalsTab";
    return {
        {-1.0, -0.5, ui::GaugeZone::Tone::Down, QCoreApplication::translate(ctx, "Strong sell")},
        {-0.5, -0.1, ui::GaugeZone::Tone::Down, QCoreApplication::translate(ctx, "Sell")},
        {-0.1, 0.1, ui::GaugeZone::Tone::Neutral, QCoreApplication::translate(ctx, "Neutral")},
        {0.1, 0.5, ui::GaugeZone::Tone::Up, QCoreApplication::translate(ctx, "Buy")},
        {0.5, 1.0, ui::GaugeZone::Tone::Up, QCoreApplication::translate(ctx, "Strong buy")},
    };
}

double tt_last(const QVector<double>& v) {
    for (int i = static_cast<int>(v.size()) - 1; i >= 0; --i)
        if (tt_ok(v[i]))
            return v[i];
    return std::nan("");
}

} // namespace technicals_tab_detail

using namespace technicals_tab_detail;

EquityTechnicalsTab::EquityTechnicalsTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::historical_loaded, this,
            [this](const QString& s, const QString& p, const QVector<services::equity::Candle>& c) { on_history(s, p, c); });
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this,
            [this](const services::equity::StockInfo& info) {
                if (info.symbol != current_symbol_ || info.currency == currency_)
                    return;
                currency_ = info.currency;
                if (daily_loaded_)
                    render();
            });
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& f) { on_load_failed(f); });
    // Re-render what is held once a theme / font change has settled, and for a currency /
    // number-format change (no new request).
    const auto rerender = [this]() {
        if (daily_loaded_)
            render();
    };
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, rerender);
    ui::formatting::on_format_changed(this, rerender);
}

void EquityTechnicalsTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    currency_.clear();
    daily_.clear();
    hourly_.clear();
    bench_.clear();
    daily_loaded_ = false;
    render();
    request(false);
}

void EquityTechnicalsTab::request(bool force) {
    if (current_symbol_.isEmpty())
        return;
    auto& svc = services::equity::EquityResearchService::instance();
    if (loading_overlay_ && !daily_loaded_)
        loading_overlay_->show_loading(tr("COMPUTING INDICATORS…"));
    status_->show_loading(tr("Loading price history for %1…").arg(current_symbol_));
    Q_UNUSED(force);
    svc.load_info_only(current_symbol_);
    svc.load_historical_only(current_symbol_, QStringLiteral("max"));
    const auto bench = er_ui::benchmark_for(current_symbol_);
    if (!bench.symbol.isEmpty())
        svc.load_historical_only(bench.symbol, QStringLiteral("max"));
    if (frame_ == Frame::Hourly)
        load_hourly();
}

void EquityTechnicalsTab::on_history(const QString& symbol, const QString& period,
                                     const QVector<services::equity::Candle>& candles) {
    if (period != QLatin1String("max") || candles.isEmpty())
        return;
    if (symbol == current_symbol_) {
        daily_ = tt_tm::normalize_daily(candles);
        daily_loaded_ = true;
        if (loading_overlay_)
            loading_overlay_->hide_loading();
        status_->clear();
        render();
    } else if (symbol == er_ui::benchmark_for(current_symbol_).symbol) {
        bench_ = tt_tm::normalize_daily(candles);
        if (daily_loaded_)
            render_performance();
    }
}

void EquityTechnicalsTab::on_load_failed(const services::equity::LoadFailure& f) {
    if (f.symbol != current_symbol_ || f.leg != QLatin1String("Historical") || f.period != QLatin1String("max") ||
        daily_loaded_)
        return;
    if (loading_overlay_)
        loading_overlay_->hide_loading();
    status_->show_error(tr("Price history unavailable for %1: %2").arg(current_symbol_, f.message.left(160)), f.message);
    if (chart_)
        chart_->set_state(ui::TimeSeriesChart::State::Error, f.message.left(200));
}

void EquityTechnicalsTab::load_hourly() {
    const QString sym = current_symbol_;
    QPointer<EquityTechnicalsTab> self = this;
    status_->show_loading(tr("Loading hourly bars…"));
    services::equity::EquityResearchService::instance().fetch_price_history(
        sym, QStringLiteral("3mo"), QStringLiteral("1h"),
        [self, sym](bool ok, QVector<services::equity::Candle> candles, QString error) {
            QMetaObject::invokeMethod(
                qApp,
                [self, sym, ok, candles, error]() {
                    if (!self || self->current_symbol_ != sym)
                        return;
                    if (!ok || candles.isEmpty()) {
                        self->status_->show_error(tr("No hourly bars: %1").arg(error.left(160)), error);
                        return;
                    }
                    self->status_->clear();
                    self->hourly_ = candles;
                    self->render();
                },
                Qt::QueuedConnection);
        });
}

tt_tm::Bars EquityTechnicalsTab::frame_bars() const {
    switch (frame_) {
        case Frame::Hourly:
            return tt_tm::bars_from_candles(hourly_);
        case Frame::Weekly:
            return tt_tm::resample(tt_tm::bars_from_candles(daily_), tt_tm::Resample::Weekly);
        case Frame::Monthly:
            return tt_tm::resample(tt_tm::bars_from_candles(daily_), tt_tm::Resample::Monthly);
        case Frame::Daily:
        default:
            return tt_tm::bars_from_candles(daily_);
    }
}

QString EquityTechnicalsTab::fmt_price(double v) const {
    return equity_fmt::price(v, currency_);
}

QString EquityTechnicalsTab::signal_text(TechSignal s) const {
    switch (s) {
        case TechSignal::StrongBuy:
            return tr("STRONG BUY");
        case TechSignal::Buy:
            return tr("BUY");
        case TechSignal::Sell:
            return tr("SELL");
        case TechSignal::StrongSell:
            return tr("STRONG SELL");
        default:
            return tr("NEUTRAL");
    }
}

// ── Layout ───────────────────────────────────────────────────────────────────

void EquityTechnicalsTab::build_ui() {
    setObjectName(QStringLiteral("equityTechnicalsTab"));
    er_ui::apply_style(this, []() {
        return QStringLiteral("#equityTechnicalsTab, QWidget#techContent { background:%1; }"
                              "QScrollArea#techScroll { background:transparent; border:0; }")
                   .arg(ui::colors::BG_BASE()) +
               er_ui::stylesheet();
    });
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("techScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("techContent"));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* bar = new QHBoxLayout;
    frame_seg_ = new er_ui::Segmented(content);
    frame_seg_->add(QStringLiteral("1h"), QString());
    frame_seg_->add(QStringLiteral("d"), QString());
    frame_seg_->add(QStringLiteral("w"), QString());
    frame_seg_->add(QStringLiteral("m"), QString());
    frame_seg_->set_current(QStringLiteral("d"));
    connect(frame_seg_, &er_ui::Segmented::changed, this, [this](const QString& k) {
        frame_ = k == QLatin1String("1h") ? Frame::Hourly
                 : k == QLatin1String("w") ? Frame::Weekly
                 : k == QLatin1String("m") ? Frame::Monthly
                                           : Frame::Daily;
        if (frame_ == Frame::Hourly && hourly_.isEmpty())
            load_hourly();
        else
            render();
    });
    bar->addWidget(frame_seg_);
    bar->addStretch();
    // Pro analytics: the server's technical rating for this symbol, in its own window
    // (opens on click only; hidden while the server does not offer Pro analytics).
    bar->addWidget(ui::pro::make_button(ui::pro::Kind::TechnicalRating, [this]() {
        ui::pro::Request r;
        r.symbol = current_symbol_;
        return r;
    }, content));
    refresh_btn_ = new QPushButton(content);
    refresh_btn_->setObjectName(QStringLiteral("erBtn"));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        hourly_.clear();
        request(true);
    });
    bar->addWidget(refresh_btn_);
    vl->addLayout(bar);

    status_ = new er_ui::StatusStrip(content);
    connect(status_, &er_ui::StatusStrip::retry_requested, this, [this]() { request(true); });
    vl->addWidget(status_);

    // Chart (rebuilt per render: the pane set is fixed per chart).
    chart_host_ = new QWidget(content);
    auto* ch_l = new QVBoxLayout(chart_host_);
    ch_l->setContentsMargins(0, 0, 0, 0);
    // Four panes (price, volume, RSI, MACD): at 420 px the RSI pane was ~50 px and its
    // 70 / 30 tags collided with the axis ticks.
    chart_host_->setMinimumHeight(ui::metrics::height(520));
    vl->addWidget(chart_host_);

    // Rating gauges.
    auto* gauges = new QHBoxLayout;
    gauges->setSpacing(6);
    ui::Gauge** gs[3] = {&gauge_osc_, &gauge_sum_, &gauge_ma_};
    for (int i = 0; i < 3; ++i) {
        QVBoxLayout* body = nullptr;
        auto* p = er_ui::panel(QString(), &body, &gauge_title_[i], content);
        *gs[i] = new ui::Gauge(p);
        (*gs[i])->set_range(-1.0, 1.0);
        (*gs[i])->set_zones(tt_zones());
        (*gs[i])->set_show_range_labels(false);
        (*gs[i])->set_value_formatter([](double v) { return tt_uf::number(v, 2); });
        (*gs[i])->setMinimumHeight(ui::metrics::height(120));
        // The dial grows with its width: uncapped, each of the three was ~300 px tall at
        // 1600 px and pushed the vote tables a screen down.
        (*gs[i])->setMaximumHeight(ui::metrics::height(170));
        body->addWidget(*gs[i]);
        gauge_counts_[i] = new QLabel(p);
        gauge_counts_[i]->setObjectName(QStringLiteral("erNote"));
        gauge_counts_[i]->setAlignment(Qt::AlignCenter);
        body->addWidget(gauge_counts_[i]);
        gauges->addWidget(p, 1);
    }
    vl->addLayout(gauges);
    method_note_ = new QLabel(content);
    method_note_->setObjectName(QStringLiteral("erNote"));
    method_note_->setWordWrap(true);
    vl->addWidget(method_note_);

    // Votes.
    auto* tables = new QHBoxLayout;
    tables->setSpacing(6);
    {
        QVBoxLayout* body = nullptr;
        auto* p = er_ui::panel(QString(), &body, &osc_title_, content);
        osc_table_ = tt_table(p, 3);
        body->addWidget(osc_table_);
        tables->addWidget(p, 1);
    }
    {
        QVBoxLayout* body = nullptr;
        auto* p = er_ui::panel(QString(), &body, &ma_title_, content);
        ma_table_ = tt_table(p, 3);
        body->addWidget(ma_table_);
        tables->addWidget(p, 1);
    }
    vl->addLayout(tables);

    // Performance & risk.
    auto* perf = new QHBoxLayout;
    perf->setSpacing(6);
    {
        QVBoxLayout* body = nullptr;
        auto* p = er_ui::panel(QString(), &body, &perf_title_, content);
        perf_table_ = tt_table(p, 4);
        body->addWidget(perf_table_);
        perf->addWidget(p, 1);
    }
    {
        QVBoxLayout* body = nullptr;
        auto* p = er_ui::panel(QString(), &body, &risk_title_, content);
        risk_table_ = tt_table(p, 2);
        body->addWidget(risk_table_);
        body->addWidget(range_title_ = new QLabel(p));
        range_title_->setObjectName(QStringLiteral("erPanelTitle"));
        range52_ = new ui::RangeBar(p);
        range52_->setMinimumHeight(ui::metrics::height(46));
        body->addWidget(range52_);
        perf->addWidget(p, 1);
    }
    vl->addLayout(perf);
    vl->addStretch();
    scroll->setWidget(content);
    loading_overlay_ = new ui::LoadingOverlay(this);
    retranslateUi();
}

// ── Render ───────────────────────────────────────────────────────────────────

void EquityTechnicalsTab::render() {
    const auto b = frame_bars();
    render_chart(b);
    render_rating(b);
    render_performance();
}

void EquityTechnicalsTab::render_chart(const tt_tm::Bars& b) {
    if (chart_) {
        chart_host_->layout()->removeWidget(chart_);
        chart_->hide();
        chart_->deleteLater();
        chart_ = nullptr;
    }
    chart_ = new ui::TimeSeriesChart(chart_host_);
    chart_host_->layout()->addWidget(chart_);
    chart_->set_transparent_background(true);
    chart_->set_show_legend(true);
    chart_->set_export_name(current_symbol_ + QStringLiteral("_technicals"));
    const bool hourly = frame_ == Frame::Hourly;
    chart_->set_utc(!hourly);
    if (!hourly)
        chart_->set_time_formatter([](double x) { return tt_uf::bar_date(static_cast<qint64>(x / 1000.0)); });
    chart_->set_range_presets(hourly ? QStringList{QStringLiteral("5D"), QStringLiteral("1M"), QStringLiteral("ALL")}
                                     : QStringList{QStringLiteral("3M"), QStringLiteral("6M"), QStringLiteral("1Y"),
                                                   QStringLiteral("3Y"), QStringLiteral("5Y"), QStringLiteral("MAX")});
    const QString ccy = currency_;
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [ccy](double v) { return equity_fmt::price(v, ccy); });
    if (b.size() < 2) {
        chart_->set_state(daily_loaded_ ? ui::TimeSeriesChart::State::Empty : ui::TimeSeriesChart::State::Loading,
                          daily_loaded_ ? tr("Not enough bars") : tr("Loading…"));
        return;
    }
    QVector<double> xs(b.size());
    for (int i = 0; i < b.size(); ++i)
        xs[i] = static_cast<double>(b.time[i]) * 1000.0;
    auto line = [&](const QString& name, const QVector<double>& ys, ui::ChartColorRole role, int palette = -1,
                    int pane = 0, bool dashed = false) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = name;
        s.xs = xs;
        s.ys = ys;
        s.color = role;
        s.palette_index = palette;
        s.pane = pane;
        s.dashed = dashed;
        s.rebase = false;
        s.line_width = pane == 0 && name == current_symbol_ ? 1.6 : 1.1;
        return chart_->add_series(s);
    };
    line(current_symbol_, b.close, ui::ChartColorRole::Accent);
    line(tr("SMA 20"), tt_tm::sma(b.close, 20), ui::ChartColorRole::Palette, 1);
    line(tr("SMA 50"), tt_tm::sma(b.close, 50), ui::ChartColorRole::Palette, 2);
    line(tr("SMA 200"), tt_tm::sma(b.close, 200), ui::ChartColorRole::Palette, 3);
    const auto bb = tt_tm::bollinger(b.close, 20, 2.0);
    line(tr("BB upper"), bb.upper, ui::ChartColorRole::Muted, -1, 0, true);
    line(tr("BB lower"), bb.lower, ui::ChartColorRole::Muted, -1, 0, true);

    const int vol_pane = chart_->add_pane(0.2);
    chart_->set_pane_title(vol_pane, tr("Volume"));
    {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("Volume");
        s.xs = xs;
        s.ys = b.volume;
        s.style = ui::TimeSeriesChart::Style::Bars;
        s.pane = vol_pane;
        s.color = ui::ChartColorRole::Muted;
        s.rebase = false;
        chart_->add_series(s);
        chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return tt_uf::volume(v); }, vol_pane);
    }
    const int rsi_pane = chart_->add_pane(0.25);
    chart_->set_pane_title(rsi_pane, tr("RSI (14)"));
    line(tr("RSI (14)"), tt_tm::rsi(b.close, 14), ui::ChartColorRole::Info, -1, rsi_pane);
    chart_->set_y_range(rsi_pane, ui::TimeSeriesChart::Axis::Left, 0, 100);
    // RSI is a 0–100 index: whole numbers on the axis and on the 70 / 30 gutter tags
    // (the default formatter printed "70.00").
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return tt_uf::number(v, 0); },
                                rsi_pane);
    chart_->add_hline(70, QString(), ui::ChartColorRole::Muted, rsi_pane);
    chart_->add_hline(30, QString(), ui::ChartColorRole::Muted, rsi_pane);
    const int macd_pane = chart_->add_pane(0.28);
    chart_->set_pane_title(macd_pane, tr("MACD (12, 26, 9)"));
    const auto m = tt_tm::macd(b.close);
    {
        ui::TimeSeriesChart::SeriesSpec h;
        h.name = tr("Histogram");
        h.xs = xs;
        h.ys = m.hist;
        h.pane = macd_pane;
        h.style = ui::TimeSeriesChart::Style::Bars;
        h.color_by_sign = true;
        h.rebase = false;
        chart_->add_series(h);
    }
    line(tr("MACD"), m.line, ui::ChartColorRole::Info, -1, macd_pane);
    line(tr("Signal"), m.signal, ui::ChartColorRole::Warning, -1, macd_pane, true);
    chart_->set_active_preset(hourly ? QStringLiteral("1M") : frame_ == Frame::Daily ? QStringLiteral("1Y")
                                                                                     : QStringLiteral("5Y"));
}

void EquityTechnicalsTab::render_rating(const tt_tm::Bars& b) {
    const tt_tm::Rating r = tt_tm::technical_rating(b);
    const services::equity::RatingGroup* groups[3] = {&r.osc, nullptr, &r.ma};
    ui::Gauge* gauges[3] = {gauge_osc_, gauge_sum_, gauge_ma_};
    for (int i = 0; i < 3; ++i) {
        const double score = i == 1 ? r.summary : groups[i]->score;
        const TechSignal sig = i == 1 ? r.summary_signal : groups[i]->signal;
        gauges[i]->set_value(r.ok ? score : std::nan(""));
        gauges[i]->set_caption(r.ok ? signal_text(sig) : QString());
        gauges[i]->set_state(r.ok ? ui::PaintedChart::Ready : ui::PaintedChart::Empty,
                             b.size() < 30 ? tr("At least 30 bars needed") : QString());
        if (i == 1) {
            const int buy = r.ma.buy + r.osc.buy, sell = r.ma.sell + r.osc.sell, neu = r.ma.neutral + r.osc.neutral;
            gauge_counts_[i]->setText(r.ok ? tr("Buy %1 · Neutral %2 · Sell %3").arg(buy).arg(neu).arg(sell) : QString());
        } else {
            gauge_counts_[i]->setText(r.ok ? tr("Buy %1 · Neutral %2 · Sell %3")
                                                 .arg(groups[i]->buy)
                                                 .arg(groups[i]->neutral)
                                                 .arg(groups[i]->sell)
                                           : QString());
        }
    }
    auto fill = [this](QTableWidget* t, const services::equity::RatingGroup& g, bool price_units) {
        t->setRowCount(g.votes.size());
        for (int i = 0; i < g.votes.size(); ++i) {
            const auto& v = g.votes[i];
            t->setItem(i, 0, tt_cell(QCoreApplication::translate("EquityTechnicalMath", v.name.toUtf8().constData()),
                                     Qt::AlignLeft | Qt::AlignVCenter));
            t->setItem(i, 1, tt_cell(price_units ? fmt_price(v.value) : tt_uf::number(v.value, 2)));
            auto* a = tt_cell(signal_text(v.signal), Qt::AlignCenter);
            // A buy / sell vote points the price's way: the direction colours.
            if (v.signal == TechSignal::Buy || v.signal == TechSignal::StrongBuy)
                a->setForeground(QColor(ui::colors::POSITIVE()));
            else if (v.signal == TechSignal::Sell || v.signal == TechSignal::StrongSell)
                a->setForeground(QColor(ui::colors::NEGATIVE()));
            else
                a->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            t->setItem(i, 2, a);
        }
        t->setFixedHeight(t->horizontalHeader()->height() + ui::metrics::height(22) * std::max(1, static_cast<int>(g.votes.size())) + 4);
    };
    fill(ma_table_, r.ma, true);
    fill(osc_table_, r.osc, false);
    const QString frame = frame_ == Frame::Hourly ? tr("hourly")
                          : frame_ == Frame::Weekly ? tr("weekly")
                          : frame_ == Frame::Monthly ? tr("monthly")
                                                     : tr("daily");
    method_note_->setText(
        tr("Rating on %1 bars (last bar %2). Moving averages vote by where the last close sits against each average; "
           "oscillators vote only in their buy / sell zones and when turning that way. Each group scores "
           "(buys − sells) ÷ voters, from −1 to +1; the summary is the mean of the two. Indicators without enough "
           "history do not vote.")
            .arg(frame, b.size() > 0 ? (frame_ == Frame::Hourly ? tt_uf::date_time(b.time.last())
                                                                 : tt_uf::bar_date(b.time.last()))
                                     : equity_fmt::dash()));
}

void EquityTechnicalsTab::render_performance() {
    perf_table_->setRowCount(0);
    risk_table_->setRowCount(0);
    if (daily_.size() < 2) {
        range52_->set_data({});
        range52_->set_state(ui::PaintedChart::Empty);
        return;
    }
    const auto b = tt_tm::bars_from_candles(daily_);
    const auto bb = tt_tm::bars_from_candles(bench_);
    const auto bench = er_ui::benchmark_for(current_symbol_);
    perf_table_->setHorizontalHeaderLabels({tr("PERIOD"), current_symbol_, bench.name.isEmpty() ? tr("BENCHMARK") : bench.name,
                                            tr("RELATIVE")});
    const QDate last = QDateTime::fromSecsSinceEpoch(b.time.last(), QTimeZone::UTC).date();
    struct Win {
        const char* label;
        QDate from;
    };
    const Win wins[] = {
        {QT_TR_NOOP("1 week"), last.addDays(-7)},    {QT_TR_NOOP("1 month"), last.addMonths(-1)},
        {QT_TR_NOOP("3 months"), last.addMonths(-3)}, {QT_TR_NOOP("6 months"), last.addMonths(-6)},
        {QT_TR_NOOP("Year to date"), QDate(last.year() - 1, 12, 31)}, {QT_TR_NOOP("1 year"), last.addYears(-1)},
        {QT_TR_NOOP("3 years"), last.addYears(-3)},   {QT_TR_NOOP("5 years"), last.addYears(-5)},
        {QT_TR_NOOP("10 years"), last.addYears(-10)},
    };
    auto signed_cell = [](double v, bool pp) {
        auto* c = tt_cell(tt_ok(v) ? (pp ? tt_uf::pp(v * 100.0, 1) : tt_uf::fraction_pct(v, 1, true)) : equity_fmt::dash());
        if (tt_ok(v) && v != 0.0)
            c->setForeground(QColor(v > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        return c;
    };
    for (const auto& w : wins) {
        const qint64 from = QDateTime(w.from, QTime(23, 59), QTimeZone::UTC).toSecsSinceEpoch();
        const double r = tt_tm::return_since(b, from);
        if (!tt_ok(r))
            continue;
        const double br = bb.size() > 1 ? tt_tm::return_since(bb, from) : std::nan("");
        const int row = perf_table_->rowCount();
        perf_table_->insertRow(row);
        perf_table_->setItem(row, 0, tt_cell(tr(w.label), Qt::AlignLeft | Qt::AlignVCenter));
        perf_table_->setItem(row, 1, signed_cell(r, false));
        perf_table_->setItem(row, 2, signed_cell(br, false));
        perf_table_->setItem(row, 3, signed_cell(tt_ok(br) ? r - br : std::nan(""), true));
    }
    perf_table_->setFixedHeight(perf_table_->horizontalHeader()->height() +
                                ui::metrics::height(22) * std::max(1, perf_table_->rowCount()) + 4);

    // Risk.
    risk_table_->setHorizontalHeaderLabels({tr("MEASURE"), tr("VALUE")});
    auto add = [this](const QString& k, const QString& v, const QString& tip = {}) {
        const int row = risk_table_->rowCount();
        risk_table_->insertRow(row);
        risk_table_->setItem(row, 0, tt_cell(k, Qt::AlignLeft | Qt::AlignVCenter));
        auto* c = tt_cell(v);
        c->setToolTip(tip);
        risk_table_->setItem(row, 1, c);
    };
    const qint64 now = b.time.last();
    add(tr("Realised volatility (20 days)"), tt_uf::fraction_pct(tt_tm::realized_vol(b, 20), 1, false),
        tr("Annualised standard deviation of daily log returns"));
    add(tr("Realised volatility (60 days)"), tt_uf::fraction_pct(tt_tm::realized_vol(b, 60), 1, false));
    add(tr("Realised volatility (1 year)"), tt_uf::fraction_pct(tt_tm::realized_vol(b, 252), 1, false));
    add(tr("Max drawdown (1 year)"), tt_uf::fraction_pct(tt_tm::max_drawdown_since(b, now - 365LL * 86400), 1, false));
    add(tr("Max drawdown (3 years)"), tt_uf::fraction_pct(tt_tm::max_drawdown_since(b, now - 3 * 365LL * 86400), 1, false));
    add(tr("Max drawdown (all history)"), tt_uf::fraction_pct(tt_tm::max_drawdown_since(b, 0), 1, false));
    const auto dd = tt_tm::drawdown(b.close);
    add(tr("Below the all-time high"), tt_uf::fraction_pct(tt_last(dd), 1, false));
    if (bb.size() > 30)
        add(tr("Beta vs %1 (1 year)").arg(bench.name), tt_uf::number(tt_tm::beta_vs(b, bb, 252), 2),
            tr("OLS slope of daily log returns against the benchmark"));
    const double atr = tt_last(tt_tm::atr(b, 14));
    add(tr("ATR (14) / price"), tt_ok(atr) ? tt_uf::fraction_pct(atr / b.close.last(), 2, false) : equity_fmt::dash(),
        tr("Average true range as a share of the last close: %1").arg(fmt_price(atr)));
    risk_table_->setFixedHeight(risk_table_->horizontalHeader()->height() +
                                ui::metrics::height(22) * std::max(1, risk_table_->rowCount()) + 4);

    // 52-week position.
    double lo = std::nan(""), hi = std::nan("");
    for (int i = 0; i < b.size(); ++i)
        if (b.time[i] >= now - 365LL * 86400) {
            lo = tt_ok(lo) ? std::min(lo, b.low[i]) : b.low[i];
            hi = tt_ok(hi) ? std::max(hi, b.high[i]) : b.high[i];
        }
    if (tt_ok(lo) && tt_ok(hi) && hi > lo) {
        ui::RangeBarData d;
        d.low = lo;
        d.high = hi;
        d.markers.append({b.close.last(), tr("Last"), ui::RangeMarker::Role::Current});
        const double s200 = tt_last(tt_tm::sma(b.close, 200));
        if (tt_ok(s200))
            d.markers.append({s200, tr("200-day"), ui::RangeMarker::Role::Mean});
        range52_->set_value_formatter([this](double v) { return fmt_price(v); });
        range52_->set_data(d);
        range52_->set_state(ui::PaintedChart::Ready);
        range_title_->setText(tr("52-WEEK POSITION — %1 of the range")
                                  .arg(tt_uf::fraction_pct((b.close.last() - lo) / (hi - lo), 0, false)));
    }
}

// ── i18n ─────────────────────────────────────────────────────────────────────

void EquityTechnicalsTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityTechnicalsTab::retranslateUi() {
    frame_seg_->set_text(QStringLiteral("1h"), tr("1H"), tr("Hourly bars (last 3 months)"));
    frame_seg_->set_text(QStringLiteral("d"), tr("D"), tr("Daily bars"));
    frame_seg_->set_text(QStringLiteral("w"), tr("W"), tr("Weekly bars"));
    frame_seg_->set_text(QStringLiteral("m"), tr("M"), tr("Monthly bars"));
    refresh_btn_->setText(tr("REFRESH"));
    gauge_title_[0]->setText(tr("OSCILLATORS"));
    gauge_title_[1]->setText(tr("SUMMARY"));
    gauge_title_[2]->setText(tr("MOVING AVERAGES"));
    for (auto* g : {gauge_osc_, gauge_sum_, gauge_ma_})
        g->set_zones(tt_zones());
    osc_title_->setText(tr("OSCILLATORS"));
    ma_title_->setText(tr("MOVING AVERAGES — PRICE VS AVERAGE"));
    perf_title_->setText(tr("PERFORMANCE (PRICE RETURN)"));
    risk_title_->setText(tr("RISK"));
    range_title_->setText(tr("52-WEEK POSITION"));
    for (auto* t : {ma_table_, osc_table_})
        t->setHorizontalHeaderLabels({tr("INDICATOR"), tr("VALUE"), tr("ACTION")});
    if (daily_loaded_ || !hourly_.isEmpty())
        render();
}

} // namespace fincept::screens
