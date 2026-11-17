// src/services/relationship_map/RelationshipMapParse.h
// relationship_map.py output -> RelationshipData. Pure QtCore: the service runs it on a
// worker thread; tests/tst_relmap_model.cpp runs it on captured payloads.
//
// CONTRACT (schema 2; every schema-1 key is unchanged, so a v1 payload still parses):
//   stdout, one JSON line:
//     { "schema": 2, "fetched_at": "2026-11-17T10:00:00Z",
//       "company": {… v1 fields …, "quote_type", "float_shares"},
//       "governance", "technicals", "short_interest", "enterprise", "margins",
//       "analyst_targets", "recommendations_summary", "upgrades_downgrades", "officers",
//       "calendar"                                                           (v1, unchanged)
//       "major_holders": {"insiders_pct", "institutions_pct", "institutions_float_pct",
//                         "institutions_count"}                              (fractions)
//       "institutional_holders" / "mutualfund_holders":
//           [{name, shares, value, percentage, change_percent, date_reported, fund_family, type}]
//       "insider_holders": [{name, title, shares, percentage, last_transaction,
//                            last_transaction_text, last_date}]
//       "insider_transactions": [{date, insider, position, text, kind, shares, value, price,
//                                 currency, ownership}]                     newest first
//       "insider_summary": {window, purchases_shares, purchases_count, sales_shares,
//                           sales_count, net_shares, net_pct, total_insider_shares,
//                           buy_value, sell_value, currency}
//       "peers": [{… v1 fields …, industry, country, operating_margins, revenue,
//                  financial_currency}],  "peer_source"
//       "correlation": {window, interval, as_of, observations, symbols[], labels{}, roles{},
//                       matrix[[…]] (null = n/a), returns{}, beta{}, spark{sym: [..]}}
//       "fund": {family, category, legal_type, total_assets,
//                holdings: [{symbol, name, weight}], sector_weights: [{sector, weight}],
//                asset_classes: {…}}
//       "subsidiaries": [{name, jurisdiction}], "subsidiaries_filing": {form, filed, accession, url}
//       "supply_chain": []
//       "sections": {<id>: {state, source, as_of, ms, count, note}}   ids: see relmap::section
//           state: ok | empty | timeout | skipped | error | unavailable | not_applicable
//       "partial", "note", "missing_sections", "elapsed_s", "data_quality" }   (v1)
//     or {"error": "<plain sentence>"}.
//   stderr, while running (one line each, after a newline):
//     @@RELMAP {"event": "plan", "ticker": "AAPL", "sections": ["profile", "holders", …]}
//     @@RELMAP {"event": "section", "ticker": "AAPL", "id": "holders", "status": {…},
//               "data": {<top-level keys of that section, same shapes as above>}}
//   Arguments: relationship_map.py <TICKER> [--sections id,id,…]  (a retry of some sections:
//     the reply carries "company" plus those sections; merge_retry() folds it in).
#pragma once

#include "screens/relationship_map/RelationshipMapData.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace fincept::relmap {

struct RelParseOutcome {
    bool ok = false;
    QString error; ///< plain sentence when !ok
    RelationshipData data;
};

/// A whole stdout payload. Fails on invalid JSON, an {"error"} envelope, or a company
/// with neither a price nor a market cap (yfinance's answer for an unknown symbol).
RelParseOutcome parse_payload(const QByteArray& json, const QString& requested_ticker = QString());

/// Fill `d` from whichever top-level keys `root` carries (a payload or a section's data).
void apply_payload_keys(RelationshipData& d, const QJsonObject& root);

/// Derived fields after keys were applied: section states of a v1 payload, calendar events,
/// the relative-value signal, partial flag. Idempotent.
void finalize_data(RelationshipData& d);

/// One "@@RELMAP {…}" stderr line. False for any other line (library chatter).
struct RelStreamEvent {
    QString kind; ///< "plan" | "section"
    QString ticker;
    QString id;
    QStringList planned;
    RelSectionStatus status;
    QJsonObject data;
};
bool parse_stream_line(const QString& line, RelStreamEvent* out);

/// Apply a streamed section to the data being built (keys + its status).
void apply_stream_event(RelationshipData& d, const RelStreamEvent& ev);

/// The top-level payload keys that belong to a section (the "data" of its stream event, and
/// what a retry of it replaces):
///   profile      company governance technicals short_interest enterprise margins
///   holders      major_holders institutional_holders mutualfund_holders
///   insiders     insider_holders insider_transactions insider_summary
///   management   officers
///   analysts     analyst_targets recommendations_summary upgrades_downgrades
///   calendar     calendar
///   peers        peers peer_source
///   correlation  correlation
///   fund         fund
///   subsidiaries subsidiaries subsidiaries_filing
///   supply_chain supply_chain
QStringList section_keys(const QString& section_id);

/// A full payload (`base`, the cached run) with the sections `ids` taken from a
/// `--sections` retry reply: the keys of each section that came back ok / empty replace the
/// base's, its status replaces the base's (a still-failing section keeps the base's data and
/// gets the new status), and partial / note / missing_sections are recomputed. JSON in, JSON
/// out, so the merged payload is cached like any other run.
QJsonObject merge_retry_json(const QJsonObject& base, const QJsonObject& retry, const QStringList& ids);

/// Section state <-> contract text.
RelSectionState section_state_from_text(const QString& text);
QString section_state_text(RelSectionState s);
/// Section ids whose last run failed (timeout / skipped / error) — what RETRY re-fetches.
QStringList failed_sections(const RelationshipData& d);

/// The relative-value heuristic (company P/E vs the median of same-currency peers).
ValuationSignal compute_valuation(const CompanyInfo& co, const QVector<PeerCompany>& peers);

} // namespace fincept::relmap
