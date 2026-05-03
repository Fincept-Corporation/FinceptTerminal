// v022_portfolio_broker_link — Link portfolios to broker accounts for
// broker-first live quote routing.
//
// Background: PortfolioService::build_summary fetches live prices via
// MarketDataService → yfinance for every portfolio. For portfolios that
// were imported from a connected broker (e.g. Zerodha), yfinance is a 15
// minute-delayed and frequently incomplete fallback when the user has
// already authenticated with a broker that provides true live ticks.
//
// This migration adds three columns:
//   - portfolios.broker_account_id  — FK-ish to broker_accounts.id (kept
//     loose so portfolios survive account deletion). Empty = manual / non-
//     broker portfolio. yfinance path is preserved for these.
//   - portfolio_assets.broker_symbol — broker-native ticker (e.g. "RELIANCE")
//   - portfolio_assets.exchange      — exchange code (e.g. "NSE")
//
// PortfolioAsset.symbol stays in yfinance format ("RELIANCE.NS") so every
// existing downstream consumer (sparklines, correlation, NAV replay, news,
// SectorResolver) keeps working unchanged. Live quote routing in
// PortfolioService picks broker_symbol+exchange when broker_account_id is
// present; everything else continues to read .symbol.
//
// Existing rows get '' defaults — backwards compatible.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

// NOTE: static helpers are renamed v022_* on purpose — Unity builds may
// concatenate multiple migration .cpp files into a single TU, and an
// anonymous-namespace `sql()` here would clash with the same symbol in v019
// or v021. The unity-build namespace trap is a documented gotcha for this
// codebase; rename or move into per-migration anonymous-namespaces. We pick
// rename so the file is grep-able by version.
static Result<void> v022_sql(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

static bool v022_column_exists(QSqlDatabase& db, const QString& table, const QString& column) {
    QSqlQuery q(db);
    q.exec(QString("PRAGMA table_info(%1)").arg(table));
    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

Result<void> apply_v022(QSqlDatabase& db) {
    // SQLite ALTER TABLE ADD COLUMN does not support IF NOT EXISTS.
    // Use PRAGMA table_info to skip already-present columns (matches v019).
    if (!v022_column_exists(db, "portfolios", "broker_account_id")) {
        auto r = v022_sql(db, "ALTER TABLE portfolios ADD COLUMN broker_account_id TEXT DEFAULT ''");
        if (r.is_err())
            return r;
    }

    if (!v022_column_exists(db, "portfolio_assets", "broker_symbol")) {
        auto r = v022_sql(db, "ALTER TABLE portfolio_assets ADD COLUMN broker_symbol TEXT DEFAULT ''");
        if (r.is_err())
            return r;
    }

    if (!v022_column_exists(db, "portfolio_assets", "exchange")) {
        auto r = v022_sql(db, "ALTER TABLE portfolio_assets ADD COLUMN exchange TEXT DEFAULT ''");
        if (r.is_err())
            return r;
    }

    // Lookup index for "find all portfolios for this broker account" queries.
    // Cheap on small tables, useful when the broker disconnect dialog wants
    // to show "you have N portfolios using this account".
    auto r = v022_sql(db, "CREATE INDEX IF NOT EXISTS idx_portfolios_broker_account "
                          "ON portfolios(broker_account_id)");
    if (r.is_err())
        return r;

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v022() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({22, "portfolio_broker_link", apply_v022});
}

} // namespace fincept
