#include "screens/dashboard/widgets/MarketGridTiles.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "services/markets/FxRateService.h"
#include "services/markets/MarketDataService.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace mg_fmt = fincept::ui::formatting;
using fincept::services::FxRateService;
using fincept::services::MarketDataService;

/// Value of one unit of `ccy` in USD from its USD-leg quote, and its % move today.
bool mg_usd_value(const QString& ccy, const services::QuoteData& q, double* value, double* pct) {
    if (!(q.price > 0))
        return false;
    const QString leg = FxRateService::usd_leg_symbol(ccy);
    const bool ccy_first = leg.startsWith(ccy); // EURUSD=X: price = USD per EUR
    *value = ccy_first ? q.price : 1.0 / q.price;
    const double r = q.change_pct / 100.0;
    *pct = ccy_first ? r : (1.0 / (1.0 + r) - 1.0);
    return std::isfinite(*pct);
}

void mg_footer_style(QLabel* l) {
    // Re-applied after every appearance change: only when the text changed.
    const QString sheet = QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                              .arg(ui::colors::TEXT_TERTIARY())
                              .arg(ui::fonts::px(-4));
    if (l->styleSheet() != sheet)
        l->setStyleSheet(sheet);
}

} // namespace

// ── FX matrix ────────────────────────────────────────────────────────────────

FxMatrixWidget::FxMatrixWidget(QWidget* parent) : BaseWidget(tr("FX CROSS MATRIX"), parent, ui::colors::INFO()) {
    matrix_ = new ui::MatrixHeatmap(this);
    matrix_->set_diverging_scale(-1.0, 1.0, 0.0);
    matrix_->set_value_formatter([](double v) { return mg_fmt::pct(v, 2); });
    matrix_->set_show_values(true);
    content_layout()->addWidget(matrix_, 1);
    footer_ = new QLabel(tr("Today's % move of the row currency against the column currency (±1 % = full colour)"),
                        this);
    footer_->setWordWrap(true);
    mg_footer_style(footer_);
    content_layout()->addWidget(footer_);
    connect(matrix_, &ui::MatrixHeatmap::cell_hovered, this, [this](int r, int c) {
        const QStringList cc = currencies();
        if (r < 0 || c < 0 || r >= cc.size() || c >= cc.size())
            return;
        const auto rate = FxRateService::instance().rate(cc[r], cc[c]);
        matrix_->setToolTip(rate ? QStringLiteral("%1/%2 = %3").arg(cc[r], cc[c], mg_fmt::number(*rate, 4))
                                 : QStringLiteral("%1/%2").arg(cc[r], cc[c]));
    });
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        QStringList t;
        for (const auto& c : currencies())
            if (!FxRateService::usd_leg_symbol(c).isEmpty())
                t << MarketDataService::quote_topic(FxRateService::usd_leg_symbol(c));
        datahub::DataHub::instance().request(t, true);
    });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (isVisible())
                    subscribe_all();
            });
    set_loading(true);
}

QStringList FxMatrixWidget::currencies() const {
    QStringList c{core::market::MarketProfileStore::instance().current().base_currency};
    // The region's currency first, then the majors (a fixed curated list — deliberate).
    for (const char* x : {"USD", "EUR", "JPY", "GBP", "CNY", "CHF", "CAD", "AUD"})
        if (!c.contains(QLatin1String(x)))
            c << QString::fromLatin1(x);
    return c.mid(0, 9);
}

void FxMatrixWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        subscribe_all();
}

void FxMatrixWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    hub_active_ = false;
}

void FxMatrixWidget::subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    QStringList topics;
    for (const auto& c : currencies()) {
        const QString leg = FxRateService::usd_leg_symbol(c);
        if (!leg.isEmpty())
            topics << MarketDataService::quote_topic(leg);
    }
    track_topics(topics, /*drive_loading_overlay=*/true);
    for (const auto& c : currencies()) {
        const QString leg = FxRateService::usd_leg_symbol(c);
        if (leg.isEmpty())
            continue;
        const QString t = MarketDataService::quote_topic(leg);
        hub.subscribe(this, t, [this, c, t](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            legs_.insert(c, v.value<services::QuoteData>());
            note_topic_value(t);
            schedule_render([this]() { render(); });
        });
    }
    hub_active_ = true;
    render();
}

void FxMatrixWidget::on_theme_changed() {
    mg_footer_style(footer_);
}

void FxMatrixWidget::on_topic_errors_changed() {
    schedule_render([this]() { render(); });
}

void FxMatrixWidget::render() {
    const QStringList cc = currencies();
    QHash<QString, double> move; // currency -> its move vs USD today (fraction)
    // USD is the pivot: every leg is quoted against it (FxRateService::usd_leg_symbol) — deliberate.
    move.insert(QStringLiteral("USD"), 0.0);
    for (const auto& c : cc) {
        if (c == QLatin1String("USD"))
            continue;
        const auto it = legs_.constFind(c);
        double value = 0, pct = 0;
        if (it != legs_.constEnd() && mg_usd_value(c, it.value(), &value, &pct))
            move.insert(c, pct);
    }
    QVector<QVector<double>> m(cc.size(), QVector<double>(cc.size(), std::numeric_limits<double>::quiet_NaN()));
    for (int r = 0; r < cc.size(); ++r)
        for (int c = 0; c < cc.size(); ++c) {
            if (r == c || !move.contains(cc[r]) || !move.contains(cc[c]))
                continue;
            m[r][c] = ((1.0 + move.value(cc[r])) / (1.0 + move.value(cc[c])) - 1.0) * 100.0;
        }
    matrix_->set_matrix(cc, cc, m);
}

// ── Breadth ──────────────────────────────────────────────────────────────────

MarketBreadthWidget::MarketBreadthWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("BREADTH"), parent, ui::colors::CYAN()) {
    auto* bar = new QWidget(this);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    universe_combo_ = new QComboBox(bar);
    universe_combo_->addItem(tr("Region large-cap basket"), QStringLiteral("large_caps"));
    universe_combo_->addItem(tr("Region sector set"), QStringLiteral("sectors"));
    hl->addWidget(universe_combo_, 1);
    content_layout()->addWidget(bar);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_spacing(4);
    for (const auto& [k, l] : {std::pair{"adv", tr("ADVANCING")}, std::pair{"dec", tr("DECLINING")},
                               std::pair{"ad", tr("A/D RATIO")}, std::pair{"ma50", tr("ABOVE 50D AVG")},
                               std::pair{"hi", tr("NEAR 52W HIGH")}, std::pair{"lo", tr("NEAR 52W LOW")},
                               std::pair{"avg", tr("AVERAGE MOVE")}})
        kpis_->add_tile(QString::fromLatin1(k), l)->set_compact(true);
    content_layout()->addWidget(kpis_, 1);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    connect(universe_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        universe_ = universe_combo_->currentData().toString();
        quotes_.clear();
        if (isVisible())
            subscribe_all();
        emit config_changed(config());
    });
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        QStringList t;
        for (const auto& s : universe())
            t << MarketDataService::quote_topic(s);
        datahub::DataHub::instance().request(t, true);
    });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (!region_pin_.isEmpty())
                    return; // pinned to its own region (RegionPin.h)
                quotes_.clear();
                if (isVisible())
                    subscribe_all();
            });
    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

QJsonObject MarketBreadthWidget::config() const {
    QJsonObject o{{"universe", universe_}};
    store_region_pin(o, region_pin_);
    return o;
}

void MarketBreadthWidget::apply_config(const QJsonObject& cfg) {
    region_pin_ = region_pin_from(cfg);
    quotes_.clear();
    universe_ = cfg.value("universe").toString(QStringLiteral("large_caps"));
    const int idx = universe_combo_->findData(universe_);
    universe_combo_->blockSignals(true);
    universe_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    universe_combo_->blockSignals(false);
    if (isVisible())
        subscribe_all();
}

QStringList MarketBreadthWidget::universe() const {
    const auto& p = tile_profile(region_pin_);
    return universe_ == QLatin1String("sectors") ? core::market::profile_symbols(p.sectors) : p.movers;
}

void MarketBreadthWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        subscribe_all();
}

void MarketBreadthWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    hub_active_ = false;
}

void MarketBreadthWidget::subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QStringList syms = universe();
    QStringList topics;
    for (const auto& s : syms)
        topics << MarketDataService::quote_topic(s);
    track_topics(topics, /*drive_loading_overlay=*/true);
    for (const auto& s : syms) {
        const QString t = MarketDataService::quote_topic(s);
        hub.subscribe(this, t, [this, s, t](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            quotes_.insert(s, v.value<services::QuoteData>());
            note_topic_value(t);
            schedule_render([this]() { render(); });
        });
    }
    hub_active_ = true;
    render();
}

void MarketBreadthWidget::render() {
    const auto& p = tile_profile(region_pin_);
    const QStringList syms = universe();
    const QString basket = universe_ == QLatin1String("sectors")
                               ? core::market::profile_text(p.sector_set_name)
                               : tr("%1 large-cap basket").arg(core::market::market_profile_name(p));
    set_title(tr("BREADTH · %n NAME(S)", "", int(syms.size())));
    int adv = 0, dec = 0, unch = 0, n = 0, ma_n = 0, above = 0, hl_n = 0, near_hi = 0, near_lo = 0;
    double sum = 0;
    for (const auto& s : syms) {
        const auto it = quotes_.constFind(s);
        if (it == quotes_.constEnd() || !(it->price > 0))
            continue;
        const auto& q = it.value();
        ++n;
        sum += q.change_pct;
        (q.change_pct > 0 ? adv : q.change_pct < 0 ? dec : unch)++;
        if (std::isfinite(q.fifty_day_average) && q.fifty_day_average > 0) {
            ++ma_n;
            above += q.price > q.fifty_day_average ? 1 : 0;
        }
        if (std::isfinite(q.fifty_two_week_high) && std::isfinite(q.fifty_two_week_low) && q.fifty_two_week_high > 0) {
            ++hl_n;
            near_hi += q.price >= q.fifty_two_week_high * 0.95 ? 1 : 0;
            near_lo += q.price <= q.fifty_two_week_low * 1.05 ? 1 : 0;
        }
    }
    auto tile = [this](const char* k) { return kpis_->tile(QString::fromLatin1(k)); };
    auto share = [](int part, int of) {
        return of > 0 ? mg_fmt::pct(100.0 * part / of, 0, false) + QStringLiteral(" (%1/%2)").arg(part).arg(of)
                      : mg_fmt::placeholder();
    };
    if (auto* t = tile("adv"))
        t->set_value(QString::number(adv));
    if (auto* t = tile("dec"))
        t->set_value(QString::number(dec));
    if (auto* t = tile("ad"))
        t->set_value(dec > 0 ? mg_fmt::number(double(adv) / dec, 2) : (adv > 0 ? QStringLiteral("∞") : mg_fmt::placeholder()));
    if (auto* t = tile("ma50")) {
        t->set_value(share(above, ma_n));
        t->set_tooltip_text(tr("Price above the 50-day moving average (Yahoo)"));
    }
    if (auto* t = tile("hi")) {
        t->set_value(share(near_hi, hl_n));
        t->set_tooltip_text(tr("Within 5 % of the 52-week high"));
    }
    if (auto* t = tile("lo")) {
        t->set_value(share(near_lo, hl_n));
        t->set_tooltip_text(tr("Within 5 % of the 52-week low"));
    }
    if (auto* t = tile("avg")) {
        const double avg = n > 0 ? sum / n : std::numeric_limits<double>::quiet_NaN();
        t->set_value(mg_fmt::pct(avg));
        t->set_tone(!(avg == avg) || avg == 0 ? ui::StatTile::Tone::Neutral
                    : avg > 0                 ? ui::StatTile::Tone::Positive
                                              : ui::StatTile::Tone::Negative);
    }
    footer_->setText(tr("Universe: %1 — %2 of %3 priced, %4 unchanged. Not exchange-wide breadth: no source in the "
                        "terminal publishes advancers / decliners for a whole exchange.")
                         .arg(basket)
                         .arg(n)
                         .arg(syms.size())
                         .arg(unch));
}

void MarketBreadthWidget::on_theme_changed() {
    mg_footer_style(footer_);
}

} // namespace fincept::screens::widgets
