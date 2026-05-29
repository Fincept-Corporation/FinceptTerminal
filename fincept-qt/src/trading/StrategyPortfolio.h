#pragma once
// StrategyPortfolio — persistence for user-saved multi-leg option strategies
// (Phase 3 §16) plus quantity-freeze limit lookups (Phase 3 §17).
//
// OpenAlgo reference: database/strategy_portfolio_db.py (§16),
//                     database/qty_freeze_db.py (§17).
//
// Both are backed by SQLite (migration v034). Legs are serialized as a JSON
// array of {symbol,exchange,side,quantity,strike,option_type,expiry}.

#include "trading/OptionsStrategyBuilder.h" // OptionsLeg

#include <QString>
#include <QVector>

#include <optional>

namespace fincept::trading {

// ----------------------------------------------------------------------------
// A saved strategy: a named bundle of legs the user can reload into order entry.
// ----------------------------------------------------------------------------
struct SavedStrategy {
    QString id;         // uuid (generated on save if empty)
    QString name;
    QString underlying; // e.g. "NIFTY"
    QString notes;
    QVector<OptionsLeg> legs;
    QString created_at; // ISO-8601, set on first save
};

// ----------------------------------------------------------------------------
// CRUD for saved strategies. Singleton; all calls are synchronous against the
// shared SQLite connection (consistent with ActionCenter / PaperTrading).
// ----------------------------------------------------------------------------
class StrategyPortfolio {
  public:
    static StrategyPortfolio& instance();

    // Insert (or upsert by id). Generates a uuid + created_at when id is empty.
    // Returns the id, or an empty string on failure.
    QString save(const SavedStrategy& s);

    // Update an existing row by id. Returns false if the id does not exist or
    // the write fails. Does not touch created_at.
    bool update(const SavedStrategy& s);

    // Delete by id. Returns true if a row was removed.
    bool remove(const QString& id);

    // Fetch one strategy by id.
    std::optional<SavedStrategy> get(const QString& id) const;

    // All saved strategies, newest first.
    QVector<SavedStrategy> list() const;

    StrategyPortfolio(const StrategyPortfolio&) = delete;
    StrategyPortfolio& operator=(const StrategyPortfolio&) = delete;

  private:
    StrategyPortfolio() = default;
};

// ----------------------------------------------------------------------------
// Quantity freeze limits (§17).
//
// Exchanges cap the max quantity per single order (e.g. NSE NIFTY futures 1800).
// These free functions read the qty_freeze table seeded/edited via migration
// v034. Returns 0 when no limit is configured for (symbol, exchange) — callers
// treat 0 as "no freeze limit, place as-is".
// ----------------------------------------------------------------------------

// Look up the freeze limit for (symbol, exchange). Returns 0 if none.
int qty_freeze_limit(const QString& symbol, const QString& exchange);

// Set / overwrite a freeze limit. Pass freeze_qty <= 0 to clear it.
bool set_qty_freeze_limit(const QString& symbol, const QString& exchange, int freeze_qty);

} // namespace fincept::trading
