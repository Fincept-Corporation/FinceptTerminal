#include "services/news/NewsNlpEngine.h"

#include "services/news/VaderSentiment.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QLocale>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace fincept::services::news_nlp {

namespace nlp_engine_detail {

struct NlpAliasCode {
    const char* alias;
    const char* code;
};

struct NlpTermName {
    const char* term;
    const char* name;
};

struct NlpWordValence {
    const char* word;
    double valence;
};

struct NlpPlace {
    const char* key;
    double lat;
    double lon;
};

// Country aliases, in match order; a code's display name is its FIRST alias.
constexpr NlpAliasCode kNlpCountries[] = {
    {"united states", "US"},
    {"usa", "US"},
    {"u.s.", "US"},
    {"america", "US"},
    {"china", "CN"},
    {"chinese", "CN"},
    {"beijing", "CN"},
    {"shanghai", "CN"},
    {"russia", "RU"},
    {"russian", "RU"},
    {"moscow", "RU"},
    {"kremlin", "RU"},
    {"ukraine", "UA"},
    {"ukrainian", "UA"},
    {"kyiv", "UA"},
    {"kiev", "UA"},
    {"united kingdom", "GB"},
    {"britain", "GB"},
    {"british", "GB"},
    {"london", "GB"},
    {"uk", "GB"},
    {"germany", "DE"},
    {"german", "DE"},
    {"berlin", "DE"},
    {"france", "FR"},
    {"french", "FR"},
    {"paris", "FR"},
    {"japan", "JP"},
    {"japanese", "JP"},
    {"tokyo", "JP"},
    {"india", "IN"},
    {"indian", "IN"},
    {"mumbai", "IN"},
    {"delhi", "IN"},
    {"brazil", "BR"},
    {"brazilian", "BR"},
    {"canada", "CA"},
    {"canadian", "CA"},
    {"australia", "AU"},
    {"australian", "AU"},
    {"south korea", "KR"},
    {"korean", "KR"},
    {"seoul", "KR"},
    {"north korea", "KP"},
    {"pyongyang", "KP"},
    {"iran", "IR"},
    {"iranian", "IR"},
    {"tehran", "IR"},
    {"israel", "IL"},
    {"israeli", "IL"},
    {"tel aviv", "IL"},
    {"palestine", "PS"},
    {"palestinian", "PS"},
    {"gaza", "PS"},
    {"west bank", "PS"},
    {"saudi arabia", "SA"},
    {"saudi", "SA"},
    {"riyadh", "SA"},
    {"turkey", "TR"},
    {"turkish", "TR"},
    {"ankara", "TR"},
    {"istanbul", "TR"},
    {"egypt", "EG"},
    {"egyptian", "EG"},
    {"cairo", "EG"},
    {"italy", "IT"},
    {"italian", "IT"},
    {"rome", "IT"},
    {"spain", "ES"},
    {"spanish", "ES"},
    {"madrid", "ES"},
    {"mexico", "MX"},
    {"mexican", "MX"},
    {"indonesia", "ID"},
    {"indonesian", "ID"},
    {"jakarta", "ID"},
    {"netherlands", "NL"},
    {"dutch", "NL"},
    {"switzerland", "CH"},
    {"swiss", "CH"},
    {"sweden", "SE"},
    {"swedish", "SE"},
    {"norway", "NO"},
    {"norwegian", "NO"},
    {"poland", "PL"},
    {"polish", "PL"},
    {"warsaw", "PL"},
    {"taiwan", "TW"},
    {"taiwanese", "TW"},
    {"taipei", "TW"},
    {"singapore", "SG"},
    {"hong kong", "HK"},
    {"south africa", "ZA"},
    {"nigeria", "NG"},
    {"nigerian", "NG"},
    {"lagos", "NG"},
    {"argentina", "AR"},
    {"colombia", "CO"},
    {"pakistan", "PK"},
    {"pakistani", "PK"},
    {"thailand", "TH"},
    {"thai", "TH"},
    {"bangkok", "TH"},
    {"vietnam", "VN"},
    {"vietnamese", "VN"},
    {"philippines", "PH"},
    {"filipino", "PH"},
    {"malaysia", "MY"},
    {"malaysian", "MY"},
    {"iraq", "IQ"},
    {"iraqi", "IQ"},
    {"baghdad", "IQ"},
    {"syria", "SY"},
    {"syrian", "SY"},
    {"damascus", "SY"},
    {"yemen", "YE"},
    {"yemeni", "YE"},
    {"libya", "LY"},
    {"libyan", "LY"},
    {"lebanon", "LB"},
    {"lebanese", "LB"},
    {"beirut", "LB"},
    {"afghanistan", "AF"},
    {"afghan", "AF"},
    {"kabul", "AF"},
    {"myanmar", "MM"},
    {"burmese", "MM"},
    {"ethiopia", "ET"},
    {"ethiopian", "ET"},
    {"sudan", "SD"},
    {"sudanese", "SD"},
    {"khartoum", "SD"},
    {"somalia", "SO"},
    {"somali", "SO"},
    {"venezuela", "VE"},
    {"venezuelan", "VE"},
    {"cuba", "CU"},
    {"cuban", "CU"},
    {"havana", "CU"},
    {"european union", "EU"},
    {"eu", "EU"},
    {"brussels", "EU"},
};

// Terms of 3 characters or fewer only count in their Capitalised/UPPER form ("UN",
// "SEC", "WHO" — not "un", "sec", "who").
constexpr NlpTermName kNlpOrganizations[] = {
    {"nato", "NATO"},
    {"united nations", "UN"},
    {"un", "UN"},
    {"imf", "IMF"},
    {"world bank", "World Bank"},
    {"federal reserve", "Federal Reserve"},
    {"the fed", "Federal Reserve"},
    {"ecb", "ECB"},
    {"european central bank", "ECB"},
    {"bank of england", "Bank of England"},
    {"boe", "Bank of England"},
    {"bank of japan", "Bank of Japan"},
    {"boj", "Bank of Japan"},
    {"peoples bank of china", "PBOC"},
    {"pboc", "PBOC"},
    {"sec", "SEC"},
    {"securities and exchange", "SEC"},
    {"opec", "OPEC"},
    {"opec+", "OPEC+"},
    {"world health organization", "WHO"},
    {"who", "WHO"},
    {"world trade organization", "WTO"},
    {"wto", "WTO"},
    {"pentagon", "Pentagon"},
    {"white house", "White House"},
    {"congress", "US Congress"},
    {"senate", "US Senate"},
    {"european commission", "European Commission"},
    {"iaea", "IAEA"},
    {"g7", "G7"},
    {"g20", "G20"},
    {"brics", "BRICS"},
    {"reuters", "Reuters"},
    {"bloomberg", "Bloomberg"},
    {"apple", "Apple"},
    {"google", "Google"},
    {"alphabet", "Alphabet"},
    {"microsoft", "Microsoft"},
    {"amazon", "Amazon"},
    {"meta", "Meta"},
    {"nvidia", "NVIDIA"},
    {"tesla", "Tesla"},
    {"openai", "OpenAI"},
    {"goldman sachs", "Goldman Sachs"},
    {"jpmorgan", "JPMorgan"},
    {"blackrock", "BlackRock"},
    {"berkshire hathaway", "Berkshire Hathaway"},
    {"hsbc", "HSBC"},
    {"ubs", "UBS"},
    {"credit suisse", "Credit Suisse"},
    {"deutsche bank", "Deutsche Bank"},
    {"barclays", "Barclays"},
    {"citigroup", "Citigroup"},
    {"morgan stanley", "Morgan Stanley"},
};
constexpr qsizetype kNlpOrgProperNounMaxLen = 3;

// Terms of 4 characters or fewer only count in their Capitalised/UPPER form ("Xi",
// "Cook" — not "taxi", "cook").
constexpr NlpTermName kNlpKnownPersons[] = {
    {"biden", "Joe Biden"},
    {"trump", "Donald Trump"},
    {"obama", "Barack Obama"},
    {"putin", "Vladimir Putin"},
    {"xi jinping", "Xi Jinping"},
    {"xi", "Xi Jinping"},
    {"modi", "Narendra Modi"},
    {"macron", "Emmanuel Macron"},
    {"scholz", "Olaf Scholz"},
    {"sunak", "Rishi Sunak"},
    {"starmer", "Keir Starmer"},
    {"kishida", "Fumio Kishida"},
    {"trudeau", "Justin Trudeau"},
    {"zelensky", "Volodymyr Zelensky"},
    {"zelenskyy", "Volodymyr Zelensky"},
    {"netanyahu", "Benjamin Netanyahu"},
    {"erdogan", "Recep Tayyip Erdogan"},
    {"musk", "Elon Musk"},
    {"bezos", "Jeff Bezos"},
    {"zuckerberg", "Mark Zuckerberg"},
    {"cook", "Tim Cook"},
    {"nadella", "Satya Nadella"},
    {"altman", "Sam Altman"},
    {"dimon", "Jamie Dimon"},
    {"powell", "Jerome Powell"},
    {"lagarde", "Christine Lagarde"},
    {"yellen", "Janet Yellen"},
};
constexpr qsizetype kNlpPersonProperNounMaxLen = 4;

// "<title> Name Surname" — lower-case titles only, matched case-sensitively.
constexpr const char* kNlpTitlesHead[] = {"president", "prime minister", "chancellor", "king",
                                          "queen",     "prince",         "princess"};
constexpr const char* kNlpTitlesOffice[] = {"ceo",      "cfo",       "chairman", "director",
                                            "minister", "secretary", "governor", "general"};
constexpr const char* kNlpTitlesHonorific[] = {"mr", "mrs", "ms", "dr", "prof"}; // followed by "."

// Words / news acronyms that look like tickers but are not. Kept in step with
// NewsService::enrich_article().
constexpr const char* kNlpTickerStopwords[] = {
    "ABOUT", "AFTER", "AI",   "ALERT", "ALL",       "AM",       "AN",    "AND",  "ARE",  "AS",   "AT",     "AUD",
    "BE",    "BEEN",  "BIG",  "BOE",   "BOJ",       "BREAKING", "BUT",   "BY",   "CAD",  "CAN",  "CEO",    "CFO",
    "CHF",   "CIA",   "CNY",  "COO",   "COVID",     "CPI",      "CTO",   "DO",   "DOJ",  "DOW",  "ECB",    "EPA",
    "ESG",   "ETF",   "EU",   "EUR",   "EXCLUSIVE", "FBI",      "FCC",   "FDA",  "FED",  "FOMC", "FOR",    "FROM",
    "FTC",   "GBP",   "GDP",  "GET",   "GO",        "GOP",      "GOT",   "HAS",  "HAVE", "HE",   "HER",    "HIS",
    "HOW",   "IF",    "IMF",  "IN",    "INR",       "INTO",     "IPO",   "IRS",  "IS",   "ITS",  "JPY",    "LET",
    "LIVE",  "MAY",   "ME",   "MY",    "NATO",      "NEW",      "NEWS",  "NEXT", "NO",   "NOT",  "NOW",    "NSA",
    "NYSE",  "OF",    "OK",   "ON",    "ONE",       "OPEC",     "OR",    "OUR",  "OUT",  "OVER", "PBOC",   "PC",
    "PM",    "PMI",   "POST", "PPI",   "SAID",      "SAY",      "SAYS",  "SEC",  "SO",   "THAT", "THE",    "THEY",
    "THIS",  "TIMES", "TO",   "TOP",   "TV",        "TWO",      "UAE",   "UK",   "UN",   "UP",   "UPDATE", "URGENT",
    "US",    "USA",   "USD",  "VIDEO", "VS",        "WAS",      "WATCH", "WE",   "WERE", "WHAT", "WHEN",   "WHO",
    "WHY",   "WILL",  "WITH", "WTO",   "YOU",
};

// Finance lexicon — VADER valences on its [-4, 4] scale, laid over vader_lexicon.txt,
// and (as integer weights) the keyword signals behind confidence and the fallback scorer.
constexpr NlpWordValence kNlpFinancePositive[] = {
    {"surge", 3.0},   {"soar", 3.2},     {"skyrocket", 3.5},  {"breakthrough", 3.0}, {"boom", 2.8},
    {"rally", 2.5},   {"gain", 2.0},     {"gains", 2.0},      {"rise", 1.8},         {"jump", 2.5},
    {"jumps", 2.5},   {"climb", 2.0},    {"rebound", 2.2},    {"boost", 2.0},        {"beat", 2.2},
    {"beats", 2.2},   {"exceed", 2.0},   {"outperform", 2.5}, {"upgrade", 2.5},      {"profit", 1.8},
    {"growth", 1.8},  {"recover", 2.0},  {"record", 1.5},     {"strong", 1.5},       {"robust", 1.5},
    {"bullish", 3.0}, {"optimism", 1.8}, {"milestone", 1.5},  {"approval", 1.8},     {"deal", 1.2},
};
constexpr NlpWordValence kNlpFinanceNegative[] = {
    {"crash", -3.5},     {"plunge", -3.2}, {"collapse", -3.5}, {"meltdown", -3.5},  {"bankruptcy", -3.8},
    {"fall", -1.8},      {"drop", -2.0},   {"decline", -2.0},  {"tumble", -2.5},    {"slump", -2.5},
    {"miss", -2.0},      {"misses", -2.0}, {"fail", -2.2},     {"recession", -2.8}, {"crisis", -2.5},
    {"sanction", -2.0},  {"tariff", -1.8}, {"escalat", -1.5},  {"layoff", -2.2},    {"layoffs", -2.2},
    {"downgrade", -2.5}, {"fraud", -3.5},  {"scandal", -2.8},  {"selloff", -2.5},   {"default", -2.5},
    {"lawsuit", -1.8},   {"probe", -1.5},  {"weak", -1.5},     {"loss", -1.5},      {"deficit", -1.2},
    {"fear", -1.5},      {"threat", -1.5}, {"warning", -1.5},  {"bearish", -3.0},   {"halt", -1.5},
};

// Inflected forms of the words above, laid over VADER's lexicon ONLY. VADER matches whole
// tokens, so "Shares plunged" scored 0 while "plunge" scored -3.2 — headlines are mostly in
// the past tense / third person. Kept out of the keyword-signal tables: their substring
// match already counts "plunge" inside "plunged", and listing both would double it.
constexpr NlpWordValence kNlpFinanceInflections[] = {
    {"surges", 3.0},      {"surged", 3.0},      {"surging", 3.0},     {"soars", 3.2},        {"soared", 3.2},
    {"soaring", 3.2},     {"rallies", 2.5},     {"rallied", 2.5},     {"rallying", 2.5},     {"gained", 2.0},
    {"rises", 1.8},       {"rose", 1.8},        {"rising", 1.8},      {"jumped", 2.5},       {"climbs", 2.0},
    {"climbed", 2.0},     {"rebounds", 2.2},    {"rebounded", 2.2},   {"boosts", 2.0},       {"boosted", 2.0},
    {"exceeds", 2.0},     {"exceeded", 2.0},    {"outperforms", 2.5}, {"outperformed", 2.5}, {"upgrades", 2.5},
    {"upgraded", 2.5},    {"profits", 1.8},     {"recovers", 2.0},    {"recovered", 2.0},    {"recovery", 2.0},
    {"crashes", -3.5},    {"crashed", -3.5},    {"plunges", -3.2},    {"plunged", -3.2},     {"plunging", -3.2},
    {"collapses", -3.5},  {"collapsed", -3.5},  {"falls", -1.8},      {"fell", -1.8},        {"falling", -1.8},
    {"drops", -2.0},      {"dropped", -2.0},    {"declines", -2.0},   {"declined", -2.0},    {"declining", -2.0},
    {"tumbles", -2.5},    {"tumbled", -2.5},    {"slumps", -2.5},     {"slumped", -2.5},     {"missed", -2.0},
    {"downgrades", -2.5}, {"downgraded", -2.5}, {"sanctions", -2.0},  {"tariffs", -1.8},     {"escalates", -1.5},
    {"escalated", -1.5},  {"escalation", -1.5}, {"defaults", -2.5},   {"defaulted", -2.5},   {"lawsuits", -1.8},
    {"losses", -1.5},     {"warns", -1.5},      {"warned", -1.5},     {"halts", -1.5},       {"halted", -1.5},
    {"bankrupt", -3.8},   {"sell-off", -2.5},   {"selloffs", -2.5},   {"recessions", -2.8},
};

// Major cities with known coordinates, checked before countries.
constexpr NlpPlace kGeoCities[] = {
    {"new york", 40.7128, -74.006},
    {"washington", 38.9072, -77.0369},
    {"london", 51.5074, -0.1278},
    {"paris", 48.8566, 2.3522},
    {"berlin", 52.52, 13.405},
    {"moscow", 55.7558, 37.6173},
    {"beijing", 39.9042, 116.4074},
    {"tokyo", 35.6762, 139.6503},
    {"mumbai", 19.076, 72.8777},
    {"delhi", 28.6139, 77.209},
    {"shanghai", 31.2304, 121.4737},
    {"hong kong", 22.3193, 114.1694},
    {"singapore", 1.3521, 103.8198},
    {"dubai", 25.2048, 55.2708},
    {"sydney", -33.8688, 151.2093},
    {"toronto", 43.6532, -79.3832},
    {"seoul", 37.5665, 126.978},
    {"taipei", 25.033, 121.5654},
    {"istanbul", 41.0082, 28.9784},
    {"cairo", 30.0444, 31.2357},
    {"tel aviv", 32.0853, 34.7818},
    {"jerusalem", 31.7683, 35.2137},
    {"riyadh", 24.7136, 46.6753},
    {"tehran", 35.6892, 51.389},
    {"baghdad", 33.3152, 44.3661},
    {"damascus", 33.5138, 36.2765},
    {"kabul", 34.5553, 69.2075},
    {"kyiv", 50.4501, 30.5234},
    {"kiev", 50.4501, 30.5234},
    {"brussels", 50.8503, 4.3517},
    {"geneva", 46.2044, 6.1432},
    {"zurich", 47.3769, 8.5417},
    {"rome", 41.9028, 12.4964},
    {"madrid", 40.4168, -3.7038},
    {"lisbon", 38.7223, -9.1393},
    {"amsterdam", 52.3676, 4.9041},
    {"stockholm", 59.3293, 18.0686},
    {"oslo", 59.9139, 10.7522},
    {"warsaw", 52.2297, 21.0122},
    {"vienna", 48.2082, 16.3738},
    {"athens", 37.9838, 23.7275},
    {"ankara", 39.9334, 32.8597},
    {"buenos aires", -34.6037, -58.3816},
    {"sao paulo", -23.5505, -46.6333},
    {"mexico city", 19.4326, -99.1332},
    {"bogota", 4.711, -74.0721},
    {"lagos", 6.5244, 3.3792},
    {"nairobi", -1.2921, 36.8219},
    {"johannesburg", -26.2041, 28.0473},
    {"addis ababa", 9.0192, 38.7525},
    {"khartoum", 15.5007, 32.5599},
    {"beirut", 33.8938, 35.5018},
    {"gaza", 31.5, 34.47},
    {"ramallah", 31.9038, 35.2034},
    {"pyongyang", 39.0392, 125.7625},
    {"hanoi", 21.0285, 105.8542},
    {"bangkok", 13.7563, 100.5018},
    {"jakarta", -6.2088, 106.8456},
    {"manila", 14.5995, 120.9842},
    {"kuala lumpur", 3.139, 101.6869},
};

// Country centroids by ISO code.
constexpr NlpPlace kGeoCountryCentroids[] = {
    {"US", 38.0, -97.0},  {"CN", 35.0, 105.0},  {"RU", 60.0, 100.0}, {"UA", 49.0, 32.0},  {"GB", 54.0, -2.0},
    {"DE", 51.0, 9.0},    {"FR", 46.0, 2.0},    {"JP", 36.0, 138.0}, {"IN", 20.0, 77.0},  {"BR", -14.0, -51.0},
    {"CA", 56.0, -106.0}, {"AU", -25.0, 133.0}, {"KR", 36.0, 128.0}, {"KP", 40.0, 127.0}, {"IR", 32.0, 53.0},
    {"IL", 31.0, 34.8},   {"PS", 31.5, 34.5},   {"SA", 24.0, 45.0},  {"TR", 39.0, 35.0},  {"EG", 27.0, 30.0},
    {"IQ", 33.0, 44.0},   {"SY", 35.0, 38.0},   {"AF", 33.0, 65.0},  {"YE", 15.0, 48.0},  {"LY", 27.0, 17.0},
    {"LB", 34.0, 36.0},   {"SD", 13.0, 30.0},   {"SO", 6.0, 46.0},   {"MM", 22.0, 98.0},  {"ET", 9.0, 40.0},
    {"VE", 7.0, -66.0},   {"CU", 22.0, -80.0},
};

// Country names that place an article; only codes with a centroid ever match.
constexpr NlpAliasCode kGeoCountryNames[] = {
    {"united states", "US"}, {"china", "CN"},       {"russia", "RU"},      {"ukraine", "UA"}, {"united kingdom", "GB"},
    {"germany", "DE"},       {"france", "FR"},      {"japan", "JP"},       {"india", "IN"},   {"brazil", "BR"},
    {"iran", "IR"},          {"israel", "IL"},      {"palestine", "PS"},   {"gaza", "PS"},    {"saudi arabia", "SA"},
    {"turkey", "TR"},        {"egypt", "EG"},       {"iraq", "IQ"},        {"syria", "SY"},   {"afghanistan", "AF"},
    {"yemen", "YE"},         {"libya", "LY"},       {"lebanon", "LB"},     {"sudan", "SD"},   {"somalia", "SO"},
    {"myanmar", "MM"},       {"north korea", "KP"}, {"south korea", "KR"}, {"taiwan", "TW"},  {"hong kong", "HK"},
    {"singapore", "SG"},
};

constexpr double kNlpDegToRad = 3.14159265358979323846 / 180.0; // as math.radians()
constexpr qsizetype kNlpMaxPerArticle = 5;
constexpr qsizetype kNlpMaxInfrastructure = 15;

// ── Text helpers ──

/// str.title() for the ASCII table entries ("united states" -> "United States").
QString nlp_title_case(const QString& s) {
    QString out = s;
    bool prev_cased = false;
    for (QChar& c : out) {
        const bool letter =
            (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'));
        if (letter)
            c = prev_cased ? c.toLower() : c.toUpper();
        prev_cased = letter;
    }
    return out;
}

bool nlp_is_lower_alnum(QChar c) {
    return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
}

bool nlp_is_alnum(QChar c) {
    return nlp_is_lower_alnum(c) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'));
}

bool nlp_is_upper_ascii(QChar c) {
    return c >= QLatin1Char('A') && c <= QLatin1Char('Z');
}

/// An occurrence of `needle` not touching a `blocking` character on either side — the
/// `(?<![a-z0-9])term(?![a-z0-9])` matchers. `needle` is ASCII, so comparing UTF-16
/// units (a surrogate is never ASCII) is the same as comparing code points.
bool nlp_find_bounded(const QString& text, const QString& needle, bool (*blocking)(QChar)) {
    for (qsizetype from = 0;;) {
        const qsizetype pos = text.indexOf(needle, from);
        if (pos < 0)
            return false;
        const qsizetype end = pos + needle.size();
        if ((pos == 0 || !blocking(text[pos - 1])) && (end == text.size() || !blocking(text[end])))
            return true;
        from = pos + 1;
    }
}

/// `list(dict.fromkeys(values))[:limit]`.
QStringList nlp_first_unique(const QStringList& values, qsizetype limit) {
    QStringList out;
    for (const QString& v : values) {
        if (out.size() >= limit)
            break;
        if (!out.contains(v))
            out.append(v);
    }
    return out;
}

// ── Entity tables ──

struct NlpCountryMatcher {
    QString alias;
    QString code;
    QString name;   // the code's first alias, title-cased
    bool substring; // multi-word aliases are specific enough for a plain substring test
};

const QVector<NlpCountryMatcher>& nlp_country_matchers() {
    static const QVector<NlpCountryMatcher> matchers = [] {
        QHash<QString, QString> first_alias;
        for (const auto& row : kNlpCountries) {
            const QString code = QString::fromLatin1(row.code);
            if (!first_alias.contains(code))
                first_alias.insert(code, QString::fromLatin1(row.alias));
        }
        QVector<NlpCountryMatcher> out;
        for (const auto& row : kNlpCountries) {
            const QString alias = QString::fromLatin1(row.alias);
            const QString code = QString::fromLatin1(row.code);
            out.append({alias, code, nlp_title_case(first_alias.value(code)), alias.contains(QLatin1Char(' '))});
        }
        return out;
    }();
    return matchers;
}

struct NlpTermMatcher {
    QString term;        // whole word in the lower-cased text
    QString capitalized; // term.capitalize() — proper-noun form
    QString upper;       // term.upper()
    QString name;
    bool proper_noun; // short/ambiguous: only Capitalised or UPPER in the original text
};

template <std::size_t N>
QVector<NlpTermMatcher> nlp_term_matchers(const NlpTermName (&rows)[N], qsizetype proper_noun_max_len) {
    QVector<NlpTermMatcher> out;
    for (const auto& row : rows) {
        const QString term = QString::fromLatin1(row.term);
        out.append({term, term.left(1).toUpper() + term.mid(1).toLower(), term.toUpper(), QString::fromLatin1(row.name),
                    term.size() <= proper_noun_max_len});
    }
    return out;
}

const QVector<NlpTermMatcher>& nlp_org_matchers() {
    static const QVector<NlpTermMatcher> matchers = nlp_term_matchers(kNlpOrganizations, kNlpOrgProperNounMaxLen);
    return matchers;
}

const QVector<NlpTermMatcher>& nlp_person_matchers() {
    static const QVector<NlpTermMatcher> matchers = nlp_term_matchers(kNlpKnownPersons, kNlpPersonProperNounMaxLen);
    return matchers;
}

bool nlp_term_hit(const NlpTermMatcher& m, const QString& text_orig, const QString& text_lower) {
    if (!m.proper_noun)
        return nlp_find_bounded(text_lower, m.term, nlp_is_lower_alnum);
    return nlp_find_bounded(text_orig, m.capitalized, nlp_is_alnum) ||
           nlp_find_bounded(text_orig, m.upper, nlp_is_alnum);
}

const QSet<QString>& nlp_ticker_stopwords() {
    static const QSet<QString> words = [] {
        QSet<QString> s;
        for (const char* w : kNlpTickerStopwords)
            s.insert(QString::fromLatin1(w));
        return s;
    }();
    return words;
}

// ── Person titles ──

/// End of `[A-Z][a-z]+` starting at `at`, or -1.
qsizetype nlp_capitalized_word_end(const QString& s, qsizetype at) {
    if (at >= s.size() || !nlp_is_upper_ascii(s[at]))
        return -1;
    qsizetype j = at + 1;
    while (j < s.size() && s[j] >= QLatin1Char('a') && s[j] <= QLatin1Char('z'))
        ++j;
    return j > at + 1 ? j : -1;
}

/// End of the whitespace run starting at `at` (`at` itself when there is none).
qsizetype nlp_skip_space(const QString& s, qsizetype at) {
    while (at < s.size() && pycompat::is_space(s[at].unicode()))
        ++at;
    return at;
}

/// re.finditer of `\b(<titles>)<"." if dotted>\s+([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)` over
/// `s`, appending group 2 of every match to `names`.
template <std::size_t N>
void nlp_titled_names(const QString& s, const char* const (&titles)[N], bool dotted, QStringList& names) {
    const qsizetype n = s.size();
    qsizetype p = 0;
    while (p < n) {
        qsizetype match_end = -1;
        // Every title starts with a lower-case letter: a match needs one here, at a
        // word boundary.
        if (s[p] >= QLatin1Char('a') && s[p] <= QLatin1Char('z') &&
            (p == 0 || !pycompat::is_word_char(pycompat::cp_before(s, p)))) {
            for (const char* title : titles) {
                const QLatin1StringView t(title);
                if (!QStringView(s).sliced(p).startsWith(t))
                    continue;
                qsizetype q = p + t.size();
                if (dotted) {
                    if (q >= n || s[q] != QLatin1Char('.'))
                        continue;
                    ++q;
                }
                const qsizetype name_start = nlp_skip_space(s, q);
                if (name_start == q)
                    continue;
                qsizetype end = nlp_capitalized_word_end(s, name_start);
                if (end < 0)
                    continue;
                for (;;) { // (?:\s+[A-Z][a-z]+)* — greedy
                    const qsizetype next = nlp_skip_space(s, end);
                    const qsizetype next_end = next > end ? nlp_capitalized_word_end(s, next) : -1;
                    if (next_end < 0)
                        break;
                    end = next_end;
                }
                names.append(s.mid(name_start, end - name_start));
                match_end = end;
                break;
            }
        }
        p = match_end > p ? match_end : p + 1;
    }
}

// ── Tickers ──

/// Cashtags (`\$([A-Z]{1,5})\b`) then bare 2-5 letter capital words (`\b[A-Z]{2,5}\b`)
/// minus stop words; bare words are skipped when the text is shouted in capitals.
QStringList nlp_extract_tickers(const QString& s) {
    QStringList found;
    const qsizetype n = s.size();
    for (qsizetype p = 0; p < n;) {
        if (s[p] == QLatin1Char('$')) {
            qsizetype j = p + 1;
            while (j < n && nlp_is_upper_ascii(s[j]))
                ++j;
            const qsizetype len = j - (p + 1);
            if (len >= 1 && len <= 5 && (j == n || !pycompat::is_word_char(pycompat::cp_at(s, j)))) {
                found.append(s.mid(p + 1, len));
                p = j;
                continue;
            }
        }
        ++p;
    }

    qsizetype letters = 0;
    qsizetype upper = 0;
    for (qsizetype i = 0; i < n;) {
        const char32_t c = pycompat::cp_at(s, i);
        i += c > 0xFFFF ? 2 : 1;
        if (pycompat::is_alpha(c)) {
            ++letters;
            if (pycompat::is_upper_char(c))
                ++upper;
        }
    }
    if (letters >= 24 && upper * 10 >= letters * 6)
        return found;

    const auto& stopwords = nlp_ticker_stopwords();
    for (qsizetype p = 0; p < n;) {
        if (!nlp_is_upper_ascii(s[p]) || (p > 0 && pycompat::is_word_char(pycompat::cp_before(s, p)))) {
            ++p;
            continue;
        }
        // A capital run at a word start: it is a match only as a whole word of 2-5
        // letters (no match can start inside the run).
        qsizetype j = p;
        while (j < n && nlp_is_upper_ascii(s[j]))
            ++j;
        const qsizetype len = j - p;
        if (len >= 2 && len <= 5 && (j == n || !pycompat::is_word_char(pycompat::cp_at(s, j)))) {
            const QString word = s.mid(p, len);
            if (!stopwords.contains(word))
                found.append(word);
        }
        p = j;
    }
    return found;
}

// ── Geolocation ──

struct NlpGeoMatcher {
    QString key; // lower-case name matched as a whole word
    QString name;
    QString code; // empty for cities
    double lat;
    double lon;
};

const QVector<NlpGeoMatcher>& nlp_city_matchers() {
    static const QVector<NlpGeoMatcher> matchers = [] {
        QVector<NlpGeoMatcher> out;
        for (const auto& row : kGeoCities) {
            const QString key = QString::fromLatin1(row.key);
            out.append({key, nlp_title_case(key), QString(), row.lat, row.lon});
        }
        return out;
    }();
    return matchers;
}

const QVector<NlpGeoMatcher>& nlp_country_name_matchers() {
    static const QVector<NlpGeoMatcher> matchers = [] {
        QVector<NlpGeoMatcher> out;
        for (const auto& row : kGeoCountryNames) {
            for (const auto& centroid : kGeoCountryCentroids) {
                if (qstrcmp(centroid.key, row.code) != 0)
                    continue;
                const QString key = QString::fromLatin1(row.alias);
                out.append({key, nlp_title_case(key), QString::fromLatin1(row.code), centroid.lat, centroid.lon});
                break;
            }
        }
        return out;
    }();
    return matchers;
}

struct NlpKeyedLocation {
    QString key;
    GeoLocation location;
};

/// Every known place in `text` (already lower-cased): cities first, then countries
/// not already named by a city of the same name.
QVector<NlpKeyedLocation> nlp_locate(const QString& text) {
    QVector<NlpKeyedLocation> found;
    for (const auto& m : nlp_city_matchers()) {
        if (nlp_find_bounded(text, m.key, nlp_is_lower_alnum))
            found.append({m.key, {m.name, QString(), m.lat, m.lon, QStringLiteral("city")}});
    }
    for (const auto& m : nlp_country_name_matchers()) {
        if (!nlp_find_bounded(text, m.key, nlp_is_lower_alnum))
            continue;
        const bool named = std::any_of(found.cbegin(), found.cend(), [&m](const NlpKeyedLocation& l) {
            return pycompat::str_lower(l.location.name) == m.key;
        });
        if (!named)
            found.append({m.key, {m.name, m.code, m.lat, m.lon, QStringLiteral("country")}});
    }
    return found;
}

QJsonObject nlp_location_json(const GeoLocation& loc) {
    QJsonObject obj{{QStringLiteral("name"), loc.name},
                    {QStringLiteral("lat"), loc.lat},
                    {QStringLiteral("lon"), loc.lon},
                    {QStringLiteral("type"), loc.type}};
    if (!loc.code.isEmpty())
        obj.insert(QStringLiteral("code"), loc.code);
    return obj;
}

// ── Sentiment ──

struct NlpKeywordWeight {
    QString word;
    int weight;
};

/// Integer weights `max(1, round(|valence| / 1.6))`.
template <std::size_t N>
QVector<NlpKeywordWeight> nlp_keyword_weights(const NlpWordValence (&rows)[N]) {
    QVector<NlpKeywordWeight> out;
    for (const auto& row : rows) {
        const double scaled = std::fabs(row.valence) / 1.6;
        out.append({QString::fromLatin1(row.word), std::max(1, static_cast<int>(pycompat::round_half_even(scaled)))});
    }
    return out;
}

const QVector<NlpKeywordWeight>& nlp_positive_keywords() {
    static const QVector<NlpKeywordWeight> weights = nlp_keyword_weights(kNlpFinancePositive);
    return weights;
}

const QVector<NlpKeywordWeight>& nlp_negative_keywords() {
    static const QVector<NlpKeywordWeight> weights = nlp_keyword_weights(kNlpFinanceNegative);
    return weights;
}

/// Summed weights of the keywords contained (as substrings) in the lower-cased text.
int nlp_keyword_signal(const QString& text, const QVector<NlpKeywordWeight>& keywords) {
    int total = 0;
    for (const auto& k : keywords) {
        if (text.contains(k.word))
            total += k.weight;
    }
    return total;
}

const VaderSentiment& nlp_finance_vader() {
    static const VaderSentiment analyzer = [] {
        QHash<QString, double> overrides;
        for (const auto& row : kNlpFinancePositive)
            overrides.insert(QString::fromLatin1(row.word), row.valence);
        for (const auto& row : kNlpFinanceNegative)
            overrides.insert(QString::fromLatin1(row.word), row.valence);
        for (const auto& row : kNlpFinanceInflections)
            overrides.insert(QString::fromLatin1(row.word), row.valence);
        return VaderSentiment(overrides);
    }();
    return analyzer;
}

QString nlp_label_from_score(double score) {
    if (score >= 0.15)
        return QStringLiteral("BULLISH");
    if (score <= -0.15)
        return QStringLiteral("BEARISH");
    return QStringLiteral("NEUTRAL");
}

// ── Network payloads ──

/// Python's repr() of a float, which the Overpass query embeds.
QString nlp_py_float_repr(double x) {
    if (std::isnan(x))
        return QStringLiteral("nan");
    if (std::isinf(x))
        return x > 0 ? QStringLiteral("inf") : QStringLiteral("-inf");
    if (x == 0.0) // QString::number() drops the sign of -0.0; repr() keeps it
        return std::signbit(x) ? QStringLiteral("-0.0") : QStringLiteral("0.0");
    const double ax = std::fabs(x);
    if (ax < 1e-4 || ax >= 1e16)
        return QString::number(x, 'e', QLocale::FloatingPointShortest);
    QString s = QString::number(x, 'f', QLocale::FloatingPointShortest);
    if (!s.contains(QLatin1Char('.')))
        s += QLatin1String(".0");
    return s;
}

/// urllib.parse.quote_plus().
QString nlp_form_encode(const QString& value) {
    return QString::fromLatin1(QUrl::toPercentEncoding(value).replace("%20", "+"));
}

/// Python truthiness of a JSON value.
bool nlp_json_truthy(const QJsonValue& v) {
    switch (v.type()) {
        case QJsonValue::Bool:
            return v.toBool();
        case QJsonValue::Double:
            return v.toDouble() != 0.0;
        case QJsonValue::String:
            return !v.toString().isEmpty();
        case QJsonValue::Array:
            return !v.toArray().isEmpty();
        case QJsonValue::Object:
            return !v.toObject().isEmpty();
        default:
            return false;
    }
}

/// `elem.get(key) or elem.get("center", {}).get(key)`; nullopt where that raises.
std::optional<QJsonValue> nlp_element_coordinate(const QJsonObject& elem, QLatin1StringView key) {
    const QJsonValue direct = elem.value(key);
    if (nlp_json_truthy(direct))
        return direct;
    const QJsonValue center = elem.value(QLatin1String("center"));
    if (center.isUndefined())
        return QJsonValue();
    if (!center.isObject())
        return std::nullopt;
    return center.toObject().value(key);
}

} // namespace nlp_engine_detail

using namespace nlp_engine_detail;

EntityResult extract_entities(const TextItem& item) {
    const QString text_orig = item.headline + QLatin1Char(' ') + item.summary;
    const QString text = pycompat::str_lower(text_orig);

    EntityResult out;
    out.id = item.id;

    // Countries — the first alias that hits keeps the code; at most 5 codes.
    QSet<QString> codes;
    for (const auto& m : nlp_country_matchers()) {
        if (out.countries.size() >= kNlpMaxPerArticle)
            break;
        if (codes.contains(m.code))
            continue;
        if (m.substring ? text.contains(m.alias) : nlp_find_bounded(text, m.alias, nlp_is_lower_alnum)) {
            codes.insert(m.code);
            out.countries.append({m.name, m.code});
        }
    }

    QStringList orgs;
    for (const auto& m : nlp_org_matchers()) {
        if (nlp_term_hit(m, text_orig, text))
            orgs.append(m.name);
    }
    out.organizations = nlp_first_unique(orgs, kNlpMaxPerArticle);

    QStringList people;
    for (const auto& m : nlp_person_matchers()) {
        if (nlp_term_hit(m, text_orig, text))
            people.append(m.name);
    }
    QStringList titled;
    nlp_titled_names(text_orig, kNlpTitlesHead, false, titled);
    nlp_titled_names(text_orig, kNlpTitlesOffice, false, titled);
    nlp_titled_names(text_orig, kNlpTitlesHonorific, true, titled);
    for (const QString& name : titled) {
        if (pycompat::cp_length(name) > 3 && !people.contains(name))
            people.append(name);
    }
    out.people = nlp_first_unique(people, kNlpMaxPerArticle);

    out.tickers = nlp_first_unique(nlp_extract_tickers(text_orig), kNlpMaxPerArticle);
    return out;
}

std::optional<ArticleGeo> geocode(const TextItem& item) {
    const QVector<NlpKeyedLocation> found =
        nlp_locate(pycompat::str_lower(item.headline + QLatin1Char(' ') + item.summary));
    if (found.isEmpty())
        return std::nullopt;
    ArticleGeo geo;
    geo.id = item.id;
    for (qsizetype i = 0; i < std::min(found.size(), kNlpMaxPerArticle); ++i)
        geo.locations.append(found[i].location);
    geo.primary_lat = found.first().location.lat;
    geo.primary_lon = found.first().location.lon;
    return geo;
}

NewsSentimentResult analyze_sentiment(const QVector<TextItem>& items) {
    const VaderSentiment& vader = nlp_finance_vader();
    const bool use_vader = vader.is_loaded();

    NewsSentimentResult out;
    out.engine = use_vader ? QStringLiteral("vader") : QStringLiteral("lexicon");
    out.per_article.reserve(items.size());
    double score_sum = 0.0;
    for (const auto& item : items) {
        const QString text = pycompat::str_lower(item.headline + QLatin1Char(' ') + item.summary);
        const int pos_score = nlp_keyword_signal(text, nlp_positive_keywords());
        const int neg_score = nlp_keyword_signal(text, nlp_negative_keywords());
        const int total = pos_score + neg_score;

        // Each product is its own statement so no compiler fuses it into a following
        // add (fma) — the scores must match the reference arithmetic bit for bit.
        QString label;
        double score = 0.0;
        double confidence = 0.0;
        if (use_vader) {
            score = vader.polarity_scores(text).compound; // -1..1
            label = nlp_label_from_score(score);
            if (label == QLatin1String("NEUTRAL")) {
                const double scaled = std::fabs(score) * 0.8;
                const double raw = 0.2 + scaled;
                confidence = pycompat::round_digits(std::min(raw, 0.4), 3);
            } else {
                const double scaled = std::fabs(score) * 0.45;
                const double base = 0.45 + scaled;
                const double signal = static_cast<double>(total) * 0.02;
                const double raw = base + signal;
                confidence = pycompat::round_digits(std::min(raw, 0.97), 3);
            }
        } else {
            // Keyword fallback scorer.
            const int net = pos_score - neg_score;
            const double ratio = static_cast<double>(net) / static_cast<double>(std::max(total, 1));
            const double signal = static_cast<double>(total) * 0.05;
            const double raw = 0.4 + signal;
            if (total == 0) {
                label = QStringLiteral("NEUTRAL");
                score = 0.0;
                confidence = 0.2;
            } else if (net >= 2) {
                label = QStringLiteral("BULLISH");
                score = std::min(ratio, 1.0);
                confidence = std::min(raw, 0.95);
            } else if (net <= -2) {
                label = QStringLiteral("BEARISH");
                score = std::max(ratio, -1.0);
                confidence = std::min(raw, 0.95);
            } else {
                label = QStringLiteral("NEUTRAL");
                score = ratio;
                confidence = 0.3;
            }
        }

        ArticleSentimentResult r;
        r.id = item.id;
        r.label = label;
        r.score = pycompat::round_digits(score, 3);
        r.confidence = pycompat::round_digits(confidence, 3);
        if (r.label == QLatin1String("BULLISH"))
            ++out.bullish;
        else if (r.label == QLatin1String("BEARISH"))
            ++out.bearish;
        else
            ++out.neutral;
        score_sum += r.score;
        out.per_article.append(r);
    }
    const double mean = score_sum / static_cast<double>(std::max<qsizetype>(items.size(), 1));
    out.overall_score = pycompat::round_digits(mean, 3);
    return out;
}

QString detect_language(const QString& text) {
    if (text.isEmpty())
        return QStringLiteral("en");
    qsizetype cjk = 0;
    qsizetype kana = 0;
    qsizetype hangul = 0;
    qsizetype arabic = 0;
    qsizetype cyrillic = 0;
    qsizetype devanagari = 0;
    qsizetype total = 0;
    for (qsizetype i = 0; i < text.size();) {
        const char32_t c = pycompat::cp_at(text, i);
        i += c > 0xFFFF ? 2 : 1;
        ++total;
        if (c >= 0x4E00 && c <= 0x9FFF)
            ++cjk;
        if (c >= 0x3040 && c <= 0x30FF)
            ++kana;
        if (c >= 0xAC00 && c <= 0xD7AF)
            ++hangul;
        if (c >= 0x0600 && c <= 0x06FF)
            ++arabic;
        if (c >= 0x0400 && c <= 0x04FF)
            ++cyrillic;
        if (c >= 0x0900 && c <= 0x097F)
            ++devanagari;
    }
    const double n = static_cast<double>(total);
    if (static_cast<double>(cjk) / n > 0.1)
        return QStringLiteral("zh");
    if (static_cast<double>(kana) / n > 0.1)
        return QStringLiteral("ja");
    if (static_cast<double>(hangul) / n > 0.1)
        return QStringLiteral("ko");
    if (static_cast<double>(arabic) / n > 0.1)
        return QStringLiteral("ar");
    if (static_cast<double>(cyrillic) / n > 0.1)
        return QStringLiteral("ru");
    if (static_cast<double>(devanagari) / n > 0.1)
        return QStringLiteral("hi");
    return QStringLiteral("en");
}

std::optional<Translation> translate_without_request(const QString& text, const QString& source,
                                                     const QString& target) {
    if (text.isEmpty() || pycompat::str_strip(text).isEmpty())
        return Translation{true, text, QStringLiteral("en")};
    const QString detected = detect_language(text);
    if (source == target)
        return Translation{true, text, detected};
    // The script guess only separates NON-Latin scripts — every Latin-script text reads
    // as "en", so for an "en" target the translator has to decide.
    if (detected == target && detected != QLatin1String("en"))
        return Translation{true, text, detected};
    return std::nullopt;
}

QString gtx_translate_url(const QString& source, const QString& target) {
    return QStringLiteral("https://translate.googleapis.com/translate_a/single?client=gtx&dt=t&sl=") +
           nlp_form_encode(source.isEmpty() ? QStringLiteral("auto") : source) + QStringLiteral("&tl=") +
           nlp_form_encode(target);
}

std::optional<QPair<QString, QString>> parse_gtx_response(const QJsonDocument& doc) {
    // [[["<translated>", "<original>", ...], ...], null, "<source lang>", ...]
    if (!doc.isArray() || doc.array().isEmpty() || !doc.array().at(0).isArray())
        return std::nullopt;
    const QJsonArray data = doc.array();
    QString translated;
    for (const QJsonValue& seg : data.at(0).toArray()) {
        if (!nlp_json_truthy(seg))
            continue;
        if (seg.isString()) { // seg[0] of a string is its first character
            const QString s = seg.toString();
            translated += s.left(pycompat::cp_at(s, 0) > 0xFFFF ? 2 : 1);
            continue;
        }
        if (!seg.isArray())
            return std::nullopt;
        const QJsonValue first = seg.toArray().at(0);
        if (!nlp_json_truthy(first))
            continue;
        if (!first.isString())
            return std::nullopt;
        translated += first.toString();
    }
    const QString detected = data.size() > 2 && data.at(2).isString() ? data.at(2).toString() : QStringLiteral("auto");
    return QPair<QString, QString>{translated, detected};
}

QString overpass_query(double lat, double lon, qint64 radius_km) {
    const QString around = QStringLiteral("(around:") + QString::number(radius_km * 1000) + QLatin1Char(',') +
                           nlp_py_float_repr(lat) + QLatin1Char(',') + nlp_py_float_repr(lon) + QStringLiteral(");\n");
    return QStringLiteral("\n    [out:json][timeout:10];\n    (\n") +
           QStringLiteral("      node[\"aeroway\"=\"aerodrome\"]") + around +
           QStringLiteral("      node[\"military\"]") + around + QStringLiteral("      node[\"power\"=\"plant\"]") +
           around + QStringLiteral("      node[\"harbour\"]") + around +
           QStringLiteral("      way[\"aeroway\"=\"aerodrome\"]") + around + QStringLiteral("      way[\"military\"]") +
           around + QStringLiteral("    );\n    out center 20;\n    ");
}

std::optional<QVector<InfrastructureItem>> parse_overpass_response(const QJsonDocument& doc, double lat, double lon) {
    if (!doc.isObject())
        return std::nullopt;
    const QJsonValue elements = doc.object().value(QLatin1String("elements"));
    QJsonArray list;
    if (elements.isArray())
        list = elements.toArray();
    else if (!elements.isUndefined() && !(elements.isObject() && elements.toObject().isEmpty()))
        return std::nullopt;

    QVector<InfrastructureItem> items;
    for (const QJsonValue& v : list) {
        if (!v.isObject())
            return std::nullopt;
        const QJsonObject elem = v.toObject();
        const QJsonValue tags_value = elem.value(QLatin1String("tags"));
        if (!tags_value.isUndefined() && !tags_value.isObject())
            return std::nullopt;
        const QJsonObject tags = tags_value.toObject();

        // Nodes carry lat/lon; ways carry a "center" (out center).
        const auto e_lat = nlp_element_coordinate(elem, QLatin1StringView("lat"));
        const auto e_lon = nlp_element_coordinate(elem, QLatin1StringView("lon"));
        if (!e_lat || !e_lon)
            return std::nullopt;
        if (!nlp_json_truthy(*e_lat) || !nlp_json_truthy(*e_lon))
            continue; // as before, a coordinate of exactly 0 counts as missing
        if (!e_lat->isDouble() || !e_lon->isDouble())
            return std::nullopt;
        const double elat = e_lat->toDouble();
        const double elon = e_lon->toDouble();

        const QJsonValue name_value = tags.value(QLatin1String("name"));
        InfrastructureItem item;
        item.name = name_value.isUndefined() ? QStringLiteral("Unknown") : name_value.toString();
        item.type = QStringLiteral("unknown");
        if (tags.contains(QLatin1String("aeroway")))
            item.type = QStringLiteral("airport");
        else if (tags.contains(QLatin1String("military")))
            item.type = QStringLiteral("military");
        else if (tags.contains(QLatin1String("power")))
            item.type = QStringLiteral("power_plant");
        else if (tags.contains(QLatin1String("harbour")))
            item.type = QStringLiteral("port");
        item.lat = elat;
        item.lon = elon;

        // Haversine, R = 6371 km; one operation per statement (see analyze_sentiment).
        const double dlat = (elat - lat) * kNlpDegToRad;
        const double dlon = (elon - lon) * kNlpDegToRad;
        const double cos_product = std::cos(lat * kNlpDegToRad) * std::cos(elat * kNlpDegToRad);
        const double lon_term = cos_product * std::pow(std::sin(dlon / 2), 2.0);
        const double a = std::pow(std::sin(dlat / 2), 2.0) + lon_term;
        const double central_angle = std::atan2(std::sqrt(a), std::sqrt(1 - a));
        const double distance_km = 12742.0 * central_angle;
        item.distance_km = pycompat::round_digits(distance_km, 1);
        items.append(item);
    }
    std::stable_sort(items.begin(), items.end(), [](const InfrastructureItem& x, const InfrastructureItem& y) {
        return x.distance_km < y.distance_km;
    });
    if (items.size() > kNlpMaxInfrastructure)
        items.resize(kNlpMaxInfrastructure);
    return items;
}

} // namespace fincept::services::news_nlp
