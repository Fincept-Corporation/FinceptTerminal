#pragma once
// Portfolio Replication — copy a source account's holdings/positions into a
// PAPER account as exact 1:1 orders. Core is UI-independent and testable:
// build_plan() is pure (no network/DB/GUI) via an injected SymbolResolver.

#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::trading::replication {

enum class ItemKind { Holding, Position };

// A normalized view of one source holding or position.
struct SourceItem {
    ItemKind kind = ItemKind::Holding;
    QString  src_symbol;        // broker-native symbol from get_holdings/get_positions
    QString  exchange;          // "NSE"/"BSE"/"NFO"... ("" → treated as NSE)
    QString  product;           // "" for holdings; "MIS"/"CNC"/"NRML" for positions
    QString  side;              // "" for holdings; "long"/"short"/"LONG"/"SHORT" for positions
    double   quantity = 0;
    double   avg_price = 0;
    double   ltp = 0;
};

// One planned paper order. Strings (side/product) match pt_place_order directly.
struct PlannedOrder {
    ItemKind kind = ItemKind::Holding;
    QString  src_symbol;
    QString  norm_symbol;       // canonical symbol used for the paper order
    QString  exchange;
    QString  side;              // "buy" | "sell"
    QString  product;           // "CNC" (holdings) | source product (positions)
    double   quantity = 0;      // floored to whole shares
    double   est_price = 0;     // ltp ?: avg_price
    double   est_value = 0;     // quantity * est_price
    bool     mapped = false;    // resolvable + tradable on target broker
    bool     included = true;   // default-unchecked when !mapped or unusable
    QString  warning;           // human-readable reason when !included/!mapped
};

struct ReplicationOptions {
    bool include_holdings = true;
    bool include_positions = true;
};

struct ReplicationPlan {
    QString               source_account_id;
    QString               target_account_id;
    QString               target_paper_portfolio_id;
    QVector<PlannedOrder> orders;
    double                required_capital = 0; // Σ est_value of *included* rows
    double                target_available = 0; // target paper balance at plan time
};

struct ReplicationResult {
    struct Row { QString symbol; bool ok = false; QString error; };
    int          placed = 0;
    int          failed = 0;
    int          skipped = 0;
    QVector<Row> rows;
};

// Resolver result: whether the symbol is tradable on the target broker, and its
// canonical (normalized) symbol to place the paper order with.
struct ResolveResult {
    bool    tradable_on_target = false;
    QString norm_symbol;
};
using SymbolResolver = std::function<ResolveResult(const QString& src_symbol, const QString& exchange)>;

// ── Source extraction ────────────────────────────────────────────────────────
// Pure converter: live broker holdings+positions → SourceItem list.
QVector<SourceItem> to_source_items(const QVector<BrokerHolding>& holdings,
                                    const QVector<BrokerPosition>& positions);
// Paper source: reads the paper engine and splits CNC/NRML-delivery → holdings,
// rest → positions (mirrors refresh_paper_panels). Reads DB on the calling thread.
QVector<SourceItem> paper_source_items(const QString& portfolio_id);

// ── Planning (PURE) ──────────────────────────────────────────────────────────
ReplicationPlan build_plan(const QVector<SourceItem>& items,
                           const ReplicationOptions& opts,
                           double target_available,
                           const QString& source_account_id,
                           const QString& target_account_id,
                           const QString& target_paper_portfolio_id,
                           const SymbolResolver& resolve);

// Production resolver backed by InstrumentService (requires both brokers'
// instruments loaded; callers should load_from_db first).
SymbolResolver make_instrument_resolver(const QString& source_broker_id,
                                        const QString& target_broker_id);

// ── Execution (GUI thread) ───────────────────────────────────────────────────
// Places each included row as a market order into the target paper portfolio.
// Paper engine throws on margin/validation failure → captured per row.
ReplicationResult execute_plan(const ReplicationPlan& plan);

} // namespace fincept::trading::replication
