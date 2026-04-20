#include "screens/polymarket/ExchangePresentation.h"

#include "services/prediction/PredictionExchangeAdapter.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "ui/theme/Theme.h"

namespace fincept::screens::polymarket {

namespace pred = fincept::services::prediction;

// ── Formatters ──────────────────────────────────────────────────────────────

QString ExchangePresentation::format_price(double prob) const {
    // prediction::* always stores outcome price as 0.0–1.0 probability. Both
    // exchanges feed the adapter in that canonical form (Polymarket native,
    // Kalshi cent-prices divided by 100 by the type map). The presentation
    // decides how to render it.
    switch (price_style) {
    case PriceStyle::ProbabilityCents:
        return QStringLiteral("%1\u00A2").arg(qRound(prob * 100.0));  // "52¢"
    case PriceStyle::Dollars:
        return QStringLiteral("$%1").arg(prob, 0, 'f', 2);             // "$0.52"
    }
    return QString::number(prob);
}

QString ExchangePresentation::format_volume(double v) const {
    const QString sym = currency_symbol == QStringLiteral("$") ? QStringLiteral("$") : QStringLiteral("$");
    // Currency badges like "USDC" are shown separately (account chip + stats
    // label) so volume formatting always uses a dollar-sign prefix; injecting
    // "USDC" into every cell would be noisy.
    if (v >= 1e9) return QStringLiteral("%1%2B").arg(sym).arg(v / 1e9, 0, 'f', 1);
    if (v >= 1e6) return QStringLiteral("%1%2M").arg(sym).arg(v / 1e6, 0, 'f', 1);
    if (v >= 1e3) return QStringLiteral("%1%2K").arg(sym).arg(v / 1e3, 0, 'f', 1);
    return QStringLiteral("%1%2").arg(sym).arg(v, 0, 'f', 0);
}

QString ExchangePresentation::format_liquidity(double v) const {
    return format_volume(v);
}

// ── Status badge ────────────────────────────────────────────────────────────

ExchangePresentation::StatusBadge ExchangePresentation::status_badge(
    const pred::PredictionMarket& market) const {

    using namespace fincept::ui;

    // Kalshi lifecycle is richer than Polymarket's binary active/closed.
    // The adapter stashes the raw status string in extras["status"] so we
    // can surface "DETERMINED" (contest over, awaiting settlement) vs
    // "SETTLED" cleanly. Polymarket doesn't populate extras["status"] — we
    // fall back to the active/closed bools.
    const QString kalshi_status = market.extras.value(QStringLiteral("status")).toString().toLower();

    if (exchange_id == QStringLiteral("kalshi") && !kalshi_status.isEmpty()) {
        if (kalshi_status == QStringLiteral("settled") || kalshi_status == QStringLiteral("finalized")) {
            return {QStringLiteral("SETTLED"), QColor(colors::POSITIVE()), QColor(22, 163, 74, 38)};
        }
        if (kalshi_status == QStringLiteral("determined")) {
            return {QStringLiteral("DETERMINED"), accent, QColor(accent.red(), accent.green(), accent.blue(), 38)};
        }
        if (kalshi_status == QStringLiteral("closed")) {
            return {QStringLiteral("CLOSED"), QColor(colors::TEXT_SECONDARY()), QColor(0, 0, 0, 0)};
        }
        if (kalshi_status == QStringLiteral("open")) {
            return {QStringLiteral("OPEN"), accent, QColor(accent.red(), accent.green(), accent.blue(), 38)};
        }
        if (kalshi_status == QStringLiteral("initialized")) {
            return {QStringLiteral("PENDING"), QColor(colors::TEXT_DIM()), QColor(0, 0, 0, 0)};
        }
    }

    if (market.closed) {
        return {QStringLiteral("RESOLVED"), QColor(colors::POSITIVE()), QColor(22, 163, 74, 38)};
    }
    if (market.active) {
        return {QStringLiteral("ACTIVE"), accent, QColor(accent.red(), accent.green(), accent.blue(), 38)};
    }
    return {QStringLiteral("INACTIVE"), QColor(colors::TEXT_DIM()), QColor(0, 0, 0, 0)};
}

// ── Factories ───────────────────────────────────────────────────────────────

ExchangePresentation ExchangePresentation::for_polymarket() {
    using namespace fincept::ui;
    ExchangePresentation p;
    p.exchange_id = QStringLiteral("polymarket");
    p.display_name = QStringLiteral("Polymarket");
    // Amber — matches the rest of the terminal's "prediction markets" theme.
    p.accent = QColor(colors::AMBER());
    p.accent_dim = QColor(colors::AMBER_DIM());
    p.currency_symbol = QStringLiteral("USDC");
    p.price_style = PriceStyle::ProbabilityCents;
    p.price_decimal_places = 4;
    p.view_names = {QStringLiteral("TRENDING"), QStringLiteral("MARKETS"),
                    QStringLiteral("EVENTS"),   QStringLiteral("SPORTS"),
                    QStringLiteral("RESOLVED")};
    p.default_view = QStringLiteral("MARKETS");
    p.category_mode = CategoryMode::Chips;
    p.category_visible_cap = 12;
    p.chart_y_label = QStringLiteral("PROBABILITY");
    p.has_open_interest = true;
    p.has_polymarket_extras = true;
    p.has_leaderboard = true;
    return p;
}

ExchangePresentation ExchangePresentation::for_kalshi() {
    ExchangePresentation p;
    p.exchange_id = QStringLiteral("kalshi");
    p.display_name = QStringLiteral("Kalshi");
    // Teal — distinct enough from amber to make the "you're on a different
    // exchange" signal unambiguous at a glance, close enough to the terminal
    // theme that it doesn't clash with the rest of the app.
    p.accent = QColor(0x2DD4BF);        // teal-400
    p.accent_dim = QColor(0x14B8A6);    // teal-500
    p.currency_symbol = QStringLiteral("USD");
    p.price_style = PriceStyle::Dollars;
    p.price_decimal_places = 2;
    p.view_names = {QStringLiteral("MARKETS"), QStringLiteral("EVENTS"),
                    QStringLiteral("SETTLED")};
    p.default_view = QStringLiteral("MARKETS");
    // Kalshi's series catalog can exceed 100 entries — a chip row doesn't
    // scale. Drop into a dropdown so users can scan + filter.
    p.category_mode = CategoryMode::ComboBox;
    p.category_visible_cap = 0;  // unused in combobox mode
    p.chart_y_label = QStringLiteral("PRICE");
    p.has_open_interest = false;      // Kalshi REST doesn't expose OI per market
    p.has_polymarket_extras = false;  // no holders / comments / related
    p.has_leaderboard = false;
    return p;
}

ExchangePresentation ExchangePresentation::for_adapter(const pred::PredictionExchangeAdapter* adapter) {
    if (!adapter) return for_polymarket();
    return for_id(adapter->id());
}

ExchangePresentation ExchangePresentation::for_id(const QString& exchange_id) {
    if (exchange_id == QStringLiteral("kalshi")) return for_kalshi();
    return for_polymarket();
}

} // namespace fincept::screens::polymarket
