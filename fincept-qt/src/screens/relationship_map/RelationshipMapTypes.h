// src/screens/relationship_map/RelationshipMapTypes.h
// Corporate intelligence relationship map — type definitions.
#pragma once

#include <QColor>
#include <QMap>
#include <QString>
#include <QVector>

namespace fincept::relmap {

// ── Node Categories ──────────────────────────────────────────────────────────

enum class NodeCategory {
    Company,
    Peer,
    Institutional,
    MutualFund,
    Insider,
    Officer,
    Analyst,
    Governance,
    Technicals,
    ShortInterest,
    Earnings,
    Event,
    SupplyChain,
    Metrics,
};

inline QString category_label(NodeCategory c) {
    switch (c) {
        case NodeCategory::Company:      return "COMPANY";
        case NodeCategory::Peer:         return "PEER";
        case NodeCategory::Institutional:return "INSTITUTIONAL";
        case NodeCategory::MutualFund:   return "MUTUAL FUND";
        case NodeCategory::Insider:      return "INSIDER";
        case NodeCategory::Officer:      return "OFFICER";
        case NodeCategory::Analyst:      return "ANALYST";
        case NodeCategory::Governance:   return "GOVERNANCE";
        case NodeCategory::Technicals:   return "TECHNICALS";
        case NodeCategory::ShortInterest:return "SHORT INTEREST";
        case NodeCategory::Earnings:     return "EARNINGS";
        case NodeCategory::Event:        return "EVENT";
        case NodeCategory::SupplyChain:  return "SUPPLY CHAIN";
        case NodeCategory::Metrics:      return "METRICS";
    }
    return "UNKNOWN";
}

inline QColor category_color(NodeCategory c) {
    switch (c) {
        case NodeCategory::Company:      return QColor("#d97706"); // amber
        case NodeCategory::Peer:         return QColor("#2563eb"); // blue
        case NodeCategory::Institutional:return QColor("#16a34a"); // green
        case NodeCategory::MutualFund:   return QColor("#059669"); // emerald
        case NodeCategory::Insider:      return QColor("#0891b2"); // cyan
        case NodeCategory::Officer:      return QColor("#0e7490"); // dark cyan
        case NodeCategory::Analyst:      return QColor("#7c3aed"); // violet
        case NodeCategory::Governance:   return QColor("#be123c"); // rose
        case NodeCategory::Technicals:   return QColor("#0369a1"); // sky
        case NodeCategory::ShortInterest:return QColor("#dc2626"); // red
        case NodeCategory::Earnings:     return QColor("#ea580c"); // orange
        case NodeCategory::Event:        return QColor("#b45309"); // amber-dark
        case NodeCategory::SupplyChain:  return QColor("#ca8a04"); // yellow
        case NodeCategory::Metrics:      return QColor("#525252"); // neutral
    }
    return QColor("#525252");
}

inline QColor category_bg(NodeCategory c) {
    QColor col = category_color(c);
    col.setAlpha(25);
    return col;
}

// ── Edge Categories ──────────────────────────────────────────────────────────

enum class EdgeCategory { Ownership, Peer, SupplyChain, Event, Internal };

inline QColor edge_color(EdgeCategory c) {
    switch (c) {
        case EdgeCategory::Ownership:
            return QColor("#16a34a");
        case EdgeCategory::Peer:
            return QColor(35, 99, 235, 120);
        case EdgeCategory::SupplyChain:
            return QColor("#ca8a04");
        case EdgeCategory::Event:
            return QColor(220, 38, 38, 100);
        case EdgeCategory::Internal:
            return QColor("#404040");
    }
    return QColor("#404040");
}

// ── Data Structures ──────────────────────────────────────────────────────────

struct CompanyInfo {
    QString ticker;
    QString name;
    QString sector;
    QString industry;
    QString website;
    QString description;
    QString country;
    QString exchange;
    QString currency;
    int employees = 0;
    // Price
    double market_cap = 0;
    double current_price = 0;
    double previous_close = 0;
    double day_change_pct = 0;
    // Valuation ratios
    double pe_ratio = 0;
    double forward_pe = 0;
    double price_to_book = 0;
    double price_to_sales = 0;
    // Growth & returns
    double roe = 0;
    double roa = 0;
    double revenue_growth = 0;
    double earnings_growth = 0;
    double profit_margins = 0;
    // Financials
    double revenue = 0;
    double ebitda = 0;
    double free_cashflow = 0;
    double operating_cashflow = 0;
    double total_cash = 0;
    double total_debt = 0;
    // Ownership
    double insider_percent = 0;
    double institutional_percent = 0;
    // Analyst
    QString recommendation;
    double recommendation_mean = 0;
    double target_high = 0;
    double target_low = 0;
    double target_mean = 0;
    double target_median = 0;
    int analyst_count = 0;
    // Dividend & EPS
    double dividend_yield = 0;
    double payout_ratio = 0;
    double trailing_eps = 0;
    double forward_eps = 0;
    double shares_outstanding = 0;
};

struct GovernanceRisk {
    int audit_risk = 0;           // 1-10
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
    double week52_change_pct = 0;
    double sp500_52wk_change = 0;
    int avg_volume = 0;
    int avg_volume_10d = 0;
};

struct ShortInterest {
    double shares_short = 0;
    double short_ratio = 0;       // days to cover
    double short_pct_float = 0;   // 0-1
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
    QString period;   // "0m", "-1m", "-2m", "-3m"
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
    QString action;       // "up", "down", "main", "reit"
    double price_target = 0;
    double prior_target = 0;
};

struct CompanyOfficer {
    QString name;
    QString title;
    int total_pay = 0;
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

struct InstitutionalHolder {
    QString name;
    double shares = 0;
    double value = 0;
    double percentage = 0;
    double change_percent = 0;
    QString fund_family;
    QString type; // "institutional" or "mutualfund"
};

struct InsiderHolder {
    QString name;
    QString title;
    double shares = 0;
    double percentage = 0;
    QString last_transaction; // "buy" / "sell"
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
    double current_price = 0;
    QString sector;
    double beta = 0;
    double ev_to_ebitda = 0;
    double price_to_book = 0;
    double week52_change = 0;
    QString recommendation;
};

struct CorporateEvent {
    QString date;
    QString form;
    QString description;
    QString category;
};

struct SupplyChainEntity {
    QString name;
    QString relationship;
    double revenue_pct = 0;
};

struct ValuationSignal {
    QString status;
    QString action;
    double score = 0; // -3 to +3
};

// ── Graph Node ───────────────────────────────────────────────────────────────

struct GraphNode {
    QString id;
    NodeCategory category;
    QString label;
    QString sublabel;
    QMap<QString, QString> properties; // key-value metadata shown in detail panel
    double value = 0;                  // for sizing/sorting (market_cap, %, etc.)
};

struct GraphEdge {
    QString source_id;
    QString target_id;
    EdgeCategory category;
    QString label;
    double weight = 1.0; // for edge thickness
};

// ── Complete Data ────────────────────────────────────────────────────────────

struct RelationshipData {
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
    QVector<InstitutionalHolder> institutional_holders;
    QVector<InstitutionalHolder> mutualfund_holders;
    QVector<InsiderHolder> insider_holders;
    QVector<PeerCompany> peers;
    QVector<CorporateEvent> events;
    QVector<SupplyChainEntity> supply_chain;
    int data_quality = 0;
    QString timestamp;
};

// ── Filter State ─────────────────────────────────────────────────────────────

struct FilterState {
    bool show_peers = true;
    bool show_institutional = true;
    bool show_insiders = true;
    bool show_officers = true;
    bool show_analysts = true;
    bool show_metrics = true;
    bool show_events = true;
    bool show_supply_chain = true;
    double min_ownership = 0; // 0-100%
};

// ── Layout ───────────────────────────────────────────────────────────────────

enum class LayoutMode { Layered, Radial, Force };

inline QString layout_label(LayoutMode m) {
    switch (m) {
        case LayoutMode::Layered:
            return "LAYERED";
        case LayoutMode::Radial:
            return "RADIAL";
        case LayoutMode::Force:
            return "FORCE";
    }
    return "LAYERED";
}

} // namespace fincept::relmap

#include <QMetaType>
Q_DECLARE_METATYPE(fincept::relmap::RelationshipData)
