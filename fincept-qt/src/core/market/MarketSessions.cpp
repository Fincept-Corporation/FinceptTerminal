// MarketSessions.cpp — venue table, holiday calendars and session arithmetic.
// See MarketSessions.h. Qt Core only.

#include "core/market/MarketSessions.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QReadWriteLock>
#include <QSet>
#include <QTime>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>

namespace fincept::core::market {

namespace {

// ── Venue table ─────────────────────────────────────────────────────────────

constexpr int ms_hm(int h, int m) {
    return h * 60 + m;
}

constexpr quint8 kMsSatSun = 0x60; // bits for dayOfWeek 6 and 7
constexpr quint8 kMsFriSat = 0x30; // bits for dayOfWeek 5 and 6
constexpr quint8 kMsEveryDay = 0x00;
constexpr const char* kMsTrContext = "MarketSessions";

struct MsHours {
    int pre;
    int open;
    int lunch_start;
    int lunch_end;
    int close;
    int post;
};

struct MsAltDay {
    int weekday = 0;
    int close = -1;
    int lunch_start = -1;
    int lunch_end = -1;
};

struct MsDstShift {
    int open = 0;
    int close = 0;
};

struct MsVenueSpec {
    const char* id;
    const char* mic;
    const char* name;
    const char* country;
    const char* tz;
    const char* currency;
    const char* calendar;
    const char* suffixes; // space separated, no dots
    const char* aliases;  // space separated
    VenueKind kind;
    MsHours hours;
    quint8 weekend;
    int days_per_year;
    MsAltDay alt;
    MsDstShift us_dst;
};

constexpr MsHours ms_hours(int pre, int open, int close, int post) {
    return MsHours{pre, open, -1, -1, close, post};
}
constexpr MsHours ms_hours_lunch(int pre, int open, int lunch_start, int lunch_end, int close, int post) {
    return MsHours{pre, open, lunch_start, lunch_end, close, post};
}

constexpr VenueKind kMsEq = VenueKind::Equity;

// Session times are the exchange's own published local times (continuous
// trading plus the closing auction as "Open"; opening auctions / extended
// hours as Pre / Post). Sources: exchange rule books and notices, checked
// against 2026 schedules (B3 and BMV follow US daylight saving; TASE moved to
// Monday-Friday on 2026-01-05 with a short Friday; UAE trades Mon-Fri since 2022).
const MsVenueSpec kMsVenues[] = {
    // ── Americas ──
    {"NYSE", "XNYS", QT_TRANSLATE_NOOP("MarketSessions", "New York Stock Exchange"), "US", "America/New_York", "USD",
     "US", "", "NYSE XNYS NYQ NYSEAMERICAN AMEX XASE ASE ARCA NYSEARCA ARCX PCX BATS BZX CBOE XCBO US USA OTC PNK",
     kMsEq, ms_hours(ms_hm(4, 0), ms_hm(9, 30), ms_hm(16, 0), ms_hm(20, 0)), kMsSatSun, 252, {}, {}},
    {"NASDAQ", "XNAS", QT_TRANSLATE_NOOP("MarketSessions", "Nasdaq"), "US", "America/New_York", "USD", "US", "",
     "NASDAQ XNAS NMS NGM NCM NAS NASDAQGS NASDAQGM NASDAQCM NSDQ", kMsEq,
     ms_hours(ms_hm(4, 0), ms_hm(9, 30), ms_hm(16, 0), ms_hm(20, 0)), kMsSatSun, 252, {}, {}},
    // Globex: Sun-Fri 17:00-16:00 CT; the session belongs to the day it closes.
    {"CME", "XCME", QT_TRANSLATE_NOOP("MarketSessions", "CME Globex"), "US", "America/Chicago", "USD", "CME", "",
     "CME XCME GLOBEX CBOT XCBT CBT NYMEX XNYM NYM COMEX XCEC CMX", VenueKind::Futures,
     ms_hours(ms_hm(17, 0) - 1440, ms_hm(17, 0) - 1440, ms_hm(16, 0), ms_hm(16, 0)), kMsSatSun, 252, {}, {}},
    {"TSX", "XTSE", QT_TRANSLATE_NOOP("MarketSessions", "Toronto Stock Exchange"), "CA", "America/Toronto", "CAD",
     "TSX", "TO", "TSX XTSE TOR CA", kMsEq, ms_hours(ms_hm(7, 0), ms_hm(9, 30), ms_hm(16, 0), ms_hm(17, 0)), kMsSatSun,
     250, {}, {}},
    {"TSXV", "XTSX", QT_TRANSLATE_NOOP("MarketSessions", "TSX Venture Exchange"), "CA", "America/Toronto", "CAD",
     "TSX", "V", "TSXV XTSX CVE VAN", kMsEq, ms_hours(ms_hm(7, 0), ms_hm(9, 30), ms_hm(16, 0), ms_hm(16, 0)),
     kMsSatSun, 250, {}, {}},
    {"CSE", "XCNQ", QT_TRANSLATE_NOOP("MarketSessions", "Canadian Securities Exchange"), "CA", "America/Toronto",
     "CAD", "TSX", "CN", "CSE XCNQ CNQ", kMsEq, ms_hours(ms_hm(9, 30), ms_hm(9, 30), ms_hm(16, 0), ms_hm(16, 0)),
     kMsSatSun, 250, {}, {}},
    {"CBOE_CA", "NEOE", QT_TRANSLATE_NOOP("MarketSessions", "Cboe Canada"), "CA", "America/Toronto", "CAD", "TSX",
     "NE", "CBOE_CA NEOE NEO", kMsEq, ms_hours(ms_hm(9, 30), ms_hm(9, 30), ms_hm(16, 0), ms_hm(16, 0)), kMsSatSun,
     250, {}, {}},
    // B3: 10:00-18:00 BRT (closing call included); 10:00-17:00 while the US is on DST.
    {"B3", "BVMF", QT_TRANSLATE_NOOP("MarketSessions", "B3 (Brasil Bolsa Balcao)"), "BR", "America/Sao_Paulo",
     "BRL", "B3", "SA", "B3 BVMF SAO BOVESPA BMFBOVESPA BR", kMsEq,
     ms_hours(ms_hm(10, 0), ms_hm(10, 0), ms_hm(18, 0), ms_hm(18, 0)), kMsSatSun, 248, {}, {0, -60}},
    // BMV: 08:30-15:00 Mexico City; 07:30-14:00 while the US is on DST (stays aligned to New York).
    {"BMV", "XMEX", QT_TRANSLATE_NOOP("MarketSessions", "Bolsa Mexicana de Valores"), "MX", "America/Mexico_City",
     "MXN", "BMV", "MX", "BMV XMEX MEX MX", kMsEq, ms_hours(ms_hm(8, 30), ms_hm(8, 30), ms_hm(15, 0), ms_hm(15, 0)),
     kMsSatSun, 250, {}, {-60, -60}},

    // ── Europe ──
    {"LSE", "XLON", QT_TRANSLATE_NOOP("MarketSessions", "London Stock Exchange"), "GB", "Europe/London", "GBp", "UK",
     "L IL", "LSE XLON LON IOB UK GB", kMsEq, ms_hours(ms_hm(7, 50), ms_hm(8, 0), ms_hm(16, 30), ms_hm(16, 40)), kMsSatSun,
     252, {}, {}},
    {"XETRA", "XETR", QT_TRANSLATE_NOOP("MarketSessions", "Xetra"), "DE", "Europe/Berlin", "EUR", "DE", "DE",
     "XETRA XETR GER ETR DE", kMsEq, ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 35)), kMsSatSun, 252,
     {}, {}},
    {"FRA", "XFRA", QT_TRANSLATE_NOOP("MarketSessions", "Frankfurt Stock Exchange"), "DE", "Europe/Berlin", "EUR",
     "DE", "F", "FRA XFRA FWB FRANKFURT", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(22, 0), ms_hm(22, 0)),
     kMsSatSun, 252, {}, {}},
    {"BER", "XBER", QT_TRANSLATE_NOOP("MarketSessions", "Berlin Stock Exchange"), "DE", "Europe/Berlin", "EUR", "DE",
     "BE", "BER XBER", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(22, 0), ms_hm(22, 0)), kMsSatSun, 252, {},
     {}},
    {"DUS", "XDUS", QT_TRANSLATE_NOOP("MarketSessions", "Dusseldorf Stock Exchange"), "DE", "Europe/Berlin", "EUR",
     "DE", "DU", "DUS XDUS", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(22, 0), ms_hm(22, 0)), kMsSatSun, 252,
     {}, {}},
    {"HAM", "XHAM", QT_TRANSLATE_NOOP("MarketSessions", "Hamburg Stock Exchange"), "DE", "Europe/Berlin", "EUR", "DE",
     "HM", "HAM XHAM", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(22, 0), ms_hm(22, 0)), kMsSatSun, 252, {},
     {}},
    {"HAN", "XHAN", QT_TRANSLATE_NOOP("MarketSessions", "Hannover Stock Exchange"), "DE", "Europe/Berlin", "EUR",
     "DE", "HA", "HAN XHAN", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(22, 0), ms_hm(22, 0)), kMsSatSun, 252,
     {}, {}},
    {"MUN", "XMUN", QT_TRANSLATE_NOOP("MarketSessions", "Munich Stock Exchange"), "DE", "Europe/Berlin", "EUR", "DE",
     "MU", "MUN XMUN", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(22, 0), ms_hm(22, 0)), kMsSatSun, 252, {},
     {}},
    {"STU", "XSTU", QT_TRANSLATE_NOOP("MarketSessions", "Stuttgart Stock Exchange"), "DE", "Europe/Berlin", "EUR",
     "DE", "SG", "STU XSTU", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(22, 0), ms_hm(22, 0)), kMsSatSun, 252,
     {}, {}},
    {"VIE", "XWBO", QT_TRANSLATE_NOOP("MarketSessions", "Vienna Stock Exchange"), "AT", "Europe/Vienna", "EUR", "VIE",
     "VI", "VIE XWBO WBAG", kMsEq, ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 35)), kMsSatSun, 250,
     {}, {}},
    {"PARIS", "XPAR", QT_TRANSLATE_NOOP("MarketSessions", "Euronext Paris"), "FR", "Europe/Paris", "EUR", "EURONEXT",
     "PA", "PARIS XPAR PAR EPA ENXTPA EURONEXT FR", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 40)), kMsSatSun, 255, {}, {}},
    {"AMSTERDAM", "XAMS", QT_TRANSLATE_NOOP("MarketSessions", "Euronext Amsterdam"), "NL", "Europe/Amsterdam", "EUR",
     "EURONEXT", "AS", "AMSTERDAM XAMS AMS AEX ENXTAM NL", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 40)), kMsSatSun, 255, {}, {}},
    {"BRUSSELS", "XBRU", QT_TRANSLATE_NOOP("MarketSessions", "Euronext Brussels"), "BE", "Europe/Brussels", "EUR",
     "EURONEXT", "BR", "BRUSSELS XBRU BRU EBR ENXTBR", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 40)), kMsSatSun, 255, {}, {}},
    {"LISBON", "XLIS", QT_TRANSLATE_NOOP("MarketSessions", "Euronext Lisbon"), "PT", "Europe/Lisbon", "EUR",
     "EURONEXT_PT", "LS", "LISBON XLIS LIS ELI ENXTLS", kMsEq,
     ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(16, 30), ms_hm(16, 40)), kMsSatSun, 255, {}, {}},
    {"DUBLIN", "XDUB", QT_TRANSLATE_NOOP("MarketSessions", "Euronext Dublin"), "IE", "Europe/Dublin", "EUR", "DUBLIN",
     "IR", "DUBLIN XDUB DUB ISE", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(8, 0), ms_hm(16, 30), ms_hm(16, 35)), kMsSatSun,
     252, {}, {}},
    {"OSLO", "XOSL", QT_TRANSLATE_NOOP("MarketSessions", "Euronext Oslo Bors"), "NO", "Europe/Oslo", "NOK", "OSLO",
     "OL", "OSLO XOSL OSL OSE", kMsEq, ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(16, 20), ms_hm(16, 25)), kMsSatSun,
     249, {}, {}},
    {"MILAN", "XMIL", QT_TRANSLATE_NOOP("MarketSessions", "Euronext Milan (Borsa Italiana)"), "IT", "Europe/Rome",
     "EUR", "MILAN", "MI", "MILAN XMIL MIL BIT MTA BORSAITALIANA IT", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 35)), kMsSatSun, 252, {}, {}},
    {"SIX", "XSWX", QT_TRANSLATE_NOOP("MarketSessions", "SIX Swiss Exchange"), "CH", "Europe/Zurich", "CHF", "SIX",
     "SW VX", "SIX XSWX XVTX VTX EBS SWX CH", kMsEq, ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 30)),
     kMsSatSun, 250, {}, {}},
    {"BME", "XMAD", QT_TRANSLATE_NOOP("MarketSessions", "BME Spanish Exchanges"), "ES", "Europe/Madrid", "EUR", "BME",
     "MC", "BME XMAD MCE MAD SIBE ES", kMsEq, ms_hours(ms_hm(8, 30), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 35)),
     kMsSatSun, 254, {}, {}},
    {"STOCKHOLM", "XSTO", QT_TRANSLATE_NOOP("MarketSessions", "Nasdaq Stockholm"), "SE", "Europe/Stockholm", "SEK",
     "STOCKHOLM", "ST", "STOCKHOLM XSTO STO OMX OMXS SE", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 30), ms_hm(17, 30)), kMsSatSun, 250, {}, {}},
    {"HELSINKI", "XHEL", QT_TRANSLATE_NOOP("MarketSessions", "Nasdaq Helsinki"), "FI", "Europe/Helsinki", "EUR",
     "HELSINKI", "HE", "HELSINKI XHEL HEL OMXH", kMsEq,
     ms_hours(ms_hm(10, 0), ms_hm(10, 0), ms_hm(18, 30), ms_hm(18, 30)), kMsSatSun, 250, {}, {}},
    {"COPENHAGEN", "XCSE", QT_TRANSLATE_NOOP("MarketSessions", "Nasdaq Copenhagen"), "DK", "Europe/Copenhagen",
     "DKK", "COPENHAGEN", "CO", "COPENHAGEN XCSE CPH OMXC", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 0), ms_hm(17, 0), ms_hm(17, 0)), kMsSatSun, 249, {}, {}},
    {"ICELAND", "XICE", QT_TRANSLATE_NOOP("MarketSessions", "Nasdaq Iceland"), "IS", "Atlantic/Reykjavik", "ISK",
     "ICELAND", "IC", "ICELAND XICE OMXI", kMsEq, ms_hours(ms_hm(9, 30), ms_hm(9, 30), ms_hm(15, 30), ms_hm(15, 30)),
     kMsSatSun, 248, {}, {}},
    {"WARSAW", "XWAR", QT_TRANSLATE_NOOP("MarketSessions", "Warsaw Stock Exchange"), "PL", "Europe/Warsaw", "PLN",
     "WARSAW", "WA", "WARSAW XWAR WSE GPW", kMsEq, ms_hours(ms_hm(8, 30), ms_hm(9, 0), ms_hm(17, 0), ms_hm(17, 5)),
     kMsSatSun, 251, {}, {}},
    {"ISTANBUL", "XIST", QT_TRANSLATE_NOOP("MarketSessions", "Borsa Istanbul"), "TR", "Europe/Istanbul", "TRY",
     "ISTANBUL", "IS", "ISTANBUL XIST BIST IST", kMsEq,
     ms_hours(ms_hm(9, 40), ms_hm(10, 0), ms_hm(18, 0), ms_hm(18, 10)), kMsSatSun, 250, {}, {}},

    // ── Middle East & Africa ──
    {"JSE", "XJSE", QT_TRANSLATE_NOOP("MarketSessions", "Johannesburg Stock Exchange"), "ZA", "Africa/Johannesburg",
     "ZAc", "JSE", "JO", "JSE XJSE JNB ZA", kMsEq, ms_hours(ms_hm(8, 30), ms_hm(9, 0), ms_hm(17, 0), ms_hm(17, 0)),
     kMsSatSun, 250, {}, {}},
    {"TADAWUL", "XSAU", QT_TRANSLATE_NOOP("MarketSessions", "Saudi Exchange (Tadawul)"), "SA", "Asia/Riyadh", "SAR",
     "TADAWUL", "SR", "TADAWUL XSAU SAU TASI", kMsEq,
     ms_hours(ms_hm(9, 30), ms_hm(10, 0), ms_hm(15, 0), ms_hm(15, 20)), kMsFriSat, 247, {}, {}},
    {"DFM", "XDFM", QT_TRANSLATE_NOOP("MarketSessions", "Dubai Financial Market"), "AE", "Asia/Dubai", "AED", "DFM",
     "AE", "DFM XDFM", kMsEq, ms_hours(ms_hm(9, 30), ms_hm(10, 0), ms_hm(15, 0), ms_hm(15, 0)), kMsSatSun, 250, {},
     {}},
    {"ADX", "XADS", QT_TRANSLATE_NOOP("MarketSessions", "Abu Dhabi Securities Exchange"), "AE", "Asia/Dubai", "AED",
     "ADX", "", "ADX XADS", kMsEq, ms_hours(ms_hm(9, 30), ms_hm(10, 0), ms_hm(15, 0), ms_hm(15, 0)), kMsSatSun, 250,
     {}, {}},
    // TASE: Monday-Friday since 2026-01-05; Friday closes at 13:50.
    {"TASE", "XTAE", QT_TRANSLATE_NOOP("MarketSessions", "Tel Aviv Stock Exchange"), "IL", "Asia/Jerusalem", "ILA",
     "TASE", "TA", "TASE XTAE TLV IL", kMsEq, ms_hours(ms_hm(10, 0), ms_hm(10, 0), ms_hm(17, 25), ms_hm(17, 25)),
     kMsSatSun, 248, {5, ms_hm(13, 50), -1, -1}, {}},

    // ── Asia-Pacific ──
    {"NSE", "XNSE", QT_TRANSLATE_NOOP("MarketSessions", "National Stock Exchange of India"), "IN", "Asia/Kolkata",
     "INR", "IN", "NS", "NSE XNSE NSI NSE_EQ NSE_INDEX NFO NSE_FO IN", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 15), ms_hm(15, 30), ms_hm(16, 0)), kMsSatSun, 248, {}, {}},
    {"BSE", "XBOM", QT_TRANSLATE_NOOP("MarketSessions", "BSE (Bombay Stock Exchange)"), "IN", "Asia/Kolkata", "INR",
     "IN", "BO", "BSE XBOM BOM BSE_EQ BSE_INDEX BFO BSE_FO", kMsEq,
     ms_hours(ms_hm(9, 0), ms_hm(9, 15), ms_hm(15, 30), ms_hm(16, 0)), kMsSatSun, 248, {}, {}},
    {"SGX", "XSES", QT_TRANSLATE_NOOP("MarketSessions", "Singapore Exchange"), "SG", "Asia/Singapore", "SGD", "SGX",
     "SI", "SGX XSES SES SG", kMsEq,
     ms_hours_lunch(ms_hm(8, 30), ms_hm(9, 0), ms_hm(12, 0), ms_hm(13, 0), ms_hm(17, 0), ms_hm(17, 16)), kMsSatSun,
     252, {}, {}},
    {"HKEX", "XHKG", QT_TRANSLATE_NOOP("MarketSessions", "Hong Kong Exchanges and Clearing"), "HK", "Asia/Hong_Kong",
     "HKD", "HK", "HK", "HKEX XHKG HKG SEHK HK", kMsEq,
     ms_hours_lunch(ms_hm(9, 0), ms_hm(9, 30), ms_hm(12, 0), ms_hm(13, 0), ms_hm(16, 0), ms_hm(16, 10)), kMsSatSun,
     247, {}, {}},
    {"SSE", "XSHG", QT_TRANSLATE_NOOP("MarketSessions", "Shanghai Stock Exchange"), "CN", "Asia/Shanghai", "CNY", "CN",
     "SS", "SSE XSHG SHH SHA CN", kMsEq,
     ms_hours_lunch(ms_hm(9, 15), ms_hm(9, 30), ms_hm(11, 30), ms_hm(13, 0), ms_hm(15, 0), ms_hm(15, 0)), kMsSatSun,
     242, {}, {}},
    {"SZSE", "XSHE", QT_TRANSLATE_NOOP("MarketSessions", "Shenzhen Stock Exchange"), "CN", "Asia/Shanghai", "CNY", "CN",
     "SZ", "SZSE XSHE SHZ SHE", kMsEq,
     ms_hours_lunch(ms_hm(9, 15), ms_hm(9, 30), ms_hm(11, 30), ms_hm(13, 0), ms_hm(15, 0), ms_hm(15, 0)), kMsSatSun,
     242, {}, {}},
    // TSE: afternoon session runs to 15:30 since 2024-11-05.
    {"TSE", "XTKS", QT_TRANSLATE_NOOP("MarketSessions", "Tokyo Stock Exchange"), "JP", "Asia/Tokyo", "JPY", "JP", "T",
     "TSE XTKS XJPX JPX TYO JP", kMsEq,
     ms_hours_lunch(ms_hm(9, 0), ms_hm(9, 0), ms_hm(11, 30), ms_hm(12, 30), ms_hm(15, 30), ms_hm(15, 30)),
     kMsSatSun, 245, {}, {}},
    {"KRX", "XKRX", QT_TRANSLATE_NOOP("MarketSessions", "Korea Exchange"), "KR", "Asia/Seoul", "KRW", "KRX", "KS KQ",
     "KRX XKRX KSC KOE KOSPI KOSDAQ KR", kMsEq, ms_hours(ms_hm(8, 30), ms_hm(9, 0), ms_hm(15, 30), ms_hm(18, 0)),
     kMsSatSun, 246, {}, {}},
    {"TWSE", "XTAI", QT_TRANSLATE_NOOP("MarketSessions", "Taiwan Stock Exchange"), "TW", "Asia/Taipei", "TWD", "TW",
     "TW", "TWSE XTAI TAI TW", kMsEq, ms_hours(ms_hm(8, 30), ms_hm(9, 0), ms_hm(13, 30), ms_hm(14, 30)), kMsSatSun, 245,
     {}, {}},
    {"TPEX", "ROCO", QT_TRANSLATE_NOOP("MarketSessions", "Taipei Exchange"), "TW", "Asia/Taipei", "TWD", "TW", "TWO",
     "TPEX ROCO TWO", kMsEq, ms_hours(ms_hm(8, 30), ms_hm(9, 0), ms_hm(13, 30), ms_hm(14, 30)), kMsSatSun, 245, {},
     {}},
    {"ASX", "XASX", QT_TRANSLATE_NOOP("MarketSessions", "Australian Securities Exchange"), "AU", "Australia/Sydney",
     "AUD", "ASX", "AX", "ASX XASX AU", kMsEq, ms_hours(ms_hm(7, 0), ms_hm(10, 0), ms_hm(16, 0), ms_hm(16, 12)),
     kMsSatSun, 253, {}, {}},
    {"NZX", "XNZE", QT_TRANSLATE_NOOP("MarketSessions", "NZX"), "NZ", "Pacific/Auckland", "NZD", "NZX", "NZ",
     "NZX XNZE NZE NZ", kMsEq, ms_hours(ms_hm(8, 0), ms_hm(10, 0), ms_hm(16, 45), ms_hm(17, 0)), kMsSatSun, 251, {}, {}},
    // IDX: Friday's midday break is 11:30-14:00 instead of 12:00-13:30.
    {"IDX", "XIDX", QT_TRANSLATE_NOOP("MarketSessions", "Indonesia Stock Exchange"), "ID", "Asia/Jakarta", "IDR",
     "IDX", "JK", "IDX XIDX JKT", kMsEq,
     ms_hours_lunch(ms_hm(8, 45), ms_hm(9, 0), ms_hm(12, 0), ms_hm(13, 30), ms_hm(16, 0), ms_hm(16, 15)), kMsSatSun,
     242, {5, -1, ms_hm(11, 30), ms_hm(14, 0)}, {}},
    {"SET", "XBKK", QT_TRANSLATE_NOOP("MarketSessions", "Stock Exchange of Thailand"), "TH", "Asia/Bangkok", "THB",
     "SET", "BK", "SET XBKK BKK", kMsEq,
     ms_hours_lunch(ms_hm(9, 30), ms_hm(10, 0), ms_hm(12, 30), ms_hm(14, 30), ms_hm(16, 30), ms_hm(16, 40)),
     kMsSatSun, 243, {}, {}},
    {"MYX", "XKLS", QT_TRANSLATE_NOOP("MarketSessions", "Bursa Malaysia"), "MY", "Asia/Kuala_Lumpur", "MYR", "MYX",
     "KL", "MYX XKLS KLS BURSA KLSE", kMsEq,
     ms_hours_lunch(ms_hm(8, 30), ms_hm(9, 0), ms_hm(12, 30), ms_hm(14, 30), ms_hm(17, 0), ms_hm(17, 0)), kMsSatSun,
     246, {}, {}},

    // ── Round-the-clock pseudo-venues ──
    // FX: Sunday 17:00 to Friday 17:00 New York; the value date rolls at 17:00 NY.
    {"FX", "", QT_TRANSLATE_NOOP("MarketSessions", "Foreign Exchange (24x5)"), "", "America/New_York", "", "FX", "",
     "FX FOREX CCY CURRENCY", VenueKind::Fx,
     ms_hours(ms_hm(17, 0) - 1440, ms_hm(17, 0) - 1440, ms_hm(17, 0), ms_hm(17, 0)), kMsSatSun, 260, {}, {}},
    {"CRYPTO", "", QT_TRANSLATE_NOOP("MarketSessions", "Crypto (24x7)"), "", "UTC", "", "CRYPTO", "", "CRYPTO CCC",
     VenueKind::Crypto, ms_hours(0, 0, 1440, 1440), kMsEveryDay, 365, {}, {}},
};

// Yahoo index tickers (without '^') -> venue id. Suffixed indices (^SET.BK,
// ^TASI.SR, 000001.SS) resolve through their suffix instead.
struct MsIndexSpec {
    const char* symbol;
    const char* venue;
};

const MsIndexSpec kMsIndices[] = {
    {"GSPC", "NYSE"},     {"SPX", "NYSE"},       {"DJI", "NYSE"},      {"DJT", "NYSE"},      {"DJU", "NYSE"},
    {"NYA", "NYSE"},      {"XAX", "NYSE"},       {"RUT", "NYSE"},      {"RUA", "NYSE"},      {"RUI", "NYSE"},
    {"VIX", "NYSE"},      {"VVIX", "NYSE"},      {"OEX", "NYSE"},      {"MID", "NYSE"},      {"SP400", "NYSE"},
    {"SP600", "NYSE"},    {"W5000", "NYSE"},     {"XOI", "NYSE"},      {"HGX", "NYSE"},      {"BKX", "NYSE"},
    {"XAU", "NYSE"},      {"TNX", "NYSE"},       {"IRX", "NYSE"},      {"FVX", "NYSE"},      {"TYX", "NYSE"},
    {"SPXEW", "NYSE"},    {"IXIC", "NASDAQ"},    {"NDX", "NASDAQ"},    {"VXN", "NASDAQ"},    {"SOX", "NASDAQ"},
    {"NBI", "NASDAQ"},    {"GSPTSE", "TSX"},     {"BVSP", "B3"},       {"MXX", "BMV"},       {"FTSE", "LSE"},
    {"FTMC", "LSE"},      {"FTAS", "LSE"},       {"FTAI", "LSE"},      {"GDAXI", "XETRA"},   {"MDAXI", "XETRA"},
    {"TECDAX", "XETRA"},  {"SDAXI", "XETRA"},    {"STOXX50E", "XETRA"}, {"STOXX", "XETRA"},  {"SX5E", "XETRA"},
    {"FCHI", "PARIS"},    {"SBF120", "PARIS"},   {"AEX", "AMSTERDAM"}, {"AMX", "AMSTERDAM"}, {"BFX", "BRUSSELS"},
    {"PSI20", "LISBON"},  {"ISEQ", "DUBLIN"},    {"OSEAX", "OSLO"},    {"OBX", "OSLO"},      {"FTSEMIB", "MILAN"},
    {"SSMI", "SIX"},      {"SSHI", "SIX"},       {"SLI", "SIX"},       {"IBEX", "BME"},      {"OMX", "STOCKHOLM"},
    {"OMXSPI", "STOCKHOLM"}, {"OMXH25", "HELSINKI"}, {"OMXHPI", "HELSINKI"}, {"OMXC25", "COPENHAGEN"},
    {"OMXC20", "COPENHAGEN"}, {"ATX", "VIE"},    {"WIG20", "WARSAW"},  {"XU100", "ISTANBUL"}, {"TA125", "TASE"},
    {"TASI", "TADAWUL"},  {"NSEI", "NSE"},       {"NSEBANK", "NSE"},   {"CNXIT", "NSE"},     {"INDIAVIX", "NSE"},
    {"NSMIDCP", "NSE"},   {"CNX100", "NSE"},     {"CNX200", "NSE"},    {"CNXAUTO", "NSE"},   {"CNXFMCG", "NSE"},
    {"CNXPHARMA", "NSE"}, {"CNXMETAL", "NSE"},   {"CNXREALTY", "NSE"}, {"CNXENERGY", "NSE"}, {"CNXINFRA", "NSE"},
    {"CNXPSUBANK", "NSE"}, {"CRSLDX", "NSE"},    {"NSEMDCP50", "NSE"}, {"BSESN", "BSE"},     {"BSE500", "BSE"},
    {"STI", "SGX"},       {"HSI", "HKEX"},       {"HSCE", "HKEX"},     {"HSCC", "HKEX"},     {"SSEC", "SSE"},
    {"N225", "TSE"},      {"TOPX", "TSE"},       {"KS11", "KRX"},      {"KQ11", "KRX"},      {"KS200", "KRX"},
    {"TWII", "TWSE"},     {"AXJO", "ASX"},       {"AORD", "ASX"},      {"AXKO", "ASX"},      {"NZ50", "NZX"},
    {"JKSE", "IDX"},      {"KLSE", "MYX"},       {"SET", "SET"},
};

struct MsRegistry {
    std::vector<Venue> venues;
    QHash<QString, int> by_code;   // id / MIC / alias -> index
    QHash<QString, int> by_suffix; // Yahoo suffix -> index
    QHash<QString, int> by_index;  // index ticker without '^' -> index
};

QStringList ms_split_words(const char* s) {
    return QString::fromLatin1(s).split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

MsRegistry ms_build_registry() {
    MsRegistry reg;
    reg.venues.reserve(std::size(kMsVenues));
    for (const MsVenueSpec& spec : kMsVenues) {
        Venue v;
        v.id = QString::fromLatin1(spec.id);
        v.mic = QString::fromLatin1(spec.mic);
        v.name = QString::fromLatin1(spec.name);
        v.country = QString::fromLatin1(spec.country);
        v.tz_id = QString::fromLatin1(spec.tz);
        v.currency = QString::fromLatin1(spec.currency);
        v.calendar = QString::fromLatin1(spec.calendar);
        if (v.calendar.isEmpty())
            v.calendar = v.id;
        v.suffixes = ms_split_words(spec.suffixes);
        v.aliases = ms_split_words(spec.aliases);
        v.kind = spec.kind;
        v.pre_open_min = spec.hours.pre;
        v.open_min = spec.hours.open;
        v.lunch_start_min = spec.hours.lunch_start;
        v.lunch_end_min = spec.hours.lunch_end;
        v.close_min = spec.hours.close;
        v.post_close_min = spec.hours.post;
        v.weekend_mask = spec.weekend;
        v.trading_days_per_year = spec.days_per_year;
        v.alt_weekday = spec.alt.weekday;
        v.alt_close_min = spec.alt.close;
        v.alt_lunch_start_min = spec.alt.lunch_start;
        v.alt_lunch_end_min = spec.alt.lunch_end;
        v.us_dst_open_shift_min = spec.us_dst.open;
        v.us_dst_close_shift_min = spec.us_dst.close;
        v.tz = v.tz_id == QLatin1String("UTC") ? QTimeZone::utc() : QTimeZone(v.tz_id.toLatin1());
        reg.venues.push_back(std::move(v));
    }
    for (int i = 0; i < static_cast<int>(reg.venues.size()); ++i) {
        const Venue& v = reg.venues[static_cast<size_t>(i)];
        // First registration wins, so a venue's own id / MIC is never shadowed by
        // a later venue's alias.
        auto add_code = [&reg, i](const QString& code) {
            if (!code.isEmpty() && !reg.by_code.contains(code))
                reg.by_code.insert(code, i);
        };
        add_code(v.id);
        add_code(v.mic);
        for (const QString& s : v.suffixes)
            if (!reg.by_suffix.contains(s))
                reg.by_suffix.insert(s, i);
    }
    for (int i = 0; i < static_cast<int>(reg.venues.size()); ++i)
        for (const QString& a : reg.venues[static_cast<size_t>(i)].aliases)
            if (!reg.by_code.contains(a))
                reg.by_code.insert(a, i);
    for (const MsIndexSpec& idx : kMsIndices) {
        const int vi = reg.by_code.value(QString::fromLatin1(idx.venue), -1);
        if (vi >= 0)
            reg.by_index.insert(QString::fromLatin1(idx.symbol), vi);
    }
    return reg;
}

const MsRegistry& ms_registry() {
    static const MsRegistry reg = ms_build_registry();
    return reg;
}

const Venue* ms_venue_at(int index) {
    const MsRegistry& reg = ms_registry();
    if (index < 0 || index >= static_cast<int>(reg.venues.size()))
        return nullptr;
    return &reg.venues[static_cast<size_t>(index)];
}

// ── Holiday calendars ───────────────────────────────────────────────────────

struct MsDayEntry {
    bool closed = true; // full-day closure when no times are given
    int open_min = -1;  // -1 = regular open
    int close_min = -1; // -1 = regular close
    QString name;
    bool translatable = false; // built-in English source text
};

struct MsCalendar {
    QHash<QDate, MsDayEntry> days;
    QSet<int> full_years;
    QSet<int> partial_years;
};

using MsCalendarMap = QHash<QString, MsCalendar>;

void ms_add_closed(MsCalendar& c, QDate d, const char* name) {
    MsDayEntry e;
    e.name = QString::fromLatin1(name);
    e.translatable = true;
    c.days.insert(d, e); // a closure always wins over an earlier early-close entry
}

void ms_add_early_close(MsCalendar& c, QDate d, int close_min, const char* name) {
    if (c.days.contains(d))
        return; // never downgrade a closure
    MsDayEntry e;
    e.closed = false;
    e.close_min = close_min;
    e.name = QString::fromLatin1(name);
    e.translatable = true;
    c.days.insert(d, e);
}

// Anonymous Gregorian computus.
QDate ms_easter_sunday(int y) {
    const int a = y % 19;
    const int b = y / 100;
    const int c = y % 100;
    const int d = b / 4;
    const int e = b % 4;
    const int f = (b + 8) / 25;
    const int g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4;
    const int k = c % 4;
    const int l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const int month = (h + l - 7 * m + 114) / 31;
    const int day = ((h + l - 7 * m + 114) % 31) + 1;
    return QDate(y, month, day);
}

// n-th `weekday` (1 = Monday) of a month, n >= 1.
QDate ms_nth_weekday(int y, int month, int weekday, int n) {
    const QDate first(y, month, 1);
    const int offset = (weekday - first.dayOfWeek() + 7) % 7;
    return first.addDays(offset + 7 * (n - 1));
}

QDate ms_last_weekday(int y, int month, int weekday) {
    const QDate first(y, month, 1);
    const QDate last(y, month, first.daysInMonth());
    const int offset = (last.dayOfWeek() - weekday + 7) % 7;
    return last.addDays(-offset);
}

// US federal observance: Saturday -> Friday, Sunday -> Monday.
QDate ms_us_observed(QDate d) {
    if (d.dayOfWeek() == 6)
        return d.addDays(-1);
    if (d.dayOfWeek() == 7)
        return d.addDays(1);
    return d;
}

// NYSE / Nasdaq (NYSE Rule 7.2). Early closes at 13:00 ET.
void ms_build_us(MsCalendar& c, int y) {
    const QDate new_year(y, 1, 1);
    if (new_year.dayOfWeek() == 7)
        ms_add_closed(c, new_year.addDays(1), QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day"));
    else if (new_year.dayOfWeek() != 6) // Saturday: not observed on Dec 31 (year-end rule)
        ms_add_closed(c, new_year, QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day"));
    if (y >= 1998)
        ms_add_closed(c, ms_nth_weekday(y, 1, 1, 3), QT_TRANSLATE_NOOP("MarketSessions", "Martin Luther King Jr. Day"));
    ms_add_closed(c, ms_nth_weekday(y, 2, 1, 3), QT_TRANSLATE_NOOP("MarketSessions", "Washington's Birthday"));
    ms_add_closed(c, ms_easter_sunday(y).addDays(-2), QT_TRANSLATE_NOOP("MarketSessions", "Good Friday"));
    ms_add_closed(c, ms_last_weekday(y, 5, 1), QT_TRANSLATE_NOOP("MarketSessions", "Memorial Day"));
    if (y >= 2022)
        ms_add_closed(c, ms_us_observed(QDate(y, 6, 19)), QT_TRANSLATE_NOOP("MarketSessions", "Juneteenth"));
    ms_add_closed(c, ms_us_observed(QDate(y, 7, 4)), QT_TRANSLATE_NOOP("MarketSessions", "Independence Day"));
    ms_add_closed(c, ms_nth_weekday(y, 9, 1, 1), QT_TRANSLATE_NOOP("MarketSessions", "Labor Day"));
    const QDate thanksgiving = ms_nth_weekday(y, 11, 4, 4);
    ms_add_closed(c, thanksgiving, QT_TRANSLATE_NOOP("MarketSessions", "Thanksgiving Day"));
    ms_add_closed(c, ms_us_observed(QDate(y, 12, 25)), QT_TRANSLATE_NOOP("MarketSessions", "Christmas Day"));

    const int early = ms_hm(13, 0);
    ms_add_early_close(c, thanksgiving.addDays(1), early, QT_TRANSLATE_NOOP("MarketSessions", "Day after Thanksgiving"));
    const QDate jul3(y, 7, 3);
    if (jul3.dayOfWeek() <= 4) // Mon-Thu (Independence Day on Tue-Fri)
        ms_add_early_close(c, jul3, early, QT_TRANSLATE_NOOP("MarketSessions", "Independence Day Eve"));
    const QDate dec24(y, 12, 24);
    if (dec24.dayOfWeek() <= 4)
        ms_add_early_close(c, dec24, early, QT_TRANSLATE_NOOP("MarketSessions", "Christmas Eve"));
}

// Unscheduled US closures (national days of mourning, 9/11, Hurricane Sandy).
void ms_build_us_special(MsCalendar& c) {
    static const int kDays[][3] = {{2001, 9, 11}, {2001, 9, 12}, {2001, 9, 13}, {2001, 9, 14}, {2004, 6, 11},
                                   {2007, 1, 2},  {2012, 10, 29}, {2012, 10, 30}, {2018, 12, 5}, {2025, 1, 9}};
    for (const auto& d : kDays)
        ms_add_closed(c, QDate(d[0], d[1], d[2]), QT_TRANSLATE_NOOP("MarketSessions", "Special closure"));
}

// London Stock Exchange: England & Wales bank holidays. Half days (12:30) on
// Christmas Eve and New Year's Eve when they are weekdays.
void ms_build_uk(MsCalendar& c, int y) {
    const QDate new_year(y, 1, 1);
    const int ny_dow = new_year.dayOfWeek();
    ms_add_closed(c, ny_dow == 6 ? new_year.addDays(2) : ny_dow == 7 ? new_year.addDays(1) : new_year,
                  QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day"));
    const QDate easter = ms_easter_sunday(y);
    ms_add_closed(c, easter.addDays(-2), QT_TRANSLATE_NOOP("MarketSessions", "Good Friday"));
    ms_add_closed(c, easter.addDays(1), QT_TRANSLATE_NOOP("MarketSessions", "Easter Monday"));

    // Early May bank holiday (moved to 8 May for VE Day in 2020).
    ms_add_closed(c, y == 2020 ? QDate(2020, 5, 8) : ms_nth_weekday(y, 5, 1, 1),
                  QT_TRANSLATE_NOOP("MarketSessions", "Early May Bank Holiday"));
    // Spring bank holiday (moved for the 2002 / 2012 / 2022 jubilees).
    if (y == 2002) {
        ms_add_closed(c, QDate(2002, 6, 3), QT_TRANSLATE_NOOP("MarketSessions", "Golden Jubilee"));
        ms_add_closed(c, QDate(2002, 6, 4), QT_TRANSLATE_NOOP("MarketSessions", "Spring Bank Holiday"));
    } else if (y == 2012) {
        ms_add_closed(c, QDate(2012, 6, 4), QT_TRANSLATE_NOOP("MarketSessions", "Spring Bank Holiday"));
        ms_add_closed(c, QDate(2012, 6, 5), QT_TRANSLATE_NOOP("MarketSessions", "Diamond Jubilee"));
    } else if (y == 2022) {
        ms_add_closed(c, QDate(2022, 6, 2), QT_TRANSLATE_NOOP("MarketSessions", "Spring Bank Holiday"));
        ms_add_closed(c, QDate(2022, 6, 3), QT_TRANSLATE_NOOP("MarketSessions", "Platinum Jubilee"));
    } else {
        ms_add_closed(c, ms_last_weekday(y, 5, 1), QT_TRANSLATE_NOOP("MarketSessions", "Spring Bank Holiday"));
    }
    ms_add_closed(c, ms_last_weekday(y, 8, 1), QT_TRANSLATE_NOOP("MarketSessions", "Summer Bank Holiday"));

    const QDate xmas(y, 12, 25);
    const char* kXmas = QT_TRANSLATE_NOOP("MarketSessions", "Christmas Day");
    const char* kBoxing = QT_TRANSLATE_NOOP("MarketSessions", "Boxing Day");
    switch (xmas.dayOfWeek()) {
        case 5: // Fri: Boxing Day (Sat) moves to Monday
            ms_add_closed(c, xmas, kXmas);
            ms_add_closed(c, QDate(y, 12, 28), kBoxing);
            break;
        case 6: // Sat: both move to Mon / Tue
            ms_add_closed(c, QDate(y, 12, 27), kXmas);
            ms_add_closed(c, QDate(y, 12, 28), kBoxing);
            break;
        case 7: // Sun: Boxing Day Monday, Christmas substitute Tuesday
            ms_add_closed(c, QDate(y, 12, 26), kBoxing);
            ms_add_closed(c, QDate(y, 12, 27), kXmas);
            break;
        default:
            ms_add_closed(c, xmas, kXmas);
            ms_add_closed(c, QDate(y, 12, 26), kBoxing);
            break;
    }

    if (y == 2011)
        ms_add_closed(c, QDate(2011, 4, 29), QT_TRANSLATE_NOOP("MarketSessions", "Royal Wedding"));
    if (y == 2022)
        ms_add_closed(c, QDate(2022, 9, 19), QT_TRANSLATE_NOOP("MarketSessions", "State Funeral"));
    if (y == 2023)
        ms_add_closed(c, QDate(2023, 5, 8), QT_TRANSLATE_NOOP("MarketSessions", "Coronation"));

    for (const QDate eve : {QDate(y, 12, 24), QDate(y, 12, 31)}) {
        if (eve.dayOfWeek() <= 5)
            ms_add_early_close(c, eve, ms_hm(12, 30),
                               eve.month() == 12 && eve.day() == 24
                                   ? QT_TRANSLATE_NOOP("MarketSessions", "Christmas Eve")
                                   : QT_TRANSLATE_NOOP("MarketSessions", "New Year's Eve"));
    }
}

// Deutsche Boerse (Xetra and the Frankfurt floor; the regional floors follow it).
void ms_build_de(MsCalendar& c, int y) {
    const QDate easter = ms_easter_sunday(y);
    ms_add_closed(c, QDate(y, 1, 1), QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day"));
    ms_add_closed(c, easter.addDays(-2), QT_TRANSLATE_NOOP("MarketSessions", "Good Friday"));
    ms_add_closed(c, easter.addDays(1), QT_TRANSLATE_NOOP("MarketSessions", "Easter Monday"));
    ms_add_closed(c, QDate(y, 5, 1), QT_TRANSLATE_NOOP("MarketSessions", "Labour Day"));
    ms_add_closed(c, QDate(y, 12, 24), QT_TRANSLATE_NOOP("MarketSessions", "Christmas Eve"));
    ms_add_closed(c, QDate(y, 12, 25), QT_TRANSLATE_NOOP("MarketSessions", "Christmas Day"));
    ms_add_closed(c, QDate(y, 12, 26), QT_TRANSLATE_NOOP("MarketSessions", "Boxing Day"));
    ms_add_closed(c, QDate(y, 12, 31), QT_TRANSLATE_NOOP("MarketSessions", "New Year's Eve"));
}

// Euronext harmonised calendar (Paris, Amsterdam, Brussels, Lisbon). Early close
// 14:05 CET on 24 and 31 December — `early_close` is in the venue's local time
// (13:05 in Lisbon).
void ms_build_euronext(MsCalendar& c, int y, int early_close) {
    const QDate easter = ms_easter_sunday(y);
    ms_add_closed(c, QDate(y, 1, 1), QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day"));
    ms_add_closed(c, easter.addDays(-2), QT_TRANSLATE_NOOP("MarketSessions", "Good Friday"));
    ms_add_closed(c, easter.addDays(1), QT_TRANSLATE_NOOP("MarketSessions", "Easter Monday"));
    ms_add_closed(c, QDate(y, 5, 1), QT_TRANSLATE_NOOP("MarketSessions", "Labour Day"));
    ms_add_closed(c, QDate(y, 12, 25), QT_TRANSLATE_NOOP("MarketSessions", "Christmas Day"));
    ms_add_closed(c, QDate(y, 12, 26), QT_TRANSLATE_NOOP("MarketSessions", "Boxing Day"));
    ms_add_early_close(c, QDate(y, 12, 24), early_close, QT_TRANSLATE_NOOP("MarketSessions", "Christmas Eve"));
    ms_add_early_close(c, QDate(y, 12, 31), early_close, QT_TRANSLATE_NOOP("MarketSessions", "New Year's Eve"));
}

// Tokyo Stock Exchange: Japanese national holidays (current law, valid from
// 2022) plus the 1-3 January and 31 December market holidays. Equinox days use
// the standard 1980-2099 approximation (matches the official 2022-2027 dates).
void ms_build_jp(MsCalendar& c, int y) {
    const int n = y - 1980;
    const int vernal = static_cast<int>(std::floor(20.8431 + 0.242194 * n - std::floor(n / 4.0)));
    const int autumnal = static_cast<int>(std::floor(23.2488 + 0.242194 * n - std::floor(n / 4.0)));

    struct JpDay {
        QDate date;
        const char* name;
    };
    const JpDay base[] = {
        {QDate(y, 1, 1), QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day")},
        {ms_nth_weekday(y, 1, 1, 2), QT_TRANSLATE_NOOP("MarketSessions", "Coming of Age Day")},
        {QDate(y, 2, 11), QT_TRANSLATE_NOOP("MarketSessions", "National Foundation Day")},
        {QDate(y, 2, 23), QT_TRANSLATE_NOOP("MarketSessions", "Emperor's Birthday")},
        {QDate(y, 3, vernal), QT_TRANSLATE_NOOP("MarketSessions", "Vernal Equinox Day")},
        {QDate(y, 4, 29), QT_TRANSLATE_NOOP("MarketSessions", "Showa Day")},
        {QDate(y, 5, 3), QT_TRANSLATE_NOOP("MarketSessions", "Constitution Memorial Day")},
        {QDate(y, 5, 4), QT_TRANSLATE_NOOP("MarketSessions", "Greenery Day")},
        {QDate(y, 5, 5), QT_TRANSLATE_NOOP("MarketSessions", "Children's Day")},
        {ms_nth_weekday(y, 7, 1, 3), QT_TRANSLATE_NOOP("MarketSessions", "Marine Day")},
        {QDate(y, 8, 11), QT_TRANSLATE_NOOP("MarketSessions", "Mountain Day")},
        {ms_nth_weekday(y, 9, 1, 3), QT_TRANSLATE_NOOP("MarketSessions", "Respect for the Aged Day")},
        {QDate(y, 9, autumnal), QT_TRANSLATE_NOOP("MarketSessions", "Autumnal Equinox Day")},
        {ms_nth_weekday(y, 10, 1, 2), QT_TRANSLATE_NOOP("MarketSessions", "Sports Day")},
        {QDate(y, 11, 3), QT_TRANSLATE_NOOP("MarketSessions", "Culture Day")},
        {QDate(y, 11, 23), QT_TRANSLATE_NOOP("MarketSessions", "Labour Thanksgiving Day")},
    };
    QSet<QDate> national;
    for (const JpDay& d : base) {
        national.insert(d.date);
        ms_add_closed(c, d.date, d.name);
    }
    // Citizens' holiday: a day sandwiched between two national holidays.
    QList<QDate> sorted(national.cbegin(), national.cend());
    std::sort(sorted.begin(), sorted.end());
    for (const QDate& d : sorted) {
        const QDate mid = d.addDays(1);
        if (!national.contains(mid) && national.contains(d.addDays(2))) {
            national.insert(mid);
            ms_add_closed(c, mid, QT_TRANSLATE_NOOP("MarketSessions", "Citizens' Holiday"));
        }
    }
    // Substitute holiday: a national holiday on Sunday moves to the next non-holiday.
    sorted = QList<QDate>(national.cbegin(), national.cend());
    std::sort(sorted.begin(), sorted.end());
    for (const QDate& d : sorted) {
        if (d.dayOfWeek() != 7)
            continue;
        QDate sub = d.addDays(1);
        while (national.contains(sub))
            sub = sub.addDays(1);
        national.insert(sub);
        ms_add_closed(c, sub, QT_TRANSLATE_NOOP("MarketSessions", "Substitute Holiday"));
    }
    ms_add_closed(c, QDate(y, 1, 2), QT_TRANSLATE_NOOP("MarketSessions", "Market Holiday"));
    ms_add_closed(c, QDate(y, 1, 3), QT_TRANSLATE_NOOP("MarketSessions", "Market Holiday"));
    ms_add_closed(c, QDate(y, 12, 31), QT_TRANSLATE_NOOP("MarketSessions", "Market Holiday"));
}

// Published lists (exchange circulars / government gazettes). close_min -1 = closed.
struct MsFixedDay {
    int y;
    int m;
    int d;
    int close_min;
    const char* name;
};

// NSE / BSE 2026 (NSE circular, plus the 15 Jan Maharashtra civic-poll closure).
const MsFixedDay kMsIndia[] = {
    {2026, 1, 15, -1, QT_TRANSLATE_NOOP("MarketSessions", "Municipal Corporation Elections (Maharashtra)")},
    {2026, 1, 26, -1, QT_TRANSLATE_NOOP("MarketSessions", "Republic Day")},
    {2026, 3, 3, -1, QT_TRANSLATE_NOOP("MarketSessions", "Holi")},
    {2026, 3, 26, -1, QT_TRANSLATE_NOOP("MarketSessions", "Shri Ram Navami")},
    {2026, 3, 31, -1, QT_TRANSLATE_NOOP("MarketSessions", "Shri Mahavir Jayanti")},
    {2026, 4, 3, -1, QT_TRANSLATE_NOOP("MarketSessions", "Good Friday")},
    {2026, 4, 14, -1, QT_TRANSLATE_NOOP("MarketSessions", "Dr. Baba Saheb Ambedkar Jayanti")},
    {2026, 5, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "Maharashtra Day")},
    {2026, 5, 28, -1, QT_TRANSLATE_NOOP("MarketSessions", "Bakri Id")},
    {2026, 6, 26, -1, QT_TRANSLATE_NOOP("MarketSessions", "Muharram")},
    {2026, 9, 14, -1, QT_TRANSLATE_NOOP("MarketSessions", "Ganesh Chaturthi")},
    {2026, 10, 2, -1, QT_TRANSLATE_NOOP("MarketSessions", "Mahatma Gandhi Jayanti")},
    {2026, 10, 20, -1, QT_TRANSLATE_NOOP("MarketSessions", "Dussehra")},
    {2026, 11, 10, -1, QT_TRANSLATE_NOOP("MarketSessions", "Diwali Balipratipada")},
    {2026, 11, 24, -1, QT_TRANSLATE_NOOP("MarketSessions", "Prakash Gurpurb Sri Guru Nanak Dev")},
    {2026, 12, 25, -1, QT_TRANSLATE_NOOP("MarketSessions", "Christmas")},
};
const int kMsIndiaYears[] = {2026};

// HKEX 2026 (HKSAR gazette + HKEX half-day circular) and 2027 (gazette;
// half days on the eves of Lunar New Year, Christmas and New Year are HKEX's
// standing rule). Half days end after the morning session (12:00).
const MsFixedDay kMsHongKong[] = {
    {2026, 1, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day")},
    {2026, 2, 16, ms_hm(12, 0), QT_TRANSLATE_NOOP("MarketSessions", "Lunar New Year's Eve")},
    {2026, 2, 17, -1, QT_TRANSLATE_NOOP("MarketSessions", "Lunar New Year")},
    {2026, 2, 18, -1, QT_TRANSLATE_NOOP("MarketSessions", "Lunar New Year")},
    {2026, 2, 19, -1, QT_TRANSLATE_NOOP("MarketSessions", "Lunar New Year")},
    {2026, 4, 3, -1, QT_TRANSLATE_NOOP("MarketSessions", "Good Friday")},
    {2026, 4, 6, -1, QT_TRANSLATE_NOOP("MarketSessions", "The day following Ching Ming Festival")},
    {2026, 4, 7, -1, QT_TRANSLATE_NOOP("MarketSessions", "The day following Easter Monday")},
    {2026, 5, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "Labour Day")},
    {2026, 5, 25, -1, QT_TRANSLATE_NOOP("MarketSessions", "The day following the Birthday of the Buddha")},
    {2026, 6, 19, -1, QT_TRANSLATE_NOOP("MarketSessions", "Tuen Ng Festival")},
    {2026, 7, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "HKSAR Establishment Day")},
    {2026, 10, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2026, 10, 19, -1, QT_TRANSLATE_NOOP("MarketSessions", "The day following Chung Yeung Festival")},
    {2026, 12, 24, ms_hm(12, 0), QT_TRANSLATE_NOOP("MarketSessions", "Christmas Eve")},
    {2026, 12, 25, -1, QT_TRANSLATE_NOOP("MarketSessions", "Christmas Day")},
    {2026, 12, 31, ms_hm(12, 0), QT_TRANSLATE_NOOP("MarketSessions", "New Year's Eve")},
    {2027, 1, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day")},
    {2027, 2, 5, ms_hm(12, 0), QT_TRANSLATE_NOOP("MarketSessions", "Lunar New Year's Eve")},
    {2027, 2, 8, -1, QT_TRANSLATE_NOOP("MarketSessions", "Lunar New Year")},
    {2027, 2, 9, -1, QT_TRANSLATE_NOOP("MarketSessions", "Lunar New Year")},
    {2027, 3, 26, -1, QT_TRANSLATE_NOOP("MarketSessions", "Good Friday")},
    {2027, 3, 29, -1, QT_TRANSLATE_NOOP("MarketSessions", "Easter Monday")},
    {2027, 4, 5, -1, QT_TRANSLATE_NOOP("MarketSessions", "Ching Ming Festival")},
    {2027, 5, 13, -1, QT_TRANSLATE_NOOP("MarketSessions", "Birthday of the Buddha")},
    {2027, 6, 9, -1, QT_TRANSLATE_NOOP("MarketSessions", "Tuen Ng Festival")},
    {2027, 7, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "HKSAR Establishment Day")},
    {2027, 9, 16, -1, QT_TRANSLATE_NOOP("MarketSessions", "The day following Mid-Autumn Festival")},
    {2027, 10, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2027, 10, 8, -1, QT_TRANSLATE_NOOP("MarketSessions", "Chung Yeung Festival")},
    {2027, 12, 24, ms_hm(12, 0), QT_TRANSLATE_NOOP("MarketSessions", "Christmas Eve")},
    {2027, 12, 27, -1, QT_TRANSLATE_NOOP("MarketSessions", "The first weekday after Christmas Day")},
    {2027, 12, 31, ms_hm(12, 0), QT_TRANSLATE_NOOP("MarketSessions", "New Year's Eve")},
};
const int kMsHongKongYears[] = {2026, 2027};

// SSE / SZSE: weekdays inside the State Council holiday arrangements for 2026
// (issued 2025-11-04) and 2027 (issued 2026-11-05). Make-up working weekends
// are not trading days.
const MsFixedDay kMsChina[] = {
    {2026, 1, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day")},
    {2026, 1, 2, -1, QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day")},
    {2026, 2, 16, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2026, 2, 17, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2026, 2, 18, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2026, 2, 19, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2026, 2, 20, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2026, 2, 23, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2026, 4, 6, -1, QT_TRANSLATE_NOOP("MarketSessions", "Qingming Festival")},
    {2026, 5, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "Labour Day")},
    {2026, 5, 4, -1, QT_TRANSLATE_NOOP("MarketSessions", "Labour Day")},
    {2026, 5, 5, -1, QT_TRANSLATE_NOOP("MarketSessions", "Labour Day")},
    {2026, 6, 19, -1, QT_TRANSLATE_NOOP("MarketSessions", "Dragon Boat Festival")},
    {2026, 9, 25, -1, QT_TRANSLATE_NOOP("MarketSessions", "Mid-Autumn Festival")},
    {2026, 10, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2026, 10, 2, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2026, 10, 5, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2026, 10, 6, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2026, 10, 7, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2027, 1, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "New Year's Day")},
    {2027, 2, 5, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2027, 2, 8, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2027, 2, 9, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2027, 2, 10, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2027, 2, 11, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2027, 2, 12, -1, QT_TRANSLATE_NOOP("MarketSessions", "Spring Festival")},
    {2027, 4, 5, -1, QT_TRANSLATE_NOOP("MarketSessions", "Qingming Festival")},
    {2027, 5, 3, -1, QT_TRANSLATE_NOOP("MarketSessions", "Labour Day")},
    {2027, 5, 4, -1, QT_TRANSLATE_NOOP("MarketSessions", "Labour Day")},
    {2027, 5, 5, -1, QT_TRANSLATE_NOOP("MarketSessions", "Labour Day")},
    {2027, 6, 9, -1, QT_TRANSLATE_NOOP("MarketSessions", "Dragon Boat Festival")},
    {2027, 9, 15, -1, QT_TRANSLATE_NOOP("MarketSessions", "Mid-Autumn Festival")},
    {2027, 10, 1, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2027, 10, 4, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2027, 10, 5, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2027, 10, 6, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
    {2027, 10, 7, -1, QT_TRANSLATE_NOOP("MarketSessions", "National Day")},
};
const int kMsChinaYears[] = {2026, 2027};

template <std::size_t N, std::size_t Y>
void ms_add_fixed(MsCalendar& c, const MsFixedDay (&days)[N], const int (&years)[Y]) {
    for (const MsFixedDay& d : days) {
        if (d.close_min < 0)
            ms_add_closed(c, QDate(d.y, d.m, d.d), d.name);
        else
            ms_add_early_close(c, QDate(d.y, d.m, d.d), d.close_min, d.name);
    }
    for (const int y : years)
        c.full_years.insert(y);
}

// Rule-based calendars are generated for this range.
constexpr int kMsRuleFirstYear = 2000;
constexpr int kMsRuleLastYear = 2060;

MsCalendarMap ms_builtin_calendars() {
    MsCalendarMap m;
    MsCalendar& us = m[QStringLiteral("US")];
    MsCalendar& uk = m[QStringLiteral("UK")];
    MsCalendar& eu = m[QStringLiteral("EURONEXT")];
    MsCalendar& pt = m[QStringLiteral("EURONEXT_PT")];
    for (int y = kMsRuleFirstYear; y <= kMsRuleLastYear; ++y) {
        ms_build_us(us, y);
        us.full_years.insert(y);
        ms_build_uk(uk, y);
        uk.full_years.insert(y);
        if (y >= 2010) {
            ms_build_euronext(eu, y, ms_hm(14, 5));
            eu.full_years.insert(y);
            ms_build_euronext(pt, y, ms_hm(13, 5));
            pt.full_years.insert(y);
        }
    }
    ms_build_us_special(us);
    // Xetra's and TSE's current rules hold from 2022 (Xetra has traded Whit
    // Monday / 3 October for years; Japan's 2019-2021 enthronement and
    // Olympic moves are not modelled).
    MsCalendar& de = m[QStringLiteral("DE")];
    MsCalendar& jp = m[QStringLiteral("JP")];
    for (int y = 2022; y <= kMsRuleLastYear; ++y) {
        ms_build_de(de, y);
        de.full_years.insert(y);
        ms_build_jp(jp, y);
        jp.full_years.insert(y);
    }
    ms_add_fixed(m[QStringLiteral("IN")], kMsIndia, kMsIndiaYears);
    ms_add_fixed(m[QStringLiteral("HK")], kMsHongKong, kMsHongKongYears);
    ms_add_fixed(m[QStringLiteral("CN")], kMsChina, kMsChinaYears);
    return m;
}

struct MsHolidayStore {
    QReadWriteLock lock;
    MsCalendarMap calendars = ms_builtin_calendars();
};

MsHolidayStore& ms_holiday_store() {
    static MsHolidayStore store;
    return store;
}

bool ms_lookup_day(const QString& calendar, QDate date, MsDayEntry* out) {
    MsHolidayStore& store = ms_holiday_store();
    QReadLocker lock(&store.lock);
    const auto cal = store.calendars.constFind(calendar);
    if (cal == store.calendars.constEnd())
        return false;
    const auto it = cal->days.constFind(date);
    if (it == cal->days.constEnd())
        return false;
    *out = it.value();
    return true;
}

QString ms_entry_name(const MsDayEntry& e) {
    if (e.translatable)
        return QCoreApplication::translate(kMsTrContext, e.name.toLatin1().constData());
    return e.name;
}

// ── Schedule arithmetic ─────────────────────────────────────────────────────

qint64 ms_floor_div(qint64 a, qint64 b) {
    qint64 q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

// Local wall-clock time `minutes` after midnight of `date` (may be negative or
// >= 1440), as a UTC instant.
QDateTime ms_local_to_utc(const Venue& v, QDate date, int minutes) {
    const qint64 day_off = ms_floor_div(minutes, 1440);
    const int m = minutes - static_cast<int>(day_off * 1440);
    return QDateTime(date.addDays(day_off), QTime(m / 60, m % 60), v.tz).toUTC();
}

bool ms_us_dst(QDate date) {
    static const QTimeZone kNewYork(QByteArrayLiteral("America/New_York"));
    return kNewYork.isValid() && kNewYork.isDaylightTime(QDateTime(date, QTime(12, 0), kNewYork));
}

bool ms_is_weekend(const Venue& v, QDate date) {
    return ((v.weekend_mask >> (date.dayOfWeek() - 1)) & 1) != 0;
}

struct MsSeg {
    QDateTime start;
    QDateTime end;
    SessionState state;
    QDate date;
};

void ms_append_segments(const DaySchedule& ds, QVector<MsSeg>& out) {
    auto add = [&](const QDateTime& a, const QDateTime& b, SessionState s) {
        if (a.isValid() && b.isValid() && a < b)
            out.append({a, b, s, ds.date});
    };
    add(ds.pre_open_utc, ds.open_utc, SessionState::Pre);
    if (ds.lunch_start_utc.isValid() && ds.lunch_end_utc.isValid()) {
        add(ds.open_utc, ds.lunch_start_utc, SessionState::Open);
        add(ds.lunch_start_utc, ds.lunch_end_utc, SessionState::Lunch);
        add(ds.lunch_end_utc, ds.close_utc, SessionState::Open);
    } else {
        add(ds.open_utc, ds.close_utc, SessionState::Open);
    }
    add(ds.close_utc, ds.post_close_utc, SessionState::Post);
}

// Fills state / next change / trading date of `st` from the sessions of the
// trading dates local-1 .. local+horizon. Returns false when the answer may lie
// beyond the horizon (only when `final_pass` is false).
bool ms_resolve_status(const Venue& v, const QDateTime& now, int horizon, bool final_pass, SessionStatus& st) {
    const QDate local = st.venue_local_now.date();
    QVector<MsSeg> segs;
    for (int off = -1; off <= horizon; ++off) {
        const DaySchedule ds = day_schedule(v, local.addDays(off));
        if (ds.trading)
            ms_append_segments(ds, segs);
    }

    int cur = -1;
    for (int i = 0; i < segs.size(); ++i) {
        if (segs[i].start <= now && now < segs[i].end) {
            cur = i;
            break;
        }
    }
    if (cur >= 0) {
        // Merge contiguous segments in the same state (FX Monday-Friday).
        QDateTime t = segs[cur].end;
        int j = cur + 1;
        while (j < segs.size() && segs[j].start == t && segs[j].state == segs[cur].state) {
            t = segs[j].end;
            ++j;
        }
        if (j >= segs.size() && !final_pass)
            return false; // the run may continue past the days built so far
        st.state = segs[cur].state;
        st.trading_date = segs[cur].date;
        st.next_change_utc = t;
        st.next_state = (j < segs.size() && segs[j].start == t) ? segs[j].state : SessionState::Closed;
        return true;
    }

    int next = -1;
    for (int i = 0; i < segs.size(); ++i) {
        if (segs[i].start > now) {
            next = i;
            break;
        }
    }
    if (next < 0 && !final_pass)
        return false;
    st.trading_date = trading_date(v, now);
    const DaySchedule today = day_schedule(v, st.trading_date);
    st.state = (today.holiday && !today.trading) ? SessionState::Holiday : SessionState::Closed;
    if (next >= 0) {
        st.next_change_utc = segs[next].start;
        st.next_state = segs[next].state;
    }
    return true;
}

const Venue* ms_venue_by_id(const char* id) {
    return venue_by_code(QString::fromLatin1(id));
}

bool ms_is_crypto_pair(const QString& s) {
    static const QSet<QString> kQuotes = {
        QStringLiteral("USD"), QStringLiteral("USDT"), QStringLiteral("USDC"), QStringLiteral("BUSD"),
        QStringLiteral("DAI"), QStringLiteral("EUR"),  QStringLiteral("GBP"),  QStringLiteral("JPY"),
        QStringLiteral("INR"), QStringLiteral("CAD"),  QStringLiteral("AUD"),  QStringLiteral("CHF"),
        QStringLiteral("CNY"), QStringLiteral("KRW"),  QStringLiteral("BRL"),  QStringLiteral("TRY"),
        QStringLiteral("BTC"), QStringLiteral("ETH"),
    };
    if (s.contains(QLatin1Char('.')) || s.startsWith(QLatin1Char('^')))
        return false;
    const qsizetype dash = s.lastIndexOf(QLatin1Char('-'));
    if (dash < 2)
        return false; // base needs 2+ characters; "BRK-B" style share classes fail the quote test anyway
    return kQuotes.contains(s.mid(dash + 1));
}

} // namespace

// ── Venue lookup ─────────────────────────────────────────────────────────────

const std::vector<Venue>& all_venues() {
    return ms_registry().venues;
}

const Venue* venue_by_code(const QString& code) {
    const QString key = code.trimmed().toUpper();
    if (key.isEmpty())
        return nullptr;
    return ms_venue_at(ms_registry().by_code.value(key, -1));
}

const Venue* venue_for_suffix(const QString& suffix) {
    QString key = suffix.trimmed().toUpper();
    if (key.startsWith(QLatin1Char('.')))
        key.remove(0, 1);
    if (key.isEmpty())
        return nullptr;
    return ms_venue_at(ms_registry().by_suffix.value(key, -1));
}

const Venue* venue_for_symbol(const QString& symbol, const QString& exchange_hint) {
    QString s = symbol.trimmed().toUpper();
    QString hint = exchange_hint.trimmed();
    // "NSE:RELIANCE-EQ" (Fyers style): the prefix names the exchange.
    const qsizetype colon = s.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        if (hint.isEmpty())
            hint = s.left(colon);
        s = s.mid(colon + 1);
    }
    const Venue* hinted = hint.isEmpty() ? nullptr : venue_by_code(hint);
    if (s.isEmpty())
        return hinted;
    if (s.endsWith(QLatin1String("=X")))
        return ms_venue_by_id("FX");
    if (s.endsWith(QLatin1String("=F")))
        return ms_venue_by_id("CME");
    if (ms_is_crypto_pair(s))
        return ms_venue_by_id("CRYPTO");

    const bool is_index = s.startsWith(QLatin1Char('^'));
    const qsizetype dot = s.lastIndexOf(QLatin1Char('.'));
    if (dot > 0 && dot < s.size() - 1) {
        if (const Venue* v = venue_for_suffix(s.mid(dot + 1)))
            return v;
        if (!is_index)
            return hinted; // unknown suffix (BRK.B): only the hint can tell
    }
    if (is_index) {
        const int vi = ms_registry().by_index.value(s.mid(1), -1);
        if (vi >= 0)
            return ms_venue_at(vi);
        return hinted;
    }
    // A bare symbol is a US listing — unless the caller named an exchange we do
    // not model (MCX, ...): then it is unknown, not NYSE.
    return hint.isEmpty() ? ms_venue_by_id("NYSE") : hinted;
}

QString venue_display_name(const Venue& venue) {
    return QCoreApplication::translate(kMsTrContext, venue.name.toLatin1().constData());
}

QString session_state_key(SessionState state) {
    switch (state) {
        case SessionState::Pre:
            return QStringLiteral("PRE");
        case SessionState::Open:
            return QStringLiteral("OPEN");
        case SessionState::Lunch:
            return QStringLiteral("LUNCH");
        case SessionState::Post:
            return QStringLiteral("POST");
        case SessionState::Holiday:
            return QStringLiteral("HOLIDAY");
        case SessionState::Closed:
            break;
    }
    return QStringLiteral("CLOSED");
}

QString session_state_label(SessionState state) {
    switch (state) {
        case SessionState::Pre:
            return QCoreApplication::translate("MarketSessions", "Pre-market");
        case SessionState::Open:
            return QCoreApplication::translate("MarketSessions", "Open");
        case SessionState::Lunch:
            return QCoreApplication::translate("MarketSessions", "Lunch break");
        case SessionState::Post:
            return QCoreApplication::translate("MarketSessions", "After hours");
        case SessionState::Holiday:
            return QCoreApplication::translate("MarketSessions", "Holiday");
        case SessionState::Closed:
            break;
    }
    return QCoreApplication::translate("MarketSessions", "Closed");
}

// ── Clock & calendar ────────────────────────────────────────────────────────

QTimeZone venue_timezone(const Venue& venue) {
    return venue.tz;
}

QDate trading_date(const Venue& venue, const QDateTime& utc) {
    if (!utc.isValid() || !venue.tz.isValid())
        return {};
    const QDateTime local = utc.toTimeZone(venue.tz);
    if (venue.open_min < 0) {
        // The session starts the previous evening: the date rolls at that open.
        const int roll = venue.open_min + 1440;
        const int m = local.time().hour() * 60 + local.time().minute();
        if (m >= roll)
            return local.date().addDays(1);
    }
    return local.date();
}

DaySchedule day_schedule(const Venue& v, QDate date) {
    DaySchedule s;
    s.date = date;
    if (!date.isValid() || !v.tz.isValid())
        return s;

    int pre = v.pre_open_min;
    int open = v.open_min;
    int lunch_start = v.lunch_start_min;
    int lunch_end = v.lunch_end_min;
    int close = v.close_min;
    int post = v.post_close_min;

    if (v.alt_weekday != 0 && v.alt_weekday == date.dayOfWeek()) {
        if (v.alt_close_min >= 0) {
            post += v.alt_close_min - close;
            close = v.alt_close_min;
        }
        if (v.alt_lunch_start_min >= 0 && v.alt_lunch_end_min > v.alt_lunch_start_min) {
            lunch_start = v.alt_lunch_start_min;
            lunch_end = v.alt_lunch_end_min;
        }
    }
    if ((v.us_dst_open_shift_min != 0 || v.us_dst_close_shift_min != 0) && ms_us_dst(date)) {
        pre += v.us_dst_open_shift_min;
        open += v.us_dst_open_shift_min;
        close += v.us_dst_close_shift_min;
        post += v.us_dst_close_shift_min;
    }

    const bool weekend = ms_is_weekend(v, date);
    bool trading = !weekend;
    MsDayEntry e;
    if (ms_lookup_day(v.calendar, date, &e)) {
        if (e.closed) {
            if (!weekend) {
                trading = false;
                s.holiday = true;
                s.holiday_name = ms_entry_name(e);
            }
        } else if (weekend) {
            if (e.open_min >= 0 && e.close_min > e.open_min) { // special session (Muhurat)
                trading = true;
                s.special = true;
                s.holiday = true;
                s.holiday_name = ms_entry_name(e);
                pre = open = e.open_min;
                close = post = e.close_min;
                lunch_start = lunch_end = -1;
            }
        } else {
            s.holiday_name = ms_entry_name(e);
            if (e.open_min >= 0) {
                open = e.open_min;
                pre = std::min(pre, open);
            }
            if (e.close_min >= 0) {
                if (e.close_min < close) {
                    s.early_close = true;
                    post = std::max(e.close_min, post - (close - e.close_min));
                } else {
                    post += e.close_min - close;
                }
                close = e.close_min;
            }
            if (lunch_start >= 0 && close <= lunch_end) {
                // The half day ends at or before the afternoon session.
                close = std::min(close, lunch_start);
                post = std::max(post, close);
                lunch_start = lunch_end = -1;
            }
        }
    }

    s.trading = trading;
    s.pre_open_utc = ms_local_to_utc(v, date, pre);
    s.open_utc = ms_local_to_utc(v, date, open);
    if (lunch_start >= 0 && lunch_end > lunch_start) {
        s.lunch_start_utc = ms_local_to_utc(v, date, lunch_start);
        s.lunch_end_utc = ms_local_to_utc(v, date, lunch_end);
    }
    s.close_utc = ms_local_to_utc(v, date, close);
    s.post_close_utc = ms_local_to_utc(v, date, post);
    return s;
}

bool is_trading_day(const Venue& venue, QDate date) {
    return day_schedule(venue, date).trading;
}

SessionStatus session_status(const Venue& v, const QDateTime& now_utc) {
    SessionStatus st;
    if (!now_utc.isValid() || !v.tz.isValid())
        return st;
    const QDateTime now = now_utc.toUTC();
    st.venue_local_now = now.toTimeZone(v.tz);
    if (v.is_24x7()) {
        st.state = SessionState::Open;
        st.next_state = SessionState::Open;
        st.trading_date = trading_date(v, now);
        return st; // never changes: next_change_utc stays invalid
    }
    // The neighbouring days settle almost every query; a weekend or holiday run
    // (or a 24x5 week) needs the two-week horizon.
    if (!ms_resolve_status(v, now, 1, false, st))
        ms_resolve_status(v, now, 15, true, st);
    return st;
}

QDateTime session_open_utc(const Venue& venue, QDate date) {
    const DaySchedule ds = day_schedule(venue, date);
    return ds.trading ? ds.open_utc : QDateTime();
}

QDateTime session_close_utc(const Venue& venue, QDate date) {
    const DaySchedule ds = day_schedule(venue, date);
    return ds.trading ? ds.close_utc : QDateTime();
}

QString holiday_name(const Venue& venue, QDate date) {
    MsDayEntry e;
    if (!date.isValid() || !ms_lookup_day(venue.calendar, date, &e))
        return {};
    return ms_entry_name(e);
}

// ── Annualisation & bar alignment ───────────────────────────────────────────

double session_minutes(const Venue& venue, QDate date) {
    if (!date.isValid()) {
        double m = venue.close_min - venue.open_min;
        if (venue.has_lunch())
            m -= venue.lunch_end_min - venue.lunch_start_min;
        return m;
    }
    const DaySchedule ds = day_schedule(venue, date);
    if (!ds.trading)
        return 0.0;
    qint64 secs = ds.open_utc.secsTo(ds.close_utc);
    if (ds.lunch_start_utc.isValid() && ds.lunch_end_utc.isValid())
        secs -= ds.lunch_start_utc.secsTo(ds.lunch_end_utc);
    return static_cast<double>(secs) / 60.0;
}

double trading_days_per_year(const Venue& venue) {
    return static_cast<double>(venue.trading_days_per_year);
}

double bars_per_year(const Venue& venue, int bar_seconds) {
    if (bar_seconds <= 0)
        return 0.0;
    const double days = trading_days_per_year(venue);
    if (bar_seconds >= 7 * 86400)
        return 365.2425 * 86400.0 / bar_seconds;
    if (bar_seconds >= 86400)
        return days * 86400.0 / bar_seconds;

    // Bars a session-aligned feed prints per day: each continuous segment is
    // rounded up (a 390-minute NYSE day has seven 1h bars, the last one short).
    auto bars_in = [bar_seconds](int minutes) -> qint64 {
        if (minutes <= 0)
            return 0;
        const qint64 secs = static_cast<qint64>(minutes) * 60;
        return (secs + bar_seconds - 1) / bar_seconds;
    };
    qint64 per_day = 0;
    if (venue.has_lunch())
        per_day = bars_in(venue.lunch_start_min - venue.open_min) + bars_in(venue.close_min - venue.lunch_end_min);
    else
        per_day = bars_in(venue.close_min - venue.open_min);
    return days * static_cast<double>(per_day);
}

QDateTime session_bucket_start(const Venue& venue, const QDateTime& ts_utc, int bar_seconds) {
    if (!ts_utc.isValid() || bar_seconds <= 0 || !venue.tz.isValid())
        return ts_utc;
    const QDateTime t = ts_utc.toUTC();
    const QDate local = t.toTimeZone(venue.tz).date();

    // The trading date whose (extended) session contains `t`; otherwise the
    // calendar trading date. Schedules carry times even on non-trading days.
    QDate date = trading_date(venue, t);
    for (int off = -1; off <= 1; ++off) {
        const DaySchedule ds = day_schedule(venue, local.addDays(off));
        if (ds.pre_open_utc <= t && t < ds.post_close_utc) {
            date = ds.date;
            break;
        }
    }
    const DaySchedule ds = day_schedule(venue, date);
    if (bar_seconds >= 86400)
        return ds.open_utc;

    QDateTime anchor = ds.open_utc;
    if (ds.lunch_end_utc.isValid() && t >= ds.lunch_end_utc)
        anchor = ds.lunch_end_utc; // the afternoon session restarts the grid
    const qint64 k = ms_floor_div(anchor.secsTo(t), bar_seconds);
    return anchor.addSecs(k * bar_seconds);
}

// ── Holiday data ────────────────────────────────────────────────────────────

HolidayCoverage holiday_coverage(const Venue& venue, int year) {
    MsHolidayStore& store = ms_holiday_store();
    QReadLocker lock(&store.lock);
    const auto cal = store.calendars.constFind(venue.calendar);
    if (cal == store.calendars.constEnd())
        return HolidayCoverage::None;
    if (cal->full_years.contains(year))
        return HolidayCoverage::Full;
    if (cal->partial_years.contains(year))
        return HolidayCoverage::Partial;
    return HolidayCoverage::None;
}

namespace {

bool ms_parse_hhmm(const QJsonValue& v, int* minutes) {
    if (v.isUndefined() || v.isNull()) {
        *minutes = -1;
        return true;
    }
    const QStringList parts = v.toString().trimmed().split(QLatin1Char(':'));
    if (parts.size() != 2)
        return false;
    bool ok_h = false;
    bool ok_m = false;
    const int h = parts.at(0).toInt(&ok_h);
    const int m = parts.at(1).toInt(&ok_m);
    if (!ok_h || !ok_m || h < 0 || h > 23 || m < 0 || m > 59)
        return false;
    *minutes = h * 60 + m;
    return true;
}

bool ms_parse_years(const QJsonValue& v, QSet<int>* out) {
    if (v.isUndefined())
        return true;
    if (!v.isArray())
        return false;
    for (const QJsonValue& y : v.toArray()) {
        const int year = y.toInt(-1);
        if (year < 1900 || year > 2200)
            return false;
        out->insert(year);
    }
    return true;
}

void ms_set_error(QString* error, const QString& msg) {
    if (error)
        *error = msg;
}

} // namespace

bool load_holiday_overrides_json(const QByteArray& json, QString* error) {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        ms_set_error(error, pe.error != QJsonParseError::NoError ? pe.errorString()
                                                                  : QStringLiteral("root is not an object"));
        return false;
    }
    const QJsonValue cals_v = doc.object().value(QStringLiteral("calendars"));
    if (!cals_v.isObject()) {
        ms_set_error(error, QStringLiteral("missing \"calendars\" object"));
        return false;
    }

    MsHolidayStore& store = ms_holiday_store();
    QWriteLocker lock(&store.lock);
    MsCalendarMap staged = store.calendars; // a bad entry leaves the live data untouched

    const QJsonObject cals = cals_v.toObject();
    for (auto it = cals.begin(); it != cals.end(); ++it) {
        const QString key = it.key().trimmed().toUpper();
        if (key.isEmpty() || !it.value().isObject()) {
            ms_set_error(error, QStringLiteral("calendar \"%1\" is not an object").arg(it.key()));
            return false;
        }
        const QJsonObject o = it.value().toObject();
        QSet<int> years;
        QSet<int> partial_years;
        if (!ms_parse_years(o.value(QStringLiteral("years")), &years) ||
            !ms_parse_years(o.value(QStringLiteral("partial_years")), &partial_years)) {
            ms_set_error(error, QStringLiteral("calendar \"%1\": bad year list").arg(key));
            return false;
        }
        MsCalendar& c = staged[key];
        if (o.value(QStringLiteral("replace_years")).toBool()) {
            for (auto d = c.days.begin(); d != c.days.end();) {
                if (years.contains(d.key().year()))
                    d = c.days.erase(d);
                else
                    ++d;
            }
        }
        const QJsonValue days_v = o.value(QStringLiteral("days"));
        if (!days_v.isUndefined() && !days_v.isArray()) {
            ms_set_error(error, QStringLiteral("calendar \"%1\": \"days\" is not an array").arg(key));
            return false;
        }
        QSet<int> touched_years;
        for (const QJsonValue& dv : days_v.toArray()) {
            const QJsonObject d = dv.toObject();
            const QString date_s = d.value(QStringLiteral("date")).toString();
            const QDate date = QDate::fromString(date_s, Qt::ISODate);
            if (!date.isValid()) {
                ms_set_error(error, QStringLiteral("calendar \"%1\": bad date \"%2\"").arg(key, date_s));
                return false;
            }
            if (d.value(QStringLiteral("remove")).toBool()) {
                c.days.remove(date);
                continue;
            }
            MsDayEntry e;
            if (!ms_parse_hhmm(d.value(QStringLiteral("open")), &e.open_min) ||
                !ms_parse_hhmm(d.value(QStringLiteral("close")), &e.close_min)) {
                ms_set_error(error, QStringLiteral("calendar \"%1\": bad time on %2 (use \"HH:MM\")").arg(key, date_s));
                return false;
            }
            e.closed = e.open_min < 0 && e.close_min < 0;
            e.name = d.value(QStringLiteral("name")).toString();
            e.translatable = false;
            c.days.insert(date, e);
            touched_years.insert(date.year());
        }
        for (const int y : years) {
            c.full_years.insert(y);
            c.partial_years.remove(y);
        }
        for (const int y : std::as_const(partial_years))
            if (!c.full_years.contains(y))
                c.partial_years.insert(y);
        for (const int y : std::as_const(touched_years))
            if (!c.full_years.contains(y))
                c.partial_years.insert(y);
    }
    store.calendars = std::move(staged);
    return true;
}

bool load_holiday_overrides(const QString& path, QString* error) {
    QFile f(path);
    if (!f.exists())
        return true;
    if (!f.open(QIODevice::ReadOnly)) {
        ms_set_error(error, f.errorString());
        return false;
    }
    return load_holiday_overrides_json(f.readAll(), error);
}

void reset_holiday_overrides() {
    MsHolidayStore& store = ms_holiday_store();
    QWriteLocker lock(&store.lock);
    store.calendars = ms_builtin_calendars();
}

} // namespace fincept::core::market
