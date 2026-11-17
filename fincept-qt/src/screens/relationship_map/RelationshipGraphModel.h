// src/screens/relationship_map/RelationshipGraphModel.h
// RelationshipData -> the graph the map draws: typed nodes, edges with numbers, groups with
// their totals and load state. Pure QtCore (unit-tested: tests/tst_relmap_model.cpp); the
// scene, the linked tables and the detail pane all read the same node ids, so a selection
// made in one of them is found in the others.
//
// Node ids: "co:<TICKER>" centre, "inst:<name>", "fund:<name>", "ins:<name>", "mgmt:<name>",
// "peer:<TICKER>", "hold:<SYMBOL|name>", "sector:<key>", "corr:<SYMBOL>", "sub:<name>",
// "sc:<name>", "more:<group>" (the "+ n more" cluster), "ph:<group>" (a loading / missing /
// unavailable placeholder). node_id() builds them.
#pragma once

#include "screens/relationship_map/RelationshipMapData.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <limits>

namespace fincept::relmap {

/// The groups of the map, in their default display order.
enum class RelGroup : int {
    Institutions, ///< owners: institutional holders (an ETF: its issuer)
    Funds,        ///< owners: mutual funds / ETFs holding the company
    Insiders,     ///< board members and other insiders (not officers)
    Management,   ///< officers, with their own insider holding when on Yahoo's roster
    Peers,        ///< competitors (same industry)
    Holdings,     ///< an ETF's / fund's top holdings
    Sectors,      ///< an ETF's sector weights
    Correlated,   ///< benchmark index and sector ETF, by daily-return correlation
    Subsidiaries, ///< SEC Exhibit 21
    SupplyChain,  ///< customers / suppliers (no free source: a designed placeholder)
    Count
};
inline constexpr int kRelGroupCount = static_cast<int>(RelGroup::Count);

/// The type filter chips of the command strip (OWNERS · INSIDERS · MANAGEMENT · PEERS ·
/// HOLDINGS / FUNDS · SUBSIDIARIES · CORRELATED · SUPPLY CHAIN).
enum class RelChip : int { Owners, Insiders, Management, Peers, HoldingsFunds, Subsidiaries, Correlated, SupplyChain, Count };
inline constexpr int kRelChipCount = static_cast<int>(RelChip::Count);
RelChip chip_of(RelGroup g);
/// Lower-case id of a chip / group (deep links, InputMemory): "owners", "insiders", …
QString chip_key(RelChip c);
QString group_key(RelGroup g);
bool group_from_key(const QString& key, RelGroup* out);
/// The contract section a group's data comes from (relmap::section ids).
QString section_of(RelGroup g);

enum class RelNodeKind : int {
    Center,      ///< the company / fund the map is about
    Company,     ///< a listed company (peer, holding)
    Institution, ///< asset manager, bank, pension fund
    Fund,        ///< mutual fund / ETF holding the company
    Person,      ///< insider / officer
    Etf,         ///< an ETF (correlated sector ETF)
    Index,       ///< a benchmark index
    Sector,      ///< a sector bucket (ETF sector weights)
    Entity,      ///< a subsidiary / unlisted entity
    Cluster,     ///< "+ n more"
    Placeholder, ///< loading / not loaded / not available
};

/// What the edge thickness and the edge labels show.
enum class RelEdgeMetric : int { Percent, Value, Change, Correlation };
QString metric_key(RelEdgeMetric m); ///< "pct" | "value" | "change" | "corr"

struct RelGraphNode {
    QString id;
    RelNodeKind kind = RelNodeKind::Entity;
    RelGroup group = RelGroup::Count; ///< Count for the centre
    QString label;       ///< full name
    QString short_label; ///< ticker / abbreviated name for small cards
    QString symbol;      ///< tradable Yahoo symbol ("" = not listed / unknown)
    QString subtitle;    ///< title, jurisdiction, sector, industry
    int rank = -1;       ///< 0 = largest in its group
    double size_value = std::numeric_limits<double>::quiet_NaN(); ///< magnitude for sizing (cap / position value)
    QString size_currency;
    double pct = std::numeric_limits<double>::quiet_NaN();    ///< % held / fund weight (percent)
    double value = std::numeric_limits<double>::quiet_NaN();  ///< position value (major units of value_currency)
    QString value_currency;
    double change = std::numeric_limits<double>::quiet_NaN(); ///< position change % / 52-week change % / 1y return %
    double corr = std::numeric_limits<double>::quiet_NaN();   ///< ρ with the centre
    double shares = std::numeric_limits<double>::quiet_NaN();
    QString date; ///< as-of of the relationship (date reported / last transaction)
    QString note; ///< last transaction kind ("buy", "sell", "grant"…), jurisdiction …
    int hidden_count = 0;   ///< Cluster: rows folded into it (expandable)
    int universe_count = 0; ///< Cluster: entries beyond the data (Yahoo's institution count …)
    double hidden_pct = std::numeric_limits<double>::quiet_NaN(); ///< Cluster: % held by the folded rows
    bool user_holds = false;   ///< in one of the user's portfolios
    bool in_watchlist = false; ///< in one of the user's watchlists
    RelSectionState state = RelSectionState::Ok; ///< Placeholder: why it is there
    QString state_note;                           ///< Placeholder: the section's sentence
};

struct RelGraphEdge {
    QString from;
    QString to;
    RelGroup group = RelGroup::Count;
    bool directed = false;   ///< owner -> company, fund -> holding, parent -> subsidiary
    bool second_hop = false; ///< between two related entities (depth 2)
    QString hop_kind;        ///< second hop: "corr" | "family" | "person"
    double pct = std::numeric_limits<double>::quiet_NaN();
    double value = std::numeric_limits<double>::quiet_NaN();
    double change = std::numeric_limits<double>::quiet_NaN();
    double corr = std::numeric_limits<double>::quiet_NaN();
    QString currency; ///< of `value`
};

struct RelGraphGroup {
    RelGroup group = RelGroup::Count;
    RelSectionStatus status;
    QStringList node_ids; ///< rank order, then the cluster / placeholder
    int shown = 0;        ///< real entities drawn
    int total_rows = 0;   ///< entities in the data
    int universe = 0;     ///< entities the source says exist (0 = unknown)
    double total_pct = std::numeric_limits<double>::quiet_NaN(); ///< percent held by the whole group (source total)
    double rows_pct = std::numeric_limits<double>::quiet_NaN();  ///< percent held by the listed rows
    double rows_value = std::numeric_limits<double>::quiet_NaN();
    QString value_currency;
    bool expandable = false; ///< more rows in the data than shown
};

struct RelGraphOptions {
    int top_n = 8;
    std::array<bool, kRelChipCount> chips{}; ///< visible chips (all true by default)
    QSet<int> expanded_groups;              ///< RelGroup values shown in full
    int depth = 1;                          ///< 2 = second-hop links among the related entities
    double second_hop_corr = 0.6;           ///< peer <-> peer link from this ρ
    QSet<QString> user_holdings;            ///< upper-case Yahoo symbols in the user's portfolios
    QSet<QString> watchlist;                ///< upper-case Yahoo symbols in the user's watchlists
    RelGraphOptions() { chips.fill(true); }
};

struct RelGraphModel {
    QString center_id;
    QVector<RelGraphNode> nodes; ///< nodes[0] = centre
    QVector<RelGraphEdge> edges;
    QVector<RelGraphGroup> groups; ///< display order
    QHash<QString, int> index;     ///< id -> nodes index

    const RelGraphNode* node(const QString& id) const {
        const auto it = index.constFind(id);
        return it == index.constEnd() ? nullptr : &nodes[it.value()];
    }
    const RelGraphGroup* group(RelGroup g) const {
        for (const auto& gr : groups)
            if (gr.group == g)
                return &gr;
        return nullptr;
    }
    int real_node_count() const; ///< entities (no clusters / placeholders), centre included
};

QString node_id(RelGroup g, const QString& key);
QString center_node_id(const QString& ticker);

RelGraphModel build_graph_model(const RelationshipData& d, const RelGraphOptions& opts);

/// Entities per chip in the data (the counts on the filter chips); -1 = section still loading.
int chip_count(const RelationshipData& d, RelChip c);

/// The value an edge carries for `m` (NaN when it has none).
double edge_metric_value(const RelGraphEdge& e, RelEdgeMetric m);
/// The metric a group's edges fall back to when `m` means nothing for them (peers have no "% held").
RelEdgeMetric effective_metric(RelGroup g, RelEdgeMetric m);
/// Thickness weights 0..1 per edge for `m` (normalised within each group; second hops by ρ).
QVector<double> edge_weights(const RelGraphModel& model, RelEdgeMetric m);

/// "Mr. Timothy D. Cook" and "COOK TIMOTHY D" are the same person (surname + first initial).
bool same_person(const QString& officer_name, const QString& roster_name);
/// "VANGUARD INDEX FUNDS-Vanguard 500 Index Fund" -> "Vanguard 500 Index Fund".
QString holder_display_name(const QString& name);
/// First word of a manager / fund family ("Vanguard", "Blackrock", "iShares" -> "Blackrock").
QString family_key(const QString& name);

} // namespace fincept::relmap
