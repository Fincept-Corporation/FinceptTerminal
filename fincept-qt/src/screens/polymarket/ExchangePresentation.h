#pragma once

#include "services/prediction/PredictionTypes.h"

#include <QColor>
#include <QString>
#include <QStringList>

namespace fincept::services::prediction {
class PredictionExchangeAdapter;
} // namespace fincept::services::prediction

namespace fincept::screens::polymarket {

/// How a given exchange should be presented to the user. Captures the
/// per-exchange UX knobs that aren't part of the data model — accent color,
/// currency symbol, price display mode, view pill set, status badge mapping,
/// and feature-flags for Polymarket-only tabs.
///
/// One profile is installed into the screen + sub-panels whenever the active
/// exchange changes. Construct via for_polymarket(), for_kalshi(), or
/// for_adapter() to route by adapter id.
struct ExchangePresentation {
    enum class PriceStyle {
        ProbabilityCents,  // 0.52 → "52¢"      (Polymarket: raw prob × 100 with cents suffix)
        Dollars,           // 0.52 → "$0.52"    (Kalshi: dollar-denominated contract price)
    };

    enum class CategoryMode {
        Chips,       // flat chip row (Polymarket: ~20 tags fits fine)
        ComboBox,    // dropdown (Kalshi: 100+ series tickers)
    };

    QString exchange_id;           // "polymarket" | "kalshi"
    QString display_name;          // "Polymarket" | "Kalshi"

    // Visual identity — a single accent color is the clearest "different
    // screen" signal without a full theme fork.
    QColor accent;                 // active pill / underline / bar color
    QColor accent_dim;             // subtle tint for backgrounds

    // Currency + number formatting.
    QString currency_symbol;       // "$" | "USDC"
    PriceStyle price_style = PriceStyle::ProbabilityCents;
    int price_decimal_places = 4;  // ActivityFeed raw price column

    // Top command bar.
    QStringList view_names;        // ["TRENDING","MARKETS","EVENTS","SPORTS","RESOLVED"] / ["MARKETS","EVENTS","SETTLED"]
    QString default_view;
    CategoryMode category_mode = CategoryMode::Chips;
    int category_visible_cap = 12; // Chips mode: hide overflow

    // Chart.
    QString chart_y_label = QStringLiteral("PROBABILITY");

    // Feature flags (mirror the detail-panel tab visibility decisions).
    bool has_open_interest = true;
    bool has_polymarket_extras = true;   // Holders / Comments / Related tabs
    bool has_leaderboard = true;

    // Formatters — helpers that bake the profile's units into rendered text.
    QString format_price(double prob_0_to_1) const;   // 0.52 → "52¢" or "$0.52"
    QString format_volume(double v) const;            // 1_234_567 → "$1.2M"
    QString format_liquidity(double v) const;         // same as volume today; kept as a separate slot for future divergence

    /// Translate the prediction::PredictionMarket.active/closed pair (+ any
    /// exchange-specific hints in extras) into a status badge tuple:
    /// (text, foreground, background). Lets Kalshi surface its
    /// "determined/settled" lifecycle distinctly from Polymarket's binary
    /// active/closed.
    struct StatusBadge {
        QString text;
        QColor fg;
        QColor bg;  // may be transparent
    };
    StatusBadge status_badge(const fincept::services::prediction::PredictionMarket& market) const;

    // Factories.
    static ExchangePresentation for_polymarket();
    static ExchangePresentation for_kalshi();
    static ExchangePresentation for_adapter(const fincept::services::prediction::PredictionExchangeAdapter* adapter);
    static ExchangePresentation for_id(const QString& exchange_id);
};

} // namespace fincept::screens::polymarket
