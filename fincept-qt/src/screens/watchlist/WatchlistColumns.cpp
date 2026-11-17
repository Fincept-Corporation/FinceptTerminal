#include "screens/watchlist/WatchlistColumns.h"

#include <QCoreApplication>

namespace fincept::screens::watchlist_columns {

namespace {

using K = WatchlistColKind;

constexpr const char* kWlColCtx = "WatchlistColumns";

// Category names (source strings for tr()).
constexpr const char* kCatIdentity = QT_TRANSLATE_NOOP("WatchlistColumns", "Identity");
constexpr const char* kCatPrice = QT_TRANSLATE_NOOP("WatchlistColumns", "Price");
constexpr const char* kCatVolume = QT_TRANSLATE_NOOP("WatchlistColumns", "Volume");
constexpr const char* kCatPerf = QT_TRANSLATE_NOOP("WatchlistColumns", "Performance");
constexpr const char* kCatFund = QT_TRANSLATE_NOOP("WatchlistColumns", "Fundamentals");
constexpr const char* kCatEvents = QT_TRANSLATE_NOOP("WatchlistColumns", "Events");
constexpr const char* kCatSession = QT_TRANSLATE_NOOP("WatchlistColumns", "Session");
constexpr const char* kCatUser = QT_TRANSLATE_NOOP("WatchlistColumns", "My data");

const QVector<WatchlistColumnDef>& wl_column_table() {
    // Data only (no appearance), so a function-local static is fine.
    static const QVector<WatchlistColumnDef> kTable = {
        {"symbol", QT_TRANSLATE_NOOP("WatchlistColumns", "SYMBOL"), QT_TRANSLATE_NOOP("WatchlistColumns", "Ticker"),
         kCatIdentity, K::Symbol, kWlNeedNone, true, 16},
        {"alert", QT_TRANSLATE_NOOP("WatchlistColumns", "ALERT"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Active alerts on the symbol (right-click a row to edit)"), kCatUser,
         K::Alert, kWlNeedNone, true, 5},
        {"name", QT_TRANSLATE_NOOP("WatchlistColumns", "NAME"), QT_TRANSLATE_NOOP("WatchlistColumns", "Instrument name"),
         kCatIdentity, K::Text, kWlNeedNone, true, 20},
        {"last", QT_TRANSLATE_NOOP("WatchlistColumns", "LAST"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Last price in the instrument's own currency"), kCatPrice, K::Price,
         kWlNeedQuote, true, 11},
        {"change", QT_TRANSLATE_NOOP("WatchlistColumns", "CHG"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Change from the previous close"), kCatPrice, K::Change, kWlNeedQuote,
         true, 10},
        {"change_pct", QT_TRANSLATE_NOOP("WatchlistColumns", "CHG %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Percent change from the previous close"), kCatPrice, K::ChangePct,
         kWlNeedQuote, true, 8},
        {"day_range", QT_TRANSLATE_NOOP("WatchlistColumns", "DAY RANGE"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Today's low to high, with the last price"), kCatPrice, K::RangeBar,
         kWlNeedQuote, true, 18},
        {"spark", QT_TRANSLATE_NOOP("WatchlistColumns", "5D TREND"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Hourly closes over the last five days"), kCatPerf, K::Spark,
         kWlNeedSpark, true, 12},
        {"volume", QT_TRANSLATE_NOOP("WatchlistColumns", "VOLUME"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Volume traded today"), kCatVolume, K::Volume, kWlNeedQuote, true, 9},
        {"rel_volume", QT_TRANSLATE_NOOP("WatchlistColumns", "REL VOL"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Today's volume / 3-month average daily volume"), kCatVolume,
         K::RelVolume, kWlNeedQuote, true, 9},
        {"perf_1m", QT_TRANSLATE_NOOP("WatchlistColumns", "1M %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Change over one month"), kCatPerf, K::HeatPct, kWlNeedHistory, true,
         8},
        {"perf_ytd", QT_TRANSLATE_NOOP("WatchlistColumns", "YTD %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Change since the last close of the previous year"), kCatPerf,
         K::HeatPct, kWlNeedHistory, true, 8},
        {"range_52w", QT_TRANSLATE_NOOP("WatchlistColumns", "52W RANGE"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "52-week low to high, with the last price"), kCatPrice, K::RangeBar,
         kWlNeedQuote, true, 18},
        {"state", QT_TRANSLATE_NOOP("WatchlistColumns", "MARKET"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Session state and the exchange's local time"), kCatSession, K::State,
         kWlNeedNone, true, 16},

        // ── Optional ────────────────────────────────────────────────────────
        {"open", QT_TRANSLATE_NOOP("WatchlistColumns", "OPEN"), QT_TRANSLATE_NOOP("WatchlistColumns", "Today's open"),
         kCatPrice, K::Price, kWlNeedQuote, false, 10},
        {"prev_close", QT_TRANSLATE_NOOP("WatchlistColumns", "PREV CLOSE"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Previous session's close"), kCatPrice, K::Price, kWlNeedQuote, false,
         10},
        {"day_high", QT_TRANSLATE_NOOP("WatchlistColumns", "DAY HIGH"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Today's high"), kCatPrice, K::Price, kWlNeedQuote, false, 10},
        {"day_low", QT_TRANSLATE_NOOP("WatchlistColumns", "DAY LOW"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Today's low"), kCatPrice, K::Price, kWlNeedQuote, false, 10},
        {"ext_price", QT_TRANSLATE_NOOP("WatchlistColumns", "PRE/POST"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Pre-market or after-hours price, whichever session is current"),
         kCatSession, K::Price, kWlNeedQuote, false, 10},
        {"ext_change_pct", QT_TRANSLATE_NOOP("WatchlistColumns", "PRE/POST %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Pre-market or after-hours change vs the regular close"), kCatSession,
         K::Pct, kWlNeedQuote, false, 9},
        {"display_price", QT_TRANSLATE_NOOP("WatchlistColumns", "LAST (CCY)"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Last price converted to the display currency"), kCatPrice, K::Price,
         kWlNeedQuote | kWlNeedFx, false, 11},
        {"currency", QT_TRANSLATE_NOOP("WatchlistColumns", "CCY"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Quote currency"), kCatIdentity, K::Text, kWlNeedQuote, false, 5},
        {"avg_volume", QT_TRANSLATE_NOOP("WatchlistColumns", "AVG VOL"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "3-month average daily volume"), kCatVolume, K::Volume, kWlNeedQuote,
         false, 9},
        {"perf_1w", QT_TRANSLATE_NOOP("WatchlistColumns", "1W %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Change over one week"), kCatPerf, K::HeatPct, kWlNeedHistory, false,
         8},
        {"perf_3m", QT_TRANSLATE_NOOP("WatchlistColumns", "3M %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Change over three months"), kCatPerf, K::HeatPct, kWlNeedHistory,
         false, 8},
        {"perf_6m", QT_TRANSLATE_NOOP("WatchlistColumns", "6M %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Change over six months"), kCatPerf, K::HeatPct, kWlNeedHistory, false,
         8},
        {"perf_1y", QT_TRANSLATE_NOOP("WatchlistColumns", "1Y %"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Change over one year"), kCatPerf, K::HeatPct, kWlNeedHistory, false,
         8},
        {"from_high_52w", QT_TRANSLATE_NOOP("WatchlistColumns", "FROM 52W HI"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Distance below the 52-week high"), kCatPerf, K::Pct, kWlNeedQuote,
         false, 9},
        {"from_low_52w", QT_TRANSLATE_NOOP("WatchlistColumns", "FROM 52W LO"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Distance above the 52-week low"), kCatPerf, K::Pct, kWlNeedQuote,
         false, 9},
        {"vol_20d", QT_TRANSLATE_NOOP("WatchlistColumns", "20D VOL"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Annualised volatility of the last 20 daily returns"), kCatPerf,
         K::Number, kWlNeedHistory, false, 8},
        {"vs_ma50", QT_TRANSLATE_NOOP("WatchlistColumns", "VS 50D"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Last price vs the 50-day moving average"), kCatPerf, K::Pct,
         kWlNeedQuote, false, 8},
        {"vs_ma200", QT_TRANSLATE_NOOP("WatchlistColumns", "VS 200D"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Last price vs the 200-day moving average"), kCatPerf, K::Pct,
         kWlNeedQuote, false, 8},
        {"market_cap", QT_TRANSLATE_NOOP("WatchlistColumns", "MKT CAP"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Market capitalisation"), kCatFund, K::Money, kWlNeedInfo, false, 12},
        {"pe", QT_TRANSLATE_NOOP("WatchlistColumns", "P/E"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Trailing price / earnings"), kCatFund, K::Number, kWlNeedInfo, false,
         7},
        {"div_yield", QT_TRANSLATE_NOOP("WatchlistColumns", "YIELD"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Dividend yield"), kCatFund, K::Number, kWlNeedInfo, false, 7},
        {"beta", QT_TRANSLATE_NOOP("WatchlistColumns", "BETA"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Beta vs the market"), kCatFund, K::Number, kWlNeedInfo, false, 6},
        {"sector", QT_TRANSLATE_NOOP("WatchlistColumns", "SECTOR"), QT_TRANSLATE_NOOP("WatchlistColumns", "Sector"),
         kCatFund, K::Text, kWlNeedInfo, false, 16},
        {"industry", QT_TRANSLATE_NOOP("WatchlistColumns", "INDUSTRY"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Industry"), kCatFund, K::Text, kWlNeedInfo, false, 20},
        {"country", QT_TRANSLATE_NOOP("WatchlistColumns", "COUNTRY"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Country of the company"), kCatFund, K::Text, kWlNeedInfo, false, 12},
        {"exchange", QT_TRANSLATE_NOOP("WatchlistColumns", "EXCHANGE"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Listing exchange"), kCatIdentity, K::Text, kWlNeedQuote, false, 12},
        {"earnings", QT_TRANSLATE_NOOP("WatchlistColumns", "EARNINGS"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Next scheduled earnings report"), kCatEvents, K::Date, kWlNeedInfo,
         false, 13},
        {"ex_div", QT_TRANSLATE_NOOP("WatchlistColumns", "EX-DIV"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Latest or next ex-dividend date"), kCatEvents, K::Date, kWlNeedInfo,
         false, 13},
        {"group", QT_TRANSLATE_NOOP("WatchlistColumns", "GROUP"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Your own group for the symbol"), kCatUser, K::Text, kWlNeedNone, false,
         12},
        {"notes", QT_TRANSLATE_NOOP("WatchlistColumns", "NOTES"),
         QT_TRANSLATE_NOOP("WatchlistColumns", "Your notes on the symbol"), kCatUser, K::Text, kWlNeedNone, false, 24},
    };
    return kTable;
}

} // namespace

const QVector<WatchlistColumnDef>& all() {
    return wl_column_table();
}

int count() {
    return static_cast<int>(wl_column_table().size());
}

int index_of(const QString& id) {
    const auto& t = wl_column_table();
    for (int i = 0; i < t.size(); ++i) {
        if (id == QLatin1String(t[i].id))
            return i;
    }
    return -1;
}

QString id_at(int column) {
    const auto& t = wl_column_table();
    return column >= 0 && column < t.size() ? QString::fromLatin1(t[column].id) : QString();
}

const WatchlistColumnDef& def(int column) {
    const auto& t = wl_column_table();
    return t[column >= 0 && column < t.size() ? column : 0];
}

QString header(int column) {
    return QCoreApplication::translate(kWlColCtx, def(column).header);
}

QString tooltip(int column) {
    return QCoreApplication::translate(kWlColCtx, def(column).tip);
}

QString category(int column) {
    return QCoreApplication::translate(kWlColCtx, def(column).category);
}

QStringList categories() {
    QStringList out;
    for (const char* c : {kCatIdentity, kCatPrice, kCatSession, kCatVolume, kCatPerf, kCatFund, kCatEvents, kCatUser})
        out << QCoreApplication::translate(kWlColCtx, c);
    return out;
}

QStringList default_visible_ids() {
    QStringList out;
    for (const auto& d : wl_column_table()) {
        if (d.default_visible)
            out << QString::fromLatin1(d.id);
    }
    return out;
}

QStringList legacy_ids() {
    return {QStringLiteral("symbol"),   QStringLiteral("name"),    QStringLiteral("last"),
            QStringLiteral("change"),   QStringLiteral("change_pct"), QStringLiteral("day_high"),
            QStringLiteral("day_low"),  QStringLiteral("volume")};
}

} // namespace fincept::screens::watchlist_columns
