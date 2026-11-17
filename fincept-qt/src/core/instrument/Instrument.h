#pragma once
// Instrument — the one record that identifies a tradable / observable instrument
// (symbol system, plan E1). Header-only, Qt Core only, no global state: safe to
// use from any thread.
//
// Identity
//   `symbol` is the canonical MARKET-DATA KEY and stays the Yahoo form every
//   market:* topic, script and persisted row already uses ("RELIANCE.NS",
//   "^NSEI", "EURUSD=X", "BTC-USD", "GC=F", "BRK-B"). Empty when Yahoo has no
//   symbol for the instrument (an NFO contract, a venue Yahoo does not carry).
//
//   `id` is the stable identity, derived from the canonical symbol by
//   instrument_id_for() (VenueSymbols.h) so a lazily resolved row and a row from
//   the symbol master get the same id:
//     listed security  "<MIC>:<listing code>"   XNSE:RELIANCE, XHKG:0700, XKOS:247540
//     US listing       "US:<code>"              US:AAPL, US:BRK-B   (Yahoo's form does not say NYSE vs
//                                                Nasdaq, and a venue transfer must not change the id)
//     FX pair          "FX:<BASE><QUOTE>"       FX:EURUSD, FX:USDINR (INR=X)
//     crypto           "CRYPTO:<yahoo symbol>"  CRYPTO:BTC-USD, CRYPTO:TON11419-USD
//     index            "IDX:<symbol w/o ^>"     IDX:NSEI, IDX:NIFTY_FIN_SERVICE.NS
//     futures          "FUT:<root or contract>" FUT:GC (continuous GC=F), FUT:CLF27.NYM
//
// Equality
//   operator== compares every field (value equality, for tests and change
//   detection). same_instrument() is the identity question callers usually mean:
//   equal ids when both records carry one, else equal canonical symbols. Two
//   listings of one company (RELIANCE.NS / RELIANCE.BO, SAP.DE / SAP ADR) are
//   DIFFERENT instruments; sharing an ISIN does not make them the same.
//
// JSON
//   to_json() writes only non-default fields; from_json() accepts any subset and
//   ignores unknown keys, so records written by older or newer builds load.
//   asset_class / status are written as the lower-case slugs below.

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fincept::core::instrument {

/// Instrument class. The slugs (asset_class_slug) are the strings SymbolRef::asset_class uses.
enum class AssetClass : quint8 {
    Unknown = 0,
    Equity,    ///< "equity"    ordinary / preferred shares, ADRs, REITs
    Etf,       ///< "etf"       exchange-traded fund
    Fund,      ///< "fund"      mutual fund / money market fund
    Index,     ///< "index"     index level (^NSEI, ^GSPC, 000001.SS)
    Fx,        ///< "fx"        currency pair (EURUSD=X, INR=X)
    Crypto,    ///< "crypto"    crypto asset pair (BTC-USD)
    Future,    ///< "future"    futures, continuous (GC=F) or dated (CLF27.NYM)
    Option,    ///< "option"    option contract
    Bond,      ///< "bond"      bond / note
    Commodity, ///< "commodity" spot commodity (no Yahoo symbol carries this; commodity futures are Future)
};

/// Lower-case slug ("equity", "etf", ...); empty for Unknown.
inline QString asset_class_slug(AssetClass c) {
    switch (c) {
        case AssetClass::Equity:
            return QStringLiteral("equity");
        case AssetClass::Etf:
            return QStringLiteral("etf");
        case AssetClass::Fund:
            return QStringLiteral("fund");
        case AssetClass::Index:
            return QStringLiteral("index");
        case AssetClass::Fx:
            return QStringLiteral("fx");
        case AssetClass::Crypto:
            return QStringLiteral("crypto");
        case AssetClass::Future:
            return QStringLiteral("future");
        case AssetClass::Option:
            return QStringLiteral("option");
        case AssetClass::Bond:
            return QStringLiteral("bond");
        case AssetClass::Commodity:
            return QStringLiteral("commodity");
        case AssetClass::Unknown:
            break;
    }
    return {};
}

/// Slug or common synonym -> class, ASCII case-insensitive, surrounding blanks ignored.
/// Accepts our slugs plus "stock", "share", "dr", "adr", "mutualfund", "mutual_fund",
/// "moneymarket", "forex", "currency", "cryptocurrency", "spot", "swap", "futures",
/// "options" (Yahoo quoteType and TradingView type strings, lower-cased). Anything
/// else (including "") is Unknown.
inline AssetClass asset_class_from_slug(const QString& slug) {
    const QString s = slug.trimmed().toLower();
    if (s.isEmpty())
        return AssetClass::Unknown;
    static const QHash<QString, AssetClass> k = {
        {QStringLiteral("equity"), AssetClass::Equity},
        {QStringLiteral("stock"), AssetClass::Equity},
        {QStringLiteral("share"), AssetClass::Equity},
        {QStringLiteral("dr"), AssetClass::Equity},
        {QStringLiteral("adr"), AssetClass::Equity},
        {QStringLiteral("etf"), AssetClass::Etf},
        {QStringLiteral("fund"), AssetClass::Fund},
        {QStringLiteral("mutualfund"), AssetClass::Fund},
        {QStringLiteral("mutual_fund"), AssetClass::Fund},
        {QStringLiteral("moneymarket"), AssetClass::Fund},
        {QStringLiteral("index"), AssetClass::Index},
        {QStringLiteral("fx"), AssetClass::Fx},
        {QStringLiteral("forex"), AssetClass::Fx},
        {QStringLiteral("currency"), AssetClass::Fx},
        {QStringLiteral("crypto"), AssetClass::Crypto},
        {QStringLiteral("cryptocurrency"), AssetClass::Crypto},
        {QStringLiteral("spot"), AssetClass::Crypto},
        {QStringLiteral("swap"), AssetClass::Crypto},
        {QStringLiteral("future"), AssetClass::Future},
        {QStringLiteral("futures"), AssetClass::Future},
        {QStringLiteral("option"), AssetClass::Option},
        {QStringLiteral("options"), AssetClass::Option},
        {QStringLiteral("bond"), AssetClass::Bond},
        {QStringLiteral("commodity"), AssetClass::Commodity},
    };
    return k.value(s, AssetClass::Unknown);
}

/// Equity, Etf and Fund: the classes an equity order ticket / equity screen accepts.
inline bool is_equity_like(AssetClass c) {
    return c == AssetClass::Equity || c == AssetClass::Etf || c == AssetClass::Fund;
}

enum class InstrumentStatus : quint8 {
    Active = 0,
    Delisted, ///< no longer trades; kept so stored rows still resolve to a record
    Renamed,  ///< the listing continues under `successor_id` (never auto-migrated)
};

inline QString instrument_status_slug(InstrumentStatus s) {
    switch (s) {
        case InstrumentStatus::Delisted:
            return QStringLiteral("delisted");
        case InstrumentStatus::Renamed:
            return QStringLiteral("renamed");
        case InstrumentStatus::Active:
            break;
    }
    return QStringLiteral("active");
}

/// "active" / "delisted" / "renamed" (case-insensitive); anything else is Active.
inline InstrumentStatus instrument_status_from_slug(const QString& slug) {
    const QString s = slug.trimmed().toLower();
    if (s == QLatin1String("delisted"))
        return InstrumentStatus::Delisted;
    if (s == QLatin1String("renamed"))
        return InstrumentStatus::Renamed;
    return InstrumentStatus::Active;
}

struct Instrument {
    QString id;      ///< stable identity (see the header comment); empty = not resolved yet
    QString symbol;  ///< canonical market-data key = Yahoo form; empty = Yahoo has no symbol
    QString display; ///< what the UI shows: "RELIANCE", "NIFTY 50", "EUR/USD", "BTC/USD"
    QString name;    ///< issuer / instrument name as published (any script, accents kept)
    AssetClass asset_class = AssetClass::Unknown;
    QString quote_type; ///< the source's own type string ("EQUITY", "ETF", "stock", ...)
    QString venue;      ///< MarketSessions venue id ("NSE", "NASDAQ", "FX", "CRYPTO", "CME"), or a
                        ///< VenueSymbols listing-only id ("ATHEX", "BCBA") for venues without a session model
    QString mic;        ///< ISO 10383 MIC; empty for pseudo venues and when unknown
    QString country;    ///< ISO 3166-1 alpha-2 of the listing venue; empty for FX / crypto
    QString currency;   ///< trading currency as quoted, minor units kept ("GBp", "ZAc", "ILA", "KWF")
    QString isin;       ///< empty when unknown. A lookup key only — never stored as `symbol`.
    QString figi;       ///< empty when unknown
    /// Provider -> provider's own symbol. Keys: "yahoo", "tv" ("EXCH:SYM"), "binance",
    /// "coinbase", "kraken", "okx", "bybit", ... (exchange ids as ExchangeService uses them,
    /// value = that exchange's unified "BASE/QUOTE" pair), and "broker:<broker_id>" with value
    /// "EXCHANGE|SYMBOL|TOKEN[|BRSYMBOL]" — SYMBOL is the terminal's normalised broker symbol
    /// (instruments.symbol, what InstrumentService::find and the order APIs take), BRSYMBOL the
    /// broker-native spelling when it differs.
    QHash<QString, QString> provider_symbols;
    InstrumentStatus status = InstrumentStatus::Active;
    QString successor_id;        ///< set when status == Renamed (the UI offers it; nothing migrates silently)
    QStringList aliases;         ///< common names / codes users type ("NIFTY 50", "S&P 500", "gold", "SPX")
    QStringList former_symbols;  ///< former listing codes / tickers of THIS instrument ("TATAMOTORS" on TMPV)
    bool primary_listing = false; ///< the issuer's home / primary listing
    int rank_weight = 0;          ///< market-cap / liquidity bucket, comparable across venues (higher = bigger)

    bool is_valid() const { return !id.isEmpty() || !symbol.isEmpty(); }
    bool is_active() const { return status == InstrumentStatus::Active; }

    /// Provider symbol, or empty. "yahoo" falls back to `symbol`.
    QString provider_symbol(const QString& provider) const {
        const QString v = provider_symbols.value(provider);
        if (v.isEmpty() && provider == QLatin1String("yahoo"))
            return symbol;
        return v;
    }

    /// Same instrument: equal `id` when both have one; otherwise equal non-empty
    /// canonical `symbol` (exact, case-sensitive — canonical symbols are upper case).
    /// A record with neither is never the same as anything.
    bool same_instrument(const Instrument& o) const {
        if (!id.isEmpty() && !o.id.isEmpty())
            return id == o.id;
        return !symbol.isEmpty() && symbol == o.symbol;
    }

    bool operator==(const Instrument& o) const {
        return id == o.id && symbol == o.symbol && display == o.display && name == o.name &&
               asset_class == o.asset_class && quote_type == o.quote_type && venue == o.venue && mic == o.mic &&
               country == o.country && currency == o.currency && isin == o.isin && figi == o.figi &&
               provider_symbols == o.provider_symbols && status == o.status && successor_id == o.successor_id &&
               aliases == o.aliases && former_symbols == o.former_symbols && primary_listing == o.primary_listing &&
               rank_weight == o.rank_weight;
    }
    bool operator!=(const Instrument& o) const { return !(*this == o); }

    QJsonObject to_json() const {
        QJsonObject o;
        auto put = [&o](const char* key, const QString& v) {
            if (!v.isEmpty())
                o.insert(QLatin1String(key), v);
        };
        put("id", id);
        put("symbol", symbol);
        put("display", display);
        put("name", name);
        put("asset_class", asset_class_slug(asset_class));
        put("quote_type", quote_type);
        put("venue", venue);
        put("mic", mic);
        put("country", country);
        put("currency", currency);
        put("isin", isin);
        put("figi", figi);
        if (!provider_symbols.isEmpty()) {
            QJsonObject p;
            for (auto it = provider_symbols.constBegin(); it != provider_symbols.constEnd(); ++it)
                p.insert(it.key(), it.value());
            o.insert(QStringLiteral("provider_symbols"), p);
        }
        if (status != InstrumentStatus::Active)
            o.insert(QStringLiteral("status"), instrument_status_slug(status));
        put("successor_id", successor_id);
        if (!aliases.isEmpty())
            o.insert(QStringLiteral("aliases"), QJsonArray::fromStringList(aliases));
        if (!former_symbols.isEmpty())
            o.insert(QStringLiteral("former_symbols"), QJsonArray::fromStringList(former_symbols));
        if (primary_listing)
            o.insert(QStringLiteral("primary_listing"), true);
        if (rank_weight != 0)
            o.insert(QStringLiteral("rank_weight"), rank_weight);
        return o;
    }

    static Instrument from_json(const QJsonObject& o) {
        auto str = [&o](const char* key) { return o.value(QLatin1String(key)).toString(); };
        auto list = [&o](const char* key) {
            QStringList out;
            const QJsonArray a = o.value(QLatin1String(key)).toArray();
            for (const auto& v : a) {
                const QString s = v.toString();
                if (!s.isEmpty())
                    out.push_back(s);
            }
            return out;
        };
        Instrument r;
        r.id = str("id");
        r.symbol = str("symbol");
        r.display = str("display");
        r.name = str("name");
        r.asset_class = asset_class_from_slug(str("asset_class"));
        r.quote_type = str("quote_type");
        r.venue = str("venue");
        r.mic = str("mic");
        r.country = str("country");
        r.currency = str("currency");
        r.isin = str("isin");
        r.figi = str("figi");
        const QJsonObject p = o.value(QStringLiteral("provider_symbols")).toObject();
        for (auto it = p.constBegin(); it != p.constEnd(); ++it) {
            const QString v = it.value().toString();
            if (!v.isEmpty())
                r.provider_symbols.insert(it.key(), v);
        }
        r.status = instrument_status_from_slug(str("status"));
        r.successor_id = str("successor_id");
        r.aliases = list("aliases");
        r.former_symbols = list("former_symbols");
        r.primary_listing = o.value(QStringLiteral("primary_listing")).toBool(false);
        r.rank_weight = o.value(QStringLiteral("rank_weight")).toInt(0);
        return r;
    }
};

} // namespace fincept::core::instrument
