// src/screens/relationship_map/RelationshipGraphModel.cpp
#include "screens/relationship_map/RelationshipGraphModel.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace fincept::relmap {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

bool rgm_finite(double v) {
    return std::isfinite(v);
}

/// Name tokens without honorifics / suffixes, lower case: "Mr. Timothy D. Cook" -> timothy d cook.
QStringList rgm_name_tokens(const QString& name) {
    static const QSet<QString> kDrop = {QStringLiteral("mr"),  QStringLiteral("ms"),  QStringLiteral("mrs"),
                                        QStringLiteral("dr"),  QStringLiteral("jr"),  QStringLiteral("sr"),
                                        QStringLiteral("ii"),  QStringLiteral("iii"), QStringLiteral("iv"),
                                        QStringLiteral("cpa"), QStringLiteral("phd"), QStringLiteral("esq"),
                                        QStringLiteral("prof"), QStringLiteral("sir"), QStringLiteral("dame")};
    QString s = name.toLower();
    s.replace(QRegularExpression(QStringLiteral("[^a-z\\s]")), QStringLiteral(" "));
    QStringList out;
    for (const QString& t : s.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        if (!kDrop.contains(t))
            out << t;
    return out;
}

QString rgm_group_section(RelGroup g) {
    switch (g) {
        case RelGroup::Institutions:
        case RelGroup::Funds:
            return QString::fromLatin1(section::kHolders);
        case RelGroup::Insiders:
            return QString::fromLatin1(section::kInsiders);
        case RelGroup::Management:
            return QString::fromLatin1(section::kManagement);
        case RelGroup::Peers:
            return QString::fromLatin1(section::kPeers);
        case RelGroup::Holdings:
        case RelGroup::Sectors:
            return QString::fromLatin1(section::kFund);
        case RelGroup::Correlated:
            return QString::fromLatin1(section::kCorrelation);
        case RelGroup::Subsidiaries:
            return QString::fromLatin1(section::kSubsidiaries);
        case RelGroup::SupplyChain:
            return QString::fromLatin1(section::kSupplyChain);
        case RelGroup::Count:
            break;
    }
    return {};
}

/// A group's rows before top-N: the nodes and the metric that ranks them.
struct RgmRows {
    QVector<RelGraphNode> nodes;
    bool owner = false;  ///< edge points at the centre
    bool outward = false; ///< edge points away from the centre
};

void rgm_sort_desc(QVector<RelGraphNode>& rows, double RelGraphNode::*key) {
    std::stable_sort(rows.begin(), rows.end(), [key](const RelGraphNode& a, const RelGraphNode& b) {
        const double va = a.*key, vb = b.*key;
        const bool fa = rgm_finite(va), fb = rgm_finite(vb);
        if (fa != fb)
            return fa; // known values first
        return fa && va > vb;
    });
}

RelGraphEdge rgm_edge_for(const RelGraphNode& n, const QString& center, bool owner, bool outward) {
    RelGraphEdge e;
    e.from = owner ? n.id : center;
    e.to = owner ? center : n.id;
    e.group = n.group;
    e.directed = owner || outward;
    e.pct = n.pct;
    e.value = n.value;
    e.change = n.change;
    e.corr = n.corr;
    e.currency = n.value_currency;
    return e;
}

} // namespace

// ── Keys ────────────────────────────────────────────────────────────────────

RelChip chip_of(RelGroup g) {
    switch (g) {
        case RelGroup::Institutions:
            return RelChip::Owners;
        case RelGroup::Insiders:
            return RelChip::Insiders;
        case RelGroup::Management:
            return RelChip::Management;
        case RelGroup::Peers:
            return RelChip::Peers;
        case RelGroup::Funds:
        case RelGroup::Holdings:
        case RelGroup::Sectors:
            return RelChip::HoldingsFunds;
        case RelGroup::Subsidiaries:
            return RelChip::Subsidiaries;
        case RelGroup::Correlated:
            return RelChip::Correlated;
        case RelGroup::SupplyChain:
        case RelGroup::Count:
            break;
    }
    return RelChip::SupplyChain;
}

QString chip_key(RelChip c) {
    static const char* const kKeys[] = {"owners",       "insiders",     "management", "peers",
                                        "holdings",     "subsidiaries", "correlated", "supply_chain"};
    const int i = static_cast<int>(c);
    return i >= 0 && i < kRelChipCount ? QString::fromLatin1(kKeys[i]) : QString();
}

QString group_key(RelGroup g) {
    static const char* const kKeys[] = {"institutions", "funds",    "insiders",   "management",   "peers",
                                        "holdings",     "sectors",  "correlated", "subsidiaries", "supply_chain"};
    const int i = static_cast<int>(g);
    return i >= 0 && i < kRelGroupCount ? QString::fromLatin1(kKeys[i]) : QString();
}

bool group_from_key(const QString& key, RelGroup* out) {
    for (int i = 0; i < kRelGroupCount; ++i) {
        if (group_key(static_cast<RelGroup>(i)) == key) {
            if (out)
                *out = static_cast<RelGroup>(i);
            return true;
        }
    }
    return false;
}

QString section_of(RelGroup g) {
    return rgm_group_section(g);
}

QString metric_key(RelEdgeMetric m) {
    switch (m) {
        case RelEdgeMetric::Percent:
            return QStringLiteral("pct");
        case RelEdgeMetric::Value:
            return QStringLiteral("value");
        case RelEdgeMetric::Change:
            return QStringLiteral("change");
        case RelEdgeMetric::Correlation:
            return QStringLiteral("corr");
    }
    return QStringLiteral("pct");
}

QString node_id(RelGroup g, const QString& key) {
    static const char* const kPrefix[] = {"inst:", "fund:", "ins:", "mgmt:", "peer:",
                                          "hold:", "sector:", "corr:", "sub:", "sc:"};
    const int i = static_cast<int>(g);
    if (i < 0 || i >= kRelGroupCount)
        return key;
    return QString::fromLatin1(kPrefix[i]) + key;
}

QString center_node_id(const QString& ticker) {
    return QStringLiteral("co:") + ticker.trimmed().toUpper();
}

int RelGraphModel::real_node_count() const {
    int n = 0;
    for (const auto& node : nodes)
        if (node.kind != RelNodeKind::Cluster && node.kind != RelNodeKind::Placeholder)
            ++n;
    return n;
}

// ── Names ───────────────────────────────────────────────────────────────────

bool same_person(const QString& officer_name, const QString& roster_name) {
    const QStringList o = rgm_name_tokens(officer_name);
    const QStringList r = rgm_name_tokens(roster_name);
    if (o.size() < 2 || r.size() < 2)
        return false;
    // Officers read "First [Middle] Last"; Yahoo's roster "LAST FIRST [MIDDLE]".
    const QString& last = o.last();
    const QString& first = o.first();
    if (r.first() != last)
        return false;
    for (int i = 1; i < r.size(); ++i)
        if (!r[i].isEmpty() && r[i].at(0) == first.at(0))
            return true;
    return false;
}

QString holder_display_name(const QString& name) {
    // Fund names lead with the trust / family ("VANGUARD INDEX FUNDS-Vanguard 500 Index Fund",
    // "iShares Trust-iShares Core S&P Small-Cap ETF"): a prefix cut made every fund of one
    // family read the same. Other names with a dash ("Coca-Cola") stay whole.
    const int dash = static_cast<int>(name.indexOf(QLatin1Char('-')));
    if (dash <= 0 || dash >= name.size() - 1)
        return name.trimmed();
    const QString family = name.left(dash).trimmed();
    if (!family.contains(QLatin1Char(' ')))
        return name.trimmed();
    static const QRegularExpression kFamilyEnd(
        QStringLiteral("\\b(trust|funds?|series|portfolios?|investment trust|etf trust|index funds)$"),
        QRegularExpression::CaseInsensitiveOption);
    if (family == family.toUpper() || kFamilyEnd.match(family).hasMatch())
        return name.mid(dash + 1).trimmed();
    return name.trimmed();
}

QString family_key(const QString& name) {
    static const QHash<QString, QString> kAlias = {
        {QStringLiteral("ishares"), QStringLiteral("blackrock")},
        {QStringLiteral("blackrock"), QStringLiteral("blackrock")},
        {QStringLiteral("spdr"), QStringLiteral("state street")},
        {QStringLiteral("state"), QStringLiteral("state street")},
        {QStringLiteral("fmr"), QStringLiteral("fidelity")},
        {QStringLiteral("fidelity"), QStringLiteral("fidelity")},
        {QStringLiteral("vanguard"), QStringLiteral("vanguard")},
        {QStringLiteral("invesco"), QStringLiteral("invesco")},
        {QStringLiteral("schwab"), QStringLiteral("schwab")},
        {QStringLiteral("geode"), QStringLiteral("geode")},
        {QStringLiteral("jpmorgan"), QStringLiteral("jpmorgan")},
        {QStringLiteral("american"), QStringLiteral("capital group")},
        {QStringLiteral("capital"), QStringLiteral("capital group")},
        {QStringLiteral("growth"), QStringLiteral("capital group")},
        {QStringLiteral("t"), QStringLiteral("t rowe price")},
    };
    QString s = name.toLower();
    s.replace(QRegularExpression(QStringLiteral("[^a-z\\s]")), QStringLiteral(" "));
    const QStringList toks = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (toks.isEmpty())
        return {};
    const auto it = kAlias.constFind(toks.first());
    return it != kAlias.constEnd() ? it.value() : toks.first();
}

// ── Metrics ─────────────────────────────────────────────────────────────────

double edge_metric_value(const RelGraphEdge& e, RelEdgeMetric m) {
    switch (m) {
        case RelEdgeMetric::Percent:
            return e.pct;
        case RelEdgeMetric::Value:
            return e.value;
        case RelEdgeMetric::Change:
            return e.change;
        case RelEdgeMetric::Correlation:
            return e.corr;
    }
    return kNaN;
}

RelEdgeMetric effective_metric(RelGroup g, RelEdgeMetric m) {
    switch (g) {
        case RelGroup::Institutions:
        case RelGroup::Funds:
        case RelGroup::Insiders:
        case RelGroup::Management:
            return m == RelEdgeMetric::Correlation ? RelEdgeMetric::Percent : m;
        case RelGroup::Peers:
            return m == RelEdgeMetric::Percent ? RelEdgeMetric::Correlation : m;
        case RelGroup::Holdings:
        case RelGroup::Sectors:
            return (m == RelEdgeMetric::Value) ? m : RelEdgeMetric::Percent;
        case RelGroup::Correlated:
            return m == RelEdgeMetric::Change ? m : RelEdgeMetric::Correlation;
        case RelGroup::Subsidiaries:
        case RelGroup::SupplyChain:
        case RelGroup::Count:
            break;
    }
    return m;
}

QVector<double> edge_weights(const RelGraphModel& model, RelEdgeMetric m) {
    const int n = static_cast<int>(model.edges.size());
    QVector<double> raw(n, kNaN), out(n, 0.35);
    QVector<RelEdgeMetric> used(n, m);
    for (int i = 0; i < n; ++i) {
        const RelGraphEdge& e = model.edges[i];
        if (e.second_hop) {
            out[i] = rgm_finite(e.corr) ? std::clamp(e.corr, 0.0, 1.0) : 0.3;
            raw[i] = kNaN;
            continue;
        }
        // The metric asked for, the group's stand-in for it, then whatever the edge carries.
        const RelEdgeMetric chain[] = {m, effective_metric(e.group, m), RelEdgeMetric::Percent, RelEdgeMetric::Value,
                                       RelEdgeMetric::Correlation, RelEdgeMetric::Change};
        for (RelEdgeMetric c : chain) {
            const double v = edge_metric_value(e, c);
            if (rgm_finite(v)) {
                raw[i] = v;
                used[i] = c;
                break;
            }
        }
    }
    // Normalise within the group (and the metric actually used).
    QHash<int, double> max_abs;
    for (int i = 0; i < n; ++i) {
        if (!rgm_finite(raw[i]))
            continue;
        const int key = static_cast<int>(model.edges[i].group) * 8 + static_cast<int>(used[i]);
        max_abs[key] = std::max(max_abs.value(key, 0.0), std::abs(raw[i]));
    }
    for (int i = 0; i < n; ++i) {
        if (!rgm_finite(raw[i]))
            continue;
        const int key = static_cast<int>(model.edges[i].group) * 8 + static_cast<int>(used[i]);
        const double mx = max_abs.value(key, 0.0);
        if (used[i] == RelEdgeMetric::Correlation) {
            out[i] = std::clamp(raw[i], 0.0, 1.0);
        } else if (mx > 0) {
            // Square root: holder values span three orders of magnitude.
            out[i] = std::sqrt(std::abs(raw[i]) / mx);
        } else {
            out[i] = 0.0;
        }
    }
    return out;
}

// ── Counts ──────────────────────────────────────────────────────────────────

namespace {

QVector<int> rgm_insiders_not_officers(const RelationshipData& d) {
    QVector<int> out;
    for (int i = 0; i < d.insider_holders.size(); ++i) {
        bool officer = false;
        for (const auto& o : d.officers)
            if (same_person(o.name, d.insider_holders[i].name)) {
                officer = true;
                break;
            }
        if (!officer)
            out << i;
    }
    return out;
}

int rgm_correlated_count(const RelationshipData& d) {
    int n = 0;
    for (auto it = d.correlation.roles.begin(); it != d.correlation.roles.end(); ++it)
        if (it.value() == QLatin1String("index") || it.value() == QLatin1String("sector_etf"))
            ++n;
    return n;
}

} // namespace

int chip_count(const RelationshipData& d, RelChip c) {
    auto loading = [&d](const char* id) {
        return d.sections.value(QString::fromLatin1(id)).state == RelSectionState::Loading;
    };
    switch (c) {
        case RelChip::Owners:
            if (d.is_fund())
                return d.fund.family.isEmpty() ? 0 : 1;
            return d.institutional_holders.isEmpty() && loading(section::kHolders)
                       ? -1
                       : static_cast<int>(d.institutional_holders.size());
        case RelChip::Insiders:
            return d.insider_holders.isEmpty() && loading(section::kInsiders)
                       ? -1
                       : static_cast<int>(rgm_insiders_not_officers(d).size());
        case RelChip::Management:
            return d.officers.isEmpty() && loading(section::kManagement) ? -1 : static_cast<int>(d.officers.size());
        case RelChip::Peers:
            return d.peers.isEmpty() && loading(section::kPeers) ? -1 : static_cast<int>(d.peers.size());
        case RelChip::HoldingsFunds:
            if (d.is_fund())
                return d.fund.holdings.isEmpty() && loading(section::kFund) ? -1
                                                                           : static_cast<int>(d.fund.holdings.size());
            return d.mutualfund_holders.isEmpty() && loading(section::kHolders)
                       ? -1
                       : static_cast<int>(d.mutualfund_holders.size());
        case RelChip::Subsidiaries:
            return d.subsidiaries.isEmpty() && loading(section::kSubsidiaries) ? -1
                                                                              : static_cast<int>(d.subsidiaries.size());
        case RelChip::Correlated:
            return d.correlation.symbols.isEmpty() && loading(section::kCorrelation) ? -1 : rgm_correlated_count(d);
        case RelChip::SupplyChain:
            return static_cast<int>(d.supply_chain.size());
        case RelChip::Count:
            break;
    }
    return 0;
}

// ── The model ───────────────────────────────────────────────────────────────

RelGraphModel build_graph_model(const RelationshipData& d, const RelGraphOptions& opts) {
    RelGraphModel m;
    const CompanyInfo& co = d.company;
    const QString ticker = co.ticker.trimmed().toUpper();
    m.center_id = center_node_id(ticker);
    const bool fund = d.is_fund();

    auto flag_user = [&opts](RelGraphNode& n) {
        if (n.symbol.isEmpty())
            return;
        const QString s = n.symbol.toUpper();
        n.user_holds = opts.user_holdings.contains(s);
        n.in_watchlist = opts.watchlist.contains(s);
    };

    {
        RelGraphNode c;
        c.id = m.center_id;
        c.kind = RelNodeKind::Center;
        c.label = co.name.isEmpty() ? ticker : co.name;
        c.short_label = ticker;
        c.symbol = ticker;
        c.subtitle = fund ? d.fund.category : (co.industry.isEmpty() ? co.sector : co.industry);
        c.size_value = fund ? d.fund.total_assets : co.market_cap;
        c.size_currency = co.currency;
        c.change = co.day_change_pct;
        flag_user(c);
        m.nodes.append(c);
        m.index.insert(m.center_id, 0);
    }

    const double price = co.current_price;
    const QString ccy = co.currency;

    // Rows of every group, before top-N.
    std::array<RgmRows, kRelGroupCount> rows;
    auto rows_of = [&rows](RelGroup g) -> RgmRows& { return rows[static_cast<size_t>(g)]; };

    if (fund) {
        if (!d.fund.family.isEmpty()) {
            RelGraphNode n;
            n.id = node_id(RelGroup::Institutions, d.fund.family);
            n.kind = RelNodeKind::Institution;
            n.group = RelGroup::Institutions;
            n.label = d.fund.family;
            n.short_label = d.fund.family;
            n.subtitle = QStringLiteral("issuer");
            n.value = d.fund.total_assets > 0 ? d.fund.total_assets : kNaN;
            n.value_currency = ccy;
            n.size_value = n.value;
            n.size_currency = ccy;
            n.note = QStringLiteral("manages");
            rows_of(RelGroup::Institutions).nodes << n;
        }
        rows_of(RelGroup::Institutions).owner = true;
        for (const auto& h : d.fund.holdings) {
            RelGraphNode n;
            const QString key = h.symbol.isEmpty() ? h.name : h.symbol;
            n.id = node_id(RelGroup::Holdings, key);
            n.kind = RelNodeKind::Company;
            n.group = RelGroup::Holdings;
            n.label = h.name.isEmpty() ? h.symbol : h.name;
            n.short_label = h.symbol.isEmpty() ? h.name : h.symbol;
            n.symbol = h.symbol;
            n.pct = h.weight * 100.0;
            if (d.fund.total_assets > 0) {
                n.value = h.weight * d.fund.total_assets;
                n.value_currency = ccy;
                n.size_value = n.value;
                n.size_currency = ccy;
            }
            n.corr = d.correlation.value(ticker, h.symbol);
            flag_user(n);
            rows_of(RelGroup::Holdings).nodes << n;
        }
        rows_of(RelGroup::Holdings).outward = true;
        rgm_sort_desc(rows_of(RelGroup::Holdings).nodes, &RelGraphNode::pct);
        for (const auto& [key, weight] : d.fund.sector_weights) {
            if (weight <= 0)
                continue;
            RelGraphNode n;
            n.id = node_id(RelGroup::Sectors, key);
            n.kind = RelNodeKind::Sector;
            n.group = RelGroup::Sectors;
            // Yahoo's sector keys ("realestate", "consumer_cyclical") as readable names.
            static const QHash<QString, QString> kSectorNames = {
                {QStringLiteral("realestate"), QStringLiteral("Real estate")},
                {QStringLiteral("real_estate"), QStringLiteral("Real estate")},
                {QStringLiteral("consumer_cyclical"), QStringLiteral("Consumer cyclical")},
                {QStringLiteral("consumer_defensive"), QStringLiteral("Consumer defensive")},
                {QStringLiteral("basic_materials"), QStringLiteral("Basic materials")},
                {QStringLiteral("communication_services"), QStringLiteral("Communication services")},
                {QStringLiteral("financial_services"), QStringLiteral("Financial services")},
            };
            QString label = kSectorNames.value(key.toLower());
            if (label.isEmpty()) {
                label = key;
                label.replace(QLatin1Char('_'), QLatin1Char(' '));
                if (!label.isEmpty())
                    label[0] = label[0].toUpper();
            }
            n.label = label;
            n.short_label = label;
            n.pct = weight * 100.0;
            if (d.fund.total_assets > 0) {
                n.value = weight * d.fund.total_assets;
                n.value_currency = ccy;
            }
            rows_of(RelGroup::Sectors).nodes << n;
        }
        rows_of(RelGroup::Sectors).outward = true;
        rgm_sort_desc(rows_of(RelGroup::Sectors).nodes, &RelGraphNode::pct);
    } else {
        // Owners.
        auto holder_rows = [&](const QVector<InstitutionalHolder>& list, RelGroup g, RelNodeKind kind) {
            RgmRows& r = rows_of(g);
            r.owner = true;
            for (const auto& h : list) {
                RelGraphNode n;
                n.id = node_id(g, h.name);
                n.kind = kind;
                n.group = g;
                n.label = holder_display_name(h.name);
                n.short_label = n.label;
                n.pct = h.percentage;
                n.value = h.value > 0 ? h.value : kNaN;
                n.value_currency = ccy;
                n.size_value = n.value;
                n.size_currency = ccy;
                n.change = h.change_percent;
                n.shares = h.shares > 0 ? h.shares : kNaN;
                n.date = h.date_reported;
                n.subtitle = h.name != n.label ? h.name.left(h.name.indexOf(QLatin1Char('-'))).trimmed() : QString();
                r.nodes << n;
            }
            rgm_sort_desc(r.nodes, &RelGraphNode::pct);
        };
        holder_rows(d.institutional_holders, RelGroup::Institutions, RelNodeKind::Institution);
        holder_rows(d.mutualfund_holders, RelGroup::Funds, RelNodeKind::Fund);

        // Management: officers with their own roster holding; insiders: everyone else on the roster.
        QSet<int> matched;
        {
            RgmRows& r = rows_of(RelGroup::Management);
            r.owner = false;
            for (const auto& o : d.officers) {
                RelGraphNode n;
                n.id = node_id(RelGroup::Management, o.name);
                n.kind = RelNodeKind::Person;
                n.group = RelGroup::Management;
                n.label = o.name;
                n.short_label = o.name;
                n.subtitle = o.title;
                if (o.total_pay > 0) {
                    n.size_value = o.total_pay;
                    n.size_currency = co.financial_currency.isEmpty() ? ccy : co.financial_currency;
                }
                for (int i = 0; i < d.insider_holders.size(); ++i) {
                    if (matched.contains(i) || !same_person(o.name, d.insider_holders[i].name))
                        continue;
                    const InsiderHolder& ih = d.insider_holders[i];
                    matched.insert(i);
                    n.shares = ih.shares > 0 ? ih.shares : kNaN;
                    n.pct = ih.percentage > 0 ? ih.percentage : kNaN;
                    if (ih.shares > 0 && price > 0) {
                        n.value = ih.shares * price;
                        n.value_currency = ccy;
                    }
                    n.note = ih.last_transaction;
                    n.date = ih.last_date;
                    break;
                }
                r.nodes << n;
            }
            // Yahoo's officer order (key executives first) is kept.
        }
        {
            RgmRows& r = rows_of(RelGroup::Insiders);
            r.owner = true;
            for (int i = 0; i < d.insider_holders.size(); ++i) {
                if (matched.contains(i))
                    continue;
                // An insider who is an officer but whose officer row was matched to someone
                // else still belongs to management: never draw one person twice.
                bool officer = false;
                for (const auto& o : d.officers)
                    if (same_person(o.name, d.insider_holders[i].name))
                        officer = true;
                if (officer)
                    continue;
                const InsiderHolder& ih = d.insider_holders[i];
                RelGraphNode n;
                n.id = node_id(RelGroup::Insiders, ih.name);
                n.kind = RelNodeKind::Person;
                n.group = RelGroup::Insiders;
                n.label = ih.name;
                n.short_label = ih.name;
                n.subtitle = ih.title;
                n.shares = ih.shares > 0 ? ih.shares : kNaN;
                n.pct = ih.percentage > 0 ? ih.percentage : kNaN;
                if (ih.shares > 0 && price > 0) {
                    n.value = ih.shares * price;
                    n.value_currency = ccy;
                    n.size_value = n.value;
                    n.size_currency = ccy;
                }
                n.note = ih.last_transaction;
                n.date = ih.last_date;
                r.nodes << n;
            }
            rgm_sort_desc(r.nodes, &RelGraphNode::shares);
        }
        // Subsidiaries (filing order).
        for (const auto& s : d.subsidiaries) {
            RelGraphNode n;
            n.id = node_id(RelGroup::Subsidiaries, s.name);
            n.kind = RelNodeKind::Entity;
            n.group = RelGroup::Subsidiaries;
            n.label = s.name;
            n.short_label = s.name;
            n.subtitle = s.jurisdiction;
            n.note = s.jurisdiction;
            n.date = d.subsidiaries_filing.filed;
            rows_of(RelGroup::Subsidiaries).nodes << n;
        }
        rows_of(RelGroup::Subsidiaries).outward = true;
        for (const auto& s : d.supply_chain) {
            RelGraphNode n;
            n.id = node_id(RelGroup::SupplyChain, s.name);
            n.kind = RelNodeKind::Company;
            n.group = RelGroup::SupplyChain;
            n.label = s.name;
            n.short_label = s.name;
            n.subtitle = s.relationship;
            n.pct = s.revenue_pct > 0 ? s.revenue_pct : kNaN;
            rows_of(RelGroup::SupplyChain).nodes << n;
        }
    }

    // Peers (companies; ETFs have none unless the script found some).
    for (const auto& p : d.peers) {
        RelGraphNode n;
        n.id = node_id(RelGroup::Peers, p.ticker);
        n.kind = RelNodeKind::Company;
        n.group = RelGroup::Peers;
        n.label = p.name.isEmpty() ? p.ticker : p.name;
        n.short_label = p.ticker;
        n.symbol = p.ticker;
        n.subtitle = p.industry.isEmpty() ? p.sector : p.industry;
        n.size_value = p.market_cap > 0 ? p.market_cap : kNaN;
        n.size_currency = p.currency;
        n.value = n.size_value; // the peer's market cap: "value" of the edge
        n.value_currency = p.currency;
        n.change = p.week52_change != 0 ? p.week52_change * 100.0 : kNaN;
        n.corr = d.correlation.value(ticker, p.ticker);
        flag_user(n);
        rows_of(RelGroup::Peers).nodes << n;
    }
    rgm_sort_desc(rows_of(RelGroup::Peers).nodes, &RelGraphNode::size_value);

    // Correlated: the benchmark index and the sector ETF.
    for (const QString& sym : d.correlation.symbols) {
        const QString role = d.correlation.roles.value(sym);
        if (role != QLatin1String("index") && role != QLatin1String("sector_etf"))
            continue;
        RelGraphNode n;
        n.id = node_id(RelGroup::Correlated, sym);
        n.kind = role == QLatin1String("index") ? RelNodeKind::Index : RelNodeKind::Etf;
        n.group = RelGroup::Correlated;
        n.label = d.correlation.labels.value(sym, sym);
        n.short_label = sym;
        n.symbol = sym;
        n.subtitle = role == QLatin1String("index") ? QStringLiteral("benchmark index") : QStringLiteral("sector ETF");
        n.corr = d.correlation.value(ticker, sym);
        const double r = d.correlation.returns.value(sym, kNaN);
        n.change = rgm_finite(r) ? r * 100.0 : kNaN;
        n.note = role;
        flag_user(n);
        rows_of(RelGroup::Correlated).nodes << n;
    }
    rgm_sort_desc(rows_of(RelGroup::Correlated).nodes, &RelGraphNode::corr);

    // Groups in display order.
    const RelGroup order_company[] = {RelGroup::Institutions, RelGroup::Funds,      RelGroup::Insiders,
                                      RelGroup::Management,   RelGroup::Peers,      RelGroup::Correlated,
                                      RelGroup::Subsidiaries, RelGroup::SupplyChain};
    const RelGroup order_fund[] = {RelGroup::Institutions, RelGroup::Sectors, RelGroup::Holdings, RelGroup::Peers,
                                   RelGroup::Correlated};
    QVector<RelGroup> order;
    if (fund)
        order = QVector<RelGroup>(std::begin(order_fund), std::end(order_fund));
    else
        order = QVector<RelGroup>(std::begin(order_company), std::end(order_company));

    const int top_n = std::max(1, opts.top_n);
    for (RelGroup g : order) {
        if (!opts.chips[static_cast<size_t>(chip_of(g))])
            continue;
        RgmRows& r = rows_of(g);
        RelGraphGroup grp;
        grp.group = g;
        grp.status = d.sections.value(rgm_group_section(g));
        if (fund && g == RelGroup::Institutions && grp.status.state == RelSectionState::Unknown)
            grp.status = d.sections.value(QString::fromLatin1(section::kFund));
        grp.total_rows = static_cast<int>(r.nodes.size());
        grp.value_currency = ccy;
        if (g == RelGroup::Institutions && !fund && d.major_holders.present) {
            grp.universe = d.major_holders.institutions_count;
            if (d.major_holders.institutions_pct > 0)
                grp.total_pct = d.major_holders.institutions_pct * 100.0;
        }
        if (g == RelGroup::Insiders && d.major_holders.present && d.major_holders.insiders_pct > 0)
            grp.total_pct = d.major_holders.insiders_pct * 100.0;

        RelSectionState st = grp.status.state;
        // The source answered but listed nobody (Yahoo has no holder list for many non-US
        // markets): say so in the group instead of dropping it.
        if (st == RelSectionState::Ok && r.nodes.isEmpty() &&
            (g == RelGroup::Institutions || g == RelGroup::Funds || g == RelGroup::Insiders || g == RelGroup::Peers)) {
            st = RelSectionState::Empty;
            grp.status.state = st;
            if (grp.status.note.isEmpty())
                grp.status.note = g == RelGroup::Peers ? QStringLiteral("No comparable companies found")
                                                       : QStringLiteral("The source lists none for this listing");
        }
        if (r.nodes.isEmpty()) {
            // Nothing to draw: a placeholder that says why, or no group at all.
            const bool show_placeholder = st == RelSectionState::Loading || section_failed(st) ||
                                          st == RelSectionState::Unavailable ||
                                          (st == RelSectionState::Empty && g != RelGroup::Sectors);
            if (!show_placeholder)
                continue;
            if (fund && (g == RelGroup::SupplyChain || g == RelGroup::Subsidiaries))
                continue;
            RelGraphNode ph;
            ph.id = QStringLiteral("ph:") + group_key(g);
            ph.kind = RelNodeKind::Placeholder;
            ph.group = g;
            ph.state = st;
            ph.state_note = grp.status.note;
            ph.label = group_key(g);
            grp.node_ids << ph.id;
            m.index.insert(ph.id, static_cast<int>(m.nodes.size()));
            m.nodes << ph;
            m.groups << grp;
            continue;
        }

        const bool expanded = opts.expanded_groups.contains(static_cast<int>(g));
        const int shown = expanded ? static_cast<int>(r.nodes.size()) : std::min<int>(top_n, static_cast<int>(r.nodes.size()));
        double rows_pct = 0, rows_value = 0;
        bool any_pct = false, any_value = false;
        for (int i = 0; i < r.nodes.size(); ++i) {
            const RelGraphNode& n = r.nodes[i];
            if (rgm_finite(n.pct)) {
                rows_pct += n.pct;
                any_pct = true;
            }
            if (rgm_finite(n.value)) {
                rows_value += n.value;
                any_value = true;
            }
        }
        if (any_pct && g != RelGroup::Peers && g != RelGroup::Correlated)
            grp.rows_pct = rows_pct;
        if (any_value && g != RelGroup::Peers)
            grp.rows_value = rows_value;
        for (int i = 0; i < shown; ++i) {
            RelGraphNode n = r.nodes[i];
            n.rank = i;
            if (m.index.contains(n.id))
                continue; // the same holder listed twice by the source
            grp.node_ids << n.id;
            m.index.insert(n.id, static_cast<int>(m.nodes.size()));
            m.edges << rgm_edge_for(n, m.center_id, r.owner, r.outward);
            m.nodes << n;
        }
        grp.shown = static_cast<int>(grp.node_ids.size());
        const int hidden = static_cast<int>(r.nodes.size()) - shown;
        const int beyond = grp.universe > grp.total_rows ? grp.universe - grp.total_rows : 0;
        if (hidden > 0 || beyond > 0) {
            RelGraphNode c;
            c.id = QStringLiteral("more:") + group_key(g);
            c.kind = RelNodeKind::Cluster;
            c.group = g;
            c.rank = shown;
            c.hidden_count = hidden;
            c.universe_count = beyond;
            double hp = 0, hv = 0;
            bool any_hp = false, any_hv = false;
            for (int i = shown; i < r.nodes.size(); ++i) {
                if (rgm_finite(r.nodes[i].pct)) {
                    hp += r.nodes[i].pct;
                    any_hp = true;
                }
                if (rgm_finite(r.nodes[i].value)) {
                    hv += r.nodes[i].value;
                    any_hv = true;
                }
            }
            // What the folded entities hold: the rows in the data, or (beyond the data) the
            // source's group total minus the listed rows.
            if (any_hp && g != RelGroup::Peers && g != RelGroup::Correlated)
                c.hidden_pct = hp;
            if (hidden == 0 && beyond > 0 && rgm_finite(grp.total_pct) && rgm_finite(grp.rows_pct))
                c.hidden_pct = std::max(0.0, grp.total_pct - grp.rows_pct);
            c.pct = c.hidden_pct;
            if (any_hv && g != RelGroup::Peers)
                c.value = hv;
            c.value_currency = ccy;
            grp.expandable = hidden > 0;
            grp.node_ids << c.id;
            m.index.insert(c.id, static_cast<int>(m.nodes.size()));
            RelGraphEdge e = rgm_edge_for(c, m.center_id, r.owner, r.outward);
            m.edges << e;
            m.nodes << c;
        }
        m.groups << grp;
    }

    // Second hop: links among the related entities already on the map.
    if (opts.depth >= 2) {
        QStringList corr_ids;
        for (const auto& n : m.nodes)
            if ((n.group == RelGroup::Peers || n.group == RelGroup::Correlated || n.group == RelGroup::Holdings) &&
                !n.symbol.isEmpty() && n.kind != RelNodeKind::Cluster)
                corr_ids << n.id;
        for (int i = 0; i < corr_ids.size(); ++i) {
            for (int j = i + 1; j < corr_ids.size(); ++j) {
                const RelGraphNode* a = m.node(corr_ids[i]);
                const RelGraphNode* b = m.node(corr_ids[j]);
                const double rho = d.correlation.value(a->symbol, b->symbol);
                if (!rgm_finite(rho) || rho < opts.second_hop_corr)
                    continue;
                RelGraphEdge e;
                e.from = a->id;
                e.to = b->id;
                e.group = a->group;
                e.second_hop = true;
                e.hop_kind = QStringLiteral("corr");
                e.corr = rho;
                m.edges << e;
            }
        }
        // Fund family: an institution and the funds it runs ("Vanguard Group" -> "Vanguard 500").
        for (const auto& inst : m.nodes) {
            if (inst.group != RelGroup::Institutions || inst.kind == RelNodeKind::Cluster ||
                inst.kind == RelNodeKind::Placeholder)
                continue;
            const QString fam = family_key(inst.label);
            if (fam.isEmpty())
                continue;
            for (const auto& f : m.nodes) {
                if (f.group != RelGroup::Funds || f.kind == RelNodeKind::Cluster || f.kind == RelNodeKind::Placeholder)
                    continue;
                if (family_key(f.label) != fam && family_key(f.subtitle) != fam)
                    continue;
                RelGraphEdge e;
                e.from = inst.id;
                e.to = f.id;
                e.group = RelGroup::Funds;
                e.second_hop = true;
                e.hop_kind = QStringLiteral("family");
                m.edges << e;
            }
        }
    }
    return m;
}

} // namespace fincept::relmap
