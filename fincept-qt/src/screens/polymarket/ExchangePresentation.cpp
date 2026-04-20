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
    const QString sym = currency_symbol.isEmpty() ? QStringLiteral("$") : currency_symbol;
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

    // Kalshi v2 statuses (Apr 2026): unopened, open, paused, closed, settled.
    // The adapter stashes the raw status string in extras["status"]. Polymarket
    // doesn't populate extras["status"] — fall back to the active/closed bools.
    const QString kalshi_status = market.extras.value(QStringLiteral("status")).toString().toLower();

    auto accent_bg = QColor(accent.red(), accent.green(), accent.blue(), 38);

    if (exchange_id == QStringLiteral("kalshi") && !kalshi_status.isEmpty()) {
        if (kalshi_status == QStringLiteral("settled")) {
            return {QStringLiteral("SETTLED"), QColor(colors::POSITIVE()),
                    QColor(22, 163, 74, 38),
                    QStringLiteral("Market has been settled. Final outcome "
                                   "determined and payouts processed.")};
        }
        if (kalshi_status == QStringLiteral("closed")) {
            return {QStringLiteral("CLOSED"), accent, accent_bg,
                    QStringLiteral("Trading halted. Awaiting settlement from "
                                   "Kalshi's source-of-truth resolution.")};
        }
        if (kalshi_status == QStringLiteral("paused")) {
            return {QStringLiteral("PAUSED"), QColor(colors::TEXT_SECONDARY()),
                    QColor(0, 0, 0, 0),
                    QStringLiteral("Trading temporarily halted by the exchange. "
                                   "Orders cannot be placed until reopened.")};
        }
        if (kalshi_status == QStringLiteral("open")) {
            QString tip = QStringLiteral("Market is open for trading.");
            if (!market.end_date_iso.isEmpty())
                tip += QStringLiteral(" Closes ") + market.end_date_iso + QStringLiteral(".");
            return {QStringLiteral("OPEN"), accent, accent_bg, tip};
        }
        if (kalshi_status == QStringLiteral("unopened")) {
            QString tip = QStringLiteral("Market has not yet opened for trading.");
            if (!market.end_date_iso.isEmpty())
                tip += QStringLiteral(" Close time: ") + market.end_date_iso + QStringLiteral(".");
            return {QStringLiteral("PENDING"), QColor(colors::TEXT_DIM()),
                    QColor(0, 0, 0, 0), tip};
        }
    }

    if (market.closed) {
        return {QStringLiteral("RESOLVED"), QColor(colors::POSITIVE()),
                QColor(22, 163, 74, 38),
                QStringLiteral("Market has resolved. No further trading.")};
    }
    if (market.active) {
        return {QStringLiteral("ACTIVE"), accent, accent_bg,
                QStringLiteral("Market is accepting orders.")};
    }
    return {QStringLiteral("INACTIVE"), QColor(colors::TEXT_DIM()),
            QColor(0, 0, 0, 0),
            QStringLiteral("Market not currently trading.")};
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
                    QStringLiteral("SETTLED"), QStringLiteral("HISTORY")};
    p.default_view = QStringLiteral("MARKETS");
    // Kalshi's series catalog can exceed 100 entries — a chip row doesn't
    // scale. Drop into a dropdown so users can scan + filter.
    p.category_mode = CategoryMode::ComboBox;
    p.category_visible_cap = 0;  // unused in combobox mode
    p.chart_y_label = QStringLiteral("PRICE");
    // Kalshi exposes open_interest_fp per market — surface it. Polymarket
    // also has it via a separate data endpoint; the detail panel renders
    // both through the same OI box.
    p.has_open_interest = true;
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
