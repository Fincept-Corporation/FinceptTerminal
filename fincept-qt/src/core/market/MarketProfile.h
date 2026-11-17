#pragma once
// MarketProfile — what "the market" means for the user's region.
//
// A profile bundles the instruments a market dashboard shows by default for one
// region: its headline indices, FX pairs, commodities, crypto, sector set,
// benchmark, volatility gauges, trading venues (ids of core/market/MarketSessions),
// base currency, the Yahoo screener region(s) and a large-cap floor for movers.
// Every symbol is a Yahoo Finance ticker, so it works on the `market:*` topics.
//
// The user's choice is held by MarketProfileStore (persisted in SettingsRepository
// under "dashboard.market_region"); until the user picks one, the region is
// inferred from the system locale (de_DE -> Eurozone, en_IN -> India, ...).
//
// Sector sets are what Yahoo carries for the region: SPDR sector ETFs (US),
// iShares STOXX Europe 600 sector ETFs on Xetra (Eurozone, UK), NSE sectoral
// indices (India), NEXT FUNDS TOPIX-17 ETFs (Japan), S&P/ASX 200 sector indices
// (Australia), iShares S&P/TSX capped sector ETFs (Canada). A region without a
// dependable set uses the global sector ETFs and says so in `sector_set_name`.
// `ProfileInstrument::weight` is an approximate index weight in percent, used
// only to size treemap cells (0 = unknown -> equal areas); it is never shown as data.

#include <QLocale>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::core::market {

struct ProfileInstrument {
    QString symbol;  ///< Yahoo ticker
    QString label;   ///< short display name (proper names are not translated)
    QString group;   ///< optional grouping key: "Energy", "Metals", "Americas", ... (translatable via profile_text)
    double weight = 0; ///< approx. index weight in percent (sectors); 0 = unknown
};

struct MarketProfile {
    QString id;            ///< "US", "CA", "BR", "UK", "EU", "IN", "JP", "CN", "KR", "AU", "GLOBAL"
    QString name;          ///< English; show market_profile_name()
    QString base_currency; ///< ISO 4217 of the region ("USD", "EUR", ...)
    QStringList countries; ///< ISO 3166-1 alpha-2 codes for calendars / news ("US"); empty = all
    QString benchmark;     ///< headline index ("^GSPC")
    QStringList volatility; ///< volatility gauges, regional first ("^VIX", "^INDIAVIX", ...)
    QStringList screener_regions; ///< Yahoo screener region codes ("us"; "de","fr",... for the Eurozone)
    double large_cap_floor = 2e9; ///< movers floor, in the region's QUOTE units (GBp for the UK)
    QString news_region;   ///< NewsArticle::region value of the region's feeds ("US"); empty = none
    QString rates_curve;   ///< /data rates route for the yield curve: "treasury" or "euro-curve"
    QVector<ProfileInstrument> indices;
    QVector<ProfileInstrument> fx;
    QVector<ProfileInstrument> commodities;
    QVector<ProfileInstrument> crypto;
    QVector<ProfileInstrument> sectors;
    QString sector_set_name;  ///< English, e.g. "S&P 500 sectors (SPDR ETFs)"; show profile_text()
    QStringList movers;       ///< fallback large-cap universe when the screener is unavailable
    QStringList venues;       ///< MarketSessions venue ids, primary first
};

/// One headline index of the world, with its region group and the venue whose
/// session it follows (MarketSessions venue id).
struct GlobalIndex {
    QString symbol;
    QString label;
    QString region; ///< English: "Americas", "Europe", "Middle East & Africa", "Asia-Pacific" (profile_text())
    QString venue;
};
/// The world's headline indices, grouped by region (Americas → Asia-Pacific).
const QVector<GlobalIndex>& global_index_universe();

/// Every built-in profile, in display order (GLOBAL last).
const QVector<MarketProfile>& all_market_profiles();
/// Profile by id (case-insensitive); unknown ids give the GLOBAL profile.
const MarketProfile& market_profile(const QString& id);
/// Translated region name ("United States", "Eurozone", ...).
QString market_profile_name(const MarketProfile& profile);
/// Translate a profile string marked for translation (sector labels, group names,
/// sector set names). Proper names pass through unchanged.
QString profile_text(const QString& english);
/// Region id inferred from a locale's territory (en_US -> US, de_DE -> EU,
/// hi_IN -> IN, zh_HK -> CN, ...); GLOBAL when nothing fits.
QString region_for_locale(const QLocale& locale);

/// Symbols of a profile list, in order.
QStringList profile_symbols(const QVector<ProfileInstrument>& list);
/// Label for `symbol` from any list of `profile` (indices, FX, commodities,
/// crypto, sectors); empty when the profile does not list it.
QString profile_label_for(const MarketProfile& profile, const QString& symbol);
/// Label for `symbol` from every built-in profile (first hit); empty if none.
QString any_profile_label_for(const QString& symbol);

/// The user's current region. GUI thread only.
class MarketProfileStore : public QObject {
    Q_OBJECT
  public:
    static MarketProfileStore& instance();

    /// Current profile: the persisted choice, else the locale's region.
    const MarketProfile& current();
    QString current_id();
    /// True when the user picked the region (false = inferred from the locale).
    bool is_user_choice();
    /// Persist `id` as the user's region and emit profile_changed() if it changed.
    void set_current(const QString& id);

  signals:
    void profile_changed(const QString& region_id);

  private:
    MarketProfileStore() = default;
    void ensure_loaded();

    QString id_;
    bool loaded_ = false;
    bool user_choice_ = false;
};

} // namespace fincept::core::market
