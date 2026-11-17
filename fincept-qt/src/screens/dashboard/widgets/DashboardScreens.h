#pragma once
// Region-aware ScreenerService queries shared by the dashboard's screener and
// movers tiles. Every query is a custom screen over the MarketProfile's Yahoo
// screener region(s) with the profile's large-cap floor (quote units), so a
// German user's "gainers" are German / Eurozone large caps, not US tickers.

#include "core/market/MarketProfile.h"
#include "services/screener/ScreenerService.h"
#include "ui/formatting/DateFormat.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

namespace fincept::screens::widgets::dash_screens {

inline QJsonObject filter(const char* field, const char* op, const QJsonValue& value) {
    return QJsonObject{{"field", QString::fromLatin1(field)}, {"op", QString::fromLatin1(op)}, {"value", value}};
}

/// {"region": [...], "filters": [market_cap >= floor, extra...]}.
inline QJsonObject region_criteria(const core::market::MarketProfile& p, const QJsonArray& extra = {}) {
    QJsonArray regions;
    for (const auto& r : p.screener_regions)
        regions.append(r);
    QJsonArray filters{filter("market_cap", "gte", p.large_cap_floor)};
    for (const auto& f : extra)
        filters.append(f);
    return QJsonObject{{"region", regions}, {"filters", filters}};
}

enum class MoverKind { Gainers, Losers, Active };

/// Restrict a region query to the region's primary venues and to liquid lines.
/// Regions also cover secondary lines of the same companies and foreign stocks:
/// OTC ordinaries in the US (ZSHOF), LSE's international order book (0Y2S.IL
/// +379 %), Frankfurt / Stuttgart / Vienna quotes of foreign shares (UTG.F at
/// EUR 0.08, APC.F = Apple topping a "largest Eurozone companies" screen), and
/// every Indian stock twice (BELRISE.NS and BELRISE.BO). The liquidity floor
/// drops the thin cross-listings a primary venue still carries (APC.DE).
inline void primary_listings_only(const core::market::MarketProfile& p, QJsonObject& criteria) {
    static const QHash<QString, QStringList> kPrimary = {
        {QStringLiteral("US"), {"NMS", "NGM", "NCM", "NYQ", "ASE"}},
        {QStringLiteral("GLOBAL"), {"NMS", "NGM", "NCM", "NYQ", "ASE"}},
        {QStringLiteral("CA"), {"TOR"}},
        {QStringLiteral("BR"), {"SAO"}},
        {QStringLiteral("UK"), {"LSE"}},
        {QStringLiteral("EU"), {"GER", "PAR", "MIL", "MCE", "AMS", "BRU", "VIE", "ISE", "LIS", "HEL"}},
        {QStringLiteral("IN"), {"NSI"}},
        {QStringLiteral("CN"), {"HKG", "SHH", "SHZ"}},
        {QStringLiteral("KR"), {"KSC", "KOE"}},
    };
    const auto venues = kPrimary.constFind(p.id);
    if (venues != kPrimary.constEnd())
        criteria.insert(QStringLiteral("exchange"), QJsonArray::fromStringList(*venues));
    QJsonArray filters = criteria.value(QStringLiteral("filters")).toArray();
    filters.append(filter("avg_volume_3m", "gte", 100000));
    criteria.insert(QStringLiteral("filters"), filters);
}

inline services::ScreenerQuery movers_query(const core::market::MarketProfile& p, MoverKind kind, int count) {
    services::ScreenerQuery q;
    // Liquidity floor + primary venues: the market-cap floor alone let thin OTC
    // lines of foreign large caps (ZSHOF, SHWDY: 100 shares traded, +39 %) top "gainers".
    q.criteria = region_criteria(p);
    primary_listings_only(p, q.criteria);
    q.count = count;
    switch (kind) {
        case MoverKind::Gainers:
            q.sort_field = QStringLiteral("change_pct");
            q.sort_asc = false;
            break;
        case MoverKind::Losers:
            q.sort_field = QStringLiteral("change_pct");
            q.sort_asc = true;
            break;
        case MoverKind::Active:
            q.sort_field = QStringLiteral("volume");
            q.sort_asc = false;
            break;
    }
    return q;
}

/// "Yahoo screener · United States large caps (≥ 2B USD) · as of 14:32 · delayed 15 min".
inline QString result_footer(const services::ScreenerResult& r, const core::market::MarketProfile& p) {
    namespace f = fincept::ui::formatting;
    QStringList bits;
    bits << QCoreApplication::translate("fincept::screens::widgets::dash_screens", "%1 large caps")
                .arg(core::market::market_profile_name(p));
    if (r.total > 0)
        bits << QCoreApplication::translate("fincept::screens::widgets::dash_screens", "%1 matches").arg(r.total);
    if (r.as_of > 0)
        bits << QCoreApplication::translate("fincept::screens::widgets::dash_screens", "as of %1").arg(f::time(r.as_of));
    if (r.max_delay_min > 0)
        bits << QCoreApplication::translate("fincept::screens::widgets::dash_screens", "delayed up to %1 min")
                    .arg(r.max_delay_min);
    bits << (r.source.isEmpty() ? QStringLiteral("Yahoo Finance screener") : r.source);
    return bits.join(QStringLiteral(" · "));
}

} // namespace fincept::screens::widgets::dash_screens
