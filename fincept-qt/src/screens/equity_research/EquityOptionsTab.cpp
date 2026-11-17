// src/screens/equity_research/EquityOptionsTab.cpp
#include "screens/equity_research/EquityOptionsTab.h"

#include "screens/equity_research/EquityResearchUi.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/widgets/ThemeHelpers.h"

#include <cmath>

#include "screens/equity_research/EquityResearchFormat.h"
#include "services/fincept_data/FinceptDataParse.h"
#include "services/fincept_data/FinceptDataService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ProAnalytics.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace fd = services::fincept_data;

namespace equity_options_tab_detail {

/// Contracts per request: ±10 strikes × calls + puts = 40, so one page normally; a
/// wider band pages (LOAD MORE) instead of pulling the whole chain at once.
constexpr int kPageSize = 100;
constexpr int kStrikeChoices[] = {5, 10, 20, 50};
constexpr int kDefaultStrikes = 10;

// Column layout: calls | strike | puts (mirrored around the strike).
enum Col {
    kCDelta, kCGamma, kCTheta, kCVega, kCIv, kCOi, kCVol, kCBid, kCAsk, kCLast,
    kStrike,
    kPLast, kPBid, kPAsk, kPVol, kPOi, kPIv, kPDelta, kPGamma, kPTheta, kPVega,
    kColCount
};

} // namespace equity_options_tab_detail

using namespace equity_options_tab_detail;

EquityOptionsTab::EquityOptionsTab(QWidget* parent) : EquityDataTab(QStringLiteral("equityOptionsTab"), parent) {
    build_ui();
    retranslate();
}

EquityOptionsTab::~EquityOptionsTab() {
    // The combo boxes outlive this class's members during QWidget teardown (children are
    // deleted later): make sure no model-driven index change can reach a handler here.
    if (expiry_combo_)
        expiry_combo_->disconnect(this);
    if (strikes_combo_)
        strikes_combo_->disconnect(this);
}

void EquityOptionsTab::build_ui() {
    // Controls go into the header row (reparented to its frame by the layout).
    expiry_caption_ = make_caption(QString(), this);
    controls_layout()->addWidget(expiry_caption_);
    expiry_combo_ = new QComboBox(this);
    expiry_combo_->setProperty("fincept_agent_risk", "spend"); // each expiration is a billed chain request
    expiry_combo_->setMinimumWidth(150);
    controls_layout()->addWidget(expiry_combo_);
    connect(expiry_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0 || symbol().isEmpty())
            return;
        const QString exp = expiry_combo_->itemData(index).toString();
        if (exp.isEmpty() || exp == chain_.expiration)
            return;
        expiration_ = exp;
        load_page(1, /*refresh=*/false);
    });

    strikes_caption_ = make_caption(QString(), this);
    controls_layout()->addWidget(strikes_caption_);
    strikes_combo_ = new QComboBox(this);
    strikes_combo_->setProperty("fincept_agent_risk", "spend"); // reloads the chain (billed)
    for (int n : kStrikeChoices)
        strikes_combo_->addItem(QStringLiteral("±%1").arg(n), n);
    strikes_combo_->setCurrentIndex(strikes_combo_->findData(kDefaultStrikes));
    controls_layout()->addWidget(strikes_combo_);
    connect(strikes_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0)
            return;
        strikes_ = strikes_combo_->itemData(index).toInt();
        if (!symbol().isEmpty())
            load_page(1, /*refresh=*/false);
    });

    // ── Underlying + feed line ───────────────────────────────────────────────
    auto* info = new QHBoxLayout;
    info->setSpacing(16);
    underlying_label_ = new QLabel(body_widget());
    underlying_label_->setTextFormat(Qt::RichText);
    underlying_label_->setStyleSheet(QString("font-size:" + fincept::ui::fonts::px(-1) + "px;"));
    info->addWidget(underlying_label_);
    info->addStretch(1);
    feed_label_ = make_note(body_widget());
    feed_label_->setWordWrap(false);
    info->addWidget(feed_label_);
    body_layout()->addLayout(info);

    auto* sides = new QHBoxLayout;
    calls_caption_ = make_section(QString(), body_widget());
    puts_caption_ = make_section(QString(), body_widget());
    sides->addWidget(calls_caption_);
    sides->addStretch(1);
    sides->addWidget(puts_caption_);
    body_layout()->addLayout(sides);

    // ── Analytics: OI / volume by strike, IV smile, put/call, max pain, expected move ──
    analytics_label_ = make_note(body_widget());
    analytics_label_->setTextFormat(Qt::RichText);
    analytics_label_->setWordWrap(true);
    body_layout()->addWidget(analytics_label_);
    auto* charts = new QHBoxLayout;
    charts->setSpacing(8);
    auto* oi_col = new QVBoxLayout;
    oi_mode_ = new er_ui::Segmented(body_widget());
    oi_mode_->add(QStringLiteral("oi"), tr("OPEN INTEREST"));
    oi_mode_->add(QStringLiteral("vol"), tr("VOLUME"));
    connect(oi_mode_, &er_ui::Segmented::changed, this, [this]() { update_analytics(); });
    oi_col->addWidget(oi_mode_, 0, Qt::AlignLeft);
    oi_chart_ = new ui::BarChart(body_widget());
    oi_chart_->setMinimumHeight(ui::metrics::height(190));
    oi_col->addWidget(oi_chart_, 1);
    charts->addLayout(oi_col, 3);
    iv_chart_ = new ui::TimeSeriesChart(body_widget());
    iv_chart_->set_x_is_time(false);
    iv_chart_->set_transparent_background(true);
    iv_chart_->set_show_legend(true);
    iv_chart_->set_zoom_enabled(false);
    iv_chart_->setMinimumHeight(ui::metrics::height(210));
    charts->addWidget(iv_chart_, 2);
    body_layout()->addLayout(charts);

    // ── Chain ────────────────────────────────────────────────────────────────
    table_ = make_table(body_widget());
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    body_layout()->addWidget(table_, 1);

    auto* footer = new QHBoxLayout;
    footer_label_ = make_note(body_widget());
    footer_label_->setWordWrap(false);
    footer->addWidget(footer_label_);
    footer->addStretch(1);
    // Pro analytics: the server's positioning report for this underlying, in its own
    // window (opens on click only; hidden while the server does not offer Pro analytics).
    footer->addWidget(ui::pro::make_button(ui::pro::Kind::OptionsPositioning, [this]() {
        ui::pro::Request r;
        r.symbol = symbol();
        return r;
    }, body_widget()));
    more_btn_ = make_button(QString(), body_widget());
    more_btn_->setProperty("fincept_agent_risk", "spend"); // next page: billed
    more_btn_->hide();
    connect(more_btn_, &QPushButton::clicked, this, [this]() {
        if (pagination_.has_more)
            load_page(pagination_.page + 1, /*refresh=*/false);
    });
    footer->addWidget(more_btn_);
    body_layout()->addLayout(footer);
}

QStringList EquityOptionsTab::headers() const {
    const QString delta = tr("Δ"), gamma = tr("Γ"), theta = tr("Θ"), vega = tr("VEGA"), iv = tr("IV"),
                  oi = tr("OI"), vol = tr("VOL"), bid = tr("BID"), ask = tr("ASK"), last = tr("LAST");
    return {delta, gamma, theta, vega, iv,  oi,  vol, bid,   ask,   last, tr("STRIKE"),
            last,  bid,   ask,   vol,  oi,  iv,  delta, gamma, theta, vega};
}

void EquityOptionsTab::retranslate() {
    if (oi_mode_) {
        oi_mode_->set_text(QStringLiteral("oi"), tr("OPEN INTEREST"), tr("Open interest by strike, calls vs puts"));
        oi_mode_->set_text(QStringLiteral("vol"), tr("VOLUME"), tr("Today's volume by strike, calls vs puts"));
    }
    expiry_caption_->setText(tr("EXPIRATION"));
    expiry_combo_->setToolTip(tr("Expiration dates listed for this underlying (days to expiry in brackets)"));
    strikes_caption_->setText(tr("STRIKES"));
    strikes_combo_->setToolTip(tr("Strikes shown below and above the underlying's price"));
    calls_caption_->setText(tr("◂ CALLS"));
    puts_caption_->setText(tr("PUTS ▸"));
    more_btn_->setText(tr("LOAD MORE"));
    more_btn_->setToolTip(tr("Load the next page of contracts (one more data call)"));
    table_->setToolTip(tr("Bright cells are in the money, dimmed ones out of the money; the amber strike is nearest "
                          "the underlying's price"));
    if (!chain_.contracts.isEmpty()) {
        populate_underlying(last_answer_);
        populate_table();
    } else {
        reset_table(table_, headers(), 0);
    }
    update_footer();
}

QString EquityOptionsTab::loading_text() const {
    return tr("LOADING OPTIONS…");
}

QString EquityOptionsTab::not_found_text(const fd::DataError& e) const {
    // "No options on 'AAPL' expire on 2026-11-21" is about the expiration, not the symbol.
    if (e.message.contains(QLatin1String("expire on"), Qt::CaseInsensitive))
        return e.message;
    return tr("No listed options for %1.").arg(symbol());
}

void EquityOptionsTab::clear_view() {
    chain_ = {};
    pagination_ = {};
    last_answer_ = {};
    expiration_.clear();
    {
        const QSignalBlocker block(expiry_combo_);
        expiry_combo_->clear();
    }
    underlying_label_->clear();
    feed_label_->clear();
    reset_table(table_, headers(), 0);
    footer_label_->clear();
    more_btn_->hide();
}

void EquityOptionsTab::request(bool refresh) {
    load_page(1, refresh);
}

bool EquityOptionsTab::apply_remembered(const fd::DataResult& r) {
    apply_answer(r, /*append=*/false);
    return !chain_.contracts.isEmpty() || !chain_.expirations.isEmpty();
}

QString EquityOptionsTab::remember_variant() const {
    return QStringLiteral("strikes=%1").arg(strikes_);
}

bool EquityOptionsTab::rememberable(const fd::DataResult& r) const {
    return r.meta.pagination.page <= 1;
}

void EquityOptionsTab::load_page(int page, bool refresh) {
    const quint64 token = start_request();
    const bool append = page > 1;
    // Later pages pin the expiration the first page answered for (the "nearest" one can roll).
    const QString expiration = append ? chain_.expiration : expiration_;
    fd::FinceptDataService::instance().fetch_option_chain(
        symbol(), expiration, strikes_, page, kPageSize, this,
        [this, token, append](const fd::DataResult& r) {
            if (!is_current(token))
                return; // another symbol / expiration / band was asked for meanwhile
            finish(r);
            if (r.ok())
                apply_answer(r, append);
        },
        refresh);
}

void EquityOptionsTab::apply_answer(const fd::DataResult& r, bool append) {
    fd::OptionChain page = fd::parse_option_chain(r.data);
    if (append && page.expiration == chain_.expiration) {
        QSet<QString> seen;
        for (const auto& k : chain_.contracts)
            seen.insert(k.contract);
        for (const auto& k : page.contracts)
            if (!seen.contains(k.contract))
                chain_.contracts.append(k);
        chain_.underlying = page.underlying;
    } else {
        chain_ = std::move(page);
    }
    pagination_ = r.meta.pagination;
    last_answer_ = r;
    populate_expirations();
    populate_underlying(r);
    populate_table();
    update_footer();
}

void EquityOptionsTab::populate_expirations() {
    const QSignalBlocker block(expiry_combo_);
    expiry_combo_->clear();
    const QDate today = QDate::currentDate();
    for (const QString& exp : chain_.expirations) {
        const QDate d = QDate::fromString(exp, Qt::ISODate);
        const qint64 dte = d.isValid() ? today.daysTo(d) : -1;
        expiry_combo_->addItem(dte >= 0 ? tr("%1  (%2d)").arg(exp).arg(dte) : exp, exp);
    }
    const int idx = expiry_combo_->findData(chain_.expiration);
    if (idx >= 0)
        expiry_combo_->setCurrentIndex(idx);
}

void EquityOptionsTab::populate_underlying(const fd::DataResult& r) {
    const auto& u = chain_.underlying;
    const QString sym = (u.symbol.isEmpty() ? symbol() : u.symbol).toHtmlEscaped();
    QString html = QStringLiteral("<span style=\"color:%1; font-weight:700;\">%2</span>&nbsp;&nbsp;<b>%3</b>")
                       .arg(ui::colors::AMBER(), sym, num(u.price, u.price < 1.0 ? 4 : 2));
    if (fd::has(u.change)) {
        html += QStringLiteral("&nbsp;&nbsp;<span style=\"color:%1;\">%2%3 (%4)</span>")
                    .arg(sign_color(u.change).name(), u.change > 0 ? QStringLiteral("+") : QString(),
                         num(u.change, 2), percent(u.change_pct, 2, true));
    }
    if (fd::has(u.iv30))
        html += QStringLiteral("&nbsp;&nbsp;<span style=\"color:%1;\">%2</span>")
                    .arg(ui::colors::TEXT_SECONDARY(), tr("IV30 %1").arg(percent(u.iv30, 1)));
    underlying_label_->setText(html);

    QStringList feed;
    if (r.meta.delayed_minutes > 0)
        feed << tr("%1-MIN DELAYED").arg(r.meta.delayed_minutes);
    if (!r.meta.source_time.isEmpty())
        feed << (r.meta.source_timezone.isEmpty()
                     ? tr("quote time %1").arg(r.meta.source_time)
                     : tr("quote time %1 (%2)").arg(r.meta.source_time, r.meta.source_timezone));
    if (!chain_.greeks_available)
        feed << tr("no Greeks from this source");
    feed_label_->setText(feed.join(QStringLiteral(" · ")));
    feed_label_->setToolTip(chain_.greeks_available
                                ? tr("Cboe delayed quotes with implied volatility and Greeks")
                                : tr("Cboe does not list this underlying; the fallback source (Yahoo Finance) has no "
                                     "Greeks and serves one expiration per request"));
}

void EquityOptionsTab::populate_table() {
    const QVector<fd::StraddleRow> rows = fd::straddle_rows(chain_.contracts);
    const double spot = chain_.underlying.price;
    const int atm = fd::at_the_money_row(rows, spot);
    reset_table(table_, headers(), static_cast<int>(rows.size()));

    // Moneyness by brightness: in-the-money cells in the primary text colour, out-of-the-money
    // ones dimmed. (A cell background would not show: the global QSS draws QTableView::item
    // with a border rule, which paints over an item's BackgroundRole.)
    const QColor itm_fg(ui::colors::TEXT_PRIMARY());
    const QColor otm_fg(ui::colors::TEXT_SECONDARY());
    const QColor dim(ui::colors::TEXT_TERTIARY());
    const QColor strike_fg(ui::colors::TEXT_PRIMARY());
    const QColor atm_fg(ui::colors::AMBER());
    QFont bold = table_->font();
    bold.setBold(true);

    auto tip_for = [](const fd::OptionContract& k) {
        QStringList t;
        t << k.contract;
        if (fd::has(k.mid))
            t << tr("Mid %1").arg(num(k.mid, 3));
        if (fd::has(k.theo))
            t << tr("Theoretical %1").arg(num(k.theo, 3));
        if (fd::has(k.rho))
            t << tr("Rho %1").arg(num(k.rho, 4));
        if (fd::has(k.change_pct))
            t << tr("Change %1").arg(percent(k.change_pct, 2, true));
        if (!k.last_trade_time.isEmpty())
            t << tr("Last trade %1").arg(k.last_trade_time);
        return t.join(QLatin1Char('\n'));
    };

    // One side's cells: `cols` in the order delta, gamma, theta, vega, iv, oi, vol, bid, ask, last.
    auto fill_side = [&](int row, const fd::OptionContract* k, const int (&cols)[10]) {
        const bool itm = k && (k->in_the_money == 1 ||
                               (k->in_the_money < 0 && fd::has(spot) &&
                                (k->is_call ? k->strike < spot : k->strike > spot)));
        const QString tip = k ? tip_for(*k) : QString();
        auto cell = [&](int col, double v, const QString& text, const QColor& fg = {}) {
            const QColor color = !k ? dim : fg.isValid() ? fg : (itm ? itm_fg : otm_fg);
            auto* item = number_item(v, k ? text : equity_fmt::dash(), color);
            item->setData(er_ui::kRawRole, fd::has(v) ? QVariant(v) : QVariant(QString()));
            if (!tip.isEmpty())
                item->setToolTip(tip);
            table_->setItem(row, col, item);
        };
        const double nan = fd::kNaN;
        cell(cols[0], k ? k->delta : nan, k ? num(k->delta, 3) : QString());
        cell(cols[1], k ? k->gamma : nan, k ? num(k->gamma, 4) : QString());
        cell(cols[2], k ? k->theta : nan, k ? num(k->theta, 3) : QString());
        cell(cols[3], k ? k->vega : nan, k ? num(k->vega, 3) : QString());
        cell(cols[4], k ? k->iv : nan, k ? percent(k->iv * 100.0, 1) : QString());
        cell(cols[5], k ? k->open_interest : nan, k ? count(k->open_interest) : QString());
        cell(cols[6], k ? k->volume : nan, k ? count(k->volume) : QString());
        cell(cols[7], k ? k->bid : nan, k ? num(k->bid, 2) : QString());
        cell(cols[8], k ? k->ask : nan, k ? num(k->ask, 2) : QString());
        cell(cols[9], k ? k->last : nan, k ? num(k->last, 2) : QString(),
            k && fd::has(k->change) ? sign_color(k->change) : QColor());
    };
    static constexpr int kCallCols[10] = {kCDelta, kCGamma, kCTheta, kCVega, kCIv,
                                          kCOi,    kCVol,   kCBid,   kCAsk,  kCLast};
    static constexpr int kPutCols[10] = {kPDelta, kPGamma, kPTheta, kPVega, kPIv,
                                         kPOi,    kPVol,   kPBid,   kPAsk,  kPLast};

    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        const fd::OptionContract* call = r.call >= 0 ? &chain_.contracts[r.call] : nullptr;
        const fd::OptionContract* put = r.put >= 0 ? &chain_.contracts[r.put] : nullptr;
        fill_side(i, call, kCallCols);
        fill_side(i, put, kPutCols);
        auto* strike = number_item(r.strike, num(r.strike, 2), i == atm ? atm_fg : strike_fg);
        strike->setTextAlignment(Qt::AlignCenter);
        strike->setFont(bold);
        if (i == atm)
            strike->setToolTip(tr("Nearest strike to the underlying's price (%1)").arg(num(spot, 2)));
        table_->setItem(i, kStrike, strike);
    }

    // The fallback source has no Greeks: hide their columns instead of a wall of dashes.
    for (int c : {kCDelta, kCGamma, kCTheta, kCVega, kPDelta, kPGamma, kPTheta, kPVega})
        table_->setColumnHidden(c, !chain_.greeks_available);
    // Where open interest and volume sit: in-cell bars on the four OI / VOL columns.
    if (!table_->property("er_bars").toBool()) {
        for (int c : {kCOi, kCVol, kPOi, kPVol}) {
            auto* d = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table_);
            d->set_value_role(er_ui::kRawRole);
            table_->setItemDelegateForColumn(c, d);
        }
        table_->setProperty("er_bars", true);
    }
    update_analytics();
    table_->resizeColumnsToContents();
    if (atm >= 0) {
        // After layout, bring the money line into view (context = the table: never runs after it).
        QTimer::singleShot(0, table_, [t = table_, atm]() {
            if (auto* item = t->item(atm, kStrike))
                t->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        });
    }
}

void EquityOptionsTab::update_footer() {
    if (chain_.contracts.isEmpty()) {
        footer_label_->setText(has_data() ? tr("No contracts in this band — try more strikes or another expiration.")
                                          : QString());
        more_btn_->hide();
        return;
    }
    const auto rows = fd::straddle_rows(chain_.contracts);
    QString text = tr("%n contract(s)", nullptr, static_cast<int>(chain_.contracts.size())) +
                   QStringLiteral(" · ") + tr("%n strike(s)", nullptr, static_cast<int>(rows.size()));
    if (fd::has(chain_.underlying.price))
        text += QStringLiteral(" · ") + tr("±%1 around %2").arg(strikes_).arg(num(chain_.underlying.price, 2));
    if (pagination_.present && pagination_.total_pages > 1)
        text += QStringLiteral(" · ") +
                tr("page %1 of %2 (%3 contracts)").arg(pagination_.page).arg(pagination_.total_pages).arg(pagination_.total);
    footer_label_->setText(text);
    more_btn_->setVisible(pagination_.has_more);
}

void EquityOptionsTab::update_analytics() {
    namespace ou = fincept::ui::formatting;
    const QVector<fd::StraddleRow> rows = fd::straddle_rows(chain_.contracts);
    const double spot = chain_.underlying.price;
    oi_chart_->clear();
    iv_chart_->clear();
    if (rows.isEmpty()) {
        oi_chart_->set_state(ui::PaintedChart::Empty, has_data() ? tr("No contracts") : QString());
        iv_chart_->set_state(ui::TimeSeriesChart::State::Empty, has_data() ? tr("No contracts") : QString());
        analytics_label_->clear();
        return;
    }
    const bool vol_mode = oi_mode_ && oi_mode_->current() == QLatin1String("vol");
    QStringList cats;
    ui::BarSeries calls, puts;
    calls.name = vol_mode ? tr("Call volume") : tr("Call OI");
    puts.name = vol_mode ? tr("Put volume") : tr("Put OI");
    calls.color = QColor(ui::colors::POSITIVE());
    puts.color = QColor(ui::colors::NEGATIVE());
    ui::TimeSeriesChart::SeriesSpec call_iv, put_iv;
    call_iv.name = tr("Call IV");
    put_iv.name = tr("Put IV");
    call_iv.rebase = put_iv.rebase = false;
    call_iv.color = ui::ChartColorRole::Positive;
    put_iv.color = ui::ChartColorRole::Negative;
    double call_oi = 0, put_oi = 0, call_vol = 0, put_vol = 0;
    for (const auto& r : rows) {
        const fd::OptionContract* c = r.call >= 0 ? &chain_.contracts[r.call] : nullptr;
        const fd::OptionContract* p = r.put >= 0 ? &chain_.contracts[r.put] : nullptr;
        cats << num(r.strike, r.strike < 10 ? 2 : r.strike == std::floor(r.strike) ? 0 : 1);
        const double cv = c ? (vol_mode ? c->volume : c->open_interest) : fd::kNaN;
        const double pv = p ? (vol_mode ? p->volume : p->open_interest) : fd::kNaN;
        calls.values << (fd::has(cv) ? cv : 0.0);
        puts.values << (fd::has(pv) ? pv : 0.0);
        if (c && fd::has(c->open_interest))
            call_oi += c->open_interest;
        if (p && fd::has(p->open_interest))
            put_oi += p->open_interest;
        if (c && fd::has(c->volume))
            call_vol += c->volume;
        if (p && fd::has(p->volume))
            put_vol += p->volume;
        if (c && fd::has(c->iv) && c->iv > 0)
            call_iv.points << QPointF(r.strike, c->iv);
        if (p && fd::has(p->iv) && p->iv > 0)
            put_iv.points << QPointF(r.strike, p->iv);
    }
    oi_chart_->set_categories(cats);
    oi_chart_->set_series({calls, puts});
    oi_chart_->set_value_formatter([](double v) { return ou::compact(v); });
    oi_chart_->set_state(ui::PaintedChart::Ready);

    if (!call_iv.points.isEmpty())
        iv_chart_->add_series(call_iv);
    if (!put_iv.points.isEmpty())
        iv_chart_->add_series(put_iv);
    iv_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return ou::fraction_pct(v, 1, false); });
    iv_chart_->set_time_formatter([](double x) { return tr("Strike %1").arg(num(x, 2)); });
    iv_chart_->set_tick_time_formatter([](double x) { return num(x, 0); });
    if (fd::has(spot)) {
        const double w = std::max(1e-6, (rows.last().strike - rows.first().strike) * 0.004);
        iv_chart_->add_band(spot - w, spot + w, ui::ChartColorRole::Accent, tr("Spot %1").arg(num(spot, 2)));
    }
    iv_chart_->set_state(call_iv.points.isEmpty() && put_iv.points.isEmpty() ? ui::TimeSeriesChart::State::Empty
                                                                             : ui::TimeSeriesChart::State::Ready,
                         tr("No implied volatility from this source"));

    // Put/call ratios, max pain (the strike where option holders lose the most at expiry,
    // over the loaded band), and the expected move priced by the at-the-money straddle.
    QStringList parts;
    if (call_oi > 0)
        parts << tr("<b>Put/call OI</b> %1").arg(ou::number(put_oi / call_oi, 2));
    if (call_vol > 0)
        parts << tr("<b>Put/call volume</b> %1").arg(ou::number(put_vol / call_vol, 2));
    double best_pain = std::numeric_limits<double>::infinity(), max_pain = fd::kNaN;
    for (const auto& k : rows) {
        double pain = 0.0;
        for (const auto& r : rows) {
            const fd::OptionContract* c = r.call >= 0 ? &chain_.contracts[r.call] : nullptr;
            const fd::OptionContract* p = r.put >= 0 ? &chain_.contracts[r.put] : nullptr;
            if (c && fd::has(c->open_interest))
                pain += c->open_interest * std::max(0.0, k.strike - r.strike);
            if (p && fd::has(p->open_interest))
                pain += p->open_interest * std::max(0.0, r.strike - k.strike);
        }
        if (pain < best_pain) {
            best_pain = pain;
            max_pain = k.strike;
        }
    }
    if (fd::has(max_pain) && (call_oi > 0 || put_oi > 0))
        parts << tr("<b>Max pain</b> %1 (within the loaded strikes)").arg(num(max_pain, 2));
    const int atm = fd::at_the_money_row(rows, spot);
    if (atm >= 0 && fd::has(spot) && spot > 0) {
        const fd::OptionContract* c = rows[atm].call >= 0 ? &chain_.contracts[rows[atm].call] : nullptr;
        const fd::OptionContract* p = rows[atm].put >= 0 ? &chain_.contracts[rows[atm].put] : nullptr;
        auto mid = [](const fd::OptionContract* k) {
            if (!k)
                return fd::kNaN;
            if (fd::has(k->mid) && k->mid > 0)
                return k->mid;
            if (fd::has(k->bid) && fd::has(k->ask) && k->ask > 0)
                return (k->bid + k->ask) / 2.0;
            return fd::has(k->last) ? k->last : fd::kNaN;
        };
        const double straddle = mid(c) + mid(p);
        if (fd::has(straddle) && straddle > 0)
            parts << tr("<b>Expected move</b> ±%1 (±%2) by expiry, from the at-the-money straddle")
                         .arg(num(straddle, 2), ou::fraction_pct(straddle / spot, 1, false));
        const double atm_iv = c && fd::has(c->iv) ? c->iv : (p && fd::has(p->iv) ? p->iv : fd::kNaN);
        if (fd::has(atm_iv))
            parts << tr("<b>ATM IV</b> %1").arg(ou::fraction_pct(atm_iv, 1, false));
    }
    parts << tr("IV term structure needs one request per expiration (credit-metered) — pick expirations above to compare");
    analytics_label_->setText(parts.join(QStringLiteral(" · ")));
}

} // namespace fincept::screens
