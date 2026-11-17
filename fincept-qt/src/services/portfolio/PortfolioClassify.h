// src/services/portfolio/PortfolioClassify.h
//
// Asset class and region of a holding — shared by the allocation breakdown
// (PortfolioAllocation) and the stress engine (screens/portfolio/PortfolioStress.h).
// Header-only, Qt Core only.
//
// Asset class comes from Yahoo's quoteType first (QuoteData / InfoData). Only a
// fund (ETF / MUTUALFUND) needs a second look — quoteType does not say what the
// fund holds — and only then are its name and a short list of well-known bond /
// commodity funds consulted. Without any quoteType the class is a guess from the
// ticker, and `guessed` says so.
#pragma once

#include <QString>
#include <QStringList>

#include <initializer_list>

namespace fincept::portfolio {

enum class AssetClass { Equity, Bond, Commodity, Crypto, Cash, RealEstate, Other };

struct AssetClassInfo {
    AssetClass cls = AssetClass::Equity;
    bool fund = false;    ///< ETF / mutual fund (class = what it holds, as far as known)
    bool guessed = false; ///< no quoteType — inferred from the ticker / name
};

inline QString asset_class_key(AssetClass c) {
    switch (c) {
        case AssetClass::Equity:
            return QStringLiteral("equity");
        case AssetClass::Bond:
            return QStringLiteral("bond");
        case AssetClass::Commodity:
            return QStringLiteral("commodity");
        case AssetClass::Crypto:
            return QStringLiteral("crypto");
        case AssetClass::Cash:
            return QStringLiteral("cash");
        case AssetClass::RealEstate:
            return QStringLiteral("real_estate");
        case AssetClass::Other:
            break;
    }
    return QStringLiteral("other");
}

namespace classify_detail {

inline bool name_has(const QString& upper_name, std::initializer_list<const char*> words) {
    for (const char* w : words)
        if (upper_name.contains(QLatin1String(w)))
            return true;
    return false;
}

inline AssetClass fund_holdings_class(const QString& sym, const QString& upper_name) {
    static const QStringList kBondFunds = {"TLT",  "AGG",  "BND",  "IEF", "SHY",  "LQD",  "HYG",  "EMB",  "TIP",
                                           "GOVT", "VGIT", "VGLT", "BSV", "BIV",  "MUB",  "SCHZ", "JNK",  "VCIT",
                                           "VCSH", "BNDX", "IGOV", "IBGL.L", "IGLT.L", "AGGG.L", "VAGF.L", "EUNA.DE"};
    static const QStringList kCommodityFunds = {"GLD", "IAU", "SLV", "USO", "DBC", "PDBC", "GSG", "UNG", "SGOL",
                                                "CPER", "PHAU.L", "SGLN.L", "IGLN.L", "4GLD.DE", "GOLDBEES.NS"};
    static const QStringList kRealEstateFunds = {"VNQ", "IYR", "SCHH", "XLRE", "RWR", "IPRP.L"};
    if (kBondFunds.contains(sym) ||
        name_has(upper_name, {"BOND", "TREASUR", "GILT", "AGGREGATE", "FIXED INCOME", "T-BILL", "MUNICIPAL",
                              "CORPORATE", "GOVT", "GOVERNMENT", "DEBT", "INCOME FUND"}))
        return AssetClass::Bond;
    if (kCommodityFunds.contains(sym) ||
        name_has(upper_name, {"GOLD", "SILVER", "COMMODIT", "OIL FUND", "CRUDE", "PRECIOUS", "PLATINUM", "COPPER"}))
        return AssetClass::Commodity;
    if (kRealEstateFunds.contains(sym) || name_has(upper_name, {"REAL ESTATE", "REIT", "PROPERTY"}))
        return AssetClass::RealEstate;
    if (name_has(upper_name, {"MONEY MARKET", "CASH FUND", "ULTRA SHORT", "LIQUIDITY"}))
        return AssetClass::Cash;
    if (name_has(upper_name, {"BITCOIN", "ETHER", "CRYPTO"}))
        return AssetClass::Crypto;
    return AssetClass::Equity;
}

} // namespace classify_detail

/// `quote_type` = Yahoo quoteType (any case), `name` = instrument name (may be empty).
inline AssetClassInfo classify_asset(const QString& symbol, const QString& quote_type, const QString& name = {}) {
    const QString sym = symbol.trimmed().toUpper();
    const QString qt = quote_type.trimmed().toUpper();
    const QString nm = name.toUpper();
    AssetClassInfo out;
    if (qt == QLatin1String("EQUITY")) {
        out.cls = classify_detail::name_has(nm, {" REIT", "REAL ESTATE INVESTMENT TRUST"}) ? AssetClass::RealEstate
                                                                                              : AssetClass::Equity;
        return out;
    }
    if (qt == QLatin1String("ETF") || qt == QLatin1String("MUTUALFUND")) {
        out.fund = true;
        out.cls = classify_detail::fund_holdings_class(sym, nm);
        return out;
    }
    if (qt == QLatin1String("CRYPTOCURRENCY")) {
        out.cls = AssetClass::Crypto;
        return out;
    }
    if (qt == QLatin1String("FUTURE")) {
        out.cls = AssetClass::Commodity; // index / rate futures are rare in a book; flagged by the symbol anyway
        return out;
    }
    if (qt == QLatin1String("CURRENCY") || qt == QLatin1String("MONEYMARKET")) {
        out.cls = AssetClass::Cash;
        return out;
    }
    if (qt == QLatin1String("INDEX")) {
        out.cls = AssetClass::Equity;
        return out;
    }
    if (!qt.isEmpty()) {
        out.cls = AssetClass::Other; // OPTION, ...
        return out;
    }
    // No quoteType: guess from the ticker.
    out.guessed = true;
    if (sym.endsWith(QLatin1String("-USD")) || sym.endsWith(QLatin1String("-USDT")) ||
        sym.endsWith(QLatin1String("-USDC")) || sym.endsWith(QLatin1String("-EUR")) ||
        sym.endsWith(QLatin1String("-GBP")))
        out.cls = AssetClass::Crypto;
    else if (sym.endsWith(QLatin1String("=F")))
        out.cls = AssetClass::Commodity;
    else if (sym.endsWith(QLatin1String("=X")))
        out.cls = AssetClass::Cash;
    else
        out.cls = classify_detail::fund_holdings_class(sym, nm); // known fund tickers, else equity
    return out;
}

/// Region bucket of a country — Yahoo country name ("United Kingdom") or ISO
/// alpha-2 ("GB"). Keys: "north_america", "uk", "europe", "japan", "china",
/// "india", "asia_pacific" (developed ex-Japan), "emerging", "" (unknown).
inline QString region_of_country(const QString& country) {
    const QString c = country.trimmed().toUpper();
    if (c.isEmpty())
        return {};
    auto any = [&c](std::initializer_list<const char*> xs) {
        for (const char* x : xs)
            if (c == QLatin1String(x))
                return true;
        return false;
    };
    if (any({"US", "USA", "UNITED STATES", "CA", "CANADA"}))
        return QStringLiteral("north_america");
    if (any({"GB", "UK", "UNITED KINGDOM", "JE", "JERSEY", "GG", "GUERNSEY", "IM", "ISLE OF MAN"}))
        return QStringLiteral("uk");
    if (any({"DE", "GERMANY", "FR", "FRANCE", "NL", "NETHERLANDS", "BE", "BELGIUM", "IT", "ITALY", "ES", "SPAIN",
             "PT", "PORTUGAL", "IE", "IRELAND", "AT", "AUSTRIA", "FI", "FINLAND", "LU", "LUXEMBOURG", "CH",
             "SWITZERLAND", "SE", "SWEDEN", "NO", "NORWAY", "DK", "DENMARK", "IS", "ICELAND", "GR", "GREECE"}))
        return QStringLiteral("europe");
    if (any({"JP", "JAPAN"}))
        return QStringLiteral("japan");
    if (any({"CN", "CHINA", "HK", "HONG KONG"}))
        return QStringLiteral("china");
    if (any({"IN", "INDIA"}))
        return QStringLiteral("india");
    // Developed Asia-Pacific ex-Japan (MSCI classification; Korea / Taiwan are emerging).
    if (any({"AU", "AUSTRALIA", "NZ", "NEW ZEALAND", "SG", "SINGAPORE", "IL", "ISRAEL"}))
        return QStringLiteral("asia_pacific");
    return QStringLiteral("emerging");
}

} // namespace fincept::portfolio
