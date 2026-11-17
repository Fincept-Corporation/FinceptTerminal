#pragma once
// src/services/news/NewsClassify.h
//
// Pure, header-only keyword rules for one news item: priority (FLASH / URGENT /
// BREAKING), market impact, the market-impact event type and the threat class.
// Qt Core only (no network, no widgets, no database), so it is unit-tested on
// its own (tests/tst_news_classify.cpp) and runs on worker threads.
//
// ── Rules ──────────────────────────────────────────────────────────────────
// Priority and market impact are market judgements. They need MARKET CONTEXT:
// a validated ticker tag, or in the HEADLINE a finance / market term (whole
// words — "stocks", "central bank", "bond yields", "OPEC") or a money amount
// ($2bn, 5 billion). An event named only in the summary counts only when the
// headline has market context.
// Generic words never colour a story on their own: an "emergency" in a road-
// safety report, a "breaking" in "record-breaking", a police "probe", a
// motorist "fined", an Israeli "settlement" or a "land acquisition" used to
// make ordinary stories URGENT / BREAKING / market impact HIGH.
//  * FLASH    — the publisher marks the headline breaking / flash / alert AND
//               the headline has market context (or a critical threat), or the
//               headline reports a market-wide breakdown (market crash, flash
//               crash, sovereign default; circuit breakers / a bank run with
//               market context) as happening, not asked about ("If the stock
//               market crashes…", "Will markets crash?").
//  * URGENT   — "urgent" / "emergency" in the headline together with market
//               context in the headline ("Fed holds emergency meeting"), again
//               not a hypothetical one.
//  * BREAKING — the headline reports a market-impact event (event_type()).
//  * Events   — strong patterns are financial by themselves ("rate decision",
//               "merger", "price target"); weak ones ("acquisition", "probe",
//               "fined", "downgraded", "earnings") count only with market
//               context in the headline.
//  * Impact HIGH   — FLASH / URGENT; a rate decision or M&A (or a distressed
//                    company) in the headline; or a strong tone (|tone| >= 0.75)
//                    from a tier-1/2 source with market context in the headline.
//                    A tier-4 source (blog) is never HIGH.
//  * Impact MEDIUM — BREAKING, any event, or a clear tone (>= 0.45) with market
//                    context.
//  * Threat   — conflict, cyber, natural disasters, sanctions and market-wide
//               breakdowns, matched as whole words ("riot" is not "patriot").

#include <QRegularExpression>
#include <QString>
#include <QStringView>

#include <cmath>

namespace fincept::services::news_classify {

enum class Prio { Flash, Urgent, Breaking, Routine };
enum class Impact { High, Medium, Low };
enum class Threat { Critical, High, Medium, Low, Info };

struct ThreatResult {
    Threat level = Threat::Info;
    QString category = QStringLiteral("general");
    double confidence = 0.3;
};

struct Input {
    QString headline;
    QString summary;
    int tier = 4;            ///< 1 wire / central bank … 4 blog
    bool has_tickers = false; ///< validated ticker tags (NewsTickerTagger)
    double tone = 0.0;       ///< -1..1
    bool tone_scored = false;
};

struct Result {
    Prio priority = Prio::Routine;
    Impact impact = Impact::Low;
    QString event_type; ///< "RATES", "M&A", "EARNINGS", "RATING", "MACRO", "CORPORATE", "POLICY", "LEGAL"; empty = none
    ThreatResult threat;
};

namespace detail {

inline const QRegularExpression& market_terms_re() {
    // Lower-cased text. Whole words only; singular "stock", "share", "bond" and
    // the bare "euro" are left out ("in stock", "share your story", "James Bond",
    // "Euro 2028").
    static const QRegularExpression re(QStringLiteral(
        "\\b(?:stocks|equit(?:y|ies)|share prices?|(?:its|their) shares|shares (?:rose|fell|rise|fall|falls|jump\\w*|"
        "slump\\w*|surg\\w*|plung\\w*|climb\\w*|drop\\w*|gain\\w*|tumbl\\w*|soar\\w*|slid|slide|sank|sink|rall\\w*|"
        "closed|ended|opened|traded)|bonds|bond (?:market|yields?|prices?|sale|issue)|(?:treasury|gilt|bund|10-year|"
        "two-year|2-year|30-year) yields?|treasuries|gilts|forex|currenc(?:y|ies)|exchange rates?|dollar|eurozone|"
        "euro (?:zone|area)|yen|yuan|renminbi|rupee|sterling|bitcoin|crypto\\w*|stablecoins?|commodit(?:y|ies)|"
        "crude|brent|wti|opec\\+?|oil|gold prices?|futures|s&p|nasdaq|dow jones|ftse|dax|nikkei|topix|"
        "hang seng|sensex|nifty|kospi|stoxx|msci|markets|stock market|stock exchange|bourse|wall street|"
        "investors?|traders?|trading|shareholders?|dividends?|earnings|revenues?|profits?|quarterly|analysts?|"
        "valuation|market (?:cap\\w*|value|share)|central bank|federal reserve|fomc|ecb|boe|boj|pboc|rbi|snb|rba|"
        "imf|interest rates?|monetary policy|inflation|gdp|recession|bailout|liquidity|hedge funds?|"
        "private equity|ipo|merger|m&a|tariffs?|basis points?|banks|banking|lenders?|insurers?)\\b"));
    return re;
}

/// "Fed" (the central bank) only as written: "fed up" / "well-fed" are not it.
inline const QRegularExpression& fed_re() {
    static const QRegularExpression re(QStringLiteral("\\b(?:Fed|FED)\\b(?!\\s+up\\b)"));
    return re;
}

inline const QRegularExpression& money_re() {
    static const QRegularExpression re(QString::fromUtf8(
        "(?:[$\xe2\x82\xac\xc2\xa3\xc2\xa5\xe2\x82\xb9]\\s?\\d|\\b(?:usd|eur|gbp|jpy|inr|cny|rmb|chf|aud|cad|kes|ksh|"
        "ngn|zar|hkd|sgd|krw|brl|mxn)\\s?\\d|\\b\\d+(?:[.,]\\d+)?\\s?(?:billion|bn|trillion|tn)\\b)"));
    return re;
}

/// The publisher's own "this is breaking" marker, in the headline only.
inline bool flash_marker(const QString& headline) {
    // Upper-case prefix (BREAKING: / FLASH / ALERT - / JUST IN) or a marker
    // followed by a separator; "Breaking the cycle…" and "record-breaking" are not.
    static const QRegularExpression upper_re(QStringLiteral("^\\W*(?:BREAKING(?: NEWS)?|FLASH|ALERT|JUST IN)\\b"));
    static const QRegularExpression sep_re(
        QString::fromUtf8("^\\W*(?:breaking news|breaking|flash|alert|just in)\\s*[:|\xe2\x80\x93\xe2\x80\x94-]"),
        QRegularExpression::CaseInsensitiveOption);
    // Other feed languages' markers (Eilmeldung, dernière minute, última hora,
    // ultim'ora, срочно, 速報, 突发, 속보, عاجل) anywhere in the headline.
    static const QRegularExpression intl_re(
        QStringLiteral("(?<![\\w-])(?:eilmeldung|derni\\x{e8}re minute|\\x{fa}ltima hora|ultim'ora|"
                       "\\x{0441}\\x{0440}\\x{043E}\\x{0447}\\x{043D}\\x{043E})\\b|\\x{901F}\\x{5831}|"
                       "\\x{7A81}\\x{53D1}|\\x{C18D}\\x{BCF4}|\\x{0639}\\x{0627}\\x{062C}\\x{0644}"),
        QRegularExpression::CaseInsensitiveOption);
    return upper_re.match(headline).hasMatch() || sep_re.match(headline).hasMatch() ||
           intl_re.match(headline).hasMatch();
}

struct EventPattern {
    const char* type;
    QRegularExpression strong; ///< financial by itself
    QRegularExpression weak;   ///< needs market context (may be empty)
};

inline const EventPattern* events(int* count) {
    // Most decisive first; the first type that matches decides. Lower-cased text,
    // English plus the commonest terms of the other feed languages.
    static const EventPattern ev[] = {
        {"RATES",
         QRegularExpression(QStringLiteral(
             "\\b(?:(?:raises|cuts|holds|lowers|hikes|keeps|leaves|slashes|lifts|trims) (?:its |key |interest |"
             "benchmark |policy |the )*rates?|rate (?:hikes?|cuts?|decision|increases?|reductions?)|"
             "monetary policy (?:decision|meeting|statement|committee)|fomc|policy rate|repo rate|"
             "taux directeurs?|leitzins|tipos? de inter[e\\x{e9}]s|tasa de inter[e\\x{e9}]s|selic|"
             "tassi di interesse)\\b|\\x{5229}\\x{7387}|\\x{91D1}\\x{5229}|\\x{AE30}\\x{C900}\\x{AE08}\\x{B9AC}")),
         QRegularExpression()},
        {"M&A",
         QRegularExpression(QStringLiteral(
             "\\b(?:mergers?|agrees to (?:buy|acquire)|deal to (?:buy|acquire)|tender offer|buyout|"
             "takeover (?:bid|offer|battle)|m&a|[\\x{fc}u]bernahme)\\b|\\x{6536}\\x{8D2D}|\\x{8CB7}\\x{53CE}")),
         QRegularExpression(QStringLiteral(
             "\\b(?:acquires?|acquired|acquisition|takeover|bid for|rachat|fusion|fusi[o\\x{f3}]n|"
             "adquisici[o\\x{f3}]n|aquisi[c\\x{e7}][a\\x{e3}]o|fus[a\\x{e3}]o|acquisizione)\\b|\\x{C778}\\x{C218}"))},
        {"EARNINGS",
         QRegularExpression(QStringLiteral(
             "\\b(?:quarterly (?:results|profit|loss|earnings|revenue)|q[1-4] (?:results|profit|revenue|earnings|"
             "sales)|(?:beats?|miss(?:es)?|tops?) (?:estimates|expectations|forecasts)|profit warning|"
             "earnings (?:per share|season|call|report|beat|miss)|net (?:profit|income)|quartalszahlen)\\b|"
             "\\x{6C7A}\\x{7B97}|\\x{8D22}\\x{62A5}")),
         QRegularExpression(QStringLiteral(
             "\\b(?:earnings|eps|guidance|b[\\x{e9}e]n[\\x{e9}e]fice|r[\\x{e9}e]sultats|gewinn|beneficio|resultados|"
             "lucro|utile)\\b|\\x{4E1A}\\x{7EE9}|\\x{C2E4}\\x{C801}"))},
        {"RATING",
         QRegularExpression(QStringLiteral("\\b(?:price target|outperform|underperform|credit rating|moody'?s|fitch|"
                                           "s&p global ratings)\\b")),
         QRegularExpression(QStringLiteral("\\b(?:upgrades?|upgraded|downgrades?|downgraded|overweight|"
                                           "underweight)\\b"))},
        {"MACRO",
         QRegularExpression(QStringLiteral(
             "\\b(?:cpi|inflation (?:data|rate|figures?|report)|jobs report|nonfarm|payrolls|unemployment rate|gdp|"
             "pmi|retail sales|industrial production|trade balance|consumer confidence|inflaci[o\\x{f3}]n|"
             "infla[c\\x{e7}][a\\x{e3}]o|inflazione|inflationsrate|pib)\\b")),
         QRegularExpression(QStringLiteral("\\b(?:arbeitslosen\\w*|ch[o\\x{f4}]mage|desemprego|desempleo)\\b"))},
        {"CORPORATE",
         QRegularExpression(QStringLiteral(
             "\\b(?:chapter 11|files? for bankruptcy|share repurchase|buyback|dividend (?:cut|hike|increase|"
             "suspension)|ipo|delisting|profit warning|ceo (?:resigns|steps down|ousted|exits))\\b")),
         QRegularExpression(QStringLiteral("\\b(?:bankrupt\\w*|insolven\\w*|defaults?|defaulted|layoffs?|job cuts|"
                                           "recalls?|recalled)\\b"))},
        {"POLICY",
         QRegularExpression(QStringLiteral("\\b(?:tariffs?|export controls?|trade war|budget deficit|"
                                           "fiscal (?:policy|stimulus|deficit)|stimulus package)\\b")),
         QRegularExpression(QStringLiteral("\\b(?:sanctions?|embargo|stimulus|subsid(?:y|ies)|fiscal)\\b"))},
        {"LEGAL",
         QRegularExpression(QStringLiteral("\\b(?:antitrust|sec charges|securities fraud|insider trading|"
                                           "class action)\\b")),
         QRegularExpression(QStringLiteral("\\b(?:lawsuits?|fined|fines? of|penalty|probes?|investigation|"
                                           "settlement|charged|indicted|sued|sues)\\b"))},
    };
    *count = static_cast<int>(sizeof(ev) / sizeof(ev[0]));
    return ev;
}

inline const QRegularExpression& distress_re() {
    static const QRegularExpression re(
        QStringLiteral("\\b(?:bankrupt\\w*|chapter 11|insolven\\w*|defaults?|defaulted|profit warning)\\b"));
    return re;
}

} // namespace detail

/// "Fed" the central bank in `text` as written ("fed up" / "well-fed" are not).
inline bool mentions_fed(const QString& text) {
    return detail::fed_re().match(text).hasMatch();
}

/// Market context in `text`: a finance / market term, "Fed", or a money amount.
/// `text` as written (case matters only for "Fed").
inline bool market_terms(const QString& text) {
    if (text.isEmpty())
        return false;
    const QString lower = text.toLower();
    return detail::market_terms_re().match(lower).hasMatch() || detail::fed_re().match(text).hasMatch() ||
           detail::money_re().match(lower).hasMatch();
}

/// The market-impact event `lower_text` reports: the first type whose strong
/// pattern matches, else (only when `market_context`) the first whose weak
/// pattern matches. Empty = none.
inline QString event_type(const QString& lower_text, bool market_context) {
    int n = 0;
    const detail::EventPattern* ev = detail::events(&n);
    for (int i = 0; i < n; ++i)
        if (ev[i].strong.match(lower_text).hasMatch())
            return QString::fromLatin1(ev[i].type);
    if (!market_context)
        return {};
    for (int i = 0; i < n; ++i) // a default-constructed (empty) pattern would match anything
        if (!ev[i].weak.pattern().isEmpty() && ev[i].weak.match(lower_text).hasMatch())
            return QString::fromLatin1(ev[i].type);
    return {};
}

/// Threat (security / physical / systemic risk) of `lower_text`. Ordinary market
/// events (rate decisions, misses, downgrades) are market impact, not threats.
inline ThreatResult threat(const QString& lower_text) {
    struct ThreatPattern {
        QRegularExpression re;
        const char* category;
        Threat level;
        double conf;
    };
    // Checked in this order; the first match decides. Whole words.
    static const ThreatPattern patterns[] = {
        // Critical — immediate, systemic or catastrophic
        {QRegularExpression(QStringLiteral("\\bnuclear (?:strike|attack)s?\\b")), "conflict", Threat::Critical, 0.95},
        {QRegularExpression(QStringLiteral("\\b(?:war declared|declares war)\\b")), "conflict", Threat::Critical, 0.95},
        {QRegularExpression(QStringLiteral("\\b(?:(?:stock )?market crash|flash crash)\\b")), "market",
         Threat::Critical, 0.9},
        // Circuit breakers only next to a market word (not the electrical kind).
        {QRegularExpression(QStringLiteral("\\b(?:trading|stocks|shares|markets?|index|exchange)\\b.{0,40}"
                                           "\\bcircuit breakers?\\b|\\bcircuit breakers?\\b.{0,40}"
                                           "\\b(?:trading|stocks|shares|markets?|index|exchange)\\b")),
         "market", Threat::Critical, 0.85},
        {QRegularExpression(QStringLiteral("\\bmarket-wide (?:halt|trading halt)\\b")), "market", Threat::Critical,
         0.85},
        {QRegularExpression(QStringLiteral("\\bbank run(?! by)\\b")), "market", Threat::Critical, 0.9},
        {QRegularExpression(QStringLiteral("\\bsovereign default\\b")), "market", Threat::Critical, 0.9},
        {QRegularExpression(QStringLiteral("\\bexchange outage\\b")), "market", Threat::High, 0.8},
        {QRegularExpression(QStringLiteral("\\bcyber[- ]?attacks?\\b")), "cyber", Threat::High, 0.8},
        {QRegularExpression(QStringLiteral("\\bdata breach(?:es)?\\b")), "cyber", Threat::High, 0.75},
        {QRegularExpression(QStringLiteral("\\bransomware\\b")), "cyber", Threat::High, 0.8},
        // High — significant security / physical events
        {QRegularExpression(QStringLiteral("(?<!pitch )\\binvasion\\b(?! of privacy)")), "conflict", Threat::High,
         0.85},
        {QRegularExpression(QStringLiteral("\\bair ?strikes?\\b")), "conflict", Threat::High, 0.85},
        {QRegularExpression(QStringLiteral("\\bmissile (?:launch|attack|strike)s?\\b")), "conflict", Threat::High,
         0.85},
        {QRegularExpression(QStringLiteral("\\bmilitary deploy\\w*")), "conflict", Threat::High, 0.8},
        {QRegularExpression(QStringLiteral("\\bcoup attempt\\b")), "conflict", Threat::High, 0.85},
        {QRegularExpression(QStringLiteral("\\bmartial law\\b")), "conflict", Threat::High, 0.85},
        {QRegularExpression(QStringLiteral("\\bterror(?:ist)? attacks?\\b")), "conflict", Threat::High, 0.85},
        {QRegularExpression(QStringLiteral("\\bembargo(?:es)?\\b")), "regulatory", Threat::High, 0.75},
        {QRegularExpression(QStringLiteral("\\bearthquakes?\\b")), "natural", Threat::High, 0.8},
        {QRegularExpression(QStringLiteral("\\btsunamis?\\b")), "natural", Threat::High, 0.85},
        {QRegularExpression(QStringLiteral("\\b(?:hurricane|typhoon|cyclone)\\b")), "natural", Threat::High, 0.75},
        {QRegularExpression(QStringLiteral("\\bpandemic\\b")), "natural", Threat::High, 0.8},
        // Medium — escalation signals
        {QRegularExpression(QStringLiteral("\\bsanctions?\\b")), "regulatory", Threat::Medium, 0.65},
        {QRegularExpression(QStringLiteral("\\bprotest(?:s|ers)?\\b")), "conflict", Threat::Medium, 0.6},
        {QRegularExpression(QStringLiteral("\\briot(?:s|ing)?\\b")), "conflict", Threat::Medium, 0.7},
        {QRegularExpression(QStringLiteral("\\bescalat(?:e|es|ed|ing|ion)\\b")), "conflict", Threat::Medium, 0.65},
        {QRegularExpression(QStringLiteral("\\bceasefire collapse\\w*")), "conflict", Threat::Medium, 0.7},
        {QRegularExpression(QStringLiteral("\\bborder clash(?:es)?\\b")), "conflict", Threat::Medium, 0.7},
        {QRegularExpression(QStringLiteral("\\boutbreaks?\\b")), "natural", Threat::Medium, 0.6},
        {QRegularExpression(QStringLiteral("\\bwildfires?\\b")), "natural", Threat::Medium, 0.6},
        {QRegularExpression(QStringLiteral("\\bfloods?\\b|\\bflooding\\b")), "natural", Threat::Medium, 0.6},
        {QRegularExpression(QStringLiteral("\\bdroughts?\\b")), "natural", Threat::Medium, 0.55},
        {QRegularExpression(QStringLiteral("\\b(?:power outage|blackouts?)\\b")), "natural", Threat::Medium, 0.55},
    };
    ThreatResult tc;
    for (const auto& p : patterns) {
        if (p.re.match(lower_text).hasMatch()) {
            tc.level = p.level;
            tc.category = QString::fromLatin1(p.category);
            tc.confidence = p.conf;
            return tc;
        }
    }
    return tc;
}

/// Priority, market impact, event type and threat of one item (rules above).
inline Result classify(const Input& in) {
    Result r;
    const QString head_lower = in.headline.toLower();
    const QString text_lower = head_lower + QLatin1Char(' ') + in.summary.toLower();
    // Market context is judged on the headline (plus the validated tags): a road-
    // safety report whose last paragraph quotes the cost to GDP is not market news.
    const bool ctx_head = in.has_tickers || market_terms(in.headline);

    r.threat = threat(text_lower);
    const QString ev_head = event_type(head_lower, ctx_head);
    // An event named only in the summary counts for a market story only.
    r.event_type = !ev_head.isEmpty() ? ev_head : (ctx_head ? event_type(text_lower, true) : QString());

    // Priority.
    // Market-wide only: "Tata Motors shares crash 8%" is one company's bad day.
    static const QRegularExpression shock_re(QStringLiteral(
        "\\b(?:(?:stock )?markets? crash(?:es)?|flash crash|markets? collapse|(?:global|world|asian|european|"
        "us|u\\.s\\.|wall street|chinese|china|indian|japanese|emerging[- ]market) (?:stocks|shares|equities|"
        "markets) (?:crash|collapse)|sovereign default|market-wide (?:halt|trading halt))\\b"));
    static const QRegularExpression shock_ctx_re(QStringLiteral("\\b(?:circuit breakers?|bank run(?! by))\\b"));
    static const QRegularExpression urgency_re(QStringLiteral("\\b(?:urgent(?:ly)?|emergency|emergencies)\\b"));
    // A crash or an emergency that is only asked about, feared or planned for is
    // not one ("If the Stock Market Crashes, Buffett Says…", "Will the Fed hold an
    // emergency meeting?") — seen lighting the FLASH banner on evergreen pieces.
    static const QRegularExpression hypothetical_re(QStringLiteral(
        "\\?|\\b(?:if|whether|will|would|could|might|should|before|how to|what to|prepar\\w*|brac\\w*|fears?|"
        "warns?|warning|risk of|odds of|chance of|is a|are we|why|when)\\b"));
    const bool actual = !hypothetical_re.match(head_lower).hasMatch();
    const bool marker = detail::flash_marker(in.headline);
    if ((actual && (shock_re.match(head_lower).hasMatch() ||
                    (ctx_head && shock_ctx_re.match(head_lower).hasMatch()))) ||
        (marker && (ctx_head || r.threat.level == Threat::Critical)))
        r.priority = Prio::Flash;
    else if (actual && ctx_head && urgency_re.match(head_lower).hasMatch())
        r.priority = Prio::Urgent;
    else if (!ev_head.isEmpty())
        r.priority = Prio::Breaking;

    // Market impact.
    const double magnitude = in.tone_scored ? std::abs(in.tone) : 0.0;
    const bool distressed = detail::distress_re().match(head_lower).hasMatch();
    const bool decisive = ev_head == QLatin1String("RATES") || ev_head == QLatin1String("M&A") ||
                          ((ev_head == QLatin1String("CORPORATE") || ev_head == QLatin1String("EARNINGS")) &&
                           distressed);
    if (r.priority == Prio::Flash || r.priority == Prio::Urgent || decisive ||
        (magnitude >= 0.75 && in.tier <= 2 && ctx_head))
        r.impact = Impact::High;
    else if (r.priority == Prio::Breaking || !r.event_type.isEmpty() || (magnitude >= 0.45 && ctx_head))
        r.impact = Impact::Medium;
    else
        r.impact = Impact::Low;
    if (in.tier >= 4 && r.impact == Impact::High)
        r.impact = Impact::Medium;
    return r;
}

} // namespace fincept::services::news_classify
