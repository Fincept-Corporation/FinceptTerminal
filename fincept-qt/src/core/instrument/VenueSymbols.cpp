// VenueSymbols.cpp — the venue / provider symbol table. See VenueSymbols.h.
// Qt Core only. Every table is immutable and built once (function-local static).
//
// Liveness of the Yahoo forms below was checked against Yahoo Finance on
// 2026-11-16 (yfinance 0.2.66, recent daily bars): every suffix marked covered,
// every index symbol, every futures root and every crypto Yahoo base quoted.
// Venues marked kVsNoYahoo returned no data for their large caps (ADX, PSE .PS,
// Beijing .BJ, Nairobi .NR, Moscow .ME, Tunis, Berlin .BE) or could not be
// verified (Casablanca, Caracas, Luxembourg, Belgrade, Bratislava, Cyprus,
// Dhaka, Lagos, HNX / UPCoM, Nordic Growth Market).

#include "core/instrument/VenueSymbols.h"

#include "core/instrument/SymbolInput.h"
#include "core/market/MarketSessions.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <iterator>

namespace fincept::core::instrument {

namespace venue_symbols_detail {
QString yahoo_spelling(const QString& upper_symbol);
QStringList table_problems();
} // namespace venue_symbols_detail

namespace {
namespace vsym_detail {

namespace mkt = fincept::core::market;

// ── Flags ───────────────────────────────────────────────────────────────────
constexpr quint8 kVsNoYahoo = 0x01; // Yahoo carries no data for the venue
constexpr quint8 kVsApprox = 0x02;  // quoted through another listing (borrowed suffix)
constexpr quint8 kVsSeries = 0x04;  // strip Indian series suffixes (-EQ, -BE, ...)
constexpr quint8 kVsHkPad = 0x08;   // digits: drop leading zeros, pad to 4 (0700)
constexpr quint8 kVsPad6 = 0x10;    // digits: pad to 6 (005930, 000001)
constexpr quint8 kVsDotDash = 0x20; // every '.' / '/' in the code becomes '-' (US, Colombo)

constexpr VenueRole kVsL = VenueRole::Listing;
constexpr VenueRole kVsU = VenueRole::UsListing;

struct VsMarketSpec {
    const char* key;
    const char* venue;
    const char* mic;
    const char* country;
    const char* currency; // empty: MarketSessions venue currency
    const char* suffixes; // space separated, first = written by to_yahoo
    VenueRole role;
    quint8 flags;
    const char* broker_exchange;
    const char* codes; // CodeSpace::Any (the key is always a code too)
    const char* tv;    // TradingView exchange codes
    const char* yahoo; // Yahoo quote.exchange codes
};

// Order matters: for a venue id the first non-approximate entry is its primary
// listing market (NYSE before AMEX / ARCA, KOSPI before KOSDAQ, LSE before IOB),
// and the first entry registering a code owns it.
const VsMarketSpec kVsMarkets[] = {
    // ── United States (Yahoo: no suffix) ──
    {"NYSE", "NYSE", "XNYS", "US", "USD", "", kVsU, kVsDotDash, "NYSE", "NYSE XNYS US USA", "NYSE", "NYQ NYS"},
    {"NASDAQ", "NASDAQ", "XNAS", "US", "USD", "", kVsU, kVsDotDash, "NASDAQ", "NASDAQ XNAS NSDQ", "NASDAQ",
     "NMS NGM NCM NAS NASDAQGS NASDAQGM NASDAQCM"},
    {"AMEX", "NYSE", "XASE", "US", "USD", "", kVsU, kVsDotDash, "AMEX", "AMEX NYSEAMERICAN XASE", "AMEX", "ASE"},
    {"ARCA", "NYSE", "ARCX", "US", "USD", "", kVsU, kVsDotDash, "ARCA", "ARCA NYSEARCA ARCX PCX", "NYSEARCA", "PCX"},
    {"CBOE_US", "NYSE", "BATS", "US", "USD", "", kVsU, kVsDotDash, "", "CBOE_US BATS BZX CBOE XCBO", "CBOE", "BTS"},
    {"OTC", "NYSE", "OTCM", "US", "USD", "", kVsU, kVsDotDash, "", "OTC OTCM PNK OTCMKTS", "OTC", "PNK OQB OQX OEM"},
    // ── Canada / Latin America ──
    {"TSX", "TSX", "XTSE", "CA", "", "TO", kVsL, 0, "", "TSX XTSE TOR", "TSX", "TOR"},
    {"TSXV", "TSXV", "XTSX", "CA", "", "V", kVsL, 0, "", "TSXV XTSX CVE", "TSXV", "VAN"},
    {"CSE", "CSE", "XCNQ", "CA", "", "CN", kVsL, 0, "", "CSE XCNQ CNSX", "CSE", "CNQ"},
    {"CBOE_CA", "CBOE_CA", "NEOE", "CA", "", "NE", kVsL, 0, "", "CBOE_CA NEOE NEO", "NEO", "NEO"},
    {"B3", "B3", "BVMF", "BR", "", "SA", kVsL, 0, "", "B3 BVMF BOVESPA BMFBOVESPA", "BMFBOVESPA", "SAO"},
    {"BMV", "BMV", "XMEX", "MX", "", "MX", kVsL, 0, "", "BMV XMEX", "BMV", "MEX"},
    {"BIVA", "BMV", "", "MX", "MXN", "MX", kVsL, kVsApprox, "", "BIVA", "BIVA", ""},
    {"BCBA", "BCBA", "XBUE", "AR", "ARS", "BA", kVsL, 0, "", "BCBA XBUE BYMA", "BCBA", "BUE"},
    {"BCS", "BCS", "XSGO", "CL", "CLP", "SN", kVsL, 0, "", "BCS XSGO", "BCS", "SGO"},
    {"BVC", "BVC", "XBOG", "CO", "COP", "CL", kVsL, 0, "", "BVC XBOG", "BVC", "BVC"},
    {"BVL", "BVL", "XLIM", "PE", "PEN", "LM", kVsL, 0, "", "BVL XLIM", "BVL", "LIM"},
    {"BVCV", "BVCV", "BVCA", "VE", "VES", "", kVsL, kVsNoYahoo, "", "BVCV BVCA", "BVCV", ""},
    // ── United Kingdom / Europe ──
    {"LSE", "LSE", "XLON", "GB", "", "L", kVsL, 0, "", "LSE XLON LON", "LSE", "LSE"},
    {"LSE_IOB", "LSE", "XLON", "GB", "USD", "IL", kVsL, 0, "", "LSE_IOB IOB", "LSIN", "IOB"},
    {"XETRA", "XETRA", "XETR", "DE", "", "DE", kVsL, 0, "", "XETRA XETR ETR GER", "XETR", "GER"},
    {"GETTEX", "XETRA", "", "DE", "EUR", "DE", kVsL, kVsApprox, "", "GETTEX", "GETTEX", ""},
    {"TRADEGATE", "XETRA", "", "DE", "EUR", "DE", kVsL, kVsApprox, "", "TRADEGATE", "TRADEGATE", ""},
    {"LANG_SCHWARZ", "XETRA", "", "DE", "EUR", "DE", kVsL, kVsApprox, "", "LANG_SCHWARZ LSX", "LS LSX", ""},
    {"FRA", "FRA", "XFRA", "DE", "", "F", kVsL, 0, "", "FRA XFRA FWB FRANKFURT", "FWB", "FRA"},
    {"BER", "BER", "XBER", "DE", "", "BE", kVsL, kVsNoYahoo, "", "BER XBER", "BER", "BER"},
    {"DUS", "DUS", "XDUS", "DE", "", "DU", kVsL, 0, "", "DUS XDUS", "DUS", "DUS"},
    {"HAM", "HAM", "XHAM", "DE", "", "HM", kVsL, 0, "", "HAM XHAM", "HAM", "HAM"},
    {"HAN", "HAN", "XHAN", "DE", "", "HA", kVsL, 0, "", "HAN XHAN", "HAN", "HAN"},
    {"MUN", "MUN", "XMUN", "DE", "", "MU", kVsL, 0, "", "MUN XMUN", "MUN", "MUN"},
    {"STU", "STU", "XSTU", "DE", "", "SG", kVsL, 0, "", "STU XSTU", "SWB", "STU"},
    {"VIE", "VIE", "XWBO", "AT", "", "VI", kVsL, 0, "", "VIE XWBO WBAG", "VIE", "VIE"},
    {"EURONEXT", "", "", "", "EUR", "", VenueRole::Euronext, 0, "", "EURONEXT ENX", "EURONEXT", ""},
    {"PARIS", "PARIS", "XPAR", "FR", "", "PA", kVsL, 0, "", "PARIS XPAR EPA ENXTPA", "", "PAR"},
    {"AMSTERDAM", "AMSTERDAM", "XAMS", "NL", "", "AS", kVsL, 0, "", "AMSTERDAM XAMS AMS ENXTAM", "", "AMS"},
    {"BRUSSELS", "BRUSSELS", "XBRU", "BE", "", "BR", kVsL, 0, "", "BRUSSELS XBRU EBR ENXTBR", "", "BRU"},
    {"LISBON", "LISBON", "XLIS", "PT", "", "LS", kVsL, 0, "", "LISBON XLIS ELI ENXTLS", "", "LIS"},
    {"DUBLIN", "DUBLIN", "XDUB", "IE", "", "IR", kVsL, 0, "", "DUBLIN XDUB", "", "ISE"},
    {"OSLO", "OSLO", "XOSL", "NO", "", "OL", kVsL, 0, "", "OSLO XOSL OSE", "OSL", "OSL"},
    {"MILAN", "MILAN", "XMIL", "IT", "", "MI", kVsL, 0, "", "MILAN XMIL BIT MTA", "MIL", "MIL"},
    {"SIX", "SIX", "XSWX", "CH", "", "SW VX", kVsL, 0, "", "SIX XSWX SWX XVTX VTX", "SIX", "EBS VTX"},
    {"BX_SWISS", "SIX", "", "CH", "CHF", "SW", kVsL, kVsApprox, "", "BX_SWISS", "BX", ""},
    {"BME", "BME", "XMAD", "ES", "", "MC", kVsL, 0, "", "BME XMAD MCE SIBE", "BME", "MCE"},
    {"STOCKHOLM", "STOCKHOLM", "XSTO", "SE", "", "ST", kVsL, 0, "", "STOCKHOLM XSTO STO", "OMXSTO", "STO"},
    {"NGM_NORDIC", "NGM_NORDIC", "XNGM", "SE", "SEK", "", kVsL, kVsNoYahoo, "", "NGM_NORDIC XNGM", "NGM", ""},
    {"HELSINKI", "HELSINKI", "XHEL", "FI", "", "HE", kVsL, 0, "", "HELSINKI XHEL HEL", "OMXHEX", "HEL"},
    {"COPENHAGEN", "COPENHAGEN", "XCSE", "DK", "", "CO", kVsL, 0, "", "COPENHAGEN XCSE CPH", "OMXCOP", "CPH"},
    {"ICELAND", "ICELAND", "XICE", "IS", "", "IC", kVsL, 0, "", "ICELAND XICE", "OMXICE", "ICE"},
    {"WARSAW", "WARSAW", "XWAR", "PL", "", "WA", kVsL, 0, "", "WARSAW XWAR WSE GPW", "GPW", "WSE"},
    {"ISTANBUL", "ISTANBUL", "XIST", "TR", "", "IS", kVsL, 0, "", "ISTANBUL XIST BIST", "BIST", "IST"},
    {"ATHEX", "ATHEX", "XATH", "GR", "EUR", "AT", kVsL, 0, "", "ATHEX XATH", "ATHEX", "ATH"},
    {"PRAGUE", "PRAGUE", "XPRA", "CZ", "CZK", "PR", kVsL, 0, "", "PRAGUE XPRA", "PSECZ", "PRA"},
    {"BUDAPEST", "BUDAPEST", "XBUD", "HU", "HUF", "BD", kVsL, 0, "", "BUDAPEST XBUD", "BET", "BUD"},
    {"BUCHAREST", "BUCHAREST", "XBSE", "RO", "RON", "RO", kVsL, 0, "", "BUCHAREST XBSE", "BVB", "BVB"},
    {"TALLINN", "TALLINN", "XTAL", "EE", "EUR", "TL", kVsL, 0, "", "TALLINN XTAL", "OMXTSE", "TAL"},
    {"RIGA", "RIGA", "XRIS", "LV", "EUR", "RG", kVsL, 0, "", "RIGA XRIS", "OMXRSE", "RIS"},
    {"VILNIUS", "VILNIUS", "XLIT", "LT", "EUR", "VS", kVsL, 0, "", "VILNIUS XLIT", "OMXVSE", "LIT"},
    {"MOEX", "MOEX", "MISX", "RU", "RUB", "ME", kVsL, kVsNoYahoo, "", "MOEX MISX", "RUS MOEX", ""},
    {"LUXSE", "LUXSE", "XLUX", "LU", "EUR", "", kVsL, kVsNoYahoo, "", "LUXSE XLUX", "LUXSE", ""},
    {"BELEX", "BELEX", "XBEL", "RS", "RSD", "", kVsL, kVsNoYahoo, "", "BELEX XBEL", "BELEX", ""},
    {"BSSE", "BSSE", "XBRA", "SK", "EUR", "", kVsL, kVsNoYahoo, "", "BSSE XBRA", "BSSE", ""},
    {"CSECY", "CSECY", "XCYS", "CY", "EUR", "", kVsL, kVsNoYahoo, "", "CSECY XCYS", "CSECY", ""},
    // ── Middle East / Africa ──
    {"JSE", "JSE", "XJSE", "ZA", "", "JO", kVsL, 0, "", "JSE XJSE JNB", "JSE", "JNB"},
    {"TADAWUL", "TADAWUL", "XSAU", "SA", "", "SR", kVsL, 0, "", "TADAWUL XSAU", "TADAWUL", "SAU"},
    {"DFM", "DFM", "XDFM", "AE", "", "AE", kVsL, 0, "", "DFM XDFM", "DFM", "DFM"},
    {"ADX", "ADX", "XADS", "AE", "", "", kVsL, kVsNoYahoo, "", "ADX XADS", "ADX", ""},
    {"NASDAQ_DUBAI", "NASDAQ_DUBAI", "DIFX", "AE", "USD", "", kVsL, kVsNoYahoo, "", "NASDAQ_DUBAI DIFX",
     "NASDAQDUBAI", ""},
    {"TASE", "TASE", "XTAE", "IL", "", "TA", kVsL, 0, "", "TASE XTAE TLV", "TASE", "TLV"},
    {"QSE", "QSE", "DSMD", "QA", "QAR", "QA", kVsL, 0, "", "QSE DSMD", "QSE", "DOH"},
    {"BOURSA_KUWAIT", "BOURSA_KUWAIT", "XKUW", "KW", "KWF", "KW", kVsL, 0, "", "BOURSA_KUWAIT XKUW", "KSE", "KUW"},
    {"BAHRAIN", "BAHRAIN", "XBAH", "BH", "BHD", "BH", kVsL, 0, "", "BAHRAIN XBAH", "BAHRAIN", ""},
    {"EGX", "EGX", "XCAI", "EG", "EGP", "CA", kVsL, 0, "", "EGX XCAI", "EGX", "CAI"},
    {"NSE_KE", "NSE_KE", "XNAI", "KE", "KES", "NR", kVsL, kVsNoYahoo, "", "NSE_KE XNAI", "NSEKE", ""},
    {"NGX", "NGX", "XNSA", "NG", "NGN", "", kVsL, kVsNoYahoo, "", "NGX XNSA", "NSENG", ""},
    {"CASABLANCA", "CASABLANCA", "XCAS", "MA", "MAD", "", kVsL, kVsNoYahoo, "", "CASABLANCA XCAS", "CSEMA", ""},
    {"BVMT", "BVMT", "XTUN", "TN", "TND", "", kVsL, kVsNoYahoo, "", "BVMT XTUN", "BVMT", ""},
    // ── India: cash, index and contract segments ──
    {"NSE", "NSE", "XNSE", "IN", "", "NS", kVsL, kVsSeries, "NSE", "NSE XNSE NSI NSE_EQ", "NSE", "NSI"},
    {"BSE", "BSE", "XBOM", "IN", "", "BO", kVsL, kVsSeries, "BSE", "BSE XBOM BOM BSE_EQ", "BSE", "BSE"},
    {"NSE_INDEX", "NSE", "", "IN", "", "", VenueRole::IndexNamespace, 0, "NSE_INDEX", "NSE_INDEX", "", ""},
    {"BSE_INDEX", "BSE", "", "IN", "", "", VenueRole::IndexNamespace, 0, "BSE_INDEX", "BSE_INDEX", "", ""},
    {"NFO", "NSE", "", "IN", "INR", "", VenueRole::Derivatives, kVsNoYahoo, "NFO", "NFO NSE_FO", "", ""},
    {"BFO", "BSE", "", "IN", "INR", "", VenueRole::Derivatives, kVsNoYahoo, "BFO", "BFO BSE_FO", "", ""},
    {"CDS", "NSE", "", "IN", "INR", "", VenueRole::Derivatives, kVsNoYahoo, "CDS", "CDS NSE_CD", "", ""},
    {"BCD", "BSE", "", "IN", "INR", "", VenueRole::Derivatives, kVsNoYahoo, "BCD", "BCD BSE_CD", "", ""},
    {"MCX", "MCX", "XIMC", "IN", "INR", "", VenueRole::Derivatives, kVsNoYahoo, "MCX", "MCX XIMC MCX_FO", "MCX", ""},
    {"MCX_INDEX", "MCX", "", "IN", "", "", VenueRole::Derivatives, kVsNoYahoo, "MCX_INDEX", "MCX_INDEX", "", ""},
    {"NCDEX", "NCDEX", "XNCD", "IN", "INR", "", VenueRole::Derivatives, kVsNoYahoo, "NCDEX", "NCDEX XNCD", "NCDEX",
     ""},
    {"EUREX", "EUREX", "XEUR", "DE", "EUR", "", VenueRole::Derivatives, kVsNoYahoo, "", "EUREX XEUR", "EUREX", ""},
    // ── Asia-Pacific ──
    {"SGX", "SGX", "XSES", "SG", "", "SI", kVsL, 0, "", "SGX XSES SES", "SGX", "SES"},
    {"HKEX", "HKEX", "XHKG", "HK", "", "HK", kVsL, kVsHkPad, "", "HKEX XHKG HKG SEHK", "HKEX", "HKG"},
    {"SSE", "SSE", "XSHG", "CN", "", "SS", kVsL, kVsPad6, "", "SSE XSHG SHH SHA", "SSE", "SHH"},
    {"SZSE", "SZSE", "XSHE", "CN", "", "SZ", kVsL, kVsPad6, "", "SZSE XSHE SHZ SHE", "SZSE", "SHZ"},
    {"BJSE", "BJSE", "BJSE", "CN", "CNY", "BJ", kVsL, kVsNoYahoo | kVsPad6, "", "BJSE", "BJSE", ""},
    {"TSE", "TSE", "XTKS", "JP", "", "T", kVsL, 0, "", "TSE XTKS XJPX JPX TYO", "TSE", "JPX OSA TYO"},
    {"KOSPI", "KRX", "XKRX", "KR", "", "KS", kVsL, kVsPad6, "", "KOSPI KRX XKRX KSC", "KRX", "KSC"},
    {"KOSDAQ", "KRX", "XKOS", "KR", "", "KQ", kVsL, kVsPad6, "", "KOSDAQ XKOS KOE", "", "KOE"},
    {"TWSE", "TWSE", "XTAI", "TW", "", "TW", kVsL, 0, "", "TWSE XTAI TAI TPE", "TWSE", "TAI"},
    {"TPEX", "TPEX", "ROCO", "TW", "", "TWO", kVsL, 0, "", "TPEX ROCO", "TPEX", "TWO"},
    {"ASX", "ASX", "XASX", "AU", "", "AX", kVsL, 0, "", "ASX XASX", "ASX", "ASX"},
    {"NZX", "NZX", "XNZE", "NZ", "", "NZ", kVsL, 0, "", "NZX XNZE NZE", "NZX", "NZE"},
    {"IDX", "IDX", "XIDX", "ID", "", "JK", kVsL, 0, "", "IDX XIDX JKT", "IDX", "JKT"},
    {"SET", "SET", "XBKK", "TH", "", "BK", kVsL, 0, "", "SET XBKK BKK", "SET", "SET"},
    {"MYX", "MYX", "XKLS", "MY", "", "KL", kVsL, 0, "", "MYX XKLS KLS KLSE BURSA", "MYX", "KLS"},
    {"PSE", "PSE", "XPHS", "PH", "PHP", "PS", kVsL, kVsNoYahoo, "", "PSE XPHS", "PSE", ""},
    {"HOSE", "HOSE", "XSTC", "VN", "VND", "VN", kVsL, 0, "", "HOSE XSTC HSX", "HOSE", "VSE"},
    {"HNX", "HNX", "HSTC", "VN", "VND", "", kVsL, kVsNoYahoo, "", "HNX HSTC UPCOM", "HNX UPCOM", ""},
    {"PSX", "PSX", "XKAR", "PK", "PKR", "KA", kVsL, 0, "", "PSX XKAR", "PSX", "KAR"},
    {"CSE_LK", "CSE_LK", "XCOL", "LK", "LKR", "CM", kVsL, kVsDotDash, "", "CSE_LK XCOL", "CSELK", "CSE"},
    {"DSE_BD", "DSE_BD", "XDHA", "BD", "BDT", "", kVsL, kVsNoYahoo, "", "DSE_BD XDHA", "DSEBD", ""},
    // ── Pseudo venues / namespaces ──
    {"FX", "FX", "", "", "", "", VenueRole::Fx, 0, "", "FX FOREX CURRENCY CCY",
     "FX FX_IDC OANDA FXCM FOREXCOM SAXO ICMARKETS PEPPERSTONE THINKMARKETS EIGHTCAP CAPITALCOM FXOPEN BLACKBULL "
     "VANTAGE EASYMARKETS ACTIVTRADES",
     "CCY"},
    {"CRYPTO", "CRYPTO", "", "", "", "", VenueRole::Crypto, 0, "", "CRYPTO CRYPTOCAP CCC",
     "BINANCE BINANCEUS COINBASE KRAKEN BITSTAMP BITFINEX OKX BYBIT KUCOIN GEMINI HTX HUOBI GATEIO GATE MEXC "
     "BITGET CRYPTOCOM BITMEX DERIBIT POLONIEX UPBIT BITHUMB COINW WHITEBIT BINGX PHEMEX HYPERLIQUID",
     "CCC"},
    {"INDEX", "", "", "", "", "", VenueRole::IndexNamespace, 0, "", "INDEX INDICES", "SP DJ TVC",
     "SNP DJI NIM WCB CXI FGI FSI CGI ZRH"},
    {"CME", "CME", "XCME", "US", "USD", "NYM CMX CBT CME NYB", VenueRole::Futures, 0, "",
     "CME XCME GLOBEX CBOT XCBT CBT NYMEX XNYM NYM COMEX XCEC CMX ICEUS IFUS NYB",
     "CME CME_MINI CBOT CBOT_MINI NYMEX NYMEX_MINI COMEX COMEX_MINI ICEUS", "CME CBT NYM CMX NYB"},
};

// Euronext market by country.
struct VsEuronextSpec {
    const char* country;
    const char* key;
};
const VsEuronextSpec kVsEuronext[] = {
    {"FR", "PARIS"}, {"NL", "AMSTERDAM"}, {"BE", "BRUSSELS"}, {"PT", "LISBON"},
    {"IE", "DUBLIN"}, {"NO", "OSLO"},     {"IT", "MILAN"},
};

// ── Index table ─────────────────────────────────────────────────────────────
// aliases: '|' separated; a leading '!' marks a text-only alias (a listed ticker or
// a venue code too: honoured for free text, never as a listing code on a venue).
struct VsIndexSpec {
    const char* yahoo;
    const char* venue;
    const char* name;
    const char* broker;
    const char* broker_exchange;
    const char* aliases;
};

const VsIndexSpec kVsIndices[] = {
    // United States
    {"^GSPC", "NYSE", "S&P 500", "", "", "SPX|S&P 500|S&P500|SP500|US500|GSPC|!S&P|!SP"},
    {"^DJI", "NYSE", "Dow Jones Industrial Average", "", "",
     "DJI|DJIA|DOW JONES|DOW 30|US30|DOW JONES INDUSTRIAL AVERAGE|!DOW"},
    {"^IXIC", "NASDAQ", "NASDAQ Composite", "", "", "IXIC|NASDAQ COMPOSITE|!NASDAQ"},
    {"^NDX", "NASDAQ", "NASDAQ-100", "", "", "NDX|NASDAQ 100|NAS100|US100"},
    {"^RUT", "NYSE", "Russell 2000", "", "", "RUT|RUSSELL 2000|US2000|!RUSSELL"},
    {"^VIX", "NYSE", "CBOE Volatility Index", "", "", "VIX|CBOE VIX|VOLATILITY INDEX"},
    {"^NYA", "NYSE", "NYSE Composite", "", "", "NYA|NYSE COMPOSITE"},
    {"^OEX", "NYSE", "S&P 100", "", "", "OEX|S&P 100|SP100"},
    {"^MID", "NYSE", "S&P MidCap 400", "", "", "S&P 400|SP400|S&P MIDCAP 400"},
    {"^SP600", "NYSE", "S&P SmallCap 600", "", "", "S&P 600|S&P SMALLCAP 600"},
    {"^RUA", "NYSE", "Russell 3000", "", "", "RUA|RUSSELL 3000"},
    {"^W5000", "NYSE", "Wilshire 5000", "", "", "W5000|WILSHIRE 5000"},
    {"^SOX", "NASDAQ", "PHLX Semiconductor", "", "", "SOX|PHLX SEMICONDUCTOR"},
    {"^NBI", "NASDAQ", "NASDAQ Biotechnology", "", "", "NBI|NASDAQ BIOTECHNOLOGY"},
    {"^TNX", "NYSE", "US 10-Year Treasury Yield", "", "", "TNX|US10Y"},
    {"^FVX", "NYSE", "US 5-Year Treasury Yield", "", "", "FVX|US05Y"},
    {"^TYX", "NYSE", "US 30-Year Treasury Yield", "", "", "TYX|US30Y"},
    {"^IRX", "NYSE", "US 13-Week Treasury Bill", "", "", "IRX|US03M"},
    {"^DJT", "NYSE", "Dow Jones Transportation Average", "", "", "DOW TRANSPORTS|DOW JONES TRANSPORTATION"},
    {"^DJU", "NYSE", "Dow Jones Utility Average", "", "", "DJU|DOW UTILITIES"},
    {"DX-Y.NYB", "CME", "US Dollar Index", "", "", "DXY|USDX|DOLLAR INDEX|US DOLLAR INDEX"},
    // Americas
    {"^GSPTSE", "TSX", "S&P/TSX Composite", "", "", "GSPTSE|TSX COMPOSITE|S&P/TSX COMPOSITE|!TSX"},
    {"^BVSP", "B3", "IBOVESPA", "", "", "IBOV|IBOVESPA|BVSP"},
    {"^MXX", "BMV", "S&P/BMV IPC", "", "", "MXX|IPC|MEXBOL|S&P/BMV IPC"},
    {"^MERV", "BCBA", "MERVAL", "", "", "MERVAL|MERV"},
    // Europe
    {"^FTSE", "LSE", "FTSE 100", "", "", "FTSE|FTSE 100|FTSE100|UKX|UK100"},
    {"^FTMC", "LSE", "FTSE 250", "", "", "FTSE 250|FTSE250|FTMC"},
    {"^FTAS", "LSE", "FTSE All-Share", "", "", "FTSE ALL SHARE|FTAS"},
    {"^GDAXI", "XETRA", "DAX", "", "", "DAX|DAX 40|DAX40|DE40|GER40|GDAXI"},
    {"^MDAXI", "XETRA", "MDAX", "", "", "MDAX|MDAXI"},
    {"^TECDAX", "XETRA", "TecDAX", "", "", "TECDAX"},
    {"^SDAXI", "XETRA", "SDAX", "", "", "SDAX|SDAXI"},
    {"^STOXX50E", "XETRA", "EURO STOXX 50", "", "",
     "SX5E|STOXX50|STOXX 50|EURO STOXX 50|EUROSTOXX 50|EU50|ESTX50|STOXX50E"},
    {"^STOXX", "XETRA", "STOXX Europe 600", "", "", "STOXX|STOXX 600|STOXX EUROPE 600|SXXP"},
    {"^FCHI", "PARIS", "CAC 40", "", "", "CAC|CAC 40|CAC40|PX1|FRA40|FCHI"},
    {"^SBF120", "PARIS", "SBF 120", "", "", "SBF 120|SBF120"},
    {"^AEX", "AMSTERDAM", "AEX", "", "", "AEX|AEX INDEX|NL25"},
    {"^AMX", "AMSTERDAM", "AMX", "", "", "AMX INDEX|AMSTERDAM MIDCAP"},
    {"^BFX", "BRUSSELS", "BEL 20", "", "", "BEL20|BEL 20|BFX"},
    {"PSI20.LS", "LISBON", "PSI", "", "", "PSI20|PSI 20|PSI"},
    {"^ISEQ", "DUBLIN", "ISEQ All Share", "", "", "ISEQ|ISEQ ALL SHARE"},
    {"OBX.OL", "OSLO", "OBX", "", "", "OBX"},
    {"FTSEMIB.MI", "MILAN", "FTSE MIB", "", "", "FTSEMIB|FTSE MIB|MIB|IT40"},
    {"^SSMI", "SIX", "SMI", "", "", "SMI|SWISS MARKET INDEX|SSMI|CH20"},
    {"^IBEX", "BME", "IBEX 35", "", "", "IBEX|IBEX 35|IBEX35|IBC|ESP35"},
    {"^OMX", "STOCKHOLM", "OMX Stockholm 30", "", "", "OMXS30|OMX|OMX STOCKHOLM 30"},
    {"^OMXSPI", "STOCKHOLM", "OMX Stockholm All-Share", "", "", "OMXSPI"},
    {"^OMXH25", "HELSINKI", "OMX Helsinki 25", "", "", "OMXH25|OMX HELSINKI 25"},
    {"^OMXC25", "COPENHAGEN", "OMX Copenhagen 25", "", "", "OMXC25|OMX COPENHAGEN 25"},
    {"^ATX", "VIE", "ATX", "", "", "ATX"},
    {"WIG20.WA", "WARSAW", "WIG20", "", "", "WIG20|WIG 20"},
    {"XU100.IS", "ISTANBUL", "BIST 100", "", "", "XU100|BIST 100|BIST100"},
    // Middle East / Africa
    {"^TA125.TA", "TASE", "TA-125", "", "", "TA125|TA 125"},
    {"^TASI.SR", "TADAWUL", "Tadawul All Share", "", "", "TASI|TADAWUL ALL SHARE"},
    {"^J203.JO", "JSE", "FTSE/JSE All Share", "", "", "J203|JSE ALL SHARE|ALSI"},
    {"^CASE30", "EGX", "EGX 30", "", "", "EGX30|EGX 30|CASE30"},
    // Asia-Pacific
    {"^N225", "TSE", "Nikkei 225", "", "", "NIKKEI|NIKKEI 225|NI225|N225|JP225|NKY"},
    {"", "TSE", "TOPIX", "", "", "TOPIX|TOPX"},
    {"^HSI", "HKEX", "Hang Seng Index", "", "", "HSI|HANG SENG|HK50|HANG SENG INDEX"},
    {"^HSCE", "HKEX", "Hang Seng China Enterprises", "", "", "HSCEI|HSCE|H SHARES"},
    {"HSTECH.HK", "HKEX", "Hang Seng TECH", "", "", "HSTECH|HSTEC|HANG SENG TECH"},
    {"000001.SS", "SSE", "SSE Composite", "", "", "SHCOMP|SSE COMPOSITE|SHANGHAI COMPOSITE|SSEC|!SHANGHAI"},
    {"399001.SZ", "SZSE", "SZSE Component", "", "", "SZSE COMPONENT|SHENZHEN COMPONENT|SZCOMP"},
    {"000300.SS", "SSE", "CSI 300", "", "", "CSI300|CSI 300"},
    {"^KS11", "KRX", "KOSPI Composite", "", "", "KOSPI|KS11|KOSPI COMPOSITE"},
    {"^KQ11", "KRX", "KOSDAQ Composite", "", "", "KOSDAQ|KQ11|KOSDAQ COMPOSITE"},
    {"^KS200", "KRX", "KOSPI 200", "", "", "KOSPI200|KOSPI 200|KS200"},
    {"^TWII", "TWSE", "TAIEX", "", "", "TAIEX|TWII|TSEC"},
    {"^AXJO", "ASX", "S&P/ASX 200", "", "", "XJO|ASX200|ASX 200|S&P/ASX 200|AU200|AXJO"},
    {"^AORD", "ASX", "All Ordinaries", "", "", "XAO|ALL ORDINARIES|AORD"},
    {"^NZ50", "NZX", "S&P/NZX 50", "", "", "NZ50|NZX50|NZX 50"},
    {"^STI", "SGX", "Straits Times Index", "", "", "STI|STRAITS TIMES"},
    {"^JKSE", "IDX", "IDX Composite", "", "", "JKSE|IHSG|IDX COMPOSITE"},
    {"^KLSE", "MYX", "FTSE Bursa Malaysia KLCI", "", "", "KLCI|FBMKLCI|KLSE"},
    {"^SET.BK", "SET", "SET Index", "", "", "SET INDEX|SETI"},
    // India (broker = instrument-master symbol, see trading/instruments/InstrumentNormalize.cpp)
    {"^NSEI", "NSE", "NIFTY 50", "NIFTY", "NSE_INDEX", "NIFTY|NIFTY 50|NSEI|CNX NIFTY"},
    {"^NSEBANK", "NSE", "NIFTY BANK", "BANKNIFTY", "NSE_INDEX", "BANKNIFTY|BANK NIFTY|NIFTY BANK|NSEBANK"},
    {"NIFTY_FIN_SERVICE.NS", "NSE", "NIFTY FIN SERVICE", "FINNIFTY", "NSE_INDEX",
     "FINNIFTY|NIFTY FIN SERVICE|NIFTY FINANCIAL SERVICES|FIN NIFTY"},
    {"NIFTY_MID_SELECT.NS", "NSE", "NIFTY MIDCAP SELECT", "MIDCPNIFTY", "NSE_INDEX",
     "MIDCPNIFTY|NIFTY MID SELECT|NIFTY MIDCAP SELECT|MIDCAP NIFTY"},
    {"^NSMIDCP", "NSE", "NIFTY NEXT 50", "NIFTYNXT50", "NSE_INDEX", "NIFTYNXT50|NIFTY NEXT 50|NIFTY JUNIOR|NSMIDCP"},
    {"^CNXIT", "NSE", "NIFTY IT", "NIFTYIT", "NSE_INDEX", "NIFTY IT|CNXIT"},
    {"^INDIAVIX", "NSE", "INDIA VIX", "INDIAVIX", "NSE_INDEX", "INDIA VIX"},
    {"^CNXAUTO", "NSE", "NIFTY AUTO", "NIFTYAUTO", "NSE_INDEX", "NIFTY AUTO|CNXAUTO"},
    {"^CNXFMCG", "NSE", "NIFTY FMCG", "NIFTYFMCG", "NSE_INDEX", "NIFTY FMCG|CNXFMCG"},
    {"^CNXPHARMA", "NSE", "NIFTY PHARMA", "NIFTYPHARMA", "NSE_INDEX", "NIFTY PHARMA|CNXPHARMA"},
    {"^CNXMETAL", "NSE", "NIFTY METAL", "NIFTYMETAL", "NSE_INDEX", "NIFTY METAL|CNXMETAL"},
    {"^CNXREALTY", "NSE", "NIFTY REALTY", "NIFTYREALTY", "NSE_INDEX", "NIFTY REALTY|CNXREALTY"},
    {"^CNXENERGY", "NSE", "NIFTY ENERGY", "NIFTYENERGY", "NSE_INDEX", "NIFTY ENERGY|CNXENERGY"},
    {"^CNXINFRA", "NSE", "NIFTY INFRA", "NIFTYINFRA", "NSE_INDEX", "NIFTY INFRA|CNXINFRA"},
    {"^CNXPSUBANK", "NSE", "NIFTY PSU BANK", "NIFTYPSUBANK", "NSE_INDEX", "NIFTY PSU BANK|CNXPSUBANK"},
    {"^CRSLDX", "NSE", "NIFTY 500", "NIFTY500", "NSE_INDEX", "NIFTY 500|CRSLDX"},
    {"^NSEMDCP50", "NSE", "NIFTY MIDCAP 50", "NIFTYMIDCAP50", "NSE_INDEX", "NIFTY MIDCAP 50|NIFTY MCAP50|NSEMDCP50"},
    {"^CNX100", "NSE", "NIFTY 100", "NIFTY100", "NSE_INDEX", "NIFTY 100|CNX100"},
    {"^CNX200", "NSE", "NIFTY 200", "NIFTY200", "NSE_INDEX", "NIFTY 200|CNX200"},
    {"^BSESN", "BSE", "S&P BSE SENSEX", "SENSEX", "BSE_INDEX", "SENSEX|BSE SENSEX|S&P BSE SENSEX|BSESN|BSE 30"},
    {"BSE-500.BO", "BSE", "BSE 500", "BSE500", "BSE_INDEX", "BSE 500"},
    {"BSE-BANK.BO", "BSE", "S&P BSE BANKEX", "BANKEX", "BSE_INDEX", "BANKEX|BSE BANKEX|S&P BSE BANKEX"},
    // Indian indices without a verified Yahoo symbol (broker canonical names only)
    {"", "BSE", "SENSEX 50", "SENSEX50", "BSE_INDEX", "SENSEX 50|SNSX50"},
    {"", "NSE", "NIFTY100 LIQ 15", "NIFTY100LIQ15", "NSE_INDEX", "NIFTY100 LIQ 15"},
    {"", "NSE", "NIFTY MIDCAP 100", "NIFTYMIDCAP100", "NSE_INDEX", "NIFTY MIDCAP 100"},
    {"", "NSE", "NIFTY SMALLCAP 50", "NIFTYSMALLCAP50", "NSE_INDEX", "NIFTY SMALLCAP 50"},
    {"", "NSE", "NIFTY SMALLCAP 100", "NIFTYSMALLCAP100", "NSE_INDEX", "NIFTY SMALLCAP 100"},
    {"", "NSE", "NIFTY MEDIA", "NIFTYMEDIA", "NSE_INDEX", "NIFTY MEDIA"},
    {"", "NSE", "NIFTY SERV SECTOR", "NIFTYSERVSECTOR", "NSE_INDEX", "NIFTY SERV SECTOR"},
    {"", "NSE", "NIFTY COMMODITIES", "NIFTYCOMMODITIES", "NSE_INDEX", "NIFTY COMMODITIES"},
    {"", "NSE", "NIFTY CONSUMPTION", "NIFTYCONSUMPTION", "NSE_INDEX", "NIFTY CONSUMPTION"},
    {"", "NSE", "NIFTY CPSE", "NIFTYCPSE", "NSE_INDEX", "NIFTY CPSE"},
    {"", "NSE", "NIFTY GROWSECT 15", "NIFTYGROWSECT15", "NSE_INDEX", "NIFTY GROWSECT 15"},
    {"", "NSE", "NIFTY100 QUALTY30", "NIFTY100QUALTY30", "NSE_INDEX", "NIFTY100 QUALTY30"},
    {"", "NSE", "NIFTY50 VALUE 20", "NIFTY50VALUE20", "NSE_INDEX", "NIFTY50 VALUE 20"},
    {"", "NSE", "NIFTY ALPHA 50", "NIFTYALPHA50", "NSE_INDEX", "NIFTY ALPHA 50"},
    {"", "NSE", "NIFTY ALPHA LOW-VOL", "NIFTYALPHALOWVOL", "NSE_INDEX", "NIFTY ALPHA LOW-VOL"},
    {"", "NSE", "NIFTY200 QUALTY30", "NIFTY200QUALTY30", "NSE_INDEX", "NIFTY200 QUALTY30"},
};

// ── Futures / commodities ───────────────────────────────────────────────────
struct VsFutureSpec {
    const char* root;
    const char* yahoo;
    const char* exchange_suffix;
    const char* name;
    const char* currency;
    bool commodity;
    const char* aliases; // '|' separated
};

const VsFutureSpec kVsFutures[] = {
    {"GC", "GC=F", "CMX", "Gold", "USD", true, "GOLD|XAU|XAUUSD|GOLD FUTURES|COMEX GOLD|SPOT GOLD"},
    {"SI", "SI=F", "CMX", "Silver", "USD", true, "SILVER|XAG|XAGUSD|SILVER FUTURES"},
    {"PL", "PL=F", "NYM", "Platinum", "USD", true, "PLATINUM|XPT|XPTUSD"},
    {"PA", "PA=F", "NYM", "Palladium", "USD", true, "PALLADIUM|XPD|XPDUSD"},
    {"HG", "HG=F", "CMX", "Copper", "USD", true, "COPPER"},
    {"CL", "CL=F", "NYM", "Crude Oil WTI", "USD", true, "CRUDE|CRUDE OIL|WTI|OIL|USOIL|WTI CRUDE"},
    {"BZ", "BZ=F", "NYM", "Brent Crude Oil", "USD", true, "BRENT|BRENT CRUDE|UKOIL|CRUDE|OIL"},
    {"NG", "NG=F", "NYM", "Natural Gas", "USD", true, "NATURAL GAS|NATGAS"},
    {"RB", "RB=F", "NYM", "RBOB Gasoline", "USD", true, "GASOLINE|RBOB"},
    {"HO", "HO=F", "NYM", "Heating Oil", "USD", true, "HEATING OIL"},
    {"ZC", "ZC=F", "CBT", "Corn", "USX", true, "CORN"},
    {"ZW", "ZW=F", "CBT", "Wheat", "USX", true, "WHEAT"},
    {"ZS", "ZS=F", "CBT", "Soybeans", "USX", true, "SOYBEANS|SOYBEAN"},
    {"KC", "KC=F", "NYB", "Coffee", "USX", true, "COFFEE"},
    {"SB", "SB=F", "NYB", "Sugar", "USX", true, "SUGAR"},
    {"CC", "CC=F", "NYB", "Cocoa", "USD", true, "COCOA"},
    {"CT", "CT=F", "NYB", "Cotton", "USX", true, "COTTON"},
    {"LE", "LE=F", "CME", "Live Cattle", "USX", true, "LIVE CATTLE|CATTLE"},
    {"HE", "HE=F", "CME", "Lean Hogs", "USX", true, "LEAN HOGS|HOGS"},
    {"ES", "ES=F", "CME", "E-mini S&P 500", "USD", false, "E-MINI S&P 500|S&P 500 FUTURES"},
    {"NQ", "NQ=F", "CME", "E-mini Nasdaq-100", "USD", false, "E-MINI NASDAQ 100|NASDAQ 100 FUTURES"},
    {"YM", "YM=F", "CBT", "E-mini Dow", "USD", false, "E-MINI DOW|DOW FUTURES"},
    {"RTY", "RTY=F", "CME", "E-mini Russell 2000", "USD", false, "E-MINI RUSSELL 2000"},
    {"ZN", "ZN=F", "CBT", "10-Year T-Note", "USD", false, "10 YEAR NOTE FUTURES"},
    {"ZB", "ZB=F", "CBT", "30-Year T-Bond", "USD", false, "30 YEAR BOND FUTURES"},
    {"ZF", "ZF=F", "CBT", "5-Year T-Note", "USD", false, "5 YEAR NOTE FUTURES"},
    {"ZT", "ZT=F", "CBT", "2-Year T-Note", "USD", false, "2 YEAR NOTE FUTURES"},
    {"6E", "6E=F", "CME", "Euro FX", "USD", false, "EURO FX FUTURES"},
};

// ── Crypto ──────────────────────────────────────────────────────────────────
struct VsCryptoSpec {
    const char* base;
    const char* yahoo_base;
    const char* name;
    bool crypto_first;
    const char* aliases; // '|' separated
};

const VsCryptoSpec kVsCryptos[] = {
    {"BTC", "BTC", "Bitcoin", true, "BITCOIN|XBT"},
    {"ETH", "ETH", "Ethereum", true, "ETHEREUM|ETHER"},
    {"USDT", "USDT", "Tether", true, "TETHER"},
    {"BNB", "BNB", "BNB", true, "BINANCE COIN"},
    {"SOL", "SOL", "Solana", true, "SOLANA"},
    {"XRP", "XRP", "XRP", true, "RIPPLE"},
    {"USDC", "USDC", "USD Coin", true, "USD COIN"},
    {"DOGE", "DOGE", "Dogecoin", true, "DOGECOIN|XDG"},
    {"ADA", "ADA", "Cardano", true, "CARDANO"},
    {"TRX", "TRX", "TRON", true, "TRON"},
    {"TON", "TON11419", "Toncoin", true, "TONCOIN"},
    {"AVAX", "AVAX", "Avalanche", true, "AVALANCHE"},
    {"SHIB", "SHIB", "Shiba Inu", true, "SHIBA INU"},
    {"DOT", "DOT", "Polkadot", true, "POLKADOT"},
    {"LINK", "LINK", "Chainlink", true, "CHAINLINK"},
    {"BCH", "BCH", "Bitcoin Cash", false, "BITCOIN CASH"},
    {"LTC", "LTC", "Litecoin", true, "LITECOIN"},
    {"POL", "POL28321", "Polygon", true, "POLYGON|MATIC"},
    {"NEAR", "NEAR", "NEAR Protocol", true, "NEAR PROTOCOL"},
    {"UNI", "UNI7083", "Uniswap", true, "UNISWAP"},
    {"ICP", "ICP", "Internet Computer", true, "INTERNET COMPUTER"},
    {"APT", "APT21794", "Aptos", true, "APTOS"},
    {"XLM", "XLM", "Stellar", true, "STELLAR"},
    {"ETC", "ETC", "Ethereum Classic", true, "ETHEREUM CLASSIC"},
    {"FIL", "FIL", "Filecoin", true, "FILECOIN"},
    {"HBAR", "HBAR", "Hedera", true, "HEDERA"},
    {"ATOM", "ATOM", "Cosmos", true, "COSMOS"},
    {"ARB", "ARB11841", "Arbitrum", true, "ARBITRUM"},
    {"OP", "OP", "Optimism", true, "OPTIMISM"},
    {"INJ", "INJ", "Injective", true, "INJECTIVE"},
    {"SUI", "SUI20947", "Sui", false, ""},
    {"XMR", "XMR", "Monero", true, "MONERO"},
    {"AAVE", "AAVE", "Aave", true, ""},
    {"MKR", "MKR", "Maker", true, "MAKER"},
    {"PEPE", "PEPE24478", "Pepe", true, ""},
    {"WIF", "WIF", "dogwifhat", true, "DOGWIFHAT"},
    {"ALGO", "ALGO", "Algorand", true, "ALGORAND"},
    {"VET", "VET", "VeChain", true, "VECHAIN"},
    {"SAND", "SAND", "The Sandbox", false, "THE SANDBOX"},
    {"MANA", "MANA", "Decentraland", true, "DECENTRALAND"},
    {"GRT", "GRT6719", "The Graph", true, "THE GRAPH"},
    {"RENDER", "RENDER", "Render", true, "RNDR"},
    {"IMX", "IMX10603", "Immutable", true, "IMMUTABLE"},
    {"STX", "STX4847", "Stacks", false, "STACKS"},
    {"TAO", "TAO22974", "Bittensor", true, "BITTENSOR"},
    {"KAS", "KAS", "Kaspa", true, "KASPA"},
    {"FET", "FET", "Artificial Superintelligence Alliance", true, "FETCH AI"},
    {"CRO", "CRO", "Cronos", true, "CRONOS"},
};

// ISO 4217 active codes, plus CNH (offshore yuan, quoted by Yahoo as CNH=X).
const char* const kVsIsoCurrencies =
    "AED AFN ALL AMD ANG AOA ARS AUD AWG AZN BAM BBD BDT BGN BHD BIF BMD BND BOB BRL BSD BTN BWP BYN BZD CAD CDF "
    "CHF CLP CNY CNH COP CRC CUP CVE CZK DJF DKK DOP DZD EGP ERN ETB EUR FJD FKP GBP GEL GHS GIP GMD GNF GTQ GYD "
    "HKD HNL HTG HUF IDR ILS INR IQD IRR ISK JMD JOD JPY KES KGS KHR KMF KPW KRW KWD KYD KZT LAK LBP LKR LRD LSL "
    "LYD MAD MDL MGA MKD MMK MNT MOP MRU MUR MVR MWK MXN MYR MZN NAD NGN NIO NOK NPR NZD OMR PAB PEN PGK PHP PKR "
    "PLN PYG QAR RON RSD RUB RWF SAR SBD SCR SDG SEK SGD SHP SLE SOS SRD SSP STN SVC SYP SZL THB TJS TMT TND TOP "
    "TRY TTD TWD TZS UAH UGX USD UYU UZS VES VND VUV WST XAF XCD XOF XPF YER ZAR ZMW ZWL";

// Crypto quote currencies, longest first so USDT / FDUSD are not read as USD.
const char* const kVsCryptoQuotes = "FDUSD USDT USDC BUSD TUSD DAI USD EUR GBP JPY KRW INR TRY BRL AUD CAD CHF BTC ETH BNB";
const char* const kVsStablecoins = "USDT USDC BUSD FDUSD TUSD DAI";
// Quote currencies Yahoo lists crypto against (the rest are mapped or dropped).
const char* const kVsYahooCryptoQuotes = "USD EUR GBP JPY KRW INR TRY BRL AUD CAD CHF BTC ETH";
// Indian series suffixes — the one set (union of the five in the code base).
const char* const kVsSeriesSuffixes = "-EQ -BE -BZ -BL -SM -ST -MF -SG";

// Exchanges whose dollar book is USD (others: USDT; hyperliquid: USDC perpetuals).
const char* const kVsUsdNativeExchanges = "COINBASE COINBASEEXCHANGE COINBASEADVANCED KRAKEN GEMINI BITSTAMP BITFINEX";

// ── Helpers ─────────────────────────────────────────────────────────────────

QStringList vs_words(const char* s) {
    return QString::fromLatin1(s).split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

QStringList vs_bars(const char* s) {
    return QString::fromLatin1(s).split(QLatin1Char('|'), Qt::SkipEmptyParts);
}

bool vs_is_upper_alpha(QChar c) {
    return c >= QLatin1Char('A') && c <= QLatin1Char('Z');
}

bool vs_is_digit(QChar c) {
    return c >= QLatin1Char('0') && c <= QLatin1Char('9');
}

bool vs_all_digits(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!vs_is_digit(c))
            return false;
    return true;
}

bool vs_all_alpha(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!vs_is_upper_alpha(c))
            return false;
    return true;
}

bool vs_has_digit(QStringView s) {
    for (const QChar c : s)
        if (vs_is_digit(c))
            return true;
    return false;
}

// Listing code characters after the venue rules ran.
bool vs_listing_chars_ok(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!vs_is_upper_alpha(c) && !vs_is_digit(c) && c != QLatin1Char('&') && c != QLatin1Char('-'))
            return false;
    return true;
}

QString vs_hk_code(const QString& digits) {
    qsizetype i = 0;
    while (i < digits.size() - 1 && digits.at(i) == QLatin1Char('0'))
        ++i;
    const QString core = digits.mid(i);
    return core.size() >= 4 ? core : core.rightJustified(4, QLatin1Char('0'));
}

// "X.Y" / "X/Y" / "X Y" with one trailing class letter -> "X-Y". Otherwise unchanged.
QString vs_class_share_dash(const QString& code) {
    const qsizetype n = code.size();
    if (n >= 3 && vs_is_upper_alpha(code.at(n - 1))) {
        const QChar sep = code.at(n - 2);
        if (sep == QLatin1Char('.') || sep == QLatin1Char('/') || sep == QLatin1Char(' '))
            return code.left(n - 2) + QLatin1Char('-') + code.at(n - 1);
    }
    return code;
}

// OCC option symbol: ROOT(1-6 letters) YYMMDD C|P STRIKE(8 digits).
bool vs_is_occ(QStringView s) {
    if (s.size() < 16 || s.size() > 21)
        return false;
    const qsizetype tail = 15; // 6 + 1 + 8
    const qsizetype root = s.size() - tail;
    if (root < 1 || root > 6)
        return false;
    for (qsizetype i = 0; i < root; ++i)
        if (!vs_is_upper_alpha(s.at(i)))
            return false;
    for (qsizetype i = root; i < root + 6; ++i)
        if (!vs_is_digit(s.at(i)))
            return false;
    const QChar cp = s.at(root + 6);
    if (cp != QLatin1Char('C') && cp != QLatin1Char('P'))
        return false;
    for (qsizetype i = root + 7; i < s.size(); ++i)
        if (!vs_is_digit(s.at(i)))
            return false;
    return true;
}

bool vs_is_month_code(QChar c) {
    return QStringView(u"FGHJKMNQUVXZ").contains(c);
}

// ── Registry ────────────────────────────────────────────────────────────────

struct VsVenueRow {
    VenueInfo info;
    quint8 flags = 0;
};

// How strongly an alias key names its target.
enum class VsAliasLevel : quint8 {
    Normal,   // free text, and a listing code on a venue of the index's country
    TextOnly, // free text only (a listed ticker / venue code too: "DOW", "NASDAQ")
    CodeOnly, // only when the caller says the code is an index (Yahoo base "DJT" of ^DJT)
};

struct VsAliasRef {
    AliasTarget target;
    int table = 0; // 0 index, 1 futures, 2 crypto
    int row = 0;
    VsAliasLevel level = VsAliasLevel::Normal;
};

// Which aliases a lookup may use.
enum class VsIndexMode : quint8 {
    FreeText,      // typed text: Normal + TextOnly
    ListingCode,   // a code on an equity venue: Normal only
    ExplicitIndex, // caller says "index": every level
};

struct VsRegistry {
    QVector<VenueInfo> venues;
    QVector<quint8> flags;
    QHash<QString, int> by_key;
    QHash<QString, int> by_code;
    QHash<QString, int> by_tv;
    QHash<QString, int> by_yahoo;
    QHash<QString, int> by_suffix;
    QHash<QString, int> primary_by_venue;
    QVector<IndexInfo> indices;
    QHash<QString, int> index_by_symbol;
    QVector<FutureInfo> futures;
    QHash<QString, int> future_by_root;
    QVector<CryptoInfo> cryptos;
    QHash<QString, int> crypto_by_code;
    QHash<QString, int> crypto_by_yahoo_base;
    QHash<QString, QVector<VsAliasRef>> aliases;
    QSet<QString> iso;
    QStringList crypto_quotes;
    QSet<QString> stablecoins;
    QSet<QString> yahoo_crypto_quotes;
    QStringList series;
    QSet<QString> usd_native;
    QStringList problems; // table consistency problems found while building (unit-tested: empty)
};

QString vs_alias_key_ascii(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        const QChar u = c.toUpper();
        if (vs_is_upper_alpha(u) || vs_is_digit(u))
            out.append(u);
        else if (c.isLetterOrNumber())
            out.append(u);
    }
    return out;
}

void vs_add_alias(VsRegistry& reg, const QString& alias_text, const AliasTarget& t, int table, int row,
                  VsAliasLevel level = VsAliasLevel::Normal) {
    const QString key = vs_alias_key_ascii(alias_text);
    if (key.isEmpty())
        return;
    QVector<VsAliasRef>& list = reg.aliases[key];
    for (VsAliasRef& r : list) {
        if (r.table == table && r.row == row) {
            // The strongest level wins (Normal < TextOnly < CodeOnly in enum order).
            if (static_cast<int>(level) < static_cast<int>(r.level)) {
                r.level = level;
                r.target.text_only = level != VsAliasLevel::Normal;
            }
            return;
        }
    }
    AliasTarget tt = t;
    tt.text_only = level != VsAliasLevel::Normal;
    list.push_back({tt, table, row, level});
}

VsRegistry vs_build() {
    VsRegistry reg;

    // Venues
    for (const VsMarketSpec& s : kVsMarkets) {
        VenueInfo v;
        v.key = QString::fromLatin1(s.key);
        v.venue = QString::fromLatin1(s.venue);
        v.mic = QString::fromLatin1(s.mic);
        v.country = QString::fromLatin1(s.country);
        v.currency = QString::fromLatin1(s.currency);
        v.suffixes = vs_words(s.suffixes);
        v.role = s.role;
        v.yahoo_covered = (s.flags & kVsNoYahoo) == 0 && v.role != VenueRole::Derivatives;
        v.approximate = (s.flags & kVsApprox) != 0;
        v.broker_exchange = QString::fromLatin1(s.broker_exchange);
        v.codes = vs_words(s.codes);
        v.tv_codes = vs_words(s.tv);
        v.yahoo_codes = vs_words(s.yahoo);
        const mkt::Venue* ms = v.venue.isEmpty() ? nullptr : mkt::venue_by_code(v.venue);
        v.session_model = ms && ms->id == v.venue;
        if (v.currency.isEmpty() && ms)
            v.currency = ms->currency;
        reg.venues.push_back(v);
        reg.flags.push_back(s.flags);
    }
    for (int i = 0; i < reg.venues.size(); ++i) {
        const VenueInfo& v = reg.venues.at(i);
        if (reg.by_key.contains(v.key))
            reg.problems << QStringLiteral("duplicate venue key %1").arg(v.key);
        reg.by_key.insert(v.key, i);
        auto add = [&reg, i](QHash<QString, int>& h, const QString& code, const char* what) {
            if (code.isEmpty())
                return;
            const auto it = h.constFind(code);
            if (it != h.constEnd()) {
                if (it.value() != i)
                    reg.problems << QStringLiteral("%1 code %2 on %3 and %4")
                                        .arg(QLatin1String(what), code, reg.venues.at(it.value()).key,
                                             reg.venues.at(i).key);
                return;
            }
            h.insert(code, i);
        };
        add(reg.by_code, v.key, "generic");
        for (const QString& c : v.codes)
            add(reg.by_code, c, "generic");
        for (const QString& c : v.tv_codes)
            add(reg.by_tv, c, "tradingview");
        for (const QString& c : v.yahoo_codes)
            add(reg.by_yahoo, c, "yahoo");
        if (!v.approximate)
            for (const QString& s : v.suffixes)
                add(reg.by_suffix, s, "suffix");
        if (!v.venue.isEmpty() && !v.approximate && !reg.primary_by_venue.contains(v.venue) &&
            v.role != VenueRole::Derivatives && v.role != VenueRole::IndexNamespace)
            reg.primary_by_venue.insert(v.venue, i);
    }

    // Reference sets
    for (const QString& c : vs_words(kVsIsoCurrencies))
        reg.iso.insert(c);
    reg.crypto_quotes = vs_words(kVsCryptoQuotes);
    for (const QString& c : vs_words(kVsStablecoins))
        reg.stablecoins.insert(c);
    for (const QString& c : vs_words(kVsYahooCryptoQuotes))
        reg.yahoo_crypto_quotes.insert(c);
    reg.series = vs_words(kVsSeriesSuffixes);
    for (const QString& c : vs_words(kVsUsdNativeExchanges))
        reg.usd_native.insert(c);

    // Indices
    for (const VsIndexSpec& s : kVsIndices) {
        IndexInfo x;
        x.yahoo = QString::fromLatin1(s.yahoo);
        x.venue = QString::fromLatin1(s.venue);
        const int vi = reg.primary_by_venue.value(x.venue, -1);
        if (vi < 0)
            reg.problems << QStringLiteral("index %1: unknown venue %2").arg(x.yahoo, x.venue);
        else
            x.country = reg.venues.at(vi).country;
        x.name = QString::fromLatin1(s.name);
        x.broker_symbol = QString::fromLatin1(s.broker);
        x.broker_exchange = QString::fromLatin1(s.broker_exchange);
        for (QString a : vs_bars(s.aliases)) {
            if (a.startsWith(QLatin1Char('!')))
                x.text_only << a.mid(1);
            else
                x.aliases << a;
        }
        const int row = reg.indices.size();
        reg.indices.push_back(x);
        if (!x.yahoo.isEmpty()) {
            if (reg.index_by_symbol.contains(x.yahoo))
                reg.problems << QStringLiteral("duplicate index %1").arg(x.yahoo);
            reg.index_by_symbol.insert(x.yahoo, row);
        }
        AliasTarget t;
        t.asset_class = AssetClass::Index;
        t.yahoo = x.yahoo;
        t.name = x.name;
        t.venue = x.venue;
        t.country = x.country;
        // Names are not auto-aliases ("AMX" is also America Movil): only the listed ones.
        for (const QString& a : x.aliases)
            vs_add_alias(reg, a, t, 0, row);
        if (!x.broker_symbol.isEmpty())
            vs_add_alias(reg, x.broker_symbol, t, 0, row);
        for (const QString& a : x.text_only)
            vs_add_alias(reg, a, t, 0, row, VsAliasLevel::TextOnly);
        if (!x.yahoo.isEmpty()) {
            // The Yahoo base ("NSEI", "DJT", "NIFTY_FIN_SERVICE") for explicit index lookups.
            QString b = x.yahoo;
            if (b.startsWith(QLatin1Char('^')))
                b.remove(0, 1);
            const qsizetype dot = b.lastIndexOf(QLatin1Char('.'));
            if (dot > 0)
                b.truncate(dot);
            vs_add_alias(reg, b, t, 0, row, VsAliasLevel::CodeOnly);
        }
    }

    // Futures
    for (const VsFutureSpec& s : kVsFutures) {
        FutureInfo f;
        f.root = QString::fromLatin1(s.root);
        f.yahoo = QString::fromLatin1(s.yahoo);
        f.exchange_suffix = QString::fromLatin1(s.exchange_suffix);
        f.name = QString::fromLatin1(s.name);
        f.currency = QString::fromLatin1(s.currency);
        f.commodity = s.commodity;
        f.aliases = vs_bars(s.aliases);
        const int row = reg.futures.size();
        reg.futures.push_back(f);
        if (reg.future_by_root.contains(f.root))
            reg.problems << QStringLiteral("duplicate futures root %1").arg(f.root);
        reg.future_by_root.insert(f.root, row);
        AliasTarget t;
        t.asset_class = AssetClass::Future;
        t.yahoo = f.yahoo;
        t.name = f.name;
        t.venue = QStringLiteral("CME");
        vs_add_alias(reg, f.name, t, 1, row);
        for (const QString& a : f.aliases)
            vs_add_alias(reg, a, t, 1, row);
    }

    // Crypto
    QSet<QString> bases;
    for (const VsCryptoSpec& s : kVsCryptos)
        bases.insert(QString::fromLatin1(s.base));
    for (const VsCryptoSpec& s : kVsCryptos) {
        CryptoInfo c;
        c.base = QString::fromLatin1(s.base);
        c.yahoo_base = QString::fromLatin1(s.yahoo_base);
        c.name = QString::fromLatin1(s.name);
        c.crypto_first = s.crypto_first;
        c.aliases = vs_bars(s.aliases);
        const int row = reg.cryptos.size();
        reg.cryptos.push_back(c);
        auto add_code = [&reg, row](const QString& code) {
            const QString k = vs_alias_key_ascii(code);
            if (!k.isEmpty() && !reg.crypto_by_code.contains(k))
                reg.crypto_by_code.insert(k, row);
        };
        add_code(c.base);
        add_code(c.yahoo_base);
        reg.crypto_by_yahoo_base.insert(c.yahoo_base, row);
        AliasTarget t;
        t.asset_class = AssetClass::Crypto;
        t.yahoo = c.yahoo_base + QStringLiteral("-USD");
        t.name = c.name;
        t.venue = QStringLiteral("CRYPTO");
        // Names and alternative codes are aliases; a bare base is not (crypto_for_base
        // handles it, with the crypto_first rule).
        QStringList texts = c.aliases;
        texts.prepend(c.name);
        for (const QString& a : texts) {
            const QString k = vs_alias_key_ascii(a);
            if (k.isEmpty() || bases.contains(k))
                continue;
            add_code(a);
            vs_add_alias(reg, a, t, 2, row);
        }
    }

    // Alias collisions across entries: only the deliberate ones are allowed.
    static const QSet<QString> kVsMultiOk = {QStringLiteral("CRUDE"), QStringLiteral("OIL")};
    for (auto it = reg.aliases.constBegin(); it != reg.aliases.constEnd(); ++it) {
        if (it.value().size() > 1 && !kVsMultiOk.contains(it.key())) {
            QStringList who;
            for (const VsAliasRef& r : it.value())
                who << (r.target.yahoo.isEmpty() ? r.target.name : r.target.yahoo);
            reg.problems << QStringLiteral("alias %1 -> %2").arg(it.key(), who.join(QLatin1Char(',')));
        }
    }

    // Every MarketSessions suffix maps to the same venue here, and every MarketSessions
    // venue has a primary entry.
    for (const mkt::Venue& mv : mkt::all_venues()) {
        if (!reg.primary_by_venue.contains(mv.id))
            reg.problems << QStringLiteral("MarketSessions venue %1 has no entry").arg(mv.id);
        for (const QString& sfx : mv.suffixes) {
            const int vi = reg.by_suffix.value(sfx, -1);
            if (vi < 0 || reg.venues.at(vi).venue != mv.id)
                reg.problems << QStringLiteral("MarketSessions suffix %1 (%2) not mapped to the same venue")
                                    .arg(sfx, mv.id);
        }
    }
    return reg;
}

const VsRegistry& vs_reg() {
    // Immortal: symbol parsing runs on worker threads (instrument seeding, imports) that can
    // still be busy when main() returns. A destructible registry was freed under them at
    // exit - a use-after-free that crashed short runs (the self-tests) on Linux and macOS.
    static const VsRegistry& reg = *new VsRegistry(vs_build());
    return reg;
}

const VenueInfo* vs_at(int i) {
    const VsRegistry& r = vs_reg();
    return (i >= 0 && i < r.venues.size()) ? &r.venues.at(i) : nullptr;
}

quint8 vs_flags(const VenueInfo* v) {
    if (!v)
        return 0;
    const VsRegistry& r = vs_reg();
    const int i = r.by_key.value(v->key, -1);
    return i >= 0 ? r.flags.at(i) : quint8(0);
}

const VenueInfo* vs_primary_for_venue(const QString& venue_id) {
    return vs_at(vs_reg().primary_by_venue.value(venue_id, -1));
}

// Clean an incoming code: NFKC, trimmed, upper case, inner whitespace collapsed.
QString vs_clean_code(const QString& in) {
    return in.normalized(QString::NormalizationForm_KC).simplified().toUpper();
}

// Index alias lookup. Returns true when `code` names an index (then *yahoo may be
// empty: an index Yahoo does not carry). `country` filters ("" = any); `mode` picks
// the alias levels allowed.
bool vs_index_for_code(const QString& code, const QString& country, VsIndexMode mode, QString* yahoo,
                       QString* venue = nullptr) {
    const VsRegistry& r = vs_reg();
    const auto it = r.aliases.constFind(vs_alias_key_ascii(code));
    if (it == r.aliases.constEnd())
        return false;
    for (const VsAliasRef& ref : it.value()) {
        if (ref.table != 0)
            continue;
        if (!country.isEmpty() && ref.target.country != country)
            continue;
        if (mode == VsIndexMode::ListingCode && ref.level != VsAliasLevel::Normal)
            continue;
        if (mode == VsIndexMode::FreeText && ref.level == VsAliasLevel::CodeOnly)
            continue;
        if (yahoo)
            *yahoo = ref.target.yahoo;
        if (venue)
            *venue = ref.target.venue;
        return true;
    }
    return false;
}

QString vs_commodity_for_code(const QString& code) {
    const VsRegistry& r = vs_reg();
    const auto it = r.aliases.constFind(vs_alias_key_ascii(code));
    if (it == r.aliases.constEnd())
        return {};
    for (const VsAliasRef& ref : it.value())
        if (ref.table == 1)
            return ref.target.yahoo;
    return {};
}

// FX pair code -> Yahoo; "" when not a pair of ISO currencies. Metals -> futures.
QString vs_fx_yahoo(QString code) {
    if (code.endsWith(QLatin1String("=X")))
        code.chop(2);
    code.remove(QLatin1Char('/'));
    code.remove(QLatin1Char('-'));
    code.remove(QLatin1Char(' '));
    code.remove(QLatin1Char('_'));
    const VsRegistry& r = vs_reg();
    if (code.size() == 3 && r.iso.contains(code) && code != QLatin1String("USD"))
        return code + QStringLiteral("=X");
    if (code.size() != 6)
        return {};
    const QString b = code.left(3);
    const QString q = code.right(3);
    if (q == QLatin1String("USD") &&
        (b == QLatin1String("XAU") || b == QLatin1String("XAG") || b == QLatin1String("XPT") || b == QLatin1String("XPD")))
        return vs_commodity_for_code(code);
    if (!r.iso.contains(b) || !r.iso.contains(q) || b == q)
        return {};
    if (b == QLatin1String("USD"))
        return q + QStringLiteral("=X");
    return code + QStringLiteral("=X");
}

// Split a crypto pair code into base / quote. Concatenated codes need a table base.
bool vs_split_crypto(QString code, QString* base, QString* quote, bool* perp) {
    const VsRegistry& r = vs_reg();
    if (perp)
        *perp = false;
    const qsizetype colon = code.indexOf(QLatin1Char(':'));
    if (colon > 0)
        code.truncate(colon); // "BTC/USDT:USDT" settle
    if (code.endsWith(QLatin1String(".P"))) {
        code.chop(2);
        if (perp)
            *perp = true;
    }
    for (const QChar sep : {QLatin1Char('/'), QLatin1Char('-')}) {
        const qsizetype at = code.indexOf(sep);
        if (at > 0) {
            *base = code.left(at);
            *quote = code.mid(at + 1);
            return !base->isEmpty() && !quote->isEmpty();
        }
    }
    for (const QString& q : r.crypto_quotes) {
        if (code.size() > q.size() + 1 && code.endsWith(q)) {
            const QString b = code.left(code.size() - q.size());
            if (r.crypto_by_code.contains(b)) {
                *base = b;
                *quote = q;
                return true;
            }
        }
    }
    if (r.crypto_by_code.contains(code)) {
        *base = code;
        quote->clear();
        return true;
    }
    return false;
}

// Crypto code -> Yahoo; "" for an unknown base or a quote Yahoo does not list.
QString vs_crypto_yahoo(const QString& code) {
    QString base, quote;
    bool perp = false;
    if (!vs_split_crypto(code, &base, &quote, &perp))
        return {};
    const CryptoInfo* c = crypto_for_base(base);
    if (!c)
        return {};
    const VsRegistry& r = vs_reg();
    QString q = quote.isEmpty() ? QStringLiteral("USD") : quote;
    if (r.stablecoins.contains(q))
        q = QStringLiteral("USD");
    if (!r.yahoo_crypto_quotes.contains(q) || q == c->base)
        return {};
    return c->yahoo_base + QLatin1Char('-') + q;
}

// Futures code -> Yahoo: root, root + 1! / 2!, dated ROOT+MONTH+YY(YY), commodity alias.
QString vs_future_yahoo(QString code) {
    const VsRegistry& r = vs_reg();
    if (code.endsWith(QLatin1String("1!")) || code.endsWith(QLatin1String("2!")))
        code.chop(2);
    if (const auto it = r.future_by_root.constFind(code); it != r.future_by_root.constEnd())
        return r.futures.at(it.value()).yahoo;
    // Dated: ROOT (1-3) + month code + 2 or 4 digit year.
    for (int rl = 3; rl >= 1; --rl) {
        for (const int yl : {2, 4}) {
            if (code.size() != rl + 1 + yl)
                continue;
            const auto it = r.future_by_root.constFind(code.left(rl));
            if (it == r.future_by_root.constEnd())
                continue;
            const QChar m = code.at(rl);
            const QStringView year = QStringView(code).mid(rl + 1);
            if (!vs_is_month_code(m) || !vs_all_digits(year))
                continue;
            const FutureInfo& f = r.futures.at(it.value());
            return f.root + m + year.right(2).toString() + QLatin1Char('.') + f.exchange_suffix;
        }
    }
    return vs_commodity_for_code(code);
}

// Equity listing code on a venue -> Yahoo, applying the venue's rules. "" when the code
// cannot be a listing there.
QString vs_listing_yahoo(QString base, const VenueInfo& v, quint8 flags) {
    while (base.endsWith(QLatin1Char('.')))
        base.chop(1); // LSE TIDM padding: "BP." -> "BP"
    base.replace(QLatin1Char('_'), QLatin1Char('-'));
    if (flags & kVsSeries)
        base = strip_series_suffix(base);
    base = vs_class_share_dash(base);
    if (v.role == VenueRole::UsListing || (flags & kVsDotDash)) {
        base.replace(QLatin1Char('.'), QLatin1Char('-'));
        base.replace(QLatin1Char('/'), QLatin1Char('-'));
    }
    if ((flags & kVsHkPad) && vs_all_digits(base))
        base = vs_hk_code(base);
    if ((flags & kVsPad6) && vs_all_digits(base) && base.size() < 6)
        base = base.rightJustified(6, QLatin1Char('0'));
    if (!vs_listing_chars_ok(base))
        return {};
    if (v.role == VenueRole::UsListing)
        return base;
    if (v.suffixes.isEmpty())
        return {};
    return base + QLatin1Char('.') + v.suffixes.first();
}

// Known Yahoo suffix of `s` ("RELIANCE.NS" -> NSE entry); base in *base.
const VenueInfo* vs_suffix_owner(const QString& s, QString* base, QString* suffix) {
    const qsizetype dot = s.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot >= s.size() - 1)
        return nullptr;
    const QString sfx = s.mid(dot + 1);
    const VenueInfo* v = vs_at(vs_reg().by_suffix.value(sfx, -1));
    if (!v)
        return nullptr;
    if (base)
        *base = s.left(dot);
    if (suffix)
        *suffix = sfx;
    return v;
}

// Yahoo crypto form BASE-QUOTE (quote a fiat / crypto quote Yahoo or exchanges use).
bool vs_is_crypto_dash(const QString& s, QString* base, QString* quote) {
    const qsizetype dash = s.lastIndexOf(QLatin1Char('-'));
    if (dash < 2 || dash >= s.size() - 1)
        return false;
    const QString q = s.mid(dash + 1);
    if (!vs_reg().crypto_quotes.contains(q))
        return false;
    const QString b = s.left(dash);
    for (const QChar c : b)
        if (!vs_is_upper_alpha(c) && !vs_is_digit(c))
            return false;
    if (base)
        *base = b;
    if (quote)
        *quote = q;
    return true;
}

bool vs_self_describing(const QString& up) {
    if (up.startsWith(QLatin1Char('^')) || up.endsWith(QLatin1String("=X")) || up.endsWith(QLatin1String("=F")))
        return true;
    if (vs_is_occ(up) || vs_reg().index_by_symbol.contains(up))
        return true;
    return vs_suffix_owner(up, nullptr, nullptr) != nullptr;
}

} // namespace vsym_detail
} // namespace

using namespace vsym_detail;

// ── Detail hooks (SymbolInput.cpp, tests) ───────────────────────────────────

namespace venue_symbols_detail {

// Canonical spelling of a Yahoo-form symbol (upper case in): see canonical_symbol().
QString yahoo_spelling(const QString& up) {
    if (up.isEmpty())
        return {};
    if (up.startsWith(QLatin1Char('^')) || up.endsWith(QLatin1String("=F")) || vs_is_occ(up) ||
        vs_reg().index_by_symbol.contains(up))
        return up;
    if (up.endsWith(QLatin1String("=X"))) {
        const QString pair = up.chopped(2);
        if (pair.size() == 6 && pair.startsWith(QLatin1String("USD")) && vs_reg().iso.contains(pair.right(3)) &&
            pair.right(3) != QLatin1String("USD"))
            return pair.right(3) + QStringLiteral("=X");
        return up;
    }
    QString base, sfx;
    if (const VenueInfo* v = vs_suffix_owner(up, &base, &sfx)) {
        if (v->role == VenueRole::Futures)
            return up;
        if (base.startsWith(QLatin1Char('^')))
            return up; // ^SET.BK
        QString b = base;
        b.replace(QLatin1Char('_'), QLatin1Char('-'));
        if (vs_flags(v) & kVsSeries)
            b = strip_series_suffix(b);
        b = vs_class_share_dash(b);
        b.replace(QLatin1Char('.'), QLatin1Char('-'));
        b.replace(QLatin1Char('/'), QLatin1Char('-'));
        if ((vs_flags(v) & kVsHkPad) && vs_all_digits(b))
            b = vs_hk_code(b);
        return b + QLatin1Char('.') + sfx;
    }
    if (vs_is_crypto_dash(up, nullptr, nullptr))
        return up; // never reinterpret a Yahoo crypto symbol (TON-USD is a real, different coin)
    QString b = up;
    b.replace(QLatin1Char('_'), QLatin1Char('-'));
    b = vs_class_share_dash(b);
    b.replace(QLatin1Char('.'), QLatin1Char('-'));
    b.replace(QLatin1Char('/'), QLatin1Char('-'));
    return b;
}

QStringList table_problems() {
    return vs_reg().problems;
}

} // namespace venue_symbols_detail

// ── Venue lookup ────────────────────────────────────────────────────────────

const QVector<VenueInfo>& all_venue_infos() {
    return vs_reg().venues;
}

const VenueInfo* venue_info(const QString& key) {
    return vs_at(vs_reg().by_key.value(key.trimmed().toUpper(), -1));
}

const VenueInfo* venue_info_for_code(const QString& code, CodeSpace space, const QString& country) {
    const QString key = code.trimmed().toUpper();
    if (key.isEmpty())
        return nullptr;
    const VsRegistry& r = vs_reg();
    auto ms = [&key]() -> int {
        const mkt::Venue* v = mkt::venue_by_code(key);
        return v ? vs_reg().primary_by_venue.value(v->id, -1) : -1;
    };
    int i = -1;
    switch (space) {
        case CodeSpace::TradingView:
            i = r.by_tv.value(key, -1);
            if (i < 0)
                i = r.by_code.value(key, -1);
            if (i < 0)
                i = ms();
            if (i < 0)
                i = r.by_yahoo.value(key, -1);
            break;
        case CodeSpace::Yahoo:
            i = r.by_yahoo.value(key, -1);
            if (i < 0)
                i = r.by_code.value(key, -1);
            if (i < 0)
                i = ms();
            if (i < 0)
                i = r.by_tv.value(key, -1);
            break;
        case CodeSpace::Any:
            i = r.by_code.value(key, -1);
            if (i < 0)
                i = ms();
            if (i < 0)
                i = r.by_tv.value(key, -1);
            if (i < 0)
                i = r.by_yahoo.value(key, -1);
            break;
    }
    const VenueInfo* v = vs_at(i);
    if (v && v->role == VenueRole::Euronext) {
        const QString cc = country.trimmed().toUpper();
        for (const VsEuronextSpec& e : kVsEuronext)
            if (cc == QLatin1String(e.country))
                return venue_info(QString::fromLatin1(e.key));
    }
    return v;
}

const VenueInfo* venue_info_for_suffix(const QString& suffix) {
    QString s = suffix.trimmed().toUpper();
    if (s.startsWith(QLatin1Char('.')))
        s.remove(0, 1);
    return vs_at(vs_reg().by_suffix.value(s, -1));
}

QString venue_id_for_code(const QString& code, CodeSpace space) {
    const VenueInfo* v = venue_info_for_code(code, space);
    return v ? v->venue : QString();
}

// ── Yahoo form ──────────────────────────────────────────────────────────────

namespace {
namespace vsym_detail {

QString vs_to_yahoo_on(QString code, const VenueInfo& v, AssetClass cls) {
    const quint8 flags = vs_flags(&v);
    if (!v.yahoo_covered && v.role != VenueRole::IndexNamespace)
        return {};
    // Fyers spells indices "NIFTY50-INDEX": the suffix says "index", and an index we do
    // not know must not fall through to an equity listing ("NIFTYAUTO-INDEX.NS").
    if (code.size() > 6 && code.endsWith(QLatin1String("-INDEX"))) {
        code.chop(6);
        cls = AssetClass::Index;
    }
    if (cls == AssetClass::Index && v.role != VenueRole::Listing && v.role != VenueRole::UsListing) {
        QString y;
        if (v.role != VenueRole::Derivatives && vs_index_for_code(code, v.country, VsIndexMode::ExplicitIndex, &y))
            return y;
        return {};
    }
    switch (v.role) {
        case VenueRole::Derivatives:
        case VenueRole::Euronext:
            return {};
        case VenueRole::Fx:
            return vs_fx_yahoo(code);
        case VenueRole::Crypto:
            return vs_crypto_yahoo(code);
        case VenueRole::Futures:
            return vs_future_yahoo(code);
        case VenueRole::IndexNamespace: {
            QString y;
            if (vs_index_for_code(code, v.country, VsIndexMode::ExplicitIndex, &y))
                return y;
            if (v.country.isEmpty())
                return vs_commodity_for_code(code);
            return {};
        }
        case VenueRole::Listing:
        case VenueRole::UsListing:
            break;
    }
    if (cls == AssetClass::Index || cls == AssetClass::Unknown) {
        QString y;
        const VsIndexMode mode = cls == AssetClass::Index ? VsIndexMode::ExplicitIndex : VsIndexMode::ListingCode;
        if (vs_index_for_code(code, v.country, mode, &y))
            return y;
        if (cls == AssetClass::Index) {
            // An index MarketSessions knows by its caret symbol (round trip of from_yahoo).
            if (vs_listing_chars_ok(code)) {
                const mkt::Venue* mv = mkt::venue_for_symbol(QLatin1Char('^') + code);
                if (mv && mv->id == v.venue)
                    return QLatin1Char('^') + code;
            }
            return {};
        }
    }
    if (cls != AssetClass::Unknown && !is_equity_like(cls))
        return {};
    if (is_exchange_contract_name(code))
        return {};
    return vs_listing_yahoo(code, v, flags);
}

} // namespace vsym_detail
} // namespace

QString to_yahoo(const QString& listing_code, const QString& venue_or_code, AssetClass cls, const QString& country) {
    const QString code = vs_clean_code(listing_code);
    if (code.isEmpty() || is_valid_isin(code))
        return {};
    if (vs_self_describing(code) && !code.contains(QLatin1Char(' ')))
        return venue_symbols_detail::yahoo_spelling(code);
    if (venue_or_code.trimmed().isEmpty()) {
        // No venue: the code is read as Yahoo / US form. An index code gets its caret.
        if (cls == AssetClass::Index && vs_listing_chars_ok(code))
            return QLatin1Char('^') + code;
        return canonical_symbol(code);
    }
    const VenueInfo* v = venue_info_for_code(venue_or_code, CodeSpace::Any, country);
    if (!v)
        return {};
    return vs_to_yahoo_on(code, *v, cls);
}

QString to_yahoo_tv(const QString& tv_symbol, const QString& tv_exchange, const QString& country, AssetClass cls) {
    QString code = vs_clean_code(tv_symbol);
    QString exch = tv_exchange.trimmed().toUpper();
    const qsizetype colon = code.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        if (exch.isEmpty())
            exch = code.left(colon);
        code = code.mid(colon + 1);
    }
    if (code.isEmpty() || is_valid_isin(code))
        return {};
    if (exch.isEmpty())
        return to_yahoo(code, QString(), cls, country);
    const VenueInfo* v = venue_info_for_code(exch, CodeSpace::TradingView, country);
    if (!v)
        return {};
    if (vs_self_describing(code))
        return venue_symbols_detail::yahoo_spelling(code);
    return vs_to_yahoo_on(code, *v, cls);
}

YahooParts from_yahoo(const QString& yahoo_symbol) {
    YahooParts p;
    const QString up = yahoo_symbol.normalized(QString::NormalizationForm_KC).trimmed().toUpper();
    if (up.isEmpty() || up.contains(QLatin1Char(' ')) || is_valid_isin(up))
        return p;
    for (const QChar c : up) {
        if (!vs_is_upper_alpha(c) && !vs_is_digit(c) && !QStringView(u".-^=&_/").contains(c))
            return p;
    }
    const QString s = venue_symbols_detail::yahoo_spelling(up);
    if (s.isEmpty())
        return p;
    const VsRegistry& r = vs_reg();
    p.symbol = s;

    // Table index (caret or suffixed).
    if (const auto it = r.index_by_symbol.constFind(s); it != r.index_by_symbol.constEnd()) {
        const IndexInfo& x = r.indices.at(it.value());
        QString b = s.startsWith(QLatin1Char('^')) ? s.mid(1) : s;
        const qsizetype dot = b.lastIndexOf(QLatin1Char('.'));
        if (dot > 0) {
            p.suffix = b.mid(dot + 1);
            b.truncate(dot);
        }
        p.code = b;
        p.base = b;
        p.venue = x.venue;
        p.country = x.country;
        p.cls = AssetClass::Index;
        p.venue_from_suffix = !p.suffix.isEmpty();
        if (const VenueInfo* pv = vs_primary_for_venue(x.venue))
            p.market = pv->key;
        return p;
    }
    if (s.startsWith(QLatin1Char('^'))) {
        QString b = s.mid(1);
        QString sfx;
        const VenueInfo* owner = nullptr;
        if (const VenueInfo* v = vs_suffix_owner(b, nullptr, &sfx)) {
            owner = v;
            b.truncate(b.size() - sfx.size() - 1);
            p.suffix = sfx;
        }
        if (b.isEmpty())
            return {};
        p.code = b;
        p.base = b;
        p.cls = AssetClass::Index;
        const mkt::Venue* mv = mkt::venue_for_symbol(s);
        p.venue = mv ? mv->id : (owner ? owner->venue : QString());
        if (const VenueInfo* pv = owner ? owner : vs_primary_for_venue(p.venue)) {
            p.country = pv->country;
            p.market = pv->key;
        }
        p.venue_from_suffix = owner != nullptr;
        return p;
    }
    if (s.endsWith(QLatin1String("=X"))) {
        const QString pair = s.chopped(2);
        p.cls = AssetClass::Fx;
        p.venue = QStringLiteral("FX");
        p.market = QStringLiteral("FX");
        if (pair.size() == 3 && r.iso.contains(pair)) {
            p.base = QStringLiteral("USD");
            p.quote = pair;
        } else if (pair.size() == 6 && r.iso.contains(pair.left(3)) && r.iso.contains(pair.right(3))) {
            p.base = pair.left(3);
            p.quote = pair.right(3);
        } else {
            return {};
        }
        p.code = p.base + p.quote;
        return p;
    }
    if (s.endsWith(QLatin1String("=F"))) {
        p.code = s.chopped(2);
        p.base = p.code;
        if (p.code.isEmpty())
            return {};
        p.cls = AssetClass::Future;
        p.venue = QStringLiteral("CME");
        p.market = QStringLiteral("CME");
        p.country = QStringLiteral("US");
        return p;
    }
    if (vs_is_occ(s)) {
        p.code = s;
        p.base = s.left(s.size() - 15);
        p.cls = AssetClass::Option;
        p.venue = QStringLiteral("NYSE");
        p.market = QStringLiteral("NYSE");
        p.country = QStringLiteral("US");
        return p;
    }
    QString base, sfx;
    if (const VenueInfo* v = vs_suffix_owner(s, &base, &sfx)) {
        p.suffix = sfx;
        p.code = base;
        p.base = base;
        p.venue = v->venue;
        p.market = v->key;
        p.country = v->country;
        p.venue_from_suffix = true;
        if (v->role == VenueRole::Futures) {
            p.cls = AssetClass::Future;
            // Root of a dated contract: strip month + 2-digit year when the root is known.
            if (base.size() > 3 && vs_is_month_code(base.at(base.size() - 3)) &&
                vs_all_digits(QStringView(base).right(2)))
                p.base = base.left(base.size() - 3);
        } else {
            p.cls = AssetClass::Equity;
        }
        return p;
    }
    QString cb, cq;
    if (vs_is_crypto_dash(s, &cb, &cq)) {
        p.cls = AssetClass::Crypto;
        p.venue = QStringLiteral("CRYPTO");
        p.market = QStringLiteral("CRYPTO");
        const auto it = r.crypto_by_yahoo_base.constFind(cb);
        p.base = it != r.crypto_by_yahoo_base.constEnd() ? r.cryptos.at(it.value()).base : cb;
        p.quote = cq;
        p.code = p.base + QLatin1Char('-') + cq;
        return p;
    }
    if (!vs_listing_chars_ok(s))
        return {};
    p.code = s;
    p.base = s;
    p.cls = AssetClass::Equity;
    p.venue = QStringLiteral("NYSE");
    p.market = QStringLiteral("NYSE");
    p.country = QStringLiteral("US");
    p.venue_from_suffix = false;
    return p;
}

AssetClass asset_class_of(const QString& symbol, const QString& quote_type) {
    const QString qt = quote_type.trimmed().toUpper();
    if (!qt.isEmpty() && qt != QLatin1String("ALTSYMBOL") && qt != QLatin1String("NONE")) {
        if (qt == QLatin1String("EQUITY"))
            return AssetClass::Equity;
        if (qt == QLatin1String("CRYPTOCURRENCY"))
            return AssetClass::Crypto;
        const AssetClass c = asset_class_from_slug(quote_type);
        if (c != AssetClass::Unknown)
            return c;
    }
    QString up = symbol.normalized(QString::NormalizationForm_KC).trimmed().toUpper();
    if (up.isEmpty() || up.contains(QLatin1Char(' ')) || is_valid_isin(up))
        return AssetClass::Unknown;
    const qsizetype colon = up.indexOf(QLatin1Char(':'));
    if (colon > 0 && !up.left(colon).contains(QLatin1Char('/')))
        up = up.mid(colon + 1);
    if (is_exchange_contract_name(up))
        return up.endsWith(QLatin1String("FUT")) ? AssetClass::Future : AssetClass::Option;
    if (up.endsWith(QLatin1String("1!")) || up.endsWith(QLatin1String("2!")))
        return AssetClass::Future;
    const qsizetype slash = up.indexOf(QLatin1Char('/'));
    if (slash > 0) {
        QString b = up.left(slash);
        QString q = up.mid(slash + 1);
        const qsizetype c2 = q.indexOf(QLatin1Char(':'));
        if (c2 > 0)
            q.truncate(c2);
        const VsRegistry& r = vs_reg();
        if (r.iso.contains(b) && r.iso.contains(q))
            return AssetClass::Fx;
        if (r.crypto_quotes.contains(q))
            return AssetClass::Crypto;
        if (q.size() == 1 && vs_all_alpha(q))
            return AssetClass::Equity;
        return AssetClass::Unknown;
    }
    const YahooParts p = from_yahoo(up);
    return p.valid() ? p.cls : AssetClass::Unknown;
}

QString instrument_id_for(const QString& yahoo_symbol) {
    const YahooParts p = from_yahoo(yahoo_symbol);
    if (!p.valid())
        return {};
    switch (p.cls) {
        case AssetClass::Fx:
            return QStringLiteral("FX:") + p.base + p.quote;
        case AssetClass::Crypto:
            return QStringLiteral("CRYPTO:") + p.symbol;
        case AssetClass::Index:
            return QStringLiteral("IDX:") + (p.symbol.startsWith(QLatin1Char('^')) ? p.symbol.mid(1) : p.symbol);
        case AssetClass::Future:
            return QStringLiteral("FUT:") + (p.symbol.endsWith(QLatin1String("=F")) ? p.code : p.symbol);
        case AssetClass::Option:
            return QStringLiteral("OPT:") + p.symbol;
        default:
            break;
    }
    if (!p.venue_from_suffix)
        return QStringLiteral("US:") + p.code;
    const VenueInfo* v = venue_info_for_suffix(p.suffix);
    if (!v)
        return {};
    return (v->mic.isEmpty() ? v->key : v->mic) + QLatin1Char(':') + p.code;
}

QString display_symbol(const QString& yahoo_symbol) {
    const YahooParts p = from_yahoo(yahoo_symbol);
    if (!p.valid())
        return {};
    switch (p.cls) {
        case AssetClass::Fx:
        case AssetClass::Crypto:
            return p.base + QLatin1Char('/') + p.quote;
        case AssetClass::Index:
            if (const IndexInfo* x = index_for_symbol(p.symbol))
                return x->name;
            return p.code;
        case AssetClass::Future:
        case AssetClass::Option:
            return p.code;
        default:
            break;
    }
    // US class shares read dotted in the UI ("BRK-B" -> "BRK.B").
    const QString& c = p.code;
    if (!p.venue_from_suffix && c.size() >= 3 && c.at(c.size() - 2) == QLatin1Char('-') && vs_is_upper_alpha(c.back()))
        return c.left(c.size() - 2) + QLatin1Char('.') + c.back();
    return c;
}

Instrument instrument_skeleton(const QString& yahoo_symbol) {
    Instrument inst;
    const YahooParts p = from_yahoo(yahoo_symbol);
    if (!p.valid())
        return inst;
    inst.symbol = p.symbol;
    inst.id = instrument_id_for(p.symbol);
    inst.display = display_symbol(p.symbol);
    inst.asset_class = p.cls;
    inst.venue = p.venue;
    inst.country = p.country;
    if (p.cls == AssetClass::Fx || p.cls == AssetClass::Crypto) {
        inst.country.clear();
        inst.currency = p.quote;
        if (p.cls == AssetClass::Crypto) {
            if (const CryptoInfo* c = crypto_for_base(p.base)) {
                if (c->yahoo_base + QLatin1Char('-') + p.quote == p.symbol) {
                    inst.name = c->name;
                    inst.aliases = c->aliases;
                }
            }
        }
        return inst;
    }
    if (p.cls == AssetClass::Index) {
        if (const IndexInfo* x = index_for_symbol(p.symbol)) {
            inst.name = x->name;
            inst.aliases = x->aliases;
        }
        return inst;
    }
    if (p.cls == AssetClass::Future) {
        if (const FutureInfo* f = future_for_root(p.base)) {
            inst.currency = f->currency;
            if (p.symbol == f->yahoo) {
                inst.name = f->name;
                inst.aliases = f->aliases;
            }
        }
        inst.mic = QStringLiteral("XCME");
        return inst;
    }
    if (p.cls == AssetClass::Option)
        return inst;
    if (p.venue_from_suffix) {
        if (const VenueInfo* v = venue_info_for_suffix(p.suffix)) {
            inst.mic = v->mic;
            inst.currency = v->currency;
        }
    } else {
        // Yahoo's bare form does not say NYSE vs Nasdaq: leave the venue to the record's source.
        inst.venue.clear();
        inst.currency = QStringLiteral("USD");
    }
    return inst;
}

// ── Broker / exchange forms ─────────────────────────────────────────────────

QString to_broker_symbol(const Instrument& inst, const QString& broker_id, QString* exchange) {
    if (exchange)
        exchange->clear();
    const QString bid = broker_id.trimmed().toLower();
    if (!bid.isEmpty()) {
        const QString v = inst.provider_symbols.value(QStringLiteral("broker:") + bid);
        if (!v.isEmpty()) {
            const QStringList parts = v.split(QLatin1Char('|'));
            if (parts.size() >= 2 && !parts.at(1).isEmpty()) {
                if (exchange)
                    *exchange = parts.at(0);
                return parts.at(1);
            }
        }
    }
    const YahooParts p = from_yahoo(inst.symbol);
    if (!p.valid())
        return {};
    if (p.cls == AssetClass::Index) {
        const IndexInfo* x = index_for_symbol(p.symbol);
        if (!x || x->broker_symbol.isEmpty())
            return {};
        if (exchange)
            *exchange = x->broker_exchange;
        return x->broker_symbol;
    }
    if (p.cls != AssetClass::Equity)
        return {};
    if (!p.venue_from_suffix) {
        QString code = p.code;
        if (code.size() >= 3 && code.at(code.size() - 2) == QLatin1Char('-') && vs_is_upper_alpha(code.back()))
            code[code.size() - 2] = bid == QLatin1String("ibkr") ? QLatin1Char(' ') : QLatin1Char('.');
        if (exchange) {
            const QString venue = inst.venue.trimmed().toUpper();
            *exchange = (venue == QLatin1String("NYSE") || venue == QLatin1String("NASDAQ")) ? venue : QString();
        }
        return code;
    }
    const VenueInfo* v = venue_info_for_suffix(p.suffix);
    if (!v)
        return {};
    if (exchange)
        *exchange = v->broker_exchange.isEmpty() ? v->venue : v->broker_exchange;
    return p.code;
}

QString crypto_pair(const Instrument& inst, const QString& exchange_id, CryptoPairStyle style) {
    const QString ex = exchange_id.trimmed().toLower();
    if (style == CryptoPairStyle::Unified && !ex.isEmpty()) {
        const QString v = inst.provider_symbols.value(ex);
        if (!v.isEmpty())
            return v;
    }
    const YahooParts p = from_yahoo(inst.symbol);
    if (!p.valid() || p.cls != AssetClass::Crypto)
        return {};
    const QString exu = ex.toUpper();
    const QString base = p.base;
    QString quote = p.quote;
    QString settle;
    if (quote == QLatin1String("USD")) {
        if (exu == QLatin1String("HYPERLIQUID")) {
            quote = QStringLiteral("USDC");
            settle = QStringLiteral(":USDC");
        } else if (!vs_reg().usd_native.contains(exu)) {
            quote = QStringLiteral("USDT");
        }
    }
    if (style == CryptoPairStyle::Unified)
        return base + QLatin1Char('/') + quote + settle;
    if (exu == QLatin1String("BINANCE") || exu == QLatin1String("BINANCEUS") || exu == QLatin1String("BYBIT") ||
        exu == QLatin1String("MEXC") || exu == QLatin1String("BITGET"))
        return base + quote;
    if (exu == QLatin1String("OKX") || exu == QLatin1String("KUCOIN") || exu.startsWith(QLatin1String("COINBASE")))
        return base + QLatin1Char('-') + quote;
    if (exu == QLatin1String("KRAKEN"))
        return (base == QLatin1String("BTC") ? QStringLiteral("XBT") : base) + QLatin1Char('/') + quote;
    if (exu == QLatin1String("BITSTAMP") || exu == QLatin1String("GEMINI") || exu == QLatin1String("HTX") ||
        exu == QLatin1String("HUOBI"))
        return (base + quote).toLower();
    if (exu == QLatin1String("BITFINEX"))
        return QLatin1Char('t') + base + quote;
    if (exu == QLatin1String("GATEIO") || exu == QLatin1String("GATE"))
        return base + QLatin1Char('_') + quote;
    return base + QLatin1Char('/') + quote + settle;
}

QString strip_series_suffix(const QString& code) {
    QString s = code.trimmed().toUpper();
    for (const QString& sfx : vs_reg().series) {
        if (s.size() > sfx.size() && s.endsWith(sfx)) {
            s.chop(sfx.size());
            break;
        }
    }
    return s;
}

bool is_exchange_contract_name(const QString& code) {
    const QString s = code.trimmed().toUpper();
    if (s.size() < 8 || s.contains(QLatin1Char(' ')) || !vs_has_digit(s) || !vs_is_upper_alpha(s.at(0)))
        return false;
    if (s.endsWith(QLatin1String("FUT")))
        return true;
    if ((s.endsWith(QLatin1String("CE")) || s.endsWith(QLatin1String("PE"))) && vs_is_digit(s.at(s.size() - 3)))
        return true;
    return false;
}

QString broker_index_symbol(const QString& raw_name) {
    const QString up = vs_clean_code(raw_name);
    const VsRegistry& r = vs_reg();
    const auto it = r.aliases.constFind(vs_alias_key_ascii(up));
    if (it != r.aliases.constEnd()) {
        for (const VsAliasRef& ref : it.value())
            if (ref.table == 0 && !r.indices.at(ref.row).broker_symbol.isEmpty())
                return r.indices.at(ref.row).broker_symbol;
    }
    QString s = up;
    s.replace(QStringLiteral("S&P "), QString());
    s.remove(QLatin1Char(' '));
    s.remove(QLatin1Char('-'));
    return s;
}

// ── Reference tables ────────────────────────────────────────────────────────

const QVector<IndexInfo>& all_indices() {
    return vs_reg().indices;
}

const IndexInfo* index_for_symbol(const QString& yahoo_symbol) {
    const VsRegistry& r = vs_reg();
    const int i = r.index_by_symbol.value(yahoo_symbol.trimmed().toUpper(), -1);
    return i >= 0 ? &r.indices.at(i) : nullptr;
}

const QVector<FutureInfo>& all_futures() {
    return vs_reg().futures;
}

const FutureInfo* future_for_root(const QString& root) {
    const VsRegistry& r = vs_reg();
    const int i = r.future_by_root.value(root.trimmed().toUpper(), -1);
    return i >= 0 ? &r.futures.at(i) : nullptr;
}

const QVector<CryptoInfo>& all_cryptos() {
    return vs_reg().cryptos;
}

const CryptoInfo* crypto_for_base(const QString& base) {
    const VsRegistry& r = vs_reg();
    const int i = r.crypto_by_code.value(vs_alias_key_ascii(base.trimmed().toUpper()), -1);
    return i >= 0 ? &r.cryptos.at(i) : nullptr;
}

bool is_iso_currency(const QString& code) {
    return vs_reg().iso.contains(code);
}

const QStringList& crypto_quote_currencies() {
    return vs_reg().crypto_quotes;
}

bool is_stablecoin(const QString& code) {
    return vs_reg().stablecoins.contains(code);
}

QString alias_key(const QString& text) {
    return vs_alias_key_ascii(text.normalized(QString::NormalizationForm_KC));
}

QVector<AliasTarget> lookup_alias(const QString& text) {
    QVector<AliasTarget> out;
    const VsRegistry& r = vs_reg();
    const auto it = r.aliases.constFind(alias_key(text));
    if (it == r.aliases.constEnd())
        return out;
    // Table order: indices, then futures, then crypto (rows in table order).
    QVector<VsAliasRef> refs = it.value();
    std::stable_sort(refs.begin(), refs.end(), [](const VsAliasRef& a, const VsAliasRef& b) {
        return a.table != b.table ? a.table < b.table : a.row < b.row;
    });
    for (const VsAliasRef& ref : refs)
        if (ref.level != VsAliasLevel::CodeOnly)
            out.push_back(ref.target);
    return out;
}

} // namespace fincept::core::instrument
