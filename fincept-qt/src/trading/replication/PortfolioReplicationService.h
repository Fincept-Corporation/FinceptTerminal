#pragma once
// Portfolio Replication — copy a source account's holdings/positions into a
// PAPER account as exact 1:1 orders. Core is UI-independent and testable:
// build_plan() is pure (no network/DB/GUI) via an injected SymbolResolver.

#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <limits>
#include <optional>

namespace fincept::trading::replication {

enum class ItemKind { Holding, Position };

// A normalized view of one source holding or position.
struct SourceItem {
    ItemKind kind = ItemKind::Holding;
    QString src_symbol; // broker-native symbol from get_holdings/get_positions
    QString exchange;   // "NSE"/"BSE"/"NFO"... ("" → treated as NSE)
    QString product;    // "" for holdings; "MIS"/"CNC"/"NRML" for positions
    QString side;       // "" for holdings; "long"/"short"/"LONG"/"SHORT" for positions
    double quantity = 0;
    double avg_price = 0;
    double ltp = 0;
    // Currency avg_price / ltp are quoted in, as quoted ("INR", "USD", "GBp" pence).
    // to_source_items(): the venue table's for `exchange` (empty for a segment it
    // does not list — the caller fills in the source account's); empty = unknown.
    QString currency{};
};

// One planned paper order. Strings (side/product) match pt_place_order directly.
struct PlannedOrder {
    ItemKind kind = ItemKind::Holding;
    QString src_symbol;
    QString norm_symbol; // canonical symbol used for the paper order
    QString exchange;
    QString side;         // "buy" | "sell"
    QString product;      // "CNC" (holdings) | source product (positions)
    double quantity = 0;  // floored to whole shares
    double est_price = 0; // ltp ?: avg_price
    double est_value = 0; // quantity * est_price, in `currency` as quoted (pence for GBp)
    bool mapped = false;  // resolvable + tradable on target broker
    bool included = true; // default-unchecked when !mapped or unusable
    QString warning;      // human-readable reason when !included/!mapped

    // Currency of est_price / est_value as quoted; empty = unknown.
    QString currency;
    // est_value in the plan's target currency (major unit). NaN = not convertible
    // (no FX rate yet, or the line's currency is unknown) — never taken 1:1.
    double est_value_target = std::numeric_limits<double>::quiet_NaN();
    // Target currency per ONE price unit as quoted (GBp in a USD book: USD per
    // penny). NaN unless fx_converted.
    double fx_to_target = std::numeric_limits<double>::quiet_NaN();
    // Priced in another currency (or minor unit) than the target: the paper order
    // books its cash at fx_to_target (execute_plan sets the engine's factor).
    bool fx_converted = false;
    // Left out (included = false) because est_value could not be converted.
    bool fx_missing = false;
};

struct ReplicationOptions {
    bool include_holdings = true;
    bool include_positions = true;
};

struct ReplicationPlan {
    QString source_account_id;
    QString target_account_id;
    QString target_paper_portfolio_id;
    QVector<PlannedOrder> orders;
    double required_capital = 0; // Σ est_value_target of *included* rows (target currency)
    double target_available = 0; // target paper balance at plan time (target currency)
    // Currency of required_capital / target_available. Empty = single-currency plan:
    // values are summed as given, no conversion (legacy callers and tests).
    QString target_currency;
    // Symbols left out of required_capital because their value could not be put in
    // target_currency (no FX rate / unknown currency): the comparison is partial.
    QStringList fx_missing;
};

// Units of `target_ccy` per ONE price unit of `price_ccy` as quoted (GBp handled:
// a penny's worth), or nullopt when no rate is known — never 1.0 for two different
// currencies. Same currency -> 1. Production: PaperMarkService::fx_factor (the
// factor the paper engine books cash at), see make_fx_factor().
using FxFactor = std::function<std::optional<double>(const QString& price_ccy, const QString& target_ccy)>;

struct ReplicationResult {
    struct Row {
        QString symbol;
        bool ok = false;
        QString error;
    };
    int placed = 0;
    int failed = 0;
    int skipped = 0;
    QVector<Row> rows;
};

// Resolver result: whether the symbol is tradable on the target broker, and its
// canonical (normalized) symbol to place the paper order with.
struct ResolveResult {
    bool tradable_on_target = false;
    QString norm_symbol;
};
using SymbolResolver = std::function<ResolveResult(const QString& src_symbol, const QString& exchange)>;

// ── Source extraction ────────────────────────────────────────────────────────
// Pure converter: live broker holdings+positions → SourceItem list.
QVector<SourceItem> to_source_items(const QVector<BrokerHolding>& holdings, const QVector<BrokerPosition>& positions);
// Paper source: reads the paper engine and splits CNC/NRML-delivery → holdings,
// rest → positions (mirrors refresh_paper_panels). Reads DB on the calling thread.
QVector<SourceItem> paper_source_items(const QString& portfolio_id);

// ── Planning (PURE) ──────────────────────────────────────────────────────────
// `target_currency` = the target paper portfolio's currency (target_available is in
// it). Each line's est_value is converted into it with `fx` before it is summed or
// compared; a line that cannot be converted is left out (included = false,
// fx_missing) and listed in plan.fx_missing. Empty `target_currency` = legacy
// single-currency plan (no conversion).
ReplicationPlan build_plan(const QVector<SourceItem>& items, const ReplicationOptions& opts, double target_available,
                           const QString& source_account_id, const QString& target_account_id,
                           const QString& target_paper_portfolio_id, const SymbolResolver& resolve,
                           const QString& target_currency = QString(), const FxFactor& fx = FxFactor());

// Production resolver backed by InstrumentService (requires both brokers'
// instruments loaded; callers should load_from_db first).
SymbolResolver make_instrument_resolver(const QString& source_broker_id, const QString& target_broker_id);
// Production FX factor: PaperMarkService::fx_factor (FxRateService rates; a missing
// rate is requested and reported as nullopt).
FxFactor make_fx_factor();

// ── Execution (GUI thread) ───────────────────────────────────────────────────
// Places each included row as a market order into the target paper portfolio.
// Paper engine throws on margin/validation failure → captured per row. A row priced
// in another currency first gets the engine's per-symbol cash factor (fx_to_target);
// one without a usable factor fails instead of booking 1:1.
ReplicationResult execute_plan(const ReplicationPlan& plan);

} // namespace fincept::trading::replication
