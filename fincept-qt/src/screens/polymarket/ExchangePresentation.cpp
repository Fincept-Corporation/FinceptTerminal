#include "screens/polymarket/ExchangePresentation.h"

#include "services/prediction/PredictionExchangeAdapter.h"
#include "services/prediction/PredictionExchangeRegistry.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>

#include <cmath>

namespace pm_fmt = fincept::ui::formatting;

namespace fincept::screens::polymarket {

namespace pred = fincept::services::prediction;

// ExchangePresentation is a plain struct (not a QObject), so user-facing text
// is translated via QCoreApplication::translate under a stable context name.
static QString pm_tr(const char* text) {
    return QCoreApplication::translate("ExchangePresentation", text);
}

// ── Formatters ──────────────────────────────────────────────────────────────

QString ExchangePresentation::format_price(double prob) const {
    // prediction::* always stores outcome price as 0.0–1.0 probability. Both
    // exchanges feed the adapter in that canonical form (Polymarket native,
    // Kalshi cent-prices divided by 100 by the type map). The presentation
    // decides how to render it.
    switch (price_style) {
        case PriceStyle::ProbabilityCents: {
            // "52¢". Markets on a 0.001 tick quote half-cents (0.525); rounding those to
            // a whole cent showed "53¢" for a price that was actually 52.5¢ - including in
            // the order-confirmation text - so keep one decimal when it matters.
            // Polymarket's own convention: a share's price in cents of USDC, i.e. the
            // probability — not a currency amount, so no currency symbol (the decimal
            // separator follows the user's number format).
            const double cents = prob * 100.0;
            if (std::abs(cents - std::round(cents)) > 0.05)
                return QStringLiteral("%1¢").arg(pm_fmt::number(cents, 1));
            return QStringLiteral("%1¢").arg(qRound(cents));
        }
        case PriceStyle::Dollars:
            // A contract price in the venue's settlement currency ("$0.52" on Kalshi):
            // a price, never converted; the unit follows the user's symbol settings.
            return pm_fmt::price_with_symbol(
                prob, pm_fmt::InstrumentFmt{currency.isEmpty() ? QStringLiteral("USD") : currency, 2});
    }
    return QString::number(prob);
}

QString ExchangePresentation::format_volume(double v) const {
    // Money in the venue's settlement currency (USDC on Polymarket, USD on Kalshi),
    // abbreviated per the user's compact-number setting. The venue's own figure:
    // never converted. A stablecoin is its own currency ("USDC 1.23M"); the user
    // can give USDC a "$" symbol in Settings > Currency & Number Format.
    return pm_fmt::money(v, currency.isEmpty() ? QStringLiteral("USD") : currency, pm_fmt::Compact::FromSettings);
}

QString ExchangePresentation::format_liquidity(double v) const {
    return format_volume(v);
}

double ExchangePresentation::display_probability(const pred::PredictionMarket& market, int index) const {
    if (index < 0 || index >= market.outcomes.size())
        return 0.0;
    const double raw = qBound(0.0, market.outcomes[index].price, 1.0);
    if (exchange_id != QStringLiteral("kalshi") || market.outcomes.size() != 2)
        return raw;
    const double last = market.extras.value(QStringLiteral("last_price_dollars")).toDouble();
    const double bid = market.extras.contains(QStringLiteral("yes_bid_dollars"))
                           ? market.extras.value(QStringLiteral("yes_bid_dollars")).toDouble()
                           : market.outcomes[0].price;
    const double ask = market.extras.value(QStringLiteral("yes_ask_dollars")).toDouble();
    double yes = 0.0;
    if (last > 0.0 && last < 1.0)
        yes = last;
    else if (bid > 0.0 && ask > bid && ask < 1.0) // both sides quoted
        yes = (bid + ask) / 2.0;
    if (yes <= 0.0)
        return raw; // nothing traded or quoted — keep the raw bids
    return index == 0 ? yes : 1.0 - yes;
}

// ── Status badge ────────────────────────────────────────────────────────────

ExchangePresentation::StatusBadge ExchangePresentation::status_badge(const pred::PredictionMarket& market) const {

    using namespace fincept::ui;

    // Kalshi v2 statuses (Apr 2026): unopened, open, paused, closed, settled.
    // The adapter stashes the raw status string in extras["status"]. Polymarket
    // doesn't populate extras["status"] — fall back to the active/closed bools.
    const QString kalshi_status = market.extras.value(QStringLiteral("status")).toString().toLower();

    auto accent_bg = QColor(accent.red(), accent.green(), accent.blue(), 38);
    // Settled / resolved is a state, not a price move — the status green.
    const QColor done_fg(colors::SUCCESS());
    QColor done_bg = done_fg;
    done_bg.setAlpha(38);
    const QColor no_bg(Qt::transparent);

    // The live API also uses "active" (open), "initialized" (not yet open),
    // "determined" (outcome known, payout pending) and "finalized" (every
    // /historical/markets row) — those used to fall through to the generic
    // ACTIVE / INACTIVE badges.
    if (exchange_id == QStringLiteral("kalshi") && !kalshi_status.isEmpty()) {
        if (kalshi_status == QStringLiteral("determined")) {
            return {pm_tr("SETTLED"), done_fg, done_bg,
                    pm_tr("Outcome determined. Payouts are being processed.")};
        }
        if (kalshi_status == QStringLiteral("settled") || kalshi_status == QStringLiteral("finalized")) {
            return {pm_tr("SETTLED"), done_fg, done_bg,
                    pm_tr("Market has been settled. Final outcome "
                          "determined and payouts processed.")};
        }
        if (kalshi_status == QStringLiteral("closed")) {
            return {pm_tr("CLOSED"), accent, accent_bg,
                    pm_tr("Trading halted. Awaiting settlement from "
                          "Kalshi's source-of-truth resolution.")};
        }
        if (kalshi_status == QStringLiteral("paused")) {
            return {pm_tr("PAUSED"), QColor(colors::TEXT_SECONDARY()), no_bg,
                    pm_tr("Trading temporarily halted by the exchange. "
                          "Orders cannot be placed until reopened.")};
        }
        if (kalshi_status == QStringLiteral("open") || kalshi_status == QStringLiteral("active")) {
            QString tip = pm_tr("Market is open for trading.");
            if (!market.end_date_iso.isEmpty())
                tip += pm_tr(" Closes ") + market.end_date_iso + QStringLiteral(".");
            return {pm_tr("OPEN"), accent, accent_bg, tip};
        }
        if (kalshi_status == QStringLiteral("unopened") || kalshi_status == QStringLiteral("initialized")) {
            QString tip = pm_tr("Market has not yet opened for trading.");
            if (!market.end_date_iso.isEmpty())
                tip += pm_tr(" Close time: ") + market.end_date_iso + QStringLiteral(".");
            return {pm_tr("PENDING"), QColor(colors::TEXT_DIM()), no_bg, tip};
        }
    }

    if (market.closed) {
        return {pm_tr("RESOLVED"), done_fg, done_bg, pm_tr("Market has resolved. No further trading.")};
    }
    if (market.active) {
        return {pm_tr("ACTIVE"), accent, accent_bg, pm_tr("Market is accepting orders.")};
    }
    return {pm_tr("INACTIVE"), QColor(colors::TEXT_DIM()), no_bg, pm_tr("Market not currently trading.")};
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
    p.currency = QStringLiteral("USDC"); // Polymarket settles in USDC
    p.price_style = PriceStyle::ProbabilityCents;
    p.price_decimal_places = 4;
    p.view_names = {QStringLiteral("TRENDING"), QStringLiteral("MARKETS"), QStringLiteral("EVENTS"),
                    QStringLiteral("SPORTS"), QStringLiteral("RESOLVED")};
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
    using namespace fincept::ui;
    ExchangePresentation p;
    p.exchange_id = QStringLiteral("kalshi");
    p.display_name = QStringLiteral("Kalshi");
    // Teal — distinct enough from amber to make the "you're on a different
    // exchange" signal unambiguous at a glance, close enough to the terminal
    // theme that it doesn't clash with the rest of the app.
    p.accent = QColor(colors::CYAN());
    p.accent_dim = colors::tint_color(p.accent, 0.6);
    p.currency = QStringLiteral("USD"); // Kalshi settles in US dollars
    p.price_style = PriceStyle::Dollars;
    p.price_decimal_places = 2;
    p.view_names = {QStringLiteral("MARKETS"), QStringLiteral("EVENTS"), QStringLiteral("SETTLED"),
                    QStringLiteral("HISTORY")};
    p.default_view = QStringLiteral("MARKETS");
    // Kalshi's series catalog can exceed 100 entries — a chip row doesn't
    // scale. Drop into a dropdown so users can scan + filter.
    p.category_mode = CategoryMode::ComboBox;
    p.category_visible_cap = 0; // unused in combobox mode
    p.chart_y_label = QStringLiteral("PRICE");
    // Kalshi exposes open_interest_fp per market — surface it. Polymarket
    // also has it via a separate data endpoint; the detail panel renders
    // both through the same OI box.
    p.has_open_interest = true;
    p.has_polymarket_extras = false; // no holders / comments / related
    p.has_leaderboard = false;
    return p;
}

ExchangePresentation ExchangePresentation::for_adapter(const pred::PredictionExchangeAdapter* adapter) {
    if (!adapter)
        return for_polymarket();
    return for_id(adapter->id());
}

ExchangePresentation ExchangePresentation::for_id(const QString& exchange_id) {
    if (exchange_id == QStringLiteral("kalshi"))
        return for_kalshi();
    if (exchange_id == QStringLiteral("fincept")) {
        // The registry also offers "Fincept Internal" (demo markets). It used to fall
        // through to Polymarket's profile, so the status bar read "POLYMARKET", the
        // confirm dialog said "on Polymarket", and the Holders / Comments / Related /
        // Leaderboard extras (which only exist on Polymarket) were shown empty.
        ExchangePresentation p = for_polymarket();
        p.exchange_id = QStringLiteral("fincept");
        // Sample markets only, no trading — the brand in the status bar / dialogs says so.
        p.display_name = QStringLiteral("Fincept (Demo)");
        p.currency = QStringLiteral("FNCPT"); // demo markets settle in $FNCPT (printed as the code)
        p.view_names = {QStringLiteral("MARKETS"), QStringLiteral("EVENTS")};
        p.default_view = QStringLiteral("MARKETS");
        p.has_open_interest = false;
        p.has_polymarket_extras = false;
        p.has_leaderboard = false;
        return p;
    }
    return for_polymarket();
}

} // namespace fincept::screens::polymarket
