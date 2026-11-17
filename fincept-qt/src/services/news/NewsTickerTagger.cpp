// NewsTickerTagger.cpp — see NewsTickerTagger.h.

#include "services/news/NewsTickerTagger.h"

#include "core/instrument/VenueSymbols.h"

#include <QRegularExpression>

#include <algorithm>

namespace fincept::services::news_tags {

namespace {

// ── Built-in list ────────────────────────────────────────────────────────────
// Large global companies and major crypto assets, Yahoo symbols. Aliases are
// '|'-separated; a leading '~' marks an alias that is also an everyday word
// ("Visa", "Shell") and only tags with finance context nearby. Non-Latin
// aliases are matched as substrings (CJK text has no word spaces).
struct NtBuiltin {
    const char* symbol;
    const char* aliases;
};

const NtBuiltin kNtBuiltins[] = {
    // United States
    {"AAPL", "Apple|Apple Inc"},
    {"MSFT", "Microsoft"},
    {"NVDA", "Nvidia"},
    {"AMZN", "Amazon|Amazon.com"},
    {"GOOGL", "Alphabet|Google"},
    {"META", "Meta Platforms|Facebook|~Meta"},
    {"TSLA", "Tesla"},
    {"BRK-B", "Berkshire Hathaway|Berkshire"},
    {"JPM", "JPMorgan|JPMorgan Chase|JP Morgan"},
    {"V", "~Visa"},
    {"MA", "Mastercard"},
    {"UNH", "UnitedHealth"},
    {"XOM", "Exxon|ExxonMobil|Exxon Mobil"},
    {"JNJ", "Johnson & Johnson"},
    {"WMT", "Walmart"},
    {"PG", "Procter & Gamble"},
    {"HD", "Home Depot"},
    {"CVX", "Chevron"},
    {"LLY", "Eli Lilly"},
    {"AVGO", "Broadcom"},
    {"ORCL", "Oracle"},
    {"COST", "Costco"},
    {"KO", "Coca-Cola"},
    {"PEP", "PepsiCo"},
    {"BAC", "Bank of America"},
    {"WFC", "Wells Fargo"},
    {"C", "Citigroup|Citi"},
    {"GS", "Goldman Sachs|Goldman"},
    {"MS", "Morgan Stanley"},
    {"BLK", "BlackRock"},
    {"AMD", "Advanced Micro Devices"},
    {"INTC", "Intel"},
    {"CSCO", "Cisco"},
    {"ADBE", "Adobe"},
    {"CRM", "Salesforce"},
    {"NFLX", "Netflix"},
    {"DIS", "Disney|Walt Disney"},
    {"NKE", "Nike"},
    {"MCD", "McDonald's|McDonalds"},
    {"BA", "Boeing"},
    {"CAT", "Caterpillar"},
    {"GE", "GE Aerospace|General Electric"},
    {"GEV", "GE Vernova"},
    {"IBM", "IBM"},
    {"QCOM", "Qualcomm"},
    {"TXN", "Texas Instruments"},
    {"MU", "Micron"},
    {"PFE", "Pfizer"},
    {"MRK", "Merck"},
    {"ABBV", "AbbVie"},
    {"T", "AT&T"},
    {"VZ", "Verizon"},
    {"UBER", "Uber"},
    {"ABNB", "Airbnb"},
    {"PYPL", "PayPal"},
    {"SBUX", "Starbucks"},
    {"F", "Ford Motor|~Ford"},
    {"GM", "General Motors"},
    {"COIN", "Coinbase"},
    {"PLTR", "Palantir"},
    {"SMCI", "Super Micro|Supermicro"},
    {"ARM", "Arm Holdings"},
    {"SNOW", "Snowflake"},
    {"SHOP", "Shopify"},
    // China / Hong Kong / Taiwan
    {"BABA", "Alibaba|\xe9\x98\xbf\xe9\x87\x8c\xe5\xb7\xb4\xe5\xb7\xb4"},
    {"PDD", "PDD Holdings|Pinduoduo|Temu|\xe6\x8b\xbc\xe5\xa4\x9a\xe5\xa4\x9a"},
    {"JD", "JD.com|\xe4\xba\xac\xe4\xb8\x9c"},
    {"BIDU", "Baidu|\xe7\x99\xbe\xe5\xba\xa6"},
    {"NIO", "Nio|\xe8\x94\x9a\xe6\x9d\xa5"},
    {"TSM", "TSMC|Taiwan Semiconductor|\xe5\x8f\xb0\xe7\xa7\xaf\xe7\x94\xb5|\xe5\x8f\xb0\xe7\xa9\x8d\xe9\x9b\xbb"},
    {"0700.HK", "Tencent|\xe8\x85\xbe\xe8\xae\xaf|\xe9\xa8\xb0\xe8\xa8\x8a"},
    {"3690.HK", "Meituan|\xe7\xbe\x8e\xe5\x9b\xa2"},
    {"1211.HK", "BYD|\xe6\xaf\x94\xe4\xba\x9a\xe8\xbf\xaa"},
    {"1810.HK", "Xiaomi|\xe5\xb0\x8f\xe7\xb1\xb3"},
    {"0939.HK", "China Construction Bank|\xe5\xbb\xba\xe8\xae\xbe\xe9\x93\xb6\xe8\xa1\x8c"},
    {"1398.HK", "ICBC|\xe5\xb7\xa5\xe5\x95\x86\xe9\x93\xb6\xe8\xa1\x8c"},
    {"0941.HK", "China Mobile|\xe4\xb8\xad\xe5\x9b\xbd\xe7\xa7\xbb\xe5\x8a\xa8"},
    {"2318.HK", "Ping An|\xe4\xb8\xad\xe5\x9b\xbd\xe5\xb9\xb3\xe5\xae\x89"},
    {"0388.HK", "Hong Kong Exchanges|HKEX"},
    {"1024.HK", "Kuaishou|\xe5\xbf\xab\xe6\x89\x8b"},
    {"0883.HK", "CNOOC"},
    {"0857.HK", "PetroChina|\xe4\xb8\xad\xe5\x9b\xbd\xe7\x9f\xb3\xe6\xb2\xb9"},
    {"600519.SS",
     "Kweichow Moutai|Moutai|\xe8\xb4\xb5\xe5\xb7\x9e\xe8\x8c\x85\xe5\x8f\xb0|\xe8\x8c\x85\xe5\x8f\xb0"},
    {"300750.SZ", "CATL|Contemporary Amperex|\xe5\xae\x81\xe5\xbe\xb7\xe6\x97\xb6\xe4\xbb\xa3"},
    {"2317.TW", "Foxconn|Hon Hai|\xe9\xb4\xbb\xe6\xb5\xb7"},
    {"2454.TW", "MediaTek|\xe8\x81\xaf\xe7\x99\xbc\xe7\xa7\x91"},
    // Japan
    {"7203.T", "Toyota|Toyota Motor|\xe3\x83\x88\xe3\x83\xa8\xe3\x82\xbf"},
    {"6758.T", "Sony|\xe3\x82\xbd\xe3\x83\x8b\xe3\x83\xbc"},
    {"9984.T", "SoftBank|SoftBank Group|\xe3\x82\xbd\xe3\x83\x95\xe3\x83\x88\xe3\x83\x90\xe3\x83\xb3\xe3\x82\xaf"},
    {"6861.T", "Keyence|\xe3\x82\xad\xe3\x83\xbc\xe3\x82\xa8\xe3\x83\xb3\xe3\x82\xb9"},
    {"8306.T", "Mitsubishi UFJ|MUFG|\xe4\xb8\x89\xe8\x8f\xb1UFJ"},
    {"7974.T", "Nintendo|\xe4\xbb\xbb\xe5\xa4\xa9\xe5\xa0\x82"},
    {"9983.T", "Fast Retailing|Uniqlo|\xe3\x83\xa6\xe3\x83\x8b\xe3\x82\xaf\xe3\x83\xad"},
    {"6501.T", "Hitachi|\xe6\x97\xa5\xe7\xab\x8b\xe8\xa3\xbd\xe4\xbd\x9c\xe6\x89\x80"},
    {"7267.T", "Honda|\xe3\x83\x9b\xe3\x83\xb3\xe3\x83\x80"},
    {"7201.T", "Nissan|\xe6\x97\xa5\xe7\x94\xa3\xe8\x87\xaa\xe5\x8b\x95\xe8\xbb\x8a"},
    {"8035.T", "Tokyo Electron|\xe6\x9d\xb1\xe4\xba\xac\xe3\x82\xa8\xe3\x83\xac\xe3\x82\xaf\xe3\x83\x88\xe3\x83\xad\xe3\x83\xb3"},
    {"4063.T", "Shin-Etsu"},
    {"8058.T", "Mitsubishi Corp|\xe4\xb8\x89\xe8\x8f\xb1\xe5\x95\x86\xe4\xba\x8b"},
    {"9432.T", "NTT"},
    {"6098.T", "Recruit Holdings"},
    // Korea
    {"005930.KS", "Samsung Electronics|Samsung|\xec\x82\xbc\xec\x84\xb1\xec\xa0\x84\xec\x9e\x90"},
    {"000660.KS", "SK Hynix|SK\xed\x95\x98\xec\x9d\xb4\xeb\x8b\x89\xec\x8a\xa4"},
    {"005380.KS", "Hyundai Motor|\xed\x98\x84\xeb\x8c\x80\xec\xb0\xa8"},
    {"035420.KS", "Naver|\xeb\x84\xa4\xec\x9d\xb4\xeb\xb2\x84"},
    {"035720.KS", "Kakao|\xec\xb9\xb4\xec\xb9\xb4\xec\x98\xa4"},
    {"373220.KS", "LG Energy Solution"},
    {"005490.KS", "POSCO"},
    // India
    {"RELIANCE.NS", "Reliance Industries|Reliance|RIL"},
    {"TCS.NS", "Tata Consultancy|TCS"},
    {"INFY.NS", "Infosys"},
    {"HDFCBANK.NS", "HDFC Bank"},
    {"ICICIBANK.NS", "ICICI Bank"},
    {"SBIN.NS", "State Bank of India|SBI"},
    {"BHARTIARTL.NS", "Bharti Airtel|Airtel"},
    {"ITC.NS", "ITC"},
    {"LT.NS", "Larsen & Toubro|L&T"},
    {"HINDUNILVR.NS", "Hindustan Unilever|HUL"},
    {"TATAMOTORS.NS", "Tata Motors"},
    {"ADANIENT.NS", "Adani Enterprises|Adani"},
    {"ADANIPORTS.NS", "Adani Ports"},
    {"WIPRO.NS", "Wipro"},
    {"MARUTI.NS", "Maruti Suzuki|Maruti"},
    {"BAJFINANCE.NS", "Bajaj Finance"},
    {"SUNPHARMA.NS", "Sun Pharma|Sun Pharmaceutical"},
    {"KOTAKBANK.NS", "Kotak Mahindra Bank|Kotak Mahindra"},
    {"AXISBANK.NS", "Axis Bank"},
    {"HCLTECH.NS", "HCLTech|HCL Technologies"},
    {"M&M.NS", "Mahindra & Mahindra"},
    // Europe
    {"ASML", "ASML"},
    {"SAP", "SAP"},
    {"NVO", "Novo Nordisk"},
    {"AZN", "AstraZeneca"},
    {"SHEL", "~Shell"},
    {"BP", "BP"},
    {"HSBC", "HSBC"},
    {"UL", "Unilever"},
    {"RIO", "Rio Tinto"},
    {"BHP", "BHP"},
    {"GSK", "GSK|GlaxoSmithKline"},
    {"BCS", "Barclays"},
    {"LYG", "Lloyds Banking|Lloyds"},
    {"VOD", "Vodafone"},
    {"RR.L", "Rolls-Royce"},
    {"BTI", "British American Tobacco"},
    {"NESN.SW", "Nestle|Nestl\xc3\xa9"},
    {"ROG.SW", "Roche"},
    {"NOVN.SW", "Novartis"},
    {"UBS", "UBS"},
    {"MC.PA", "LVMH"},
    {"OR.PA", "L'Oreal|L'Or\xc3\xa9" "al"},
    {"TTE", "TotalEnergies"},
    {"AIR.PA", "Airbus"},
    {"SAN.PA", "Sanofi"},
    {"BNP.PA", "BNP Paribas"},
    {"RMS.PA", "Herm\xc3\xa8s|Hermes International"},
    {"SIE.DE", "Siemens"},
    {"ENR.DE", "Siemens Energy"},
    {"SHL.DE", "Siemens Healthineers"},
    {"ABN.AS", "ABN Amro"},
    {"ALV.DE", "Allianz"},
    {"DTE.DE", "Deutsche Telekom"},
    {"VOW3.DE", "Volkswagen|VW"},
    {"MBG.DE", "Mercedes-Benz"},
    {"BMW.DE", "BMW"},
    {"BAS.DE", "BASF"},
    {"DB", "Deutsche Bank"},
    {"ADS.DE", "Adidas"},
    {"IFX.DE", "Infineon"},
    {"ENEL.MI", "Enel"},
    {"ISP.MI", "Intesa Sanpaolo"},
    {"UCG.MI", "UniCredit"},
    {"STLA", "Stellantis"},
    {"SAN", "Banco Santander|Santander"},
    {"ITX.MC", "Inditex"},
    {"IBE.MC", "Iberdrola"},
    {"BBVA", "BBVA"},
    {"ING", "~ING"},
    {"PRX.AS", "Prosus"},
    {"ADYEN.AS", "Adyen"},
    {"EQNR", "Equinor"},
    {"ERIC", "Ericsson"},
    {"NOK", "Nokia"},
    // Australia / Canada
    {"CBA.AX", "Commonwealth Bank"},
    {"CSL.AX", "CSL"},
    {"WBC.AX", "Westpac"},
    {"NAB.AX", "National Australia Bank"},
    {"ANZ.AX", "ANZ"},
    {"FMG.AX", "Fortescue"},
    {"WDS.AX", "Woodside"},
    {"RY", "Royal Bank of Canada|RBC"},
    {"TD", "TD Bank|Toronto-Dominion"},
    {"ENB", "Enbridge"},
    {"CNQ", "Canadian Natural"},
    {"CNI", "Canadian National Railway|CN Rail"},
    {"BN", "Brookfield"},
    // Latin America
    {"VALE", "~Vale"},
    {"PBR", "Petrobras"},
    {"ITUB", "Ita\xc3\xba Unibanco|Itau Unibanco|Ita\xc3\xba"},
    {"NU", "Nubank|Nu Holdings"},
    {"MELI", "MercadoLibre|Mercado Libre"},
    {"AMX", "Am\xc3\xa9rica M\xc3\xb3vil|America Movil"},
    {"CX", "Cemex"},
    {"BBD", "Bradesco"},
    {"WALMEX.MX", "Walmex|Walmart de M\xc3\xa9xico"},
    {"GGAL", "Grupo Galicia"},
    {"YPF", "YPF"},
    // Middle East / Africa
    {"2222.SR", "Saudi Aramco|Aramco|\xd8\xa3\xd8\xb1\xd8\xa7\xd9\x85\xd9\x83\xd9\x88"},
    {"1120.SR", "Al Rajhi Bank|Al Rajhi"},
    {"TEVA", "Teva"},
    {"CHKP", "Check Point"},
    {"NPN.JO", "Naspers"},
    {"MTN.JO", "MTN Group"},
    {"SBK.JO", "Standard Bank"},
    {"FSR.JO", "FirstRand"},
    // Crypto
    {"BTC-USD", "Bitcoin|\xe6\xaf\x94\xe7\x89\xb9\xe5\xb8\x81|\xe3\x83\x93\xe3\x83\x83\xe3\x83\x88\xe3\x82\xb3\xe3\x82\xa4\xe3\x83\xb3"},
    {"ETH-USD", "Ethereum|Ether"},
    {"SOL-USD", "Solana"},
    {"XRP-USD", "XRP|Ripple"},
    {"DOGE-USD", "Dogecoin"},
    {"BNB-USD", "BNB"},
    {"ADA-USD", "Cardano"},
};

/// Upper-case words that are never symbols on their own (news / finance
/// acronyms, common words, currencies, agencies, media).
const QSet<QString>& nt_not_symbols() {
    static const QSet<QString> words = {
        "THE",    "FOR",   "AND",   "BUT",   "NOT",  "FROM", "WITH",  "THIS",   "THAT",    "HAVE",   "WILL",  "BEEN",
        "THEY",   "WERE",  "SAID",  "HAS",   "ITS",  "NEW",  "ARE",   "WAS",    "WHO",     "HOW",    "WHY",   "ALL",
        "CAN",    "MAY",   "NOW",   "OUT",   "ONE",  "TWO",  "OUR",   "YOU",    "HER",     "HIS",    "SAYS",  "SAY",
        "GET",    "GOT",   "LET",   "TOP",   "BIG",  "NEXT", "OVER",  "INTO",   "AFTER",   "WHAT",   "WHEN",  "ABOUT",
        "AM",     "PM",    "TV",    "PC",    "OK",   "VS",   "NO",    "OR",     "IN",      "IS",     "OF",    "ON",
        "TO",     "AT",    "BY",    "AS",    "AN",   "IF",   "SO",    "UP",     "DO",      "GO",     "BE",    "WE",
        "HE",     "ME",    "MY",    "US",    "UK",   "EU",   "UN",    "UAE",    "USA",     "AI",     "CEO",   "CFO",
        "COO",    "CTO",   "IPO",   "ETF",   "ETFS", "GDP",  "CPI",   "PPI",    "PMI",     "FED",    "FOMC",  "ECB",
        "BOE",    "BOJ",   "PBOC",  "RBI",   "RBA",  "SNB",  "IMF",   "WTO",    "NATO",    "OPEC",   "SEC",   "FDA",
        "DOJ",    "FBI",   "CIA",   "NSA",   "IRS",  "FTC",  "FCC",   "EPA",    "NYSE",    "DOW",    "USD",   "EUR",
        "GBP",    "JPY",   "CNY",   "INR",   "AUD",  "CAD",  "CHF",   "HKD",    "KRW",     "BRL",    "MXN",   "ZAR",
        "GOP",    "ESG",   "COVID", "UPDATE", "LIVE", "VIDEO", "WATCH", "EXCLUSIVE", "ALERT", "BREAKING", "URGENT", "NEWS",
        "TIMES",  "POST",  "BBC",   "CNN",   "CNBC", "AP",   "AFP",   "NPR",    "ABC",     "NBC",    "CBS",   "WSJ",
        "FT",     "NYT",   "RTRS",  "EV",    "EVS",  "LNG",  "Q1",    "Q2",     "Q3",      "Q4",     "YOY",   "QOQ",
        "EPS",    "M&A",   "LLC",   "INC",   "LTD",  "PLC",  "CORP",  "AG",     "SA",      "NV",     "SE",    "SPAC",
        "API",    "APP",   "APPS",  "CHIP",  "CHIPS", "OIL", "GAS",   "WAR",    "LAW",     "TAX",    "JOBS",  "BANK",
        "FUND",   "DEAL",  "SALE",  "BUY",   "SELL", "HOLD", "RATE",  "DATA",   "NET",     "PER",    "VIA",   "AMID",
        "KEY",    "RISK",  "HIGH",  "LOW",   "PRE",  "PRO",  "MORE",  "LESS",   "BEST",    "WORST",  "YEAR",  "WEEK",
        "DAY",    "DAYS",  "MAN",   "CITY",  "STATE", "LIFE", "WORLD", "GLOBAL", "NORTH",  "SOUTH",  "EAST",  "WEST",
        "OECD",   "WHO",   "ASEAN", "BRICS", "G7",   "G20",  "UNSC",  "IEA",    "EIA",     "OCC",    "FDIC",  "CFTC",
        "FINRA",  "SEBI",  "NSE",   "BSE",   "ASX",  "TSX",  "LSE",   "HKEX",   "SGX",     "JPX",    "KRX",   "SSE",
        "SZSE",   "NASDAQ", "AMEX", "SMB",   "B2B",  "B2C",  "SAAS",  "ARR",    "EBITDA",  "ROI",    "ROE",   "PE",
        "VC",     "VCS",   "LP",    "GP",    "PR",   "HR",   "IT",    "RD",     "R&D",     "PHD",    "MBA",   "ESPN",
    };
    return words;
}

/// Lower-case context words that confirm an ambiguous alias ("Visa shares").
const QSet<QString>& nt_context_words() {
    static const QSet<QString> words = {
        "shares",   "share",      "stock",    "stocks",  "earnings",  "results",    "revenue",  "revenues",
        "profit",   "profits",    "quarter",  "quarterly", "ceo",     "investors",  "analyst",  "analysts",
        "upgrade",  "upgrades",   "downgrade", "downgrades", "guidance", "sales",    "dividend", "buyback",
        "inc",      "corp",       "plc",      "ltd",     "nyse",      "nasdaq",     "valuation", "ipo",
        "outlook",  "forecast",   "acquisition", "merger", "deal",    "rating",     "target",   "market",
    };
    return words;
}

/// Everyday words that make a company-name alias ambiguous.
const QSet<QString>& nt_ambiguous_names() {
    static const QSet<QString> words = {
        "target", "visa",   "shell",   "gap",       "block",    "snap",   "ford",    "vale",   "lilly",  "meta",
        "arm",    "delta",  "southern", "dominion", "progressive", "discover", "booking", "ball", "match", "zoom",
        "unity",  "eagle",  "square",  "circle",    "general",  "american", "united", "first", "national", "global",
        "international", "energy", "capital", "bank", "group", "holdings", "trust", "apple", "amazon", "oracle",
        "mercury", "jupiter", "orange", "next", "sky", "intuitive", "corning", "harmony", "alliance", "fortune",
    };
    return words;
}

struct NtToken {
    QString norm;     // lower case, possessive "'s" removed
    int start = 0;    // offset in the text
    bool upper = false; // first character is upper case (or a digit)
};

bool nt_is_word_char(QChar c) {
    return c.isLetterOrNumber();
}

bool nt_is_joiner(QChar c) {
    return c == QLatin1Char('&') || c == QLatin1Char('\'') || c == QChar(0x2019) || c == QLatin1Char('.') ||
           c == QLatin1Char('-');
}

/// Word tokens: letters / digits, joined across & ' . - when a word character
/// follows ("AT&T", "L'Oréal", "JD.com", "Coca-Cola").
QVector<NtToken> nt_tokens(const QString& text) {
    QVector<NtToken> out;
    const int n = static_cast<int>(text.size());
    int i = 0;
    while (i < n) {
        if (!nt_is_word_char(text.at(i))) {
            ++i;
            continue;
        }
        const int start = i;
        while (i < n) {
            if (nt_is_word_char(text.at(i))) {
                ++i;
                continue;
            }
            if (nt_is_joiner(text.at(i)) && i + 1 < n && nt_is_word_char(text.at(i + 1))) {
                ++i;
                continue;
            }
            break;
        }
        QString word = text.mid(start, i - start);
        word.replace(QChar(0x2019), QLatin1Char('\''));
        QString norm = word.toLower();
        if (norm.endsWith(QLatin1String("'s")) && norm.size() > 2)
            norm.chop(2);
        NtToken t;
        t.norm = norm;
        t.start = start;
        t.upper = word.at(0).isUpper() || word.at(0).isDigit();
        out.append(t);
    }
    return out;
}

QString nt_normalize_alias(const QString& alias) {
    const auto toks = nt_tokens(alias);
    QStringList parts;
    parts.reserve(toks.size());
    for (const auto& t : toks)
        parts.append(t.norm);
    return parts.join(QLatin1Char(' '));
}

bool nt_has_non_latin(const QString& s) {
    for (const QChar c : s)
        if (c.unicode() >= 0x0370 && c.isLetter())
            return true;
    return false;
}

/// Bases a symbol is known by: "RELIANCE.NS" -> {RELIANCE.NS, RELIANCE},
/// "BTC-USD" -> {BTC-USD, BTC}, "BRK-B" -> {BRK-B, BRK.B}.
QStringList nt_symbol_bases(const QString& symbol_in) {
    const QString symbol = symbol_in.trimmed().toUpper();
    QStringList out{symbol};
    const int dot = static_cast<int>(symbol.lastIndexOf(QLatin1Char('.')));
    if (dot > 0 && symbol.size() - dot <= 4)
        out.append(symbol.left(dot));
    static const QRegularExpression fx_re(QStringLiteral("^([A-Z0-9]{2,10})-(USD|USDT|USDC|EUR|GBP)$"));
    const auto m = fx_re.match(symbol);
    if (m.hasMatch())
        out.append(m.captured(1));
    if (symbol.contains(QLatin1Char('-')) && !m.hasMatch())
        out.append(QString(symbol).replace(QLatin1Char('-'), QLatin1Char('.')));
    return out;
}

/// Zero-pad a numeric listing code the way Yahoo writes it for that market.
QString nt_pad_code(const QString& code, const QString& suffix) {
    bool numeric = !code.isEmpty();
    for (const QChar c : code)
        numeric = numeric && c.isDigit();
    if (!numeric)
        return code;
    int width = 0;
    if (suffix == QLatin1String(".HK") || suffix == QLatin1String(".T") || suffix == QLatin1String(".TW"))
        width = 4;
    else if (suffix == QLatin1String(".SS") || suffix == QLatin1String(".SZ") || suffix == QLatin1String(".KS") ||
             suffix == QLatin1String(".KQ"))
        width = 6;
    return width > 0 ? code.rightJustified(width, QLatin1Char('0')) : code;
}

void nt_add_tag(QVector<TickerTag>& tags, const QString& symbol, const QString& matched, TagSource source,
                double confidence, int position) {
    if (symbol.isEmpty())
        return;
    for (auto& t : tags) {
        if (t.symbol == symbol) {
            if (confidence > t.confidence) {
                t.confidence = confidence;
                t.source = source;
                t.matched = matched;
            }
            t.position = std::min(t.position, position);
            return;
        }
    }
    tags.append({symbol, matched, source, confidence, position});
}

} // namespace

// ── Public helpers ───────────────────────────────────────────────────────────

QString tag_source_label(TagSource source) {
    switch (source) {
        case TagSource::Cashtag:
            return QStringLiteral("cashtag");
        case TagSource::Exchange:
            return QStringLiteral("exchange code");
        case TagSource::Alias:
            return QStringLiteral("company name");
        case TagSource::Symbol:
            return QStringLiteral("symbol");
    }
    return QStringLiteral("symbol");
}

QString normalize_company_name(const QString& name) {
    static const QRegularExpression suffix_re(
        QStringLiteral("(?i)[,\\s]+(inc\\.?|incorporated|corp\\.?|corporation|co\\.?|company|ltd\\.?|limited|plc|"
                       "ag|sa|s\\.a\\.?|nv|n\\.v\\.?|se|spa|s\\.p\\.a\\.?|asa|ab|oyj|holdings?|group|"
                       "class [a-c]|adr|ads|the|common stock|ordinary shares)\\s*$"));
    QString n = name.trimmed();
    for (int guard = 0; guard < 4; ++guard) {
        const QString before = n;
        n.remove(suffix_re);
        n = n.trimmed();
        if (n == before)
            break;
    }
    if (n.startsWith(QLatin1String("The "), Qt::CaseInsensitive))
        n = n.mid(4);
    return n.trimmed();
}

QString yahoo_for_exchange_code(const QString& exchange, const QString& code_in) {
    namespace ci = fincept::core::instrument;
    const ci::VenueInfo* venue = ci::venue_info_for_code(exchange);
    if (!venue)
        return {};
    QString code = code_in.trimmed().toUpper();
    if (code.isEmpty())
        return {};
    // Numeric codes in the venue's own width: to_yahoo pads Hong Kong and the
    // Chinese / Korean boards itself, not Tokyo or Taiwan ("TPE: 50" is 0050.TW).
    if (!code.contains(QLatin1Char('.')) && !venue->suffixes.isEmpty())
        code = nt_pad_code(code, QLatin1Char('.') + venue->suffixes.first());
    return ci::to_yahoo(code, exchange);
}

// ── SymbolDictionary ─────────────────────────────────────────────────────────

SymbolDictionary SymbolDictionary::builtin() {
    SymbolDictionary d;
    for (const auto& b : kNtBuiltins) {
        const QString symbol = QString::fromLatin1(b.symbol);
        d.add_symbol(symbol);
        const QStringList aliases = QString::fromUtf8(b.aliases).split(QLatin1Char('|'), Qt::SkipEmptyParts);
        for (QString a : aliases) {
            const bool ambiguous = a.startsWith(QLatin1Char('~'));
            if (ambiguous)
                a.remove(0, 1);
            d.add_alias(a, symbol, ambiguous);
        }
    }
    return d;
}

void SymbolDictionary::add_symbol(const QString& symbol_in, const QString& name, bool user_owned) {
    const QString symbol = symbol_in.trimmed().toUpper();
    if (symbol.isEmpty() || symbol.size() > 24)
        return;
    symbols_.insert(symbol);
    const QStringList bases = nt_symbol_bases(symbol);
    for (const QString& b : bases) {
        // A symbol always owns its own exact form ("SAN" is Santander even though
        // SAN.PA was registered first); an explicit user symbol also wins a
        // derived base ("SAN" in a watchlist as SAN.MC); otherwise the first
        // registration of a derived base stays.
        if (user_owned || b == symbol || !base_to_symbol_.contains(b))
            base_to_symbol_.insert(b, symbol);
        if (user_owned)
            user_bases_.insert(b);
    }
    if (!name.trimmed().isEmpty()) {
        const QString clean = normalize_company_name(name);
        const QString norm = nt_normalize_alias(clean);
        if (norm.size() >= 3 && !norm.contains(QLatin1Char(' ')) && !nt_has_non_latin(clean)) {
            // A one-word name that is an everyday word (Target, Shell) is ambiguous.
            add_alias(clean, symbol, nt_ambiguous_names().contains(norm));
        } else if (norm.size() >= 3) {
            add_alias(clean, symbol, false);
        }
    }
}

void SymbolDictionary::add_alias(const QString& alias, const QString& symbol_in, bool ambiguous) {
    const QString symbol = symbol_in.trimmed().toUpper();
    const QString a = alias.trimmed();
    if (a.isEmpty() || symbol.isEmpty())
        return;
    if (!symbols_.contains(symbol))
        add_symbol(symbol);
    if (nt_has_non_latin(a)) {
        if (a.size() < 2)
            return;
        for (const auto& existing : std::as_const(script_aliases_))
            if (existing.first == a)
                return;
        script_aliases_.append({a, symbol});
        // Longest first, so "贵州茅台" wins over "茅台" in the same text.
        std::sort(script_aliases_.begin(), script_aliases_.end(),
                  [](const auto& x, const auto& y) { return x.first.size() > y.first.size(); });
        return;
    }
    const QString norm = nt_normalize_alias(a);
    if (norm.size() < 2)
        return;
    if (aliases_.contains(norm))
        return; // first registration wins (built-in names are added first)
    aliases_.insert(norm, {symbol, ambiguous});
    max_alias_words_ = std::max(max_alias_words_, static_cast<int>(norm.count(QLatin1Char(' '))) + 1);
}

QString SymbolDictionary::resolve(const QString& token) const {
    const QString up = token.trimmed().toUpper();
    if (up.isEmpty())
        return {};
    const auto it = base_to_symbol_.constFind(up);
    return it != base_to_symbol_.constEnd() ? it.value() : QString();
}

// ── Tagging ──────────────────────────────────────────────────────────────────

QVector<TickerTag> tag(const QString& headline, const QString& summary, const SymbolDictionary& dict,
                       QStringList* unverified, int max_tags) {
    QVector<TickerTag> tags;
    const QString text = summary.isEmpty() ? headline : headline + QLatin1Char('\n') + summary;
    if (text.trimmed().isEmpty())
        return tags;

    // 1. Cashtags: $AAPL, $btc, $RELIANCE.NS
    static const QRegularExpression cashtag_re(
        QStringLiteral("(?<![\\w$])\\$([A-Za-z]{1,6}(?:[.\\-][A-Za-z0-9]{1,4})?)(?![\\w])"));
    for (auto it = cashtag_re.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        const QString raw = m.captured(1).toUpper();
        const QString known = dict.resolve(raw);
        nt_add_tag(tags, known.isEmpty() ? raw : known, m.captured(0), TagSource::Cashtag, known.isEmpty() ? 0.8 : 0.95,
                   static_cast<int>(m.capturedStart(0)));
    }

    // 2. Exchange-qualified: NSE:RELIANCE, (NASDAQ: AAPL), HKG: 700, TYO:7203
    static const QRegularExpression exchange_re(QStringLiteral(
        "(?<![\\w])(NYSE|NASDAQ|Nasdaq|AMEX|NYSEARCA|NSE|BSE|TSXV|TSX|ASX|LSE|LON|HKEX|HKG|SEHK|TYO|TSE|JPX|KRX|"
        "KOSPI|KOSDAQ|SGX|SSE|SHA|SZSE|SHE|TWSE|TPE|JSE|SIX|SWX|XETRA|ETR|FRA|EPA|BIT|BME|AMS|STO|OSL|CPH|HEL|BMV|"
        "B3|BVMF|BOVESPA|TADAWUL|TASE|NZX|IDX|SET|KLSE|BURSA)\\s?:\\s?([A-Za-z0-9][A-Za-z0-9&\\-]{0,11}"
        "(?:\\.[A-Za-z]{1,2})?)(?![\\w])"));
    for (auto it = exchange_re.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        const QString symbol = yahoo_for_exchange_code(m.captured(1), m.captured(2));
        if (symbol.isEmpty())
            continue; // a venue Yahoo does not carry: nothing quotable to tag
        const QString known = dict.resolve(symbol);
        nt_add_tag(tags, known.isEmpty() ? symbol : known, m.captured(0), TagSource::Exchange, 0.95,
                   static_cast<int>(m.capturedStart(0)));
    }

    // 3. Suffixed codes: RELIANCE.NS, 7203.T, 0700.HK, 600519.SS
    static const QRegularExpression suffix_re(QStringLiteral(
        "(?<![\\w.$])([A-Z0-9][A-Z0-9&\\-]{0,11})\\.(NS|BO|T|HK|L|DE|F|PA|AS|BR|MI|MC|LS|SW|ST|OL|CO|HE|IR|VI|WA|"
        "IS|AX|NZ|TO|V|KS|KQ|SS|SZ|SI|TW|TWO|JK|BK|KL|MX|SA|BA|SN|JO|SR|TA|QA|AE)(?![\\w.])"));
    for (auto it = suffix_re.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        const QString code = m.captured(1);
        bool numeric = true;
        for (const QChar c : code)
            numeric = numeric && c.isDigit();
        if (!numeric && code.size() < 2)
            continue; // "B.V", "N.V" — legal forms, not listings
        const QString suffix = QLatin1Char('.') + m.captured(2);
        const QString symbol = nt_pad_code(code, suffix) + suffix;
        nt_add_tag(tags, symbol, m.captured(0), TagSource::Exchange, 0.9, static_cast<int>(m.capturedStart(0)));
    }

    // 4. Company names and local-script aliases.
    const QVector<NtToken> toks = nt_tokens(text);
    QSet<QString> lower_words;
    lower_words.reserve(toks.size());
    for (const auto& t : toks)
        lower_words.insert(t.norm);
    auto has_context = [&lower_words, &dict](const QString& symbol) {
        for (const QString& w : nt_context_words())
            if (lower_words.contains(w))
                return true;
        for (const QString& b : nt_symbol_bases(symbol))
            if (lower_words.contains(b.toLower()) && b.size() >= 2 && dict.resolve(b) == symbol)
                return true;
        return false;
    };
    const auto& aliases = dict.latin_aliases();
    const int max_words = dict.max_alias_words();
    for (int i = 0; i < toks.size();) {
        int matched_words = 0;
        QString key;
        QString best_key;
        for (int n = 1; n <= max_words && i + n <= toks.size(); ++n) {
            key = n == 1 ? toks[i].norm : key + QLatin1Char(' ') + toks[i + n - 1].norm;
            if (aliases.contains(key)) {
                matched_words = n;
                best_key = key;
            }
        }
        // Hyphenated compounds name the company first ("VW-Chef", "Nvidia-Spitze",
        // "Toyota-backed"): try the part before the hyphen on its own.
        if (matched_words == 0 && toks[i].upper) {
            const int dash = static_cast<int>(toks[i].norm.indexOf(QLatin1Char('-')));
            if (dash >= 2 && aliases.contains(toks[i].norm.left(dash))) {
                matched_words = 1;
                best_key = toks[i].norm.left(dash);
            }
        }
        if (matched_words > 0 && toks[i].upper) {
            const auto& info = aliases.value(best_key);
            if (!info.ambiguous || has_context(info.symbol)) {
                const int start = toks[i].start;
                const int end = toks[i + matched_words - 1].start + static_cast<int>(toks[i + matched_words - 1].norm.size());
                nt_add_tag(tags, info.symbol, text.mid(start, end - start), TagSource::Alias, info.ambiguous ? 0.7 : 0.85,
                           start);
            }
            i += matched_words;
            continue;
        }
        ++i;
    }
    if (nt_has_non_latin(text)) {
        QVector<QPair<int, int>> taken; // [start, end) of matched script aliases
        for (const auto& [alias, symbol] : dict.script_aliases()) {
            const int pos = static_cast<int>(text.indexOf(alias));
            if (pos < 0)
                continue;
            const int end = pos + static_cast<int>(alias.size());
            bool overlaps = false;
            for (const auto& r : std::as_const(taken))
                overlaps = overlaps || (pos < r.second && end > r.first);
            if (overlaps)
                continue; // "茅台" inside an already matched "贵州茅台"
            taken.append({pos, end});
            nt_add_tag(tags, symbol, alias, TagSource::Alias, 0.85, pos);
        }
    }

    // 5. Bare upper-case symbols the dictionary knows; 6. the rest is unverified.
    int letters = 0;
    int upper = 0;
    for (const QChar ch : headline) {
        if (ch.isLetter()) {
            ++letters;
            if (ch.isUpper())
                ++upper;
        }
    }
    const bool shouting = letters >= 16 && upper * 10 >= letters * 6;
    if (!shouting) {
        static const QRegularExpression bare_re(QStringLiteral("(?<![\\w$.&\\-])([A-Z]{2,6})(?![\\w&\\-]|\\.[A-Za-z])"));
        const auto& stop = nt_not_symbols();
        for (auto it = bare_re.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            const QString word = m.captured(1);
            if (stop.contains(word))
                continue;
            const QString known = dict.resolve(word);
            const bool user = dict.is_user_symbol(word);
            if (!known.isEmpty() && (word.size() >= 3 || user)) {
                nt_add_tag(tags, known, word, TagSource::Symbol, user ? 0.75 : 0.65,
                           static_cast<int>(m.capturedStart(1)));
                continue;
            }
            if (unverified && known.isEmpty() && word.size() <= 5 && unverified->size() < 3 &&
                !unverified->contains(word)) {
                bool already = false;
                for (const auto& t : std::as_const(tags))
                    already = already || t.matched.contains(word);
                if (!already)
                    unverified->append(word);
            }
        }
    }

    std::stable_sort(tags.begin(), tags.end(), [](const TickerTag& a, const TickerTag& b) {
        if (a.confidence != b.confidence)
            return a.confidence > b.confidence;
        return a.position < b.position;
    });
    if (tags.size() > max_tags)
        tags.resize(max_tags);
    return tags;
}

} // namespace fincept::services::news_tags
