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
    FundFamily,
    Insider,
    Event,
    SupplyChain,
    Metrics,
};

inline QString category_label(NodeCategory c) {
    switch (c) {
    case NodeCategory::Company:        return "COMPANY";
    case NodeCategory::Peer:           return "PEER";
    case NodeCategory::Institutional:  return "INSTITUTIONAL";
    case NodeCategory::FundFamily:     return "FUND FAMILY";
    case NodeCategory::Insider:        return "INSIDER";
    case NodeCategory::Event:          return "EVENT";
    case NodeCategory::SupplyChain:    return "SUPPLY CHAIN";
    case NodeCategory::Metrics:        return "METRICS";
    }
    return "UNKNOWN";
}

inline QColor category_color(NodeCategory c) {
    switch (c) {
    case NodeCategory::Company:        return QColor("#d97706"); // amber
    case NodeCategory::Peer:           return QColor("#2563eb"); // blue
    case NodeCategory::Institutional:  return QColor("#16a34a"); // green
    case NodeCategory::FundFamily:     return QColor("#9333ea"); // purple
    case NodeCategory::Insider:        return QColor("#0891b2"); // cyan
    case NodeCategory::Event:          return QColor("#dc2626"); // red
    case NodeCategory::SupplyChain:    return QColor("#ca8a04"); // yellow
    case NodeCategory::Metrics:        return QColor("#808080"); // gray
    }
    return QColor("#808080");
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
    case EdgeCategory::Ownership:    return QColor("#16a34a");
    case EdgeCategory::Peer:         return QColor(35, 99, 235, 120);
    case EdgeCategory::SupplyChain:  return QColor("#ca8a04");
    case EdgeCategory::Event:        return QColor(220, 38, 38, 100);
    case EdgeCategory::Internal:     return QColor("#404040");
    }
    return QColor("#404040");
}

// ── Data Structures ──────────────────────────────────────────────────────────

struct CompanyInfo {
    QString ticker;
    QString name;
    QString sector;
    QString industry;
    double market_cap = 0;
    double current_price = 0;
    double pe_ratio = 0;
    double forward_pe = 0;
    double price_to_book = 0;
    double roe = 0;
    double revenue_growth = 0;
    double profit_margins = 0;
    double revenue = 0;
    double ebitda = 0;
    double free_cashflow = 0;
    double total_cash = 0;
    double total_debt = 0;
    double insider_percent = 0;
    double institutional_percent = 0;
    int employees = 0;
    QString recommendation;
    double target_price = 0;
    int analyst_count = 0;
};

struct InstitutionalHolder {
    QString name;
    double shares = 0;
    double value = 0;
    double percentage = 0;
    double change_percent = 0;
    QString fund_family;
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
    double roe = 0;
    double revenue_growth = 0;
    double profit_margins = 0;
    double current_price = 0;
    QString sector;
};

struct CorporateEvent {
    QString date;
    QString form;
    QString description;
    QString category; // earnings, management, merger, etc.
};

struct SupplyChainEntity {
    QString name;
    QString relationship; // "customer", "supplier", "partner"
    double revenue_pct = 0;
};

struct ValuationSignal {
    QString status;     // UNDERVALUED, FAIRLY_VALUED, OVERVALUED
    QString action;     // BUY, HOLD, SELL
    double score = 0;   // -3 to +3
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
    ValuationSignal valuation;
    QVector<InstitutionalHolder> institutional_holders;
    QVector<InsiderHolder> insider_holders;
    QVector<PeerCompany> peers;
    QVector<CorporateEvent> events;
    QVector<SupplyChainEntity> supply_chain;
    int data_quality = 0; // 0-100
    QString timestamp;
};

// ── Filter State ─────────────────────────────────────────────────────────────

struct FilterState {
    bool show_institutional = true;
    bool show_insiders = true;
    bool show_peers = true;
    bool show_events = true;
    bool show_metrics = true;
    bool show_supply_chain = true;
    double min_ownership = 0; // 0-100%
};

// ── Layout ───────────────────────────────────────────────────────────────────

enum class LayoutMode { Layered, Radial, Force };

inline QString layout_label(LayoutMode m) {
    switch (m) {
    case LayoutMode::Layered: return "LAYERED";
    case LayoutMode::Radial:  return "RADIAL";
    case LayoutMode::Force:   return "FORCE";
    }
    return "LAYERED";
}

} // namespace fincept::relmap
