#pragma once
// RegionPin — a market tile pinned to one MarketProfile region.
//
// Market tiles follow the dashboard's region selector (MarketProfileStore). A
// region template (India, Europe, ...) instead pins its tiles to that region's
// profile through the tile config key "market_region" ("IN", "EU", ...), so an
// India desk shows the Nifty, the NSE sectors and India VIX whatever region the
// rest of the terminal is set to. A pinned tile ignores region changes and names
// its region in the title. Tiles keep the key in config() so it survives every
// later config change; a config without it follows the selector as before.

#include "core/market/MarketProfile.h"

#include <QJsonObject>
#include <QString>

namespace fincept::screens::widgets {

inline constexpr const char* kMarketRegionKey = "market_region";

/// The pinned region id of a tile config: a known MarketProfile id, else "".
inline QString region_pin_from(const QJsonObject& cfg) {
    const QString id = cfg.value(QLatin1String(kMarketRegionKey)).toString().trimmed().toUpper();
    if (id.isEmpty())
        return {};
    for (const auto& p : core::market::all_market_profiles())
        if (p.id == id)
            return id;
    return {};
}

/// The profile a tile shows: the pinned region's, else the user's current one.
inline const core::market::MarketProfile& tile_profile(const QString& pin) {
    return pin.isEmpty() ? core::market::MarketProfileStore::instance().current() : core::market::market_profile(pin);
}

/// Write the pin (if any) into a config the tile is about to persist.
inline void store_region_pin(QJsonObject& cfg, const QString& pin) {
    if (!pin.isEmpty())
        cfg.insert(QLatin1String(kMarketRegionKey), pin);
}

} // namespace fincept::screens::widgets
