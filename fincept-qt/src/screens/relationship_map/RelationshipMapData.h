// src/screens/relationship_map/RelationshipMapData.h
// Relationship Map — the data of one company (or fund) as relationship_map.py reports it.
// Pure QtCore (no widgets, no theme): parsed on a worker thread by the service, unit-tested
// standalone (tests/tst_relmap_model.cpp), published on the DataHub as RelationshipData.
//
// Units, as the script delivers them (never re-scaled here):
//   * holder / insider "percentage" and "change_percent": percent (7.96 = 7.96 %);
//   * every other ratio (margins, ROE, growth, major_holders.*_pct, fund weights): a fraction;
//   * per-share prices: the QUOTE currency (minor units kept, "GBp");
//   * market cap and holding values: the quote currency's major unit; statement figures
//     (revenue, EPS, pay): the financial currency. "" = unknown, never assumed USD.
#pragma once

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::relmap {

// ── Company profile ─────────────────────────────────────────────────────────

struct CompanyInfo {
    QString ticker;
    QString name;
    QString sector;
    QString industry;
    QString website;
    QString description;
    QString country;
    QString exchange;
    QString currency;           ///< quote currency as Yahoo reports it (minor units kept: "GBp"); "" = unknown
    QString financial_currency; ///< currency of the statements (revenue, EPS, pay); "" = unknown
    QString quote_type;         ///< Yahoo quoteType: EQUITY, ETF, MUTUALFUND, CRYPTOCURRENCY, CURRENCY, INDEX, … ("" = old payload)
    int employees = 0;
    double market_cap = 0;
    double current_price = 0;
    double previous_close = 0;
    double day_change_pct = 0; ///< percent
    double pe_ratio = 0;
    double forward_pe = 0;
    double price_to_book = 0;
    double price_to_sales = 0;
    double roe = 0;
    double roa = 0;
    double revenue_growth = 0;
    double earnings_growth = 0;
    double profit_margins = 0;
    double revenue = 0;
    double ebitda = 0;
    double free_cashflow = 0;
    double operating_cashflow = 0;
    double total_cash = 0;
    double total_debt = 0;
    double insider_percent = 0;       ///< fraction
    double institutional_percent = 0; ///< fraction
    QString recommendation;
    double recommendation_mean = 0;
    double target_high = 0;
    double target_low = 0;
    double target_mean = 0;
    double target_median = 0;
    int analyst_count = 0;
    double dividend_yield = 0; ///< percent (Yahoo reports 0.31 for 0.31 %)
    double payout_ratio = 0;
    double trailing_eps = 0;
    double forward_eps = 0;
    double shares_outstanding = 0;
    double float_shares = 0;
};

struct GovernanceRisk {
    int audit_risk = 0; // 1-10
    int board_risk = 0;
    int compensation_risk = 0;
    int shareholder_rights_risk = 0;
    int overall_risk = 0;
};

struct Technicals {
    double fifty_two_week_high = 0;
    double fifty_two_week_low = 0;
    double fifty_day_avg = 0;
    double two_hundred_day_avg = 0;
    double beta = 0;
    double week52_change_pct = 0; ///< fraction
    double sp500_52wk_change = 0; ///< fraction
    double avg_volume = 0;
    double avg_volume_10d = 0;
};

struct ShortInterest {
    double shares_short = 0;
    double short_ratio = 0;     // days to cover
    double short_pct_float = 0; // fraction
    double float_shares = 0;
};

struct EnterpriseMetrics {
    double enterprise_value = 0;
    double ev_to_revenue = 0;
    double ev_to_ebitda = 0;
    double peg_ratio = 0;
    double price_to_sales = 0;
    double book_value = 0;
};

struct MarginsDebt {
    double gross = 0;
    double operating = 0;
    double ebitda = 0;
    double net = 0;
    double debt_to_equity = 0;
    double current_ratio = 0;
    double quick_ratio = 0;
};

struct AnalystTargets {
    double current = 0;
    double high = 0;
    double low = 0;
    double mean = 0;
    double median = 0;
};

struct RecommendationSnapshot {
    QString period; // "0m", "-1m", "-2m", "-3m"
    int strong_buy = 0;
    int buy = 0;
    int hold = 0;
    int sell = 0;
    int strong_sell = 0;
};

struct AnalystUpgrade {
    QString date;
    QString firm;
    QString to_grade;
    QString from_grade;
    QString action; // "up", "down", "main", "reit", "init"
    double price_target = 0;
    double prior_target = 0;
};

struct CompanyOfficer {
    QString name;
    QString title;
    double total_pay = 0; ///< financial currency
    int year_born = 0;
};

struct EarningsCalendar {
    QString earnings_date;
    double earnings_avg = 0;
    double earnings_low = 0;
    double earnings_high = 0;
    double revenue_avg = 0;
    double revenue_low = 0;
    double revenue_high = 0;
    QString ex_dividend_date;
    QString dividend_date;
};

// ── Relationships ───────────────────────────────────────────────────────────

/// An institution or fund holding the company (Yahoo's top-10 lists from 13F / N-PORT data).
struct InstitutionalHolder {
    QString name;
    double shares = 0;
    double value = 0;          ///< holding value, quote currency's major unit
    double percentage = 0;     ///< percent of shares outstanding
    double change_percent = 0; ///< percent change of the position since the previous report
    QString date_reported;     ///< "YYYY-MM-DD" ("" in old payloads)
    QString fund_family;
    QString type; // "institutional" or "mutualfund"
};

/// An insider on Yahoo's roster (officers, directors, 10 % owners).
struct InsiderHolder {
    QString name;
    QString title;
    double shares = 0;
    double percentage = 0;         ///< percent of shares outstanding
    QString last_transaction;      ///< "buy" / "sell" / the raw kind ("Stock Gift")
    QString last_transaction_text; ///< Yahoo's wording
    QString last_date;             ///< "YYYY-MM-DD"
};

/// One insider transaction (Form 4 data as Yahoo lists it).
struct RelInsiderTrade {
    QString date;     ///< "YYYY-MM-DD"
    QString insider;  ///< name as filed ("COOK TIMOTHY D")
    QString position; ///< "Chief Executive Officer", "Director", …
    QString text;     ///< Yahoo's sentence ("Sale at price 351.36 per share.")
    QString kind;     ///< "buy" | "sell" | "grant" | "gift" | "option" | "other"
    double shares = 0;
    double value = 0;  ///< in `currency`
    double price = 0;  ///< per share, in `currency`
    QString currency;  ///< "" = unknown (Yahoo states no currency; the script checks it against the quote)
    QString ownership; ///< "D" direct, "I" indirect
};

/// Insider buying and selling over the last six months.
struct RelInsiderSummary {
    bool present = false;
    QString window; ///< "6m"
    double purchases_shares = 0;
    int purchases_count = 0;
    double sales_shares = 0;
    int sales_count = 0;
    double net_shares = 0;
    double net_pct = 0; ///< fraction of insider shares
    double total_insider_shares = 0;
    double buy_value = 0;  ///< open-market buys (transactions), `currency`
    double sell_value = 0; ///< open-market sales (transactions), `currency`
    QString currency;
};

/// Yahoo's ownership breakdown (fractions of shares outstanding / float).
struct MajorHolders {
    bool present = false;
    double insiders_pct = 0;
    double institutions_pct = 0;
    double institutions_float_pct = 0;
    int institutions_count = 0;
};

struct PeerCompany {
    QString ticker;
    QString name;
    double market_cap = 0;
    double pe_ratio = 0;
    double forward_pe = 0;
    double roe = 0;
    double revenue_growth = 0;
    double profit_margins = 0;
    double gross_margins = 0;
    double operating_margins = 0;
    double revenue = 0; ///< financial currency
    double current_price = 0;
    QString sector;
    QString industry;
    QString country;
    double beta = 0;
    double ev_to_ebitda = 0;
    double price_to_book = 0;
    double week52_change = 0; ///< fraction
    QString recommendation;
    QString currency;           ///< quote currency (ISO; "GBp" = pence); empty = unknown
    QString financial_currency; ///< "" = unknown
};

/// Daily-return correlation of the company with its peers, its sector ETF and its benchmark
/// index, computed by the script from one price-history download.
struct CorrelationSet {
    bool present = false;
    QString window;   ///< "1y"
    QString interval; ///< "1d"
    QString as_of;    ///< last date in the window
    int observations = 0;
    QStringList symbols;          ///< matrix order; symbols[0] is the company
    QHash<QString, QString> labels; ///< symbol -> display name
    QHash<QString, QString> roles;  ///< symbol -> "self" | "peer" | "index" | "sector_etf"
    QVector<QVector<double>> matrix; ///< NaN = not enough overlapping data
    QHash<QString, double> returns;  ///< symbol -> total return over the window (fraction)
    QHash<QString, double> beta;     ///< symbol -> beta of the company against it
    QHash<QString, QVector<double>> spark; ///< symbol -> weekly closes rebased to 100

    /// ρ(a, b); NaN when either is not in the set.
    double value(const QString& a, const QString& b) const {
        const qsizetype i = symbols.indexOf(a.toUpper());
        const qsizetype j = symbols.indexOf(b.toUpper());
        if (i < 0 || j < 0 || i >= matrix.size() || j >= matrix[i].size())
            return std::numeric_limits<double>::quiet_NaN();
        return matrix[i][j];
    }
};

/// A fund's (ETF / mutual fund) own profile and holdings.
struct FundHolding {
    QString symbol;
    QString name;
    double weight = 0; ///< fraction of the fund
};

struct FundProfile {
    bool present = false;
    QString family;
    QString category;
    QString legal_type;
    double total_assets = 0; ///< quote currency's major unit
    QVector<FundHolding> holdings;
    QVector<QPair<QString, double>> sector_weights; ///< sector key -> fraction, largest first
    QMap<QString, double> asset_classes;           ///< "stockPosition" -> fraction …
};

/// A subsidiary listed in the latest annual report (SEC Exhibit 21 / 20-F exhibit 8).
struct SubsidiaryEntry {
    QString name;
    QString jurisdiction;
};

struct FilingRef {
    QString form;
    QString filed;
    QString accession;
    QString url;
};

struct SupplyChainEntity {
    QString name;
    QString relationship; ///< "customer" | "supplier"
    double revenue_pct = 0;
};

// ── Sections ────────────────────────────────────────────────────────────────

/// What became of one section of the map in the run that produced the data.
enum class RelSectionState {
    Unknown,       ///< not reported (old payload) and nothing in it
    Loading,       ///< planned, not delivered yet (streaming)
    Ok,            ///< delivered with rows
    Empty,         ///< the source answered with nothing for this instrument
    Timeout,       ///< the source did not answer inside the time budget
    Skipped,       ///< the time budget was used up before it was asked
    Error,         ///< the source failed
    Unavailable,   ///< no free source provides it (supply chain)
    NotApplicable, ///< meaningless for this instrument (insiders of an ETF)
};

/// True for a state that means "the data was asked for and did not arrive" (RETRY fetches it).
inline bool section_failed(RelSectionState s) {
    return s == RelSectionState::Timeout || s == RelSectionState::Skipped || s == RelSectionState::Error;
}

struct RelSectionStatus {
    RelSectionState state = RelSectionState::Unknown;
    QString source; ///< "Yahoo Finance", "SEC EDGAR", "Computed from Yahoo prices"
    QString as_of;  ///< date the source reports for its data ("" = fetch time)
    int ms = 0;     ///< how long the section took
    int count = 0;  ///< rows delivered
    QString note;   ///< plain sentence for the user ("Not available from free sources")
};

/// Section ids of the v2 contract (also the deep-link section names of the screen).
namespace section {
inline constexpr const char* kProfile = "profile";
inline constexpr const char* kHolders = "holders";
inline constexpr const char* kInsiders = "insiders";
inline constexpr const char* kManagement = "management";
inline constexpr const char* kAnalysts = "analysts";
inline constexpr const char* kCalendar = "calendar";
inline constexpr const char* kPeers = "peers";
inline constexpr const char* kCorrelation = "correlation";
inline constexpr const char* kFund = "fund";
inline constexpr const char* kSubsidiaries = "subsidiaries";
inline constexpr const char* kSupplyChain = "supply_chain";
} // namespace section

/// Relative-value screen: the company's trailing P/E against the median of its
/// peer set, nudged by revenue growth and margin. A heuristic, not a trade signal —
/// the UI shows `status` + `pe_vs_peers`, never a BUY / SELL.
struct ValuationSignal {
    QString status;         // "UNDERVALUED", ..., "INSUFFICIENT DATA", "N/A — ..."
    QString action;         // legacy BUY / HOLD / SELL (kept for the hub payload)
    double score = 0;       // -3 to +3
    double pe_vs_peers = 0; // company P/E / peer-median P/E (0 = not computed)
    int peer_count = 0;     // peers with a positive P/E behind the median
};

struct CorporateEvent {
    QString date;
    QString form;
    QString description;
    QString category;
};

// ── Complete data ───────────────────────────────────────────────────────────

struct RelationshipData {
    int schema = 1; ///< 1 = the original payload, 2 = sections + enrichment
    CompanyInfo company;
    GovernanceRisk governance;
    Technicals technicals;
    ShortInterest short_interest;
    EnterpriseMetrics enterprise;
    MarginsDebt margins;
    AnalystTargets analyst_targets;
    QVector<RecommendationSnapshot> recommendations;
    QVector<AnalystUpgrade> upgrades_downgrades;
    QVector<CompanyOfficer> officers;
    EarningsCalendar calendar;
    ValuationSignal valuation;
    MajorHolders major_holders;
    QVector<InstitutionalHolder> institutional_holders;
    QVector<InstitutionalHolder> mutualfund_holders;
    QVector<InsiderHolder> insider_holders;
    QVector<RelInsiderTrade> insider_trades; ///< newest first
    RelInsiderSummary insider_summary;
    QVector<PeerCompany> peers;
    CorrelationSet correlation;
    FundProfile fund;
    QVector<SubsidiaryEntry> subsidiaries;
    FilingRef subsidiaries_filing;
    QVector<CorporateEvent> events;
    QVector<SupplyChainEntity> supply_chain;
    /// How the peers were picked: "screen:industry" (Yahoo industry screen, the Equity
    /// Research peers service), "industry:<key>" (curated US list), "generic_fallback"
    /// (mega-caps — not comparable), "none".
    QString peer_source;
    QHash<QString, RelSectionStatus> sections; ///< section id -> status
    int data_quality = 0;
    QString timestamp;  ///< when the payload was parsed (ISO, UTC)
    QDateTime fetched_at; ///< when the script fetched it (UTC); invalid = unknown
    /// Slow upstream: the script stopped waiting for some sections. `note` is a
    /// plain sentence for the notice line; `missing_sections` the script's labels
    /// ("institutional holders", "calendar", "peer MSFT", …).
    bool partial = false;
    QString note;
    QStringList missing_sections;
    double elapsed_s = 0; ///< how long the script ran (0 = not reported)

    /// ETF / mutual fund.
    bool is_fund() const {
        const QString& t = company.quote_type;
        return t == QLatin1String("ETF") || t == QLatin1String("MUTUALFUND") || t == QLatin1String("MONEYMARKET") ||
               (t.isEmpty() && fund.present);
    }
    /// EQUITY (or an old payload without a quote type that is not a fund).
    bool is_company() const {
        return company.quote_type == QLatin1String("EQUITY") || (company.quote_type.isEmpty() && !is_fund());
    }
    /// Crypto, FX, index, future: an instrument that has no holders, officers or peers.
    bool has_no_relationships() const {
        const QString& t = company.quote_type;
        return t == QLatin1String("CRYPTOCURRENCY") || t == QLatin1String("CURRENCY") || t == QLatin1String("INDEX") ||
               t == QLatin1String("FUTURE") || t == QLatin1String("OPTION") || t == QLatin1String("COMMODITY");
    }
    RelSectionStatus section(const QString& id) const { return sections.value(id); }
};

} // namespace fincept::relmap

Q_DECLARE_METATYPE(fincept::relmap::RelationshipData)
