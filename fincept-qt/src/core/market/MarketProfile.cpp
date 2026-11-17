#include "core/market/MarketProfile.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

#include <QCoreApplication>

namespace fincept::core::market {

namespace {

constexpr const char* kMpSettingsKey = "dashboard.market_region";
constexpr const char* kMpTrContext = "MarketProfile";

using MpList = QVector<ProfileInstrument>;

ProfileInstrument mp_item(const char* sym, const char* label, const char* group = "", double weight = 0) {
    return {QString::fromLatin1(sym), QString::fromUtf8(label), QString::fromLatin1(group), weight};
}

// ── Shared building blocks ──────────────────────────────────────────────────

MpList mp_crypto(const QString& ccy) {
    MpList l;
    if (ccy != QLatin1String("USD")) {
        l.append({QStringLiteral("BTC-") + ccy, QStringLiteral("BTC/") + ccy, {}, 0});
        l.append({QStringLiteral("ETH-") + ccy, QStringLiteral("ETH/") + ccy, {}, 0});
    }
    l += MpList{mp_item("BTC-USD", "Bitcoin"),  mp_item("ETH-USD", "Ethereum"), mp_item("SOL-USD", "Solana"),
                mp_item("XRP-USD", "XRP"),      mp_item("BNB-USD", "BNB"),      mp_item("DOGE-USD", "Dogecoin"),
                mp_item("ADA-USD", "Cardano")};
    return l;
}

const char* const kMpMetals = QT_TRANSLATE_NOOP("MarketProfile", "Metals");
const char* const kMpEnergy = QT_TRANSLATE_NOOP("MarketProfile", "Energy");
const char* const kMpAgri = QT_TRANSLATE_NOOP("MarketProfile", "Agriculture");

ProfileInstrument mp_cmd(const char* sym, const char* label, const char* group) {
    return mp_item(sym, label, group);
}

MpList mp_us_sectors() {
    // Approximate S&P 500 sector weights (percent) — cell area only.
    return {mp_item("XLK", QT_TRANSLATE_NOOP("MarketProfile", "Technology"), "", 32),
            mp_item("XLF", QT_TRANSLATE_NOOP("MarketProfile", "Financials"), "", 13.5),
            mp_item("XLY", QT_TRANSLATE_NOOP("MarketProfile", "Consumer Discretionary"), "", 10.5),
            mp_item("XLC", QT_TRANSLATE_NOOP("MarketProfile", "Communication Services"), "", 10),
            mp_item("XLV", QT_TRANSLATE_NOOP("MarketProfile", "Health Care"), "", 9),
            mp_item("XLI", QT_TRANSLATE_NOOP("MarketProfile", "Industrials"), "", 8.5),
            mp_item("XLP", QT_TRANSLATE_NOOP("MarketProfile", "Consumer Staples"), "", 5),
            mp_item("XLE", QT_TRANSLATE_NOOP("MarketProfile", "Energy"), "", 3),
            mp_item("XLU", QT_TRANSLATE_NOOP("MarketProfile", "Utilities"), "", 2.4),
            mp_item("XLRE", QT_TRANSLATE_NOOP("MarketProfile", "Real Estate"), "", 2),
            mp_item("XLB", QT_TRANSLATE_NOOP("MarketProfile", "Materials"), "", 1.8)};
}

MpList mp_global_sectors() {
    return {mp_item("IXN", QT_TRANSLATE_NOOP("MarketProfile", "Technology")),
            mp_item("IXG", QT_TRANSLATE_NOOP("MarketProfile", "Financials")),
            mp_item("IXJ", QT_TRANSLATE_NOOP("MarketProfile", "Health Care")),
            mp_item("RXI", QT_TRANSLATE_NOOP("MarketProfile", "Consumer Discretionary")),
            mp_item("IXP", QT_TRANSLATE_NOOP("MarketProfile", "Communication Services")),
            mp_item("EXI", QT_TRANSLATE_NOOP("MarketProfile", "Industrials")),
            mp_item("KXI", QT_TRANSLATE_NOOP("MarketProfile", "Consumer Staples")),
            mp_item("IXC", QT_TRANSLATE_NOOP("MarketProfile", "Energy")),
            mp_item("JXI", QT_TRANSLATE_NOOP("MarketProfile", "Utilities")),
            mp_item("MXI", QT_TRANSLATE_NOOP("MarketProfile", "Materials")),
            mp_item("REET", QT_TRANSLATE_NOOP("MarketProfile", "Real Estate"))};
}

MpList mp_stoxx_sectors() {
    return {mp_item("EXV1.DE", QT_TRANSLATE_NOOP("MarketProfile", "Banks")),
            mp_item("EXH2.DE", QT_TRANSLATE_NOOP("MarketProfile", "Financial Services")),
            mp_item("EXH5.DE", QT_TRANSLATE_NOOP("MarketProfile", "Insurance")),
            mp_item("EXV3.DE", QT_TRANSLATE_NOOP("MarketProfile", "Technology")),
            mp_item("EXV4.DE", QT_TRANSLATE_NOOP("MarketProfile", "Health Care")),
            mp_item("EXH4.DE", QT_TRANSLATE_NOOP("MarketProfile", "Industrial Goods & Services")),
            mp_item("EXV5.DE", QT_TRANSLATE_NOOP("MarketProfile", "Automobiles & Parts")),
            mp_item("EXH1.DE", QT_TRANSLATE_NOOP("MarketProfile", "Oil & Gas")),
            mp_item("EXH9.DE", QT_TRANSLATE_NOOP("MarketProfile", "Utilities")),
            mp_item("EXV6.DE", QT_TRANSLATE_NOOP("MarketProfile", "Basic Resources")),
            mp_item("EXV7.DE", QT_TRANSLATE_NOOP("MarketProfile", "Chemicals")),
            mp_item("EXV8.DE", QT_TRANSLATE_NOOP("MarketProfile", "Construction & Materials")),
            mp_item("EXH3.DE", QT_TRANSLATE_NOOP("MarketProfile", "Food & Beverage")),
            mp_item("EXH6.DE", QT_TRANSLATE_NOOP("MarketProfile", "Personal & Household Goods")),
            mp_item("EXH8.DE", QT_TRANSLATE_NOOP("MarketProfile", "Retail")),
            mp_item("EXH7.DE", QT_TRANSLATE_NOOP("MarketProfile", "Media")),
            mp_item("EXV2.DE", QT_TRANSLATE_NOOP("MarketProfile", "Telecommunications")),
            mp_item("EXV9.DE", QT_TRANSLATE_NOOP("MarketProfile", "Travel & Leisure")),
            mp_item("EXI5.DE", QT_TRANSLATE_NOOP("MarketProfile", "Real Estate"))};
}

MpList mp_india_sectors() {
    return {mp_item("^NSEBANK", QT_TRANSLATE_NOOP("MarketProfile", "Banks")),
            mp_item("NIFTY_FIN_SERVICE.NS", QT_TRANSLATE_NOOP("MarketProfile", "Financial Services")),
            mp_item("^CNXIT", QT_TRANSLATE_NOOP("MarketProfile", "IT")),
            mp_item("^CNXAUTO", QT_TRANSLATE_NOOP("MarketProfile", "Auto")),
            mp_item("^CNXPHARMA", QT_TRANSLATE_NOOP("MarketProfile", "Pharma")),
            mp_item("^CNXFMCG", QT_TRANSLATE_NOOP("MarketProfile", "FMCG")),
            mp_item("^CNXENERGY", QT_TRANSLATE_NOOP("MarketProfile", "Energy")),
            mp_item("^CNXMETAL", QT_TRANSLATE_NOOP("MarketProfile", "Metal")),
            mp_item("^CNXINFRA", QT_TRANSLATE_NOOP("MarketProfile", "Infrastructure")),
            mp_item("^CNXPSUBANK", QT_TRANSLATE_NOOP("MarketProfile", "PSU Banks")),
            mp_item("^CNXREALTY", QT_TRANSLATE_NOOP("MarketProfile", "Realty")),
            mp_item("^CNXMEDIA", QT_TRANSLATE_NOOP("MarketProfile", "Media"))};
}

MpList mp_topix17_sectors() {
    return {mp_item("1617.T", QT_TRANSLATE_NOOP("MarketProfile", "Foods")),
            mp_item("1618.T", QT_TRANSLATE_NOOP("MarketProfile", "Energy Resources")),
            mp_item("1619.T", QT_TRANSLATE_NOOP("MarketProfile", "Construction & Materials")),
            mp_item("1620.T", QT_TRANSLATE_NOOP("MarketProfile", "Raw Materials & Chemicals")),
            mp_item("1621.T", QT_TRANSLATE_NOOP("MarketProfile", "Pharmaceutical")),
            mp_item("1622.T", QT_TRANSLATE_NOOP("MarketProfile", "Automobiles & Transport Equipment")),
            mp_item("1623.T", QT_TRANSLATE_NOOP("MarketProfile", "Steel & Nonferrous Metals")),
            mp_item("1624.T", QT_TRANSLATE_NOOP("MarketProfile", "Machinery")),
            mp_item("1625.T", QT_TRANSLATE_NOOP("MarketProfile", "Electric Appliances & Precision")),
            mp_item("1626.T", QT_TRANSLATE_NOOP("MarketProfile", "IT & Services")),
            mp_item("1627.T", QT_TRANSLATE_NOOP("MarketProfile", "Electric Power & Gas")),
            mp_item("1628.T", QT_TRANSLATE_NOOP("MarketProfile", "Transportation & Logistics")),
            mp_item("1629.T", QT_TRANSLATE_NOOP("MarketProfile", "Commercial & Wholesale Trade")),
            mp_item("1630.T", QT_TRANSLATE_NOOP("MarketProfile", "Retail Trade")),
            mp_item("1631.T", QT_TRANSLATE_NOOP("MarketProfile", "Banks")),
            mp_item("1632.T", QT_TRANSLATE_NOOP("MarketProfile", "Financials (ex Banks)")),
            mp_item("1633.T", QT_TRANSLATE_NOOP("MarketProfile", "Real Estate"))};
}

MpList mp_asx_sectors() {
    return {mp_item("^AXFJ", QT_TRANSLATE_NOOP("MarketProfile", "Financials")),
            mp_item("^AXMJ", QT_TRANSLATE_NOOP("MarketProfile", "Materials")),
            mp_item("^AXHJ", QT_TRANSLATE_NOOP("MarketProfile", "Health Care")),
            mp_item("^AXDJ", QT_TRANSLATE_NOOP("MarketProfile", "Consumer Discretionary")),
            mp_item("^AXNJ", QT_TRANSLATE_NOOP("MarketProfile", "Industrials")),
            mp_item("^AXPJ", QT_TRANSLATE_NOOP("MarketProfile", "Real Estate (A-REIT)")),
            mp_item("^AXEJ", QT_TRANSLATE_NOOP("MarketProfile", "Energy")),
            mp_item("^AXSJ", QT_TRANSLATE_NOOP("MarketProfile", "Consumer Staples")),
            mp_item("^AXTJ", QT_TRANSLATE_NOOP("MarketProfile", "Communication Services")),
            mp_item("^AXIJ", QT_TRANSLATE_NOOP("MarketProfile", "Technology")),
            mp_item("^AXUJ", QT_TRANSLATE_NOOP("MarketProfile", "Utilities"))};
}

MpList mp_tsx_sectors() {
    return {mp_item("XFN.TO", QT_TRANSLATE_NOOP("MarketProfile", "Financials")),
            mp_item("XEG.TO", QT_TRANSLATE_NOOP("MarketProfile", "Energy")),
            mp_item("XMA.TO", QT_TRANSLATE_NOOP("MarketProfile", "Materials")),
            mp_item("XIT.TO", QT_TRANSLATE_NOOP("MarketProfile", "Technology")),
            mp_item("XRE.TO", QT_TRANSLATE_NOOP("MarketProfile", "Real Estate")),
            mp_item("XUT.TO", QT_TRANSLATE_NOOP("MarketProfile", "Utilities")),
            mp_item("XST.TO", QT_TRANSLATE_NOOP("MarketProfile", "Consumer Staples"))};
}

const char* const kMpSetUs = QT_TRANSLATE_NOOP("MarketProfile", "S&P 500 sectors (SPDR ETFs)");
const char* const kMpSetGlobal = QT_TRANSLATE_NOOP("MarketProfile", "Global sectors (iShares Global ETFs, US-listed)");
const char* const kMpSetStoxx = QT_TRANSLATE_NOOP("MarketProfile", "STOXX Europe 600 sectors (iShares ETFs, Xetra)");
const char* const kMpSetNse = QT_TRANSLATE_NOOP("MarketProfile", "NSE sectoral indices");
const char* const kMpSetTopix = QT_TRANSLATE_NOOP("MarketProfile", "TOPIX-17 sectors (NEXT FUNDS ETFs)");
const char* const kMpSetAsx = QT_TRANSLATE_NOOP("MarketProfile", "S&P/ASX 200 sector indices");
const char* const kMpSetTsx = QT_TRANSLATE_NOOP("MarketProfile", "S&P/TSX capped sectors (iShares ETFs)");

QStringList mp_us_movers() {
    return {"AAPL", "MSFT", "NVDA", "AMZN", "GOOGL", "META", "TSLA", "AVGO", "JPM", "LLY", "V",  "XOM", "UNH",
            "MA",   "WMT",  "COST", "HD",   "PG",    "JNJ",  "NFLX", "ORCL", "BAC", "CRM", "AMD", "KO", "PEP"};
}

QVector<MarketProfile> mp_build_all() {
    QVector<MarketProfile> out;
    const MpList us_cmd = {mp_cmd("GC=F", "Gold", kMpMetals),         mp_cmd("SI=F", "Silver", kMpMetals),
                           mp_cmd("HG=F", "Copper", kMpMetals),       mp_cmd("CL=F", "WTI Crude", kMpEnergy),
                           mp_cmd("BZ=F", "Brent Crude", kMpEnergy),  mp_cmd("NG=F", "Natural Gas (HH)", kMpEnergy),
                           mp_cmd("RB=F", "Gasoline (RBOB)", kMpEnergy), mp_cmd("ZC=F", "Corn", kMpAgri),
                           mp_cmd("ZW=F", "Wheat", kMpAgri),          mp_cmd("ZS=F", "Soybeans", kMpAgri)};
    const MpList eu_cmd = {mp_cmd("BZ=F", "Brent Crude", kMpEnergy),  mp_cmd("TTF=F", "Dutch TTF Gas", kMpEnergy),
                           mp_cmd("CL=F", "WTI Crude", kMpEnergy),    mp_cmd("NG=F", "Natural Gas (HH)", kMpEnergy),
                           mp_cmd("GC=F", "Gold", kMpMetals),         mp_cmd("SI=F", "Silver", kMpMetals),
                           mp_cmd("HG=F", "Copper", kMpMetals),       mp_cmd("PL=F", "Platinum", kMpMetals),
                           mp_cmd("ZW=F", "Wheat", kMpAgri),          mp_cmd("CC=F", "Cocoa", kMpAgri)};
    const MpList asia_cmd = {mp_cmd("GC=F", "Gold", kMpMetals),       mp_cmd("SI=F", "Silver", kMpMetals),
                             mp_cmd("HG=F", "Copper", kMpMetals),     mp_cmd("PL=F", "Platinum", kMpMetals),
                             mp_cmd("BZ=F", "Brent Crude", kMpEnergy), mp_cmd("CL=F", "WTI Crude", kMpEnergy),
                             mp_cmd("NG=F", "Natural Gas (HH)", kMpEnergy), mp_cmd("ZS=F", "Soybeans", kMpAgri),
                             mp_cmd("ZW=F", "Wheat", kMpAgri),        mp_cmd("SB=F", "Sugar", kMpAgri)};

    // ── United States ────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "US";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "United States");
        p.base_currency = "USD";
        p.countries = {"US"};
        p.benchmark = "^GSPC";
        p.volatility = {"^VIX", "^VIX9D", "^VIX3M", "^VVIX"};
        p.screener_regions = {"us"};
        p.large_cap_floor = 2e9;
        p.news_region = "US";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^GSPC", "S&P 500"),      mp_item("^DJI", "Dow Jones"),
                     mp_item("^IXIC", "Nasdaq Comp."), mp_item("^NDX", "Nasdaq 100"),
                     mp_item("^RUT", "Russell 2000"),  mp_item("^SP400", "S&P MidCap 400"),
                     mp_item("^NYA", "NYSE Composite"), mp_item("^SOX", "PHLX Semiconductor"),
                     mp_item("^DJT", "Dow Transports")};
        p.fx = {mp_item("DX-Y.NYB", "US Dollar Index"), mp_item("EURUSD=X", "EUR/USD"), mp_item("USDJPY=X", "USD/JPY"),
                mp_item("GBPUSD=X", "GBP/USD"),        mp_item("USDCAD=X", "USD/CAD"), mp_item("AUDUSD=X", "AUD/USD"),
                mp_item("USDCHF=X", "USD/CHF"),        mp_item("USDCNY=X", "USD/CNY"), mp_item("USDMXN=X", "USD/MXN")};
        p.commodities = us_cmd;
        p.crypto = mp_crypto("USD");
        p.sectors = mp_us_sectors();
        p.sector_set_name = kMpSetUs;
        p.movers = mp_us_movers();
        p.venues = {"NYSE", "NASDAQ", "CME"};
        out.append(p);
    }
    // ── Canada ───────────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "CA";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "Canada");
        p.base_currency = "CAD";
        p.countries = {"CA", "US"};
        p.benchmark = "^GSPTSE";
        p.volatility = {"^VIX"};
        p.screener_regions = {"ca"};
        p.large_cap_floor = 2e9;
        p.rates_curve = "treasury";
        p.indices = {mp_item("^GSPTSE", "S&P/TSX Comp."), mp_item("^GSPC", "S&P 500"), mp_item("^DJI", "Dow Jones"),
                     mp_item("^IXIC", "Nasdaq Comp."),   mp_item("^RUT", "Russell 2000")};
        p.fx = {mp_item("USDCAD=X", "USD/CAD"), mp_item("EURCAD=X", "EUR/CAD"), mp_item("GBPCAD=X", "GBP/CAD"),
                mp_item("CADJPY=X", "CAD/JPY"), mp_item("AUDCAD=X", "AUD/CAD"), mp_item("EURUSD=X", "EUR/USD"),
                mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = us_cmd;
        p.crypto = mp_crypto("CAD");
        p.sectors = mp_tsx_sectors();
        p.sector_set_name = kMpSetTsx;
        p.movers = {"RY.TO", "TD.TO",  "SHOP.TO", "ENB.TO", "CNR.TO", "CP.TO",  "BMO.TO",
                    "BN.TO", "CNQ.TO", "SU.TO",   "TRI.TO", "ATD.TO", "BNS.TO", "CM.TO"};
        p.venues = {"TSX", "NYSE"};
        out.append(p);
    }
    // ── Brazil / Latin America ───────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "BR";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "Brazil");
        p.base_currency = "BRL";
        p.countries = {"BR", "US"};
        p.benchmark = "^BVSP";
        p.volatility = {"^VIX"};
        p.screener_regions = {"br"};
        p.large_cap_floor = 1e10;
        p.rates_curve = "treasury";
        // No Merval: Yahoo returns no data for ^MERV (or any Merval ticker) any more.
        p.indices = {mp_item("^BVSP", "Ibovespa"), mp_item("^MXX", "IPC Mexico"), mp_item("^GSPC", "S&P 500"),
                     mp_item("^DJI", "Dow Jones")};
        p.fx = {mp_item("USDBRL=X", "USD/BRL"), mp_item("EURBRL=X", "EUR/BRL"), mp_item("USDMXN=X", "USD/MXN"),
                mp_item("EURUSD=X", "EUR/USD"), mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = {mp_cmd("BZ=F", "Brent Crude", kMpEnergy), mp_cmd("CL=F", "WTI Crude", kMpEnergy),
                         mp_cmd("GC=F", "Gold", kMpMetals),         mp_cmd("SI=F", "Silver", kMpMetals),
                         mp_cmd("HG=F", "Copper", kMpMetals),       mp_cmd("KC=F", "Coffee", kMpAgri),
                         mp_cmd("SB=F", "Sugar", kMpAgri),          mp_cmd("ZS=F", "Soybeans", kMpAgri),
                         mp_cmd("ZC=F", "Corn", kMpAgri),           mp_cmd("LE=F", "Live Cattle", kMpAgri)};
        p.crypto = mp_crypto("USD"); // Yahoo has no BTC-BRL / ETH-BRL
        p.sectors = mp_global_sectors();
        p.sector_set_name = kMpSetGlobal;
        p.movers = {"PETR4.SA", "VALE3.SA", "ITUB4.SA", "BBDC4.SA", "ABEV3.SA", "BBAS3.SA",
                    "WEGE3.SA", "B3SA3.SA", "RENT3.SA", "SUZB3.SA", "PRIO3.SA", "AXIA3.SA"}; // AXIA3 = ex-Eletrobras ELET3
        p.venues = {"B3", "BMV", "NYSE"};
        out.append(p);
    }
    // ── United Kingdom ───────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "UK";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "United Kingdom");
        p.base_currency = "GBP";
        p.countries = {"GB", "EU", "US"};
        p.benchmark = "^FTSE";
        p.volatility = {"^VIX"};
        p.screener_regions = {"gb"};
        p.large_cap_floor = 1e11; // GBp: Yahoo compares LSE caps in pence
        p.news_region = "UK";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^FTSE", "FTSE 100"),     mp_item("^FTMC", "FTSE 250"),
                     mp_item("^FTAS", "FTSE All-Share"), mp_item("^FTAI", "FTSE AIM All-Share"),
                     mp_item("^STOXX", "STOXX 600"),   mp_item("^STOXX50E", "Euro Stoxx 50"),
                     mp_item("^GDAXI", "DAX"),         mp_item("^GSPC", "S&P 500")};
        p.fx = {mp_item("GBPUSD=X", "GBP/USD"), mp_item("EURGBP=X", "EUR/GBP"), mp_item("GBPJPY=X", "GBP/JPY"),
                mp_item("GBPCHF=X", "GBP/CHF"), mp_item("EURUSD=X", "EUR/USD"), mp_item("USDJPY=X", "USD/JPY"),
                mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = eu_cmd;
        p.crypto = mp_crypto("GBP");
        p.sectors = mp_stoxx_sectors();
        p.sector_set_name = kMpSetStoxx;
        p.movers = {"AZN.L",  "SHEL.L", "HSBA.L", "ULVR.L", "BP.L",   "RIO.L",  "GSK.L", "REL.L",
                    "DGE.L",  "BATS.L", "LSEG.L", "BARC.L", "LLOY.L", "NG.L",   "GLEN.L", "RR.L"};
        p.venues = {"LSE", "XETRA", "PARIS", "NYSE"};
        out.append(p);
    }
    // ── Eurozone ─────────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "EU";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "Eurozone");
        p.base_currency = "EUR";
        p.countries = {"EU", "DE", "FR", "IT", "ES", "NL"};
        p.benchmark = "^STOXX";
        p.volatility = {"^VIX"};
        p.screener_regions = {"de", "fr", "it", "es", "nl", "be", "at", "ie", "pt", "fi"};
        p.large_cap_floor = 2e9;
        p.news_region = "EU";
        p.rates_curve = "euro-curve";
        p.indices = {mp_item("^STOXX50E", "Euro Stoxx 50"), mp_item("^STOXX", "STOXX 600"),
                     mp_item("^GDAXI", "DAX"),              mp_item("^FCHI", "CAC 40"),
                     mp_item("FTSEMIB.MI", "FTSE MIB"),     mp_item("^IBEX", "IBEX 35"),
                     mp_item("^AEX", "AEX"),                mp_item("^SSMI", "SMI")};
        p.fx = {mp_item("EURUSD=X", "EUR/USD"), mp_item("EURGBP=X", "EUR/GBP"), mp_item("EURJPY=X", "EUR/JPY"),
                mp_item("EURCHF=X", "EUR/CHF"), mp_item("EURCNY=X", "EUR/CNY"), mp_item("EURSEK=X", "EUR/SEK"),
                mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = eu_cmd;
        p.crypto = mp_crypto("EUR");
        p.sectors = mp_stoxx_sectors();
        p.sector_set_name = kMpSetStoxx;
        p.movers = {"ASML.AS", "SAP.DE", "MC.PA",  "SIE.DE",  "TTE.PA",  "SAN.PA", "OR.PA",   "ALV.DE",
                    "AIR.PA",  "RMS.PA", "DTE.DE", "BNP.PA",  "IBE.MC",  "ENEL.MI", "SU.PA", "INGA.AS"};
        p.venues = {"XETRA", "PARIS", "AMSTERDAM", "MILAN", "BME", "SIX"};
        out.append(p);
    }
    // ── India ────────────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "IN";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "India");
        p.base_currency = "INR";
        p.countries = {"IN", "US"};
        p.benchmark = "^NSEI";
        p.volatility = {"^INDIAVIX", "^VIX"};
        p.screener_regions = {"in"};
        p.large_cap_floor = 1e11;
        p.news_region = "INDIA";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^NSEI", "Nifty 50"),    mp_item("^BSESN", "Sensex"),     mp_item("^NSEBANK", "Bank Nifty"),
                     mp_item("^CNXIT", "Nifty IT"),   mp_item("^GSPC", "S&P 500"),     mp_item("^N225", "Nikkei 225"),
                     mp_item("^HSI", "Hang Seng")};
        p.fx = {mp_item("USDINR=X", "USD/INR"), mp_item("EURINR=X", "EUR/INR"), mp_item("GBPINR=X", "GBP/INR"),
                mp_item("JPYINR=X", "JPY/INR"), mp_item("EURUSD=X", "EUR/USD"), mp_item("USDCNY=X", "USD/CNY"),
                mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = asia_cmd;
        p.crypto = mp_crypto("INR");
        p.sectors = mp_india_sectors();
        p.sector_set_name = kMpSetNse;
        p.movers = {"RELIANCE.NS",  "HDFCBANK.NS",   "ICICIBANK.NS", "INFY.NS",   "TCS.NS",      "BHARTIARTL.NS",
                    "ITC.NS",       "LT.NS",         "SBIN.NS",      "HINDUNILVR.NS", "KOTAKBANK.NS", "AXISBANK.NS",
                    "BAJFINANCE.NS", "MARUTI.NS",    "SUNPHARMA.NS", "TMPV.NS"}; // TMPV = Tata Motors after the demerger
        p.venues = {"NSE", "BSE"};
        out.append(p);
    }
    // ── Japan ────────────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "JP";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "Japan");
        p.base_currency = "JPY";
        p.countries = {"JP", "US"};
        p.benchmark = "^N225";
        p.volatility = {"^VIX"};
        p.screener_regions = {"jp"};
        p.large_cap_floor = 3e11;
        p.news_region = "ASIA";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^N225", "Nikkei 225"), mp_item("1306.T", "TOPIX ETF (1306)"),
                     mp_item("^HSI", "Hang Seng"),   mp_item("000001.SS", "SSE Composite"),
                     mp_item("^KS11", "KOSPI"),      mp_item("^TWII", "TAIEX"),
                     mp_item("^GSPC", "S&P 500"),    mp_item("^IXIC", "Nasdaq Comp.")};
        p.fx = {mp_item("USDJPY=X", "USD/JPY"), mp_item("EURJPY=X", "EUR/JPY"), mp_item("GBPJPY=X", "GBP/JPY"),
                mp_item("AUDJPY=X", "AUD/JPY"), mp_item("CNYJPY=X", "CNY/JPY"), mp_item("EURUSD=X", "EUR/USD"),
                mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = asia_cmd;
        p.crypto = mp_crypto("JPY");
        p.sectors = mp_topix17_sectors();
        p.sector_set_name = kMpSetTopix;
        p.movers = {"7203.T", "6758.T", "8306.T", "9984.T", "6861.T", "9432.T", "8035.T", "6501.T",
                    "4063.T", "7974.T", "8316.T", "9983.T", "6098.T", "4502.T", "8058.T", "7011.T"};
        p.venues = {"TSE", "HKEX", "SSE"};
        out.append(p);
    }
    // ── Greater China ────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "CN";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "China & Hong Kong");
        p.base_currency = "CNY"; // owner decision: the region's display currency is the yuan (lead index 000001.SS)
        p.countries = {"CN", "HK", "US"};
        p.benchmark = "^HSI";
        p.volatility = {"^VIX"};
        p.screener_regions = {"hk", "cn"};
        p.large_cap_floor = 1.5e10;
        p.news_region = "ASIA";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^HSI", "Hang Seng"),          mp_item("^HSCE", "HS China Ent."),
                     mp_item("000001.SS", "SSE Composite"), mp_item("399001.SZ", "SZSE Component"),
                     mp_item("000300.SS", "CSI 300"),       mp_item("^TWII", "TAIEX"),
                     mp_item("^N225", "Nikkei 225"),        mp_item("^GSPC", "S&P 500")};
        p.fx = {mp_item("USDCNY=X", "USD/CNY"), mp_item("CNH=X", "USD/CNH"), mp_item("USDHKD=X", "USD/HKD"),
                mp_item("EURCNY=X", "EUR/CNY"), mp_item("CNYJPY=X", "CNY/JPY"), mp_item("USDTWD=X", "USD/TWD"),
                mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = asia_cmd;
        p.crypto = mp_crypto("USD");
        p.sectors = mp_global_sectors();
        p.sector_set_name = kMpSetGlobal;
        p.movers = {"0700.HK", "9988.HK", "3690.HK", "1299.HK", "0939.HK", "0005.HK",
                    "1810.HK", "2318.HK", "0941.HK", "9618.HK", "1211.HK", "0388.HK"};
        p.venues = {"HKEX", "SSE", "SZSE", "TWSE"};
        out.append(p);
    }
    // ── South Korea ──────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "KR";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "South Korea");
        p.base_currency = "KRW";
        p.countries = {"KR", "US"};
        p.benchmark = "^KS11";
        p.volatility = {"^VIX"};
        p.screener_regions = {"kr"};
        p.large_cap_floor = 2.5e12;
        p.news_region = "ASIA";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^KS11", "KOSPI"),        mp_item("^KQ11", "KOSDAQ"), mp_item("^N225", "Nikkei 225"),
                     mp_item("^HSI", "Hang Seng"),     mp_item("000001.SS", "SSE Composite"),
                     mp_item("^GSPC", "S&P 500")};
        p.fx = {mp_item("USDKRW=X", "USD/KRW"), mp_item("EURKRW=X", "EUR/KRW"), mp_item("JPYKRW=X", "JPY/KRW"),
                mp_item("CNYKRW=X", "CNY/KRW"), mp_item("USDJPY=X", "USD/JPY"), mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = asia_cmd;
        p.crypto = mp_crypto("KRW");
        p.sectors = mp_global_sectors();
        p.sector_set_name = kMpSetGlobal;
        p.movers = {"005930.KS", "000660.KS", "373220.KS", "207940.KS", "005380.KS",
                    "000270.KS", "068270.KS", "035420.KS", "105560.KS", "055550.KS"};
        p.venues = {"KRX", "TSE", "HKEX"};
        out.append(p);
    }
    // ── Australia & New Zealand ──────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "AU";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "Australia & New Zealand");
        p.base_currency = "AUD";
        p.countries = {"AU", "NZ", "US"};
        p.benchmark = "^AXJO";
        p.volatility = {"^VIX"};
        p.screener_regions = {"au"};
        p.large_cap_floor = 2e9;
        p.news_region = "ASIA";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^AXJO", "S&P/ASX 200"), mp_item("^AORD", "All Ordinaries"), mp_item("^NZ50", "NZX 50"),
                     mp_item("^N225", "Nikkei 225"),  mp_item("^HSI", "Hang Seng"),       mp_item("^GSPC", "S&P 500")};
        p.fx = {mp_item("AUDUSD=X", "AUD/USD"), mp_item("NZDUSD=X", "NZD/USD"), mp_item("AUDJPY=X", "AUD/JPY"),
                mp_item("EURAUD=X", "EUR/AUD"), mp_item("AUDCNY=X", "AUD/CNY"), mp_item("AUDNZD=X", "AUD/NZD"),
                mp_item("DX-Y.NYB", "US Dollar Index")};
        p.commodities = asia_cmd;
        p.crypto = mp_crypto("AUD");
        p.sectors = mp_asx_sectors();
        p.sector_set_name = kMpSetAsx;
        p.movers = {"BHP.AX", "CBA.AX", "CSL.AX", "NAB.AX", "WBC.AX", "ANZ.AX", "WES.AX",
                    "MQG.AX", "FMG.AX", "WDS.AX", "TLS.AX", "RIO.AX", "GMG.AX", "TCL.AX"};
        p.venues = {"ASX", "NZX", "TSE"};
        out.append(p);
    }
    // ── Global ───────────────────────────────────────────────────────────────
    {
        MarketProfile p;
        p.id = "GLOBAL";
        p.name = QT_TRANSLATE_NOOP("MarketProfile", "Global");
        p.base_currency = "USD";
        p.benchmark = "^GSPC";
        p.volatility = {"^VIX", "^VIX3M"};
        p.screener_regions = {"us"};
        p.large_cap_floor = 2e9;
        p.news_region = "GLOBAL";
        p.rates_curve = "treasury";
        p.indices = {mp_item("^GSPC", "S&P 500"),         mp_item("^IXIC", "Nasdaq Comp."),
                     mp_item("^STOXX50E", "Euro Stoxx 50"), mp_item("^FTSE", "FTSE 100"),
                     mp_item("^GDAXI", "DAX"),             mp_item("^N225", "Nikkei 225"),
                     mp_item("^HSI", "Hang Seng"),         mp_item("000001.SS", "SSE Composite"),
                     mp_item("^NSEI", "Nifty 50"),         mp_item("^AXJO", "S&P/ASX 200"),
                     mp_item("^BVSP", "Ibovespa")};
        p.fx = {mp_item("DX-Y.NYB", "US Dollar Index"), mp_item("EURUSD=X", "EUR/USD"), mp_item("USDJPY=X", "USD/JPY"),
                mp_item("GBPUSD=X", "GBP/USD"),        mp_item("USDCNY=X", "USD/CNY"), mp_item("AUDUSD=X", "AUD/USD"),
                mp_item("USDCHF=X", "USD/CHF"),        mp_item("USDINR=X", "USD/INR")};
        p.commodities = us_cmd;
        p.crypto = mp_crypto("USD");
        p.sectors = mp_global_sectors();
        p.sector_set_name = kMpSetGlobal;
        p.movers = mp_us_movers();
        p.venues = {"NYSE", "LSE", "XETRA", "TSE", "HKEX", "NSE"};
        out.append(p);
    }
    return out;
}

GlobalIndex mp_gi(const char* sym, const char* label, const char* region, const char* venue) {
    GlobalIndex g;
    g.symbol = QString::fromLatin1(sym);
    g.label = QString::fromUtf8(label);
    g.region = QString::fromLatin1(region);
    g.venue = QString::fromLatin1(venue);
    return g;
}

} // namespace

const QVector<GlobalIndex>& global_index_universe() {
    static const char* const am = QT_TRANSLATE_NOOP("MarketProfile", "Americas");
    static const char* const eu = QT_TRANSLATE_NOOP("MarketProfile", "Europe");
    static const char* const me = QT_TRANSLATE_NOOP("MarketProfile", "Middle East & Africa");
    static const char* const ap = QT_TRANSLATE_NOOP("MarketProfile", "Asia-Pacific");
    static const QVector<GlobalIndex> kAll = {
        mp_gi("^GSPC", "S&P 500", am, "NYSE"),          mp_gi("^DJI", "Dow Jones", am, "NYSE"),
        mp_gi("^IXIC", "Nasdaq Comp.", am, "NASDAQ"),   mp_gi("^RUT", "Russell 2000", am, "NYSE"),
        mp_gi("^GSPTSE", "S&P/TSX Comp.", am, "TSX"),   mp_gi("^BVSP", "Ibovespa", am, "B3"),
        mp_gi("^MXX", "IPC Mexico", am, "BMV"),
        mp_gi("^STOXX50E", "Euro Stoxx 50", eu, "XETRA"), mp_gi("^STOXX", "STOXX 600", eu, "XETRA"),
        mp_gi("^FTSE", "FTSE 100", eu, "LSE"),          mp_gi("^GDAXI", "DAX", eu, "XETRA"),
        mp_gi("^FCHI", "CAC 40", eu, "PARIS"),          mp_gi("FTSEMIB.MI", "FTSE MIB", eu, "MILAN"),
        mp_gi("^IBEX", "IBEX 35", eu, "BME"),           mp_gi("^AEX", "AEX", eu, "AMSTERDAM"),
        mp_gi("^SSMI", "SMI", eu, "SIX"),
        mp_gi("^TASI.SR", "Tadawul All Share", me, "TADAWUL"), mp_gi("^TA125.TA", "TA-125", me, "TASE"),
        mp_gi("^N225", "Nikkei 225", ap, "TSE"),        mp_gi("^HSI", "Hang Seng", ap, "HKEX"),
        mp_gi("000001.SS", "SSE Composite", ap, "SSE"), mp_gi("399001.SZ", "SZSE Component", ap, "SZSE"),
        mp_gi("^KS11", "KOSPI", ap, "KRX"),             mp_gi("^TWII", "TAIEX", ap, "TWSE"),
        mp_gi("^NSEI", "Nifty 50", ap, "NSE"),          mp_gi("^BSESN", "Sensex", ap, "BSE"),
        mp_gi("^STI", "Straits Times", ap, "SGX"),      mp_gi("^AXJO", "S&P/ASX 200", ap, "ASX"),
        mp_gi("^NZ50", "NZX 50", ap, "NZX"),
    };
    return kAll;
}

const QVector<MarketProfile>& all_market_profiles() {
    static const QVector<MarketProfile> kAll = mp_build_all();
    return kAll;
}

const MarketProfile& market_profile(const QString& id) {
    const auto& all = all_market_profiles();
    for (const auto& p : all)
        if (p.id.compare(id, Qt::CaseInsensitive) == 0)
            return p;
    return all.last(); // GLOBAL
}

QString market_profile_name(const MarketProfile& profile) {
    return QCoreApplication::translate(kMpTrContext, profile.name.toUtf8().constData());
}

QString profile_text(const QString& english) {
    if (english.isEmpty())
        return english;
    return QCoreApplication::translate(kMpTrContext, english.toUtf8().constData());
}

QString region_for_locale(const QLocale& locale) {
    switch (locale.territory()) {
        case QLocale::UnitedStates:
        case QLocale::PuertoRico:
            return QStringLiteral("US");
        case QLocale::Canada:
            return QStringLiteral("CA");
        case QLocale::Brazil:
        case QLocale::Mexico:
        case QLocale::Argentina:
        case QLocale::Chile:
        case QLocale::Colombia:
        case QLocale::Peru:
            return QStringLiteral("BR");
        case QLocale::UnitedKingdom:
        case QLocale::Guernsey:
        case QLocale::Jersey:
        case QLocale::IsleOfMan:
        case QLocale::Gibraltar:
            return QStringLiteral("UK");
        case QLocale::Germany:
        case QLocale::France:
        case QLocale::Italy:
        case QLocale::Spain:
        case QLocale::Netherlands:
        case QLocale::Belgium:
        case QLocale::Austria:
        case QLocale::Ireland:
        case QLocale::Portugal:
        case QLocale::Finland:
        case QLocale::Greece:
        case QLocale::Luxembourg:
        case QLocale::Slovakia:
        case QLocale::Slovenia:
        case QLocale::Estonia:
        case QLocale::Latvia:
        case QLocale::Lithuania:
        case QLocale::Croatia:
        case QLocale::Malta:
        case QLocale::Cyprus:
        case QLocale::Switzerland:
        case QLocale::Sweden:
        case QLocale::Denmark:
        case QLocale::Norway:
        case QLocale::Poland:
        case QLocale::CzechRepublic:
        case QLocale::Hungary:
            return QStringLiteral("EU");
        case QLocale::India:
            return QStringLiteral("IN");
        case QLocale::Japan:
            return QStringLiteral("JP");
        case QLocale::China:
        case QLocale::HongKong:
        case QLocale::Macao:
        case QLocale::Taiwan:
            return QStringLiteral("CN");
        case QLocale::SouthKorea:
            return QStringLiteral("KR");
        case QLocale::Australia:
        case QLocale::NewZealand:
            return QStringLiteral("AU");
        default:
            return QStringLiteral("GLOBAL");
    }
}

QStringList profile_symbols(const QVector<ProfileInstrument>& list) {
    QStringList out;
    out.reserve(list.size());
    for (const auto& i : list)
        out.append(i.symbol);
    return out;
}

QString profile_label_for(const MarketProfile& profile, const QString& symbol) {
    for (const auto* list : {&profile.indices, &profile.fx, &profile.commodities, &profile.crypto, &profile.sectors})
        for (const auto& i : *list)
            if (i.symbol.compare(symbol, Qt::CaseInsensitive) == 0)
                return profile_text(i.label);
    return {};
}

QString any_profile_label_for(const QString& symbol) {
    for (const auto& p : all_market_profiles()) {
        const QString l = profile_label_for(p, symbol);
        if (!l.isEmpty())
            return l;
    }
    return {};
}

// ── Store ────────────────────────────────────────────────────────────────────

MarketProfileStore& MarketProfileStore::instance() {
    static MarketProfileStore s;
    return s;
}

void MarketProfileStore::ensure_loaded() {
    if (loaded_)
        return;
    loaded_ = true;
    const auto r = SettingsRepository::instance().get(QString::fromLatin1(kMpSettingsKey));
    const QString saved = r.is_ok() ? r.value().trimmed().toUpper() : QString();
    bool known = false;
    for (const auto& p : all_market_profiles())
        known = known || (p.id == saved);
    if (known) {
        id_ = saved;
        user_choice_ = true;
    } else {
        id_ = region_for_locale(QLocale::system());
        user_choice_ = false;
    }
}

const MarketProfile& MarketProfileStore::current() {
    ensure_loaded();
    return market_profile(id_);
}

QString MarketProfileStore::current_id() {
    ensure_loaded();
    return id_;
}

bool MarketProfileStore::is_user_choice() {
    ensure_loaded();
    return user_choice_;
}

void MarketProfileStore::set_current(const QString& id) {
    ensure_loaded();
    const QString next = market_profile(id).id;
    const bool changed = next != id_;
    id_ = next;
    user_choice_ = true;
    const auto w = SettingsRepository::instance().set(QString::fromLatin1(kMpSettingsKey), next, "dashboard");
    if (w.is_err())
        LOG_WARN("MarketProfile", QString("could not persist the market region: %1").arg(QString::fromStdString(w.error())));
    if (changed)
        emit profile_changed(next);
}

} // namespace fincept::core::market
