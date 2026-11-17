// src/screens/equity_research/EquityOverviewTab.cpp
#include "screens/equity_research/EquityOverviewTab.h"

#include "core/market/MarketSessions.h"
#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "services/equity/EquityResearchService.h"
#include "ui/charts/CandleChart.h"
#include "ui/charts/RangeBar.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingOverlay.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace overview_detail {

namespace ov_uf = fincept::ui::formatting;
namespace ov_tm = fincept::services::equity::tmath;

bool ov_ok(double v) {
    return std::isfinite(v);
}

/// Yahoo info reports a field it lacks as 0.
double ov_pos(double v) {
    return v > 0.0 ? v : std::nan("");
}
double ov_nz(double v) {
    return v != 0.0 ? v : std::nan("");
}

void ov_set_tone(QLabel* l, const char* tone) {
    if (!l || l->property("tone").toString() == QLatin1String(tone))
        return;
    l->setProperty("tone", QString::fromLatin1(tone));
    l->style()->unpolish(l);
    l->style()->polish(l);
}

constexpr const char* kOvDailyPeriod = "max";

const char* const kOvPresets[] = {"1D", "5D", "1M", "3M", "6M", "YTD", "1Y", "3Y", "5Y", "10Y", "MAX"};

/// Bars shown in candle mode for a preset (trading days, approximately).
int ov_candle_bars(const QString& preset) {
    static const QHash<QString, int> kBars = {{"1M", 22},   {"3M", 66},    {"6M", 130},  {"YTD", 0},
                                              {"1Y", 252},  {"3Y", 756},   {"5Y", 1260}, {"10Y", 2520},
                                              {"MAX", 100000}};
    if (preset == QLatin1String("YTD"))
        return std::max(5, QDate::currentDate().dayOfYear() * 252 / 365);
    return kBars.value(preset, 252);
}

} // namespace overview_detail

using namespace overview_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════

EquityOverviewTab::EquityOverviewTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::quote_loaded, this, &EquityOverviewTab::on_quote_loaded);
    connect(&svc, &services::equity::EquityResearchService::info_loaded, this, &EquityOverviewTab::on_info_loaded);
    connect(&svc, &services::equity::EquityResearchService::historical_loaded, this,
            &EquityOverviewTab::on_historical_loaded);
    connect(&svc, &services::equity::EquityResearchService::events_loaded, this, &EquityOverviewTab::on_events_loaded);
    connect(&svc, &services::equity::EquityResearchService::load_failed, this,
            [this](const services::equity::LoadFailure& f) { on_load_failed(f); });
    // Colours of the chart, tiles and range bars are read at paint time; the labels carry
    // tone properties re-polished by the sheet. The company block is re-rendered once an
    // appearance change has settled (never inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { render_company(); });
    // Currency / number format changed: re-format everything held (no new request).
    ui::formatting::on_format_changed(this, [this]() {
        render_company();
        render_info();
        render_quote();
        render_ranges();
        render_returns();
        schedule_rebuild(); // the chart's price axis
    });
}

void EquityOverviewTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    info_ = {};
    quote_ = {};
    events_ = {};
    info_loaded_ = quote_loaded_ = history_loaded_ = events_loaded_ = false;
    daily_.clear();
    intraday_.clear();
    intraday_preset_.clear();
    compare_data_.clear();
    compare_symbols_.clear();
    leg_errors_.clear();
    if (pending_preset_ == QLatin1String("1D") || pending_preset_ == QLatin1String("5D"))
        pending_preset_ = QStringLiteral("1Y");
    render_company();
    render_info();
    render_quote();
    render_ranges();
    render_returns();
    rebuild_chart();
    request_all();
}

void EquityOverviewTab::request_all() {
    if (current_symbol_.isEmpty())
        return;
    if (loading_overlay_ && !(info_loaded_ && quote_loaded_ && history_loaded_))
        loading_overlay_->show_loading(tr("LOADING OVERVIEW…"));
    status_->clear();
    auto& svc = services::equity::EquityResearchService::instance();
    svc.load_quote_only(current_symbol_);
    svc.load_info_only(current_symbol_);
    svc.load_historical_only(current_symbol_, QString::fromLatin1(kOvDailyPeriod));
    svc.fetch_events(current_symbol_);
    const auto bench = er_ui::benchmark_for(current_symbol_);
    if (!bench.symbol.isEmpty())
        svc.load_historical_only(bench.symbol, QString::fromLatin1(kOvDailyPeriod));
}

// ═════════════════════════════════════════════════════════════════════════════
// Layout
// ═════════════════════════════════════════════════════════════════════════════

void EquityOverviewTab::build_ui() {
    setObjectName(QStringLiteral("equityOverviewTab"));
    er_ui::apply_style(this, []() {
        return QStringLiteral("#equityOverviewTab, QWidget#ovContent { background:%1; }"
                              "QScrollArea#ovScroll { background:transparent; border:0; }"
                              "QLabel#ovName { color:%2; font-size:%4px; font-weight:700; background:transparent; }"
                              "QLabel#ovBadge { font-size:%5px; font-weight:700; padding:2px 10px; border:1px solid %3;"
                              " background:transparent; }"
                              "QLabel#ovDesc { color:%2; font-size:%6px; background:transparent; }")
                   .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                   .arg(ui::fonts::px(4), ui::fonts::px(-3), ui::fonts::px(-2)) +
               er_ui::stylesheet();
    });

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("ovScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("ovContent"));
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    status_ = new er_ui::StatusStrip(content);
    connect(status_, &er_ui::StatusStrip::retry_requested, this, [this]() {
        leg_errors_.clear();
        request_all();
    });
    vl->addWidget(status_);
    vl->addWidget(build_company_block());

    returns_ = new ui::KpiStrip(content);
    returns_->set_max_columns(8);
    vl->addWidget(returns_);

    // Chart and ABOUT stack on the left of the key-stats column. The chart used to take
    // the whole height of that column (~1,150 px): its time axis sat a screen below the
    // fold. It is capped now and the description fills the space under it.
    auto* main = new QHBoxLayout;
    main->setSpacing(6);
    auto* left = new QVBoxLayout;
    left->setSpacing(6);
    left->addWidget(build_chart_panel(), 1);

    QVBoxLayout* desc_body = nullptr;
    QLabel* desc_title = nullptr;
    auto* desc_panel = er_ui::panel(tr("ABOUT"), &desc_body, &desc_title, content);
    i18n_.insert(desc_title, QT_TR_NOOP("ABOUT"));
    desc_lbl_ = new QLabel(desc_panel);
    desc_lbl_->setObjectName(QStringLiteral("ovDesc"));
    desc_lbl_->setWordWrap(true);
    desc_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    desc_body->addWidget(desc_lbl_);
    left->addWidget(desc_panel);
    left->addStretch(0);
    main->addLayout(left, 1);

    auto* side = build_side_panels();
    const auto apply_width = [side]() { side->setFixedWidth(ui::metrics::width(320)); };
    apply_width();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, side, apply_width);
    main->addWidget(side, 0, Qt::AlignTop);
    vl->addLayout(main, 1);

    scroll->setWidget(content);
    loading_overlay_ = new ui::LoadingOverlay(this);
    retranslateUi();
}

QWidget* EquityOverviewTab::build_company_block() {
    QVBoxLayout* body = nullptr;
    auto* p = er_ui::panel(QString(), &body, nullptr, this);
    auto* top = new QHBoxLayout;
    top->setSpacing(10);
    name_lbl_ = new QLabel(p);
    name_lbl_->setObjectName(QStringLiteral("ovName"));
    name_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    top->addWidget(name_lbl_);
    rec_badge_ = new QLabel(p);
    rec_badge_->setObjectName(QStringLiteral("ovBadge"));
    rec_badge_->setVisible(false);
    top->addWidget(rec_badge_);
    top->addStretch();
    body->addLayout(top);
    meta_lbl_ = new QLabel(p);
    meta_lbl_->setObjectName(QStringLiteral("erKey"));
    meta_lbl_->setWordWrap(true);
    meta_lbl_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->addWidget(meta_lbl_);
    ccy_lbl_ = new QLabel(p);
    ccy_lbl_->setObjectName(QStringLiteral("erKey"));
    ccy_lbl_->setWordWrap(true);
    body->addWidget(ccy_lbl_);
    dates_lbl_ = new QLabel(p);
    dates_lbl_->setObjectName(QStringLiteral("erValue"));
    dates_lbl_->setWordWrap(true);
    body->addWidget(dates_lbl_);
    return p;
}

QWidget* EquityOverviewTab::build_chart_panel() {
    QVBoxLayout* body = nullptr;
    auto* p = er_ui::panel(QString(), &body, nullptr, this);
    auto* bar = new QHBoxLayout;
    bar->setSpacing(6);
    style_seg_ = new er_ui::Segmented(p);
    style_seg_->add(QStringLiteral("line"), QString());
    style_seg_->add(QStringLiteral("candles"), QString());
    connect(style_seg_, &er_ui::Segmented::changed, this, [this](const QString& key) {
        candles_mode_ = key == QLatin1String("candles");
        rebuild_chart();
    });
    bar->addWidget(style_seg_);
    auto make_btn = [&](QPushButton*& out) {
        out = new QPushButton(p);
        out->setObjectName(QStringLiteral("erSeg"));
        out->setCursor(Qt::PointingHandCursor);
        bar->addWidget(out);
    };
    make_btn(indicators_btn_);
    connect(indicators_btn_, &QPushButton::clicked, this, &EquityOverviewTab::show_indicator_menu);
    make_btn(compare_btn_);
    connect(compare_btn_, &QPushButton::clicked, this, &EquityOverviewTab::show_compare_menu);
    make_btn(events_btn_);
    events_btn_->setCheckable(true);
    events_btn_->setChecked(show_events_);
    connect(events_btn_, &QPushButton::toggled, this, [this](bool on) {
        show_events_ = on;
        rebuild_chart();
    });
    bar->addStretch();
    body->addLayout(bar);

    chart_stack_ = new QStackedWidget(p);
    chart_stack_->setMinimumHeight(ui::metrics::height(380));
    chart_stack_->setMaximumHeight(ui::metrics::height(600));
    candles_ = new ui::CandleChart(chart_stack_);
    chart_stack_->addWidget(candles_);
    body->addWidget(chart_stack_, 1);
    // The panel itself stops growing with the chart (no empty frame under a capped chart).
    body->setSizeConstraint(QLayout::SetMinAndMaxSize);
    return p;
}

QLabel* EquityOverviewTab::add_stat(QGridLayout* grid, int row, const char* key) {
    auto* k = new QLabel(tr(key));
    k->setObjectName(QStringLiteral("erKey"));
    i18n_.insert(k, key);
    auto* v = new QLabel(equity_fmt::dash());
    v->setObjectName(QStringLiteral("erValue"));
    v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(k, row, 0);
    grid->addWidget(v, row, 1);
    stats_.insert(QString::fromLatin1(key), v);
    return v;
}

QWidget* EquityOverviewTab::build_side_panels() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);

    // 52-week range and analyst targets as range bars.
    {
        QVBoxLayout* body = nullptr;
        auto* p = er_ui::panel(QString(), &body, &range52_title_, w);
        range52_ = new ui::RangeBar(p);
        range52_->setMinimumHeight(ui::metrics::height(46));
        body->addWidget(range52_);
        body->addWidget(targets_title_ = new QLabel(p));
        targets_title_->setObjectName(QStringLiteral("erPanelTitle"));
        targets_ = new ui::RangeBar(p);
        targets_->setMinimumHeight(ui::metrics::height(46));
        body->addWidget(targets_);
        targets_note_ = new QLabel(p);
        targets_note_->setObjectName(QStringLiteral("erNote"));
        targets_note_->setWordWrap(true);
        body->addWidget(targets_note_);
        vl->addWidget(p);
    }
    struct PanelDef {
        const char* title;
        std::initializer_list<const char*> keys;
    };
    const PanelDef defs[] = {
        {QT_TR_NOOP("TRADING"),
         {QT_TR_NOOP("Open"), QT_TR_NOOP("Day high"), QT_TR_NOOP("Day low"), QT_TR_NOOP("Previous close"),
          QT_TR_NOOP("Volume"), QT_TR_NOOP("Avg volume")}},
        {QT_TR_NOOP("VALUATION"),
         {QT_TR_NOOP("Market cap"), QT_TR_NOOP("Enterprise value"), QT_TR_NOOP("P/E (TTM)"), QT_TR_NOOP("Forward P/E"),
          QT_TR_NOOP("PEG"), QT_TR_NOOP("Price / book"), QT_TR_NOOP("EV / EBITDA"), QT_TR_NOOP("EV / revenue"),
          QT_TR_NOOP("Dividend yield"), QT_TR_NOOP("Beta")}},
        {QT_TR_NOOP("SHARES"),
         {QT_TR_NOOP("Shares outstanding"), QT_TR_NOOP("Float"), QT_TR_NOOP("Insiders"), QT_TR_NOOP("Institutions"),
          QT_TR_NOOP("Short % of float"), QT_TR_NOOP("Short ratio")}},
        {QT_TR_NOOP("PROFITABILITY & GROWTH"),
         {QT_TR_NOOP("Gross margin"), QT_TR_NOOP("Operating margin"), QT_TR_NOOP("Net margin"), QT_TR_NOOP("ROA"),
          QT_TR_NOOP("ROE"), QT_TR_NOOP("Revenue growth (qtr YoY)"), QT_TR_NOOP("Earnings growth (qtr YoY)")}},
        {QT_TR_NOOP("FINANCIAL HEALTH"),
         {QT_TR_NOOP("Cash"), QT_TR_NOOP("Debt"), QT_TR_NOOP("Net cash"), QT_TR_NOOP("Free cash flow (TTM)"),
          QT_TR_NOOP("Operating cash flow (TTM)"), QT_TR_NOOP("Revenue (TTM)")}},
    };
    for (const auto& d : defs) {
        QVBoxLayout* body = nullptr;
        QLabel* title = nullptr;
        auto* p = er_ui::panel(tr(d.title), &body, &title, w);
        i18n_.insert(title, d.title);
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(2);
        int row = 0;
        for (const char* k : d.keys)
            add_stat(grid, row++, k);
        body->addLayout(grid);
        vl->addWidget(p);
    }
    vl->addStretch();
    return w;
}

// ═════════════════════════════════════════════════════════════════════════════
// Data
// ═════════════════════════════════════════════════════════════════════════════

void EquityOverviewTab::on_quote_loaded(services::equity::QuoteData q) {
    if (q.symbol != current_symbol_)
        return;
    quote_ = q;
    quote_loaded_ = true;
    leg_errors_.remove(QStringLiteral("Quote"));
    if (info_loaded_ && history_loaded_ && loading_overlay_)
        loading_overlay_->hide_loading();
    render_quote();
    render_ranges();
}

void EquityOverviewTab::on_info_loaded(services::equity::StockInfo info) {
    if (info.symbol != current_symbol_)
        return;
    info_ = info;
    info_loaded_ = true;
    leg_errors_.remove(QStringLiteral("Info"));
    if (quote_loaded_ && history_loaded_ && loading_overlay_)
        loading_overlay_->hide_loading();
    equity_fmt::listing_of(info_).ensure_fx();
    render_company();
    render_info();
    render_quote();
    render_ranges();
    schedule_rebuild(); // price formatting follows the currency
}

void EquityOverviewTab::schedule_rebuild() {
    if (rebuild_pending_)
        return;
    rebuild_pending_ = true;
    QTimer::singleShot(60, this, [this]() {
        rebuild_pending_ = false;
        rebuild_chart();
    });
}

void EquityOverviewTab::on_events_loaded(services::equity::EquityEvents events) {
    if (events.symbol != current_symbol_)
        return;
    events_ = events;
    events_loaded_ = true;
    render_company();
    if (show_events_)
        schedule_rebuild();
}

void EquityOverviewTab::on_historical_loaded(QString symbol, QString period, QVector<services::equity::Candle> candles) {
    if (period != QLatin1String(kOvDailyPeriod) || candles.isEmpty())
        return;
    if (symbol == current_symbol_) {
        daily_ = ov_tm::normalize_daily(candles);
        history_loaded_ = true;
        leg_errors_.remove(QStringLiteral("Historical"));
        if (info_loaded_ && quote_loaded_ && loading_overlay_)
            loading_overlay_->hide_loading();
        render_ranges();
        render_returns();
        if (intraday_preset_.isEmpty())
            schedule_rebuild();
        return;
    }
    const auto bench = er_ui::benchmark_for(current_symbol_);
    if (symbol == bench.symbol || compare_symbols_.contains(symbol)) {
        compare_data_.insert(symbol, ov_tm::normalize_daily(candles));
        if (symbol == bench.symbol)
            render_returns();
        if (compare_symbols_.contains(symbol))
            schedule_rebuild();
    }
}

void EquityOverviewTab::on_load_failed(const services::equity::LoadFailure& f) {
    if (f.symbol != current_symbol_)
        return;
    const bool relevant = f.leg == QLatin1String("Quote") || f.leg == QLatin1String("Info") ||
                          (f.leg == QLatin1String("Historical") && f.period == QLatin1String(kOvDailyPeriod));
    if (!relevant)
        return;
    // A refresh of data already on screen (the header's 30 s quote poll) keeps what is shown.
    if ((f.leg == QLatin1String("Quote") && quote_loaded_) || (f.leg == QLatin1String("Info") && info_loaded_) ||
        (f.leg == QLatin1String("Historical") && history_loaded_))
        return;
    leg_errors_.insert(f.leg, f.message);
    if (loading_overlay_)
        loading_overlay_->hide_loading();
    if (f.leg == QLatin1String("Info"))
        render_company(); // drops the venue guessed for a symbol Yahoo does not know
    QStringList parts;
    for (auto it = leg_errors_.cbegin(); it != leg_errors_.cend(); ++it) {
        const QString leg = it.key() == QLatin1String("Quote")  ? tr("Quote")
                            : it.key() == QLatin1String("Info") ? tr("Company info")
                                                                : tr("Price history");
        parts << tr("%1: %2").arg(leg, it.value().left(160));
    }
    status_->show_error(tr("Some data could not be loaded — %1").arg(parts.join(QStringLiteral(" · "))),
                        parts.join(QLatin1Char('\n')));
    if (f.leg == QLatin1String("Historical") && chart_)
        chart_->set_state(ui::TimeSeriesChart::State::Error, f.message.left(200));
}

QString EquityOverviewTab::fmt_quote_price(double v) const {
    return equity_fmt::price(v, info_.currency);
}

// ── Rendering ────────────────────────────────────────────────────────────────

void EquityOverviewTab::render_company() {
    const auto& s = info_;
    name_lbl_->setText(s.company_name.isEmpty() || s.company_name == QLatin1String("N/A")
                           ? current_symbol_
                           : QStringLiteral("%1  ·  %2").arg(s.company_name, current_symbol_));
    auto clean = [](const QString& x) { return (x.isEmpty() || x == QLatin1String("N/A")) ? QString() : x; };
    QStringList meta;
    QString exch = clean(events_.full_exchange_name);
    if (exch.isEmpty())
        exch = clean(s.exchange);
    // The venue is only guessed from the ticker's form; for a symbol Yahoo does not know
    // ("ZZQXW") it read "New York Stock Exchange" under a "Symbol not found" error.
    if (const auto* venue = core::market::venue_for_symbol(current_symbol_, s.exchange);
        venue && exch.isEmpty() && !leg_errors_.contains(QStringLiteral("Info")))
        exch = core::market::venue_display_name(*venue);
    if (!exch.isEmpty())
        meta << exch;
    if (!clean(s.country).isEmpty())
        meta << clean(s.country);
    if (!clean(s.sector).isEmpty())
        meta << (clean(s.industry).isEmpty() ? clean(s.sector) : tr("%1 › %2").arg(clean(s.sector), clean(s.industry)));
    if (s.employees > 0)
        meta << tr("%1 employees").arg(ov_uf::number(s.employees, 0));
    if (!clean(s.website).isEmpty())
        meta << clean(s.website);
    meta_lbl_->setText(meta.join(QStringLiteral("  ·  ")));

    const auto listing = equity_fmt::listing_of(s);
    QString ccy = listing.describe();
    if (!listing.same_currency() && !listing.quote.isEmpty()) {
        const auto fx = listing.fx_reporting_to_quote();
        if (fx)
            ccy += tr(" · 1 %1 = %2 %3").arg(listing.reporting_major(), ov_uf::number(*fx, 4), listing.quote_major());
    }
    ccy_lbl_->setText(ccy);

    QStringList dates;
    const QLocale loc;
    if (events_.next_earnings.isValid()) {
        // Yahoo keeps the last report date in its "next" field until the company
        // announces the following one (VOD.L on Nov 16 showed "Next earnings: Nov 10").
        const QDate last_day = events_.next_earnings_end.isValid() ? events_.next_earnings_end : events_.next_earnings;
        const bool past = last_day < QDate::currentDate();
        QString e = ov_uf::calendar_date(events_.next_earnings);
        if (!past && events_.next_earnings_end.isValid() && events_.next_earnings_end != events_.next_earnings)
            e += QStringLiteral(" – ") + ov_uf::calendar_date(events_.next_earnings_end);
        if (!past && ov_ok(events_.est_eps_avg))
            e += tr(" (EPS est. %1)").arg(ov_uf::number(events_.est_eps_avg, 2));
        dates << (past ? tr("Last earnings: %1") : tr("Next earnings: %1")).arg(e);
    }
    if (events_.ex_dividend_date.isValid()) {
        const bool past = events_.ex_dividend_date < QDate::currentDate();
        dates << (past ? tr("Last ex-dividend: %1") : tr("Ex-dividend: %1")).arg(ov_uf::calendar_date(events_.ex_dividend_date));
    }
    if (events_.dividend_pay_date.isValid() && events_.dividend_pay_date >= QDate::currentDate())
        dates << tr("Pay date: %1").arg(ov_uf::calendar_date(events_.dividend_pay_date));
    if (ov_ok(events_.dividend_rate) && events_.dividend_rate > 0.0) {
        // Yahoo's dividendRate is in the QUOTE's major currency (VOD.L 0.04 = £0.04: 3.4 % of
        // 117.8p; TSM 3.77 = US$ per ADR), not the reporting currency — it read "€0.0400".
        const QString dccy = listing.quote_major().isEmpty() ? listing.reporting_major() : listing.quote_major();
        dates << tr("Dividend %1 / yr").arg(equity_fmt::price(events_.dividend_rate, dccy));
    }
    if (events_.last_fiscal_year_end.isValid())
        dates << tr("Fiscal year ends %1").arg(loc.monthName(events_.last_fiscal_year_end.month()));
    dates_lbl_->setText(dates.join(QStringLiteral("  ·  ")));
    dates_lbl_->setVisible(!dates.isEmpty());

    const auto rec = equity_fmt::recommendation(s.recommendation_key);
    rec_badge_->setVisible(!rec.text.isEmpty());
    if (!rec.text.isEmpty()) {
        rec_badge_->setText(s.analyst_count > 0 ? tr("%1 · %n analyst(s)", "", s.analyst_count).arg(rec.text) : rec.text);
        // An analyst view is an opinion state, not a price move: status colours.
        const QString c = rec.tone > 0 ? ui::colors::SUCCESS() : rec.tone < 0 ? ui::colors::DANGER() : ui::colors::WARNING();
        rec_badge_->setStyleSheet(QStringLiteral("color:%1; border-color:%1;").arg(c));
    }
    desc_lbl_->setText(s.description == QLatin1String("N/A") ? QString() : s.description);
}

void EquityOverviewTab::render_quote() {
    auto set = [this](const char* key, const QString& text) {
        if (auto* l = stats_.value(QString::fromLatin1(key)))
            l->setText(text);
    };
    set("Open", fmt_quote_price(ov_pos(quote_.open)));
    set("Day high", fmt_quote_price(ov_pos(quote_.high)));
    set("Day low", fmt_quote_price(ov_pos(quote_.low)));
    set("Previous close", fmt_quote_price(ov_pos(quote_.prev_close)));
    set("Volume", quote_.volume > 0 ? ov_uf::volume(quote_.volume) : equity_fmt::dash());
}

void EquityOverviewTab::render_info() {
    const auto& s = info_;
    auto set = [this](const char* key, const QString& text) {
        if (auto* l = stats_.value(QString::fromLatin1(key)))
            l->setText(text);
    };
    const QString quote_ccy = s.currency;
    const QString rep_ccy = s.financial_currency.isEmpty() ? s.currency : s.financial_currency;
    set("Avg volume", s.avg_volume > 0 ? ov_uf::volume(s.avg_volume) : equity_fmt::dash());
    // Market cap and EV are in the quote's MAJOR currency (pounds for a pence quote).
    set("Market cap", equity_fmt::money(ov_pos(s.market_cap), quote_ccy));
    set("Enterprise value", equity_fmt::money(ov_nz(s.enterprise_value), quote_ccy));
    set("P/E (TTM)", equity_fmt::multiple(s.pe_ratio, 1));
    set("Forward P/E", equity_fmt::multiple(s.forward_pe, 1));
    set("PEG", equity_fmt::multiple(s.peg_ratio, 2));
    set("Price / book", equity_fmt::multiple(s.price_to_book, 2));
    set("EV / EBITDA", equity_fmt::multiple(s.ev_to_ebitda, 1));
    set("EV / revenue", equity_fmt::multiple(s.ev_to_revenue, 2));
    // yfinance 0.2.66 reports dividendYield in percent (0.33 = 0.33 %).
    set("Dividend yield", s.dividend_yield > 0 ? ov_uf::pct(s.dividend_yield, 2, false) : equity_fmt::dash());
    set("Beta", s.beta != 0.0 ? ov_uf::number(s.beta, 2) : equity_fmt::dash());
    set("Shares outstanding", equity_fmt::large(ov_pos(s.shares_outstanding)));
    set("Float", equity_fmt::large(ov_pos(s.float_shares)));
    set("Insiders", equity_fmt::pct(ov_nz(s.held_insiders_pct), 2));
    set("Institutions", equity_fmt::pct(ov_nz(s.held_institutions_pct), 2));
    set("Short % of float", equity_fmt::pct(ov_nz(s.short_pct_of_float), 2));
    set("Short ratio", s.short_ratio > 0 ? tr("%1 days").arg(ov_uf::number(s.short_ratio, 1)) : equity_fmt::dash());
    set("Gross margin", equity_fmt::pct(ov_nz(s.gross_margins), 1));
    set("Operating margin", equity_fmt::pct(ov_nz(s.operating_margins), 1));
    set("Net margin", equity_fmt::pct(ov_nz(s.profit_margins), 1));
    set("ROA", equity_fmt::pct(ov_nz(s.roa), 1));
    set("ROE", equity_fmt::pct(ov_nz(s.roe), 1));
    set("Revenue growth (qtr YoY)", equity_fmt::pct(ov_nz(s.revenue_growth), 1, true));
    set("Earnings growth (qtr YoY)", equity_fmt::pct(ov_nz(s.earnings_growth), 1, true));
    // Statement figures are in the REPORTING currency.
    set("Cash", equity_fmt::money(ov_pos(s.total_cash), rep_ccy));
    set("Debt", equity_fmt::money(ov_pos(s.total_debt), rep_ccy));
    const double net = (s.total_cash > 0 || s.total_debt > 0) ? s.total_cash - s.total_debt : std::nan("");
    set("Net cash", equity_fmt::signed_money(net, rep_ccy));
    set("Free cash flow (TTM)", equity_fmt::money(ov_nz(s.free_cashflow), rep_ccy));
    set("Operating cash flow (TTM)", equity_fmt::money(ov_nz(s.operating_cashflow), rep_ccy));
    set("Revenue (TTM)", equity_fmt::money(ov_pos(s.total_revenue), rep_ccy));
    if (auto* l = stats_.value(QStringLiteral("Market cap")))
        l->setToolTip(tr("In %1 (the quote's major currency)").arg(equity_fmt::listing_of(s).quote_major()));
    for (const char* k : {"Cash", "Debt", "Net cash", "Free cash flow (TTM)", "Operating cash flow (TTM)", "Revenue (TTM)"})
        if (auto* l = stats_.value(QString::fromLatin1(k)))
            l->setToolTip(tr("Reporting currency: %1").arg(rep_ccy));
}

void EquityOverviewTab::render_ranges() {
    const double price = quote_.price > 0 ? quote_.price : info_.current_price;
    const auto fmt = [this](double v) { return fmt_quote_price(v); };
    range52_->set_value_formatter(fmt);
    targets_->set_value_formatter(fmt);
    double lo = ov_pos(info_.week52_low), hi = ov_pos(info_.week52_high);
    if ((!ov_ok(lo) || !ov_ok(hi)) && !daily_.isEmpty()) {
        const qint64 from = daily_.last().timestamp - 365LL * 86400;
        for (const auto& c : daily_)
            if (c.timestamp >= from) {
                lo = ov_ok(lo) ? std::min(lo, c.low) : c.low;
                hi = ov_ok(hi) ? std::max(hi, c.high) : c.high;
            }
    }
    if (ov_ok(lo) && ov_ok(hi) && hi > lo) {
        ui::RangeBarData d;
        d.low = std::min(lo, price > 0 ? price : lo);
        d.high = std::max(hi, price > 0 ? price : hi);
        if (price > 0)
            d.markers.append({price, tr("Last"), ui::RangeMarker::Role::Current});
        if (!daily_.isEmpty()) {
            const auto b = ov_tm::bars_from_candles(daily_);
            const auto s50 = ov_tm::sma(b.close, 50), s200 = ov_tm::sma(b.close, 200);
            if (!s50.isEmpty() && ov_ok(s50.last()))
                d.markers.append({s50.last(), tr("50-day"), ui::RangeMarker::Role::Neutral});
            if (!s200.isEmpty() && ov_ok(s200.last()))
                d.markers.append({s200.last(), tr("200-day"), ui::RangeMarker::Role::Mean});
        }
        range52_->set_data(d);
        range52_->set_state(ui::PaintedChart::Ready);
        const double pos = price > 0 ? (price - lo) / (hi - lo) : std::nan("");
        range52_->setToolTip(ov_ok(pos) ? tr("At %1 of the 52-week range").arg(ov_uf::fraction_pct(std::clamp(pos, 0.0, 1.0), 0, false))
                                        : QString());
    } else {
        range52_->set_data({});
        range52_->set_state(ui::PaintedChart::Empty, tr("No 52-week range"));
    }

    const double tl = ov_pos(info_.target_low), th = ov_pos(info_.target_high), tm = ov_pos(info_.target_mean);
    if (ov_ok(tl) && ov_ok(th) && th >= tl && info_.analyst_count > 0) {
        ui::RangeBarData d;
        d.low = std::min(tl, price > 0 ? price : tl);
        d.high = std::max(th, price > 0 ? price : th);
        d.fill_to_current = false;
        d.markers.append({tl, tr("Low"), ui::RangeMarker::Role::Low});
        d.markers.append({th, tr("High"), ui::RangeMarker::Role::High});
        if (ov_ok(tm))
            d.markers.append({tm, tr("Mean"), ui::RangeMarker::Role::Target});
        if (price > 0)
            d.markers.append({price, tr("Last"), ui::RangeMarker::Role::Current});
        targets_->set_data(d);
        targets_->set_state(ui::PaintedChart::Ready);
        targets_note_->setText(price > 0 && ov_ok(tm)
                                   ? tr("Mean target %1 — %2 vs last · %n analyst(s)", "", info_.analyst_count)
                                         .arg(fmt_quote_price(tm), ov_uf::fraction_pct(tm / price - 1.0, 1, true))
                                   : tr("%n analyst(s)", "", info_.analyst_count));
    } else {
        targets_->set_data({});
        targets_->set_state(ui::PaintedChart::Empty, tr("No analyst targets"));
        targets_note_->clear();
    }
}

void EquityOverviewTab::render_returns() {
    returns_->clear();
    if (daily_.size() < 2)
        return;
    const auto b = ov_tm::bars_from_candles(daily_);
    const auto bench = er_ui::benchmark_for(current_symbol_);
    const auto bench_it = compare_data_.constFind(bench.symbol);
    const bool has_bench = bench_it != compare_data_.constEnd() && bench_it->size() > 1;
    const ov_tm::Bars bb = has_bench ? ov_tm::bars_from_candles(*bench_it) : ov_tm::Bars{};
    const QDate last = QDateTime::fromSecsSinceEpoch(b.time.last(), QTimeZone::UTC).date();
    struct Win {
        const char* label;
        QDate from;
    };
    const Win wins[] = {
        {QT_TR_NOOP("1W"), last.addDays(-7)},     {QT_TR_NOOP("1M"), last.addMonths(-1)},
        {QT_TR_NOOP("3M"), last.addMonths(-3)},   {QT_TR_NOOP("6M"), last.addMonths(-6)},
        {QT_TR_NOOP("YTD"), QDate(last.year() - 1, 12, 31)}, {QT_TR_NOOP("1Y"), last.addYears(-1)},
        {QT_TR_NOOP("3Y"), last.addYears(-3)},    {QT_TR_NOOP("5Y"), last.addYears(-5)},
    };
    for (const auto& w : wins) {
        const qint64 from = QDateTime(w.from, QTime(23, 59), QTimeZone::UTC).toSecsSinceEpoch();
        const double r = ov_tm::return_since(b, from);
        if (!ov_ok(r))
            continue;
        auto* t = returns_->add_tile(tr(w.label));
        t->set_compact(true);
        t->set_value(ov_uf::fraction_pct(r, 1, true));
        t->set_tone(r > 0 ? ui::StatTile::Tone::Positive : r < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral);
        // Price returns (not total return): dividends are not reinvested.
        QString tip = tr("Price return since %1 (dividends excluded)").arg(ov_uf::calendar_date(w.from));
        if (has_bench) {
            const double br = ov_tm::return_since(bb, from);
            if (ov_ok(br)) {
                // Keep the delta short: the tile draws it at full width on the value row and
                // elides the return itself to fit, so "+1.6 pp vs S&P 500" left only "…".
                const QString rel = ov_uf::pp((r - br) * 100.0, 1);
                t->set_delta((r - br) * 100.0, rel);
                t->set_sub_label(tr("%1 %2").arg(bench.name, ov_uf::fraction_pct(br, 1, true)));
                tip += QLatin1Char('\n') + tr("%1 vs %2").arg(rel, bench.name);
            }
        }
        t->set_tooltip_text(tip);
    }
}

// ── Chart ────────────────────────────────────────────────────────────────────

void EquityOverviewTab::rebuild_chart() {
    if (!chart_stack_)
        return;
    // A fresh chart per rebuild: panes cannot be removed from a TimeSeriesChart, and the
    // indicator set decides which panes exist.
    if (chart_) {
        chart_stack_->removeWidget(chart_);
        chart_->deleteLater();
        chart_ = nullptr;
    }
    chart_ = new ui::TimeSeriesChart(chart_stack_);
    chart_stack_->insertWidget(0, chart_);
    chart_->set_transparent_background(true);
    chart_->set_show_legend(true);
    chart_->set_show_mode_buttons(true);
    QStringList presets;
    for (const char* p : kOvPresets)
        presets << QString::fromLatin1(p);
    chart_->set_range_presets(presets);
    connect(chart_, &ui::TimeSeriesChart::range_preset_selected, this, &EquityOverviewTab::on_preset);
    chart_->set_export_name(current_symbol_);

    const bool intraday = !intraday_preset_.isEmpty();
    const QVector<services::equity::Candle>& src = intraday ? intraday_ : daily_;
    const auto* venue = core::market::venue_for_symbol(current_symbol_, info_.exchange);
    const QTimeZone tz = venue ? venue->tz : QTimeZone();
    chart_->set_utc(!intraday);
    if (intraday) {
        const qint64 span = intraday_preset_ == QLatin1String("1D") ? 86400 : 5 * 86400;
        chart_->set_time_formatter([tz](double x) {
            return ov_uf::date_time(static_cast<qint64>(x / 1000.0), ov_uf::DateStyle::Medium, false, tz);
        });
        chart_->set_tick_time_formatter([tz, span](double x) {
            return ov_uf::axis_label(static_cast<qint64>(x / 1000.0), span, tz);
        });
    } else {
        chart_->set_time_formatter([](double x) { return ov_uf::bar_date(static_cast<qint64>(x / 1000.0)); });
    }
    const QString ccy = info_.currency;
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                [ccy](double v) { return equity_fmt::price(v, ccy); });

    if (src.isEmpty()) {
        chart_->set_state(history_loaded_ ? ui::TimeSeriesChart::State::Empty : ui::TimeSeriesChart::State::Loading,
                          history_loaded_ ? tr("No price history") : tr("Loading price history…"));
        candles_->clear();
        chart_stack_->setCurrentWidget(chart_);
        return;
    }

    const auto b = ov_tm::bars_from_candles(src);
    QVector<double> xs(b.size());
    for (int i = 0; i < b.size(); ++i)
        xs[i] = static_cast<double>(b.time[i]) * 1000.0;
    const bool comparing = !compare_symbols_.isEmpty();

    ui::TimeSeriesChart::SeriesSpec price;
    price.name = current_symbol_;
    price.id = QStringLiteral("price");
    price.xs = xs;
    price.ys = b.close;
    price.style = comparing ? ui::TimeSeriesChart::Style::Line : ui::TimeSeriesChart::Style::Area;
    price.color = ui::ChartColorRole::Accent;
    price.formatter = [ccy](double v) { return equity_fmt::price(v, ccy); };
    const int price_id = chart_->add_series(price);

    if (!comparing) {
        auto overlay = [&](const QString& name, const QVector<double>& ys, int palette, bool dashed = false) {
            ui::TimeSeriesChart::SeriesSpec s;
            s.name = name;
            s.xs = xs;
            s.ys = ys;
            s.color = ui::ChartColorRole::Palette;
            s.palette_index = palette;
            s.line_width = 1.1;
            s.dashed = dashed;
            s.formatter = price.formatter;
            chart_->add_series(s);
        };
        if (indicators_.contains(QStringLiteral("sma20")))
            overlay(tr("SMA 20"), ov_tm::sma(b.close, 20), 1);
        if (indicators_.contains(QStringLiteral("sma50")))
            overlay(tr("SMA 50"), ov_tm::sma(b.close, 50), 2);
        if (indicators_.contains(QStringLiteral("sma200")))
            overlay(tr("SMA 200"), ov_tm::sma(b.close, 200), 3);
        if (indicators_.contains(QStringLiteral("ema20")))
            overlay(tr("EMA 20"), ov_tm::ema(b.close, 20), 4);
        if (indicators_.contains(QStringLiteral("bb"))) {
            const auto bb = ov_tm::bollinger(b.close, 20, 2.0);
            overlay(tr("BB upper"), bb.upper, 5, true);
            overlay(tr("BB lower"), bb.lower, 5, true);
        }
    }
    if (indicators_.contains(QStringLiteral("volume"))) {
        const int pane = chart_->add_pane(0.22);
        chart_->set_pane_title(pane, tr("Volume"));
        ui::TimeSeriesChart::SeriesSpec v;
        v.name = tr("Volume");
        v.xs = xs;
        v.ys = b.volume;
        v.style = ui::TimeSeriesChart::Style::Bars;
        v.pane = pane;
        v.color = ui::ChartColorRole::Muted;
        v.rebase = false;
        v.formatter = [](double x) { return ov_uf::volume(x); };
        chart_->add_series(v);
        chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double x) { return ov_uf::volume(x); }, pane);
    }
    if (indicators_.contains(QStringLiteral("rsi"))) {
        const int pane = chart_->add_pane(0.28);
        chart_->set_pane_title(pane, tr("RSI (14)"));
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("RSI (14)");
        s.xs = xs;
        s.ys = ov_tm::rsi(b.close, 14);
        s.pane = pane;
        s.rebase = false;
        s.color = ui::ChartColorRole::Info;
        chart_->add_series(s);
        chart_->set_y_range(pane, ui::TimeSeriesChart::Axis::Left, 0, 100);
        chart_->add_hline(70, tr("70"), ui::ChartColorRole::Muted, pane);
        chart_->add_hline(30, tr("30"), ui::ChartColorRole::Muted, pane);
    }
    if (indicators_.contains(QStringLiteral("macd"))) {
        const int pane = chart_->add_pane(0.3);
        chart_->set_pane_title(pane, tr("MACD (12, 26, 9)"));
        const auto m = ov_tm::macd(b.close, 12, 26, 9);
        ui::TimeSeriesChart::SeriesSpec h;
        h.name = tr("Histogram");
        h.xs = xs;
        h.ys = m.hist;
        h.pane = pane;
        h.style = ui::TimeSeriesChart::Style::Bars;
        h.color_by_sign = true;
        h.rebase = false;
        chart_->add_series(h);
        ui::TimeSeriesChart::SeriesSpec l;
        l.name = tr("MACD");
        l.xs = xs;
        l.ys = m.line;
        l.pane = pane;
        l.rebase = false;
        l.color = ui::ChartColorRole::Info;
        chart_->add_series(l);
        ui::TimeSeriesChart::SeriesSpec sg;
        sg.name = tr("Signal");
        sg.xs = xs;
        sg.ys = m.signal;
        sg.pane = pane;
        sg.rebase = false;
        sg.color = ui::ChartColorRole::Warning;
        sg.dashed = true;
        chart_->add_series(sg);
    }

    // Comparison series, rebased to the first visible point.
    if (comparing && !intraday) {
        int palette = 6;
        for (const auto& sym : compare_symbols_) {
            const auto it = compare_data_.constFind(sym);
            if (it == compare_data_.constEnd())
                continue;
            ui::TimeSeriesChart::SeriesSpec s;
            s.name = sym;
            s.color = ui::ChartColorRole::Palette;
            s.palette_index = palette++;
            for (const auto& c : *it)
                s.points << QPointF(static_cast<double>(c.timestamp) * 1000.0, c.close);
            chart_->add_series(s);
        }
        chart_->set_value_mode(ui::TimeSeriesChart::ValueMode::Percent);
    }

    // Earnings / dividend / split markers on the price line.
    if (show_events_ && events_loaded_ && !intraday) {
        const double x0 = xs.first(), x1 = xs.last();
        auto ms = [](const QDate& d) {
            return static_cast<double>(QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch());
        };
        const QString qccy = events_.currency.isEmpty() ? ccy : events_.currency;
        for (const auto& d : events_.dividends) {
            const double x = ms(d.date);
            if (x >= x0 && x <= x1)
                chart_->add_marker(x, price_id, ui::ChartMarkerKind::Dividend, QString(),
                                   tr("Dividend %1 · ex-date %2").arg(equity_fmt::price(d.amount, qccy), ov_uf::calendar_date(d.date)));
        }
        for (const auto& sp : events_.splits) {
            const double x = ms(sp.date);
            if (x < x0 || x > x1)
                continue;
            const QString ratio = sp.ratio >= 1.0 ? tr("%1-for-1 split").arg(ov_uf::number(sp.ratio, sp.ratio == std::floor(sp.ratio) ? 0 : 2))
                                                  : tr("1-for-%1 reverse split").arg(ov_uf::number(1.0 / sp.ratio, 2));
            chart_->add_marker(x, price_id, ui::ChartMarkerKind::Split, QString(),
                               tr("%1 · %2").arg(ratio, ov_uf::calendar_date(sp.date)));
        }
        for (const auto& e : events_.earnings) {
            if (!e.report_date.isValid())
                continue;
            const double x = ms(e.report_date);
            if (x < x0 || x > x1)
                continue;
            QString tip = tr("Earnings %1").arg(ov_uf::calendar_date(e.report_date));
            if (e.date_estimated)
                tip += tr(" (date estimated from the trading volume)");
            if (ov_ok(e.eps_actual))
                tip += tr("\nEPS %1").arg(ov_uf::number(e.eps_actual, 2));
            if (ov_ok(e.eps_estimate))
                tip += tr(" vs %1 est.").arg(ov_uf::number(e.eps_estimate, 2));
            if (ov_ok(e.surprise_pct))
                tip += tr(" (%1)").arg(ov_uf::pct(e.surprise_pct, 1, true));
            chart_->add_marker(x, price_id, ui::ChartMarkerKind::Earnings, QString(), tip);
        }
    }

    // Candle view of the same bars.
    QVector<ui::CandleChart::Bar> cbars;
    cbars.reserve(b.size());
    for (int i = 0; i < b.size(); ++i)
        cbars.append({xs[i], b.open[i], b.high[i], b.low[i], b.close[i], b.volume[i]});
    candles_->set_bars(cbars);
    candles_->set_utc(!intraday);
    candles_->set_price_formatter([ccy](double v) { return equity_fmt::price(v, ccy); });
    candles_->set_volume_formatter([](double v) { return ov_uf::volume(v); });
    candles_->clear_overlays();
    if (indicators_.contains(QStringLiteral("sma50")))
        candles_->add_overlay_series(tr("SMA 50"), ov_tm::sma(b.close, 50));
    if (indicators_.contains(QStringLiteral("sma200")))
        candles_->add_overlay_series(tr("SMA 200"), ov_tm::sma(b.close, 200));
    candles_->set_default_visible_bars(intraday ? b.size() : ov_candle_bars(pending_preset_));
    candles_->reset_view();
    candles_->set_title(current_symbol_);

    chart_->set_active_preset(intraday ? intraday_preset_ : pending_preset_);
    chart_stack_->setCurrentWidget(candles_mode_ && !comparing ? static_cast<QWidget*>(candles_) : chart_);
}

void EquityOverviewTab::on_preset(const QString& preset) {
    if (preset == QLatin1String("1D") || preset == QLatin1String("5D")) {
        load_intraday(preset);
        return;
    }
    pending_preset_ = preset;
    if (!intraday_preset_.isEmpty()) {
        intraday_preset_.clear(); // back to the daily history
        rebuild_chart();
    } else if (candles_mode_) {
        rebuild_chart();
    }
}

void EquityOverviewTab::load_intraday(const QString& preset) {
    if (current_symbol_.isEmpty())
        return;
    const QString sym = current_symbol_;
    const QString period = preset == QLatin1String("1D") ? QStringLiteral("1d") : QStringLiteral("5d");
    const QString interval = preset == QLatin1String("1D") ? QStringLiteral("5m") : QStringLiteral("15m");
    if (chart_)
        chart_->set_state(ui::TimeSeriesChart::State::Loading, tr("Loading intraday bars…"));
    QPointer<EquityOverviewTab> self = this;
    services::equity::EquityResearchService::instance().fetch_price_history(
        sym, period, interval, [self, sym, preset](bool ok, QVector<services::equity::Candle> candles, QString error) {
            QMetaObject::invokeMethod(
                qApp,
                [self, sym, preset, ok, candles, error]() {
                    if (!self || self->current_symbol_ != sym)
                        return;
                    if (!ok || candles.isEmpty()) {
                        if (self->chart_)
                            self->chart_->set_state(ui::TimeSeriesChart::State::Error,
                                                    tr("No intraday bars: %1").arg(error.left(160)));
                        return;
                    }
                    self->intraday_ = candles;
                    self->intraday_preset_ = preset;
                    self->rebuild_chart();
                },
                Qt::QueuedConnection);
        });
}

void EquityOverviewTab::show_indicator_menu() {
    QMenu menu(this);
    struct Item {
        const char* key;
        const char* text;
    };
    const Item items[] = {
        {"sma20", QT_TR_NOOP("SMA 20")},   {"sma50", QT_TR_NOOP("SMA 50")},   {"sma200", QT_TR_NOOP("SMA 200")},
        {"ema20", QT_TR_NOOP("EMA 20")},   {"bb", QT_TR_NOOP("Bollinger Bands (20, 2)")},
        {"volume", QT_TR_NOOP("Volume pane")}, {"rsi", QT_TR_NOOP("RSI (14) pane")},
        {"macd", QT_TR_NOOP("MACD (12, 26, 9) pane")},
    };
    for (const auto& it : items) {
        auto* a = menu.addAction(tr(it.text));
        a->setCheckable(true);
        const QString key = QString::fromLatin1(it.key);
        a->setChecked(indicators_.contains(key));
        connect(a, &QAction::toggled, this, [this, key](bool on) {
            if (on)
                indicators_.insert(key);
            else
                indicators_.remove(key);
            rebuild_chart();
        });
        if (QLatin1String(it.key) == QLatin1String("bb"))
            menu.addSeparator();
    }
    menu.exec(indicators_btn_->mapToGlobal(QPoint(0, indicators_btn_->height())));
}

void EquityOverviewTab::add_comparison(const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty() || sym == current_symbol_ || compare_symbols_.contains(sym))
        return;
    compare_symbols_ << sym;
    if (compare_data_.contains(sym))
        rebuild_chart();
    else
        services::equity::EquityResearchService::instance().load_historical_only(sym, QString::fromLatin1(kOvDailyPeriod));
}

void EquityOverviewTab::show_compare_menu() {
    QMenu menu(this);
    const auto bench = er_ui::benchmark_for(current_symbol_);
    if (!bench.symbol.isEmpty()) {
        auto* a = menu.addAction(tr("Benchmark: %1 (%2)").arg(bench.name, bench.symbol));
        a->setEnabled(!compare_symbols_.contains(bench.symbol));
        connect(a, &QAction::triggered, this, [this, s = bench.symbol]() { add_comparison(s); });
    }
    connect(menu.addAction(tr("Add symbol…")), &QAction::triggered, this, [this]() {
        bool ok = false;
        const QString s = QInputDialog::getText(this, tr("Compare"), tr("Symbol to compare (Yahoo form, e.g. MSFT, BT-A.L):"),
                                                QLineEdit::Normal, QString(), &ok);
        if (ok)
            add_comparison(s);
    });
    if (!compare_symbols_.isEmpty()) {
        menu.addSeparator();
        for (const auto& s : compare_symbols_)
            connect(menu.addAction(tr("Remove %1").arg(s)), &QAction::triggered, this, [this, s]() {
                compare_symbols_.removeAll(s);
                rebuild_chart();
            });
        connect(menu.addAction(tr("Clear comparisons")), &QAction::triggered, this, [this]() {
            compare_symbols_.clear();
            rebuild_chart();
        });
    }
    menu.exec(compare_btn_->mapToGlobal(QPoint(0, compare_btn_->height())));
}

// ═════════════════════════════════════════════════════════════════════════════
// i18n
// ═════════════════════════════════════════════════════════════════════════════

void EquityOverviewTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityOverviewTab::retranslateUi() {
    for (auto it = i18n_.constBegin(); it != i18n_.constEnd(); ++it)
        it.key()->setText(tr(it.value()));
    style_seg_->set_text(QStringLiteral("line"), tr("LINE"), tr("Close price with overlays, panes and comparisons"));
    style_seg_->set_text(QStringLiteral("candles"), tr("CANDLES"), tr("OHLC candlesticks with volume"));
    indicators_btn_->setText(tr("INDICATORS ▾"));
    compare_btn_->setText(tr("COMPARE ▾"));
    compare_btn_->setToolTip(tr("Overlay the home benchmark or another symbol, rebased to the first visible day"));
    events_btn_->setText(tr("EVENTS"));
    events_btn_->setToolTip(tr("Earnings (E), dividend (D) and split (S) markers"));
    if (range52_title_)
        range52_title_->setText(tr("52-WEEK RANGE"));
    if (targets_title_)
        targets_title_->setText(tr("ANALYST TARGETS"));
    render_company();
    render_info();
    render_quote();
    render_ranges();
    render_returns();
}

} // namespace fincept::screens
