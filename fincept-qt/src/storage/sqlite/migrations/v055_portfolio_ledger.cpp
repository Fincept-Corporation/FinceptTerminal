// v055_portfolio_ledger — multi-currency ledger, cash rows and per-portfolio
// analytics settings.
//
//   portfolio_transactions
//     transaction_type CHECK widened from BUY/SELL/DIVIDEND/SPLIT to also accept
//         DEPOSIT, WITHDRAWAL, FEE, TAX, INTEREST
//     fees      REAL NOT NULL DEFAULT 0    commissions, in `currency`
//     taxes     REAL NOT NULL DEFAULT 0    stamp duty / withholding, in `currency`
//     currency  TEXT NOT NULL DEFAULT ''   price currency ('' = instrument's quoted currency)
//     fx_rate   REAL NOT NULL DEFAULT 0    base units per 1 major unit of `currency` (0 = not recorded)
//     lot_id    TEXT NOT NULL DEFAULT ''   SELL under the SPECIFIC lot method: BUY row consumed first
//
//   portfolios
//     benchmark          TEXT DEFAULT ''        '' = default for the base currency; "SPY" or "SPY:60,AGG:40"
//     risk_free_override REAL                   NULL = automatic
//     track_cash         INTEGER NOT NULL DEFAULT 0
//     lot_method         TEXT NOT NULL DEFAULT 'FIFO'
//     long_term_days     INTEGER NOT NULL DEFAULT 365
//
// SQLite cannot ALTER a CHECK constraint, so portfolio_transactions is rebuilt —
// but only when its stored definition still carries the old four-type CHECK.
// SAFE ON A POPULATED DATABASE:
//   * every existing row is copied with every column it has (including columns this
//     build does not know about, which are re-created on the new table), ids and
//     created_at unchanged, so cloud-sync mappings keyed on the row id stay valid;
//   * the copy is verified by row count before the old table is dropped; any
//     mismatch fails the migration, which the runner rolls back as a whole (and a
//     pre-migration backup of the file exists — see MigrationRunner);
//   * rows whose portfolio no longer exists (possible only if they were written
//     with foreign keys off) cannot be inserted under the FK and are preserved
//     verbatim in portfolio_transactions_orphans_v055 instead of being lost;
//   * new columns are added only when missing; re-running is a no-op.

#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace fincept {

void register_migration_v055();

namespace {

// Uniquely named — unity builds concatenate migration TUs into one batch.
Result<void> sql_v055(QSqlDatabase& db, const QString& stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString() + " [" + stmt.left(120).toStdString() + "]");
    return Result<void>::ok();
}

struct ColumnV055 {
    QString name;
    QString type;
};

QVector<ColumnV055> columns_v055(QSqlDatabase& db, const QString& table) {
    QVector<ColumnV055> out;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table)))
        return out;
    while (q.next())
        out.append({q.value(1).toString(), q.value(2).toString()});
    return out;
}

bool has_column_v055(const QVector<ColumnV055>& cols, const QString& name) {
    for (const auto& c : cols)
        if (c.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

QString table_sql_v055(QSqlDatabase& db, const QString& table) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT sql FROM sqlite_master WHERE type = 'table' AND name = ?"));
    q.addBindValue(table);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return {};
}

qint64 count_v055(QSqlDatabase& db, const QString& stmt, bool* ok) {
    QSqlQuery q(db);
    *ok = q.exec(stmt) && q.next();
    return *ok ? q.value(0).toLongLong() : -1;
}

Result<void> add_column_if_missing_v055(QSqlDatabase& db, const QString& table, const QString& name,
                                        const QString& decl) {
    if (has_column_v055(columns_v055(db, table), name))
        return Result<void>::ok();
    return sql_v055(db, QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, name, decl));
}

const char* const kTxnTableV055 = "portfolio_transactions";

/// The new definition. Kept in one place so a fresh database and a rebuilt one
/// end up with the identical shape.
QString new_txn_ddl_v055(const QString& table_name) {
    return QStringLiteral(
               "CREATE TABLE %1 ("
               "  id                TEXT PRIMARY KEY,"
               "  portfolio_id      TEXT NOT NULL REFERENCES portfolios(id) ON DELETE CASCADE,"
               "  symbol            TEXT NOT NULL,"
               "  transaction_type  TEXT NOT NULL CHECK(transaction_type IN "
               "('BUY','SELL','DIVIDEND','SPLIT','DEPOSIT','WITHDRAWAL','FEE','TAX','INTEREST')),"
               "  quantity          REAL NOT NULL,"
               "  price             REAL NOT NULL,"
               "  total_value       REAL NOT NULL,"
               "  transaction_date  TEXT NOT NULL,"
               "  notes             TEXT DEFAULT '',"
               "  created_at        TEXT DEFAULT (datetime('now')),"
               "  fees              REAL NOT NULL DEFAULT 0,"
               "  taxes             REAL NOT NULL DEFAULT 0,"
               "  currency          TEXT NOT NULL DEFAULT '',"
               "  fx_rate           REAL NOT NULL DEFAULT 0,"
               "  lot_id            TEXT NOT NULL DEFAULT ''"
               ")")
        .arg(table_name);
}

Result<void> rebuild_transactions_v055(QSqlDatabase& db) {
    const QString table = QString::fromLatin1(kTxnTableV055);
    const QString tmp = QStringLiteral("portfolio_transactions_v055");
    const QVector<ColumnV055> old_cols = columns_v055(db, table);

    auto r = sql_v055(db, QStringLiteral("DROP TABLE IF EXISTS %1").arg(tmp)); // leftover of nothing: runs in one txn
    if (r.is_err())
        return r;
    r = sql_v055(db, new_txn_ddl_v055(tmp));
    if (r.is_err())
        return r;

    // Columns a newer schema (or another migration) added that this definition
    // lacks are carried over rather than dropped.
    const QVector<ColumnV055> new_cols = columns_v055(db, tmp);
    for (const auto& c : old_cols) {
        if (has_column_v055(new_cols, c.name))
            continue;
        r = sql_v055(db, QStringLiteral("ALTER TABLE %1 ADD COLUMN \"%2\" %3").arg(tmp, c.name, c.type));
        if (r.is_err())
            return r;
    }

    QStringList names;
    for (const auto& c : old_cols)
        names << QStringLiteral("\"%1\"").arg(c.name);
    const QString col_list = names.join(QLatin1Char(','));

    // Orphans (portfolio gone) cannot satisfy the FK of the new table: keep them aside.
    bool ok = false;
    const qint64 orphans = count_v055(db,
                                      QStringLiteral("SELECT COUNT(*) FROM %1 t WHERE NOT EXISTS "
                                                     "(SELECT 1 FROM portfolios p WHERE p.id = t.portfolio_id)")
                                          .arg(table),
                                      &ok);
    if (!ok)
        return Result<void>::err("v055: cannot count orphaned transactions");
    if (orphans > 0) {
        r = sql_v055(db, QStringLiteral("CREATE TABLE IF NOT EXISTS portfolio_transactions_orphans_v055 AS "
                                        "SELECT * FROM %1 WHERE 0")
                             .arg(table));
        if (r.is_err())
            return r;
        r = sql_v055(db, QStringLiteral("INSERT INTO portfolio_transactions_orphans_v055 SELECT * FROM %1 t "
                                        "WHERE NOT EXISTS (SELECT 1 FROM portfolios p WHERE p.id = t.portfolio_id)")
                             .arg(table));
        if (r.is_err())
            return r;
    }

    // Any row the old CHECK let through is valid under the wider one; a row with a
    // lower-case type (legacy imports) is normalised so it is not rejected.
    QStringList select_exprs;
    for (const auto& c : old_cols) {
        if (c.name.compare(QLatin1String("transaction_type"), Qt::CaseInsensitive) == 0)
            select_exprs << QStringLiteral("UPPER(TRIM(t.\"transaction_type\"))");
        else
            select_exprs << QStringLiteral("t.\"%1\"").arg(c.name);
    }
    r = sql_v055(db, QStringLiteral("INSERT INTO %1 (%2) SELECT %3 FROM %4 t WHERE EXISTS "
                                    "(SELECT 1 FROM portfolios p WHERE p.id = t.portfolio_id)")
                         .arg(tmp, col_list, select_exprs.join(QLatin1Char(',')), table));
    if (r.is_err())
        return r;

    const qint64 before = count_v055(db, QStringLiteral("SELECT COUNT(*) FROM %1").arg(table), &ok);
    if (!ok)
        return Result<void>::err("v055: cannot count portfolio_transactions");
    const qint64 copied = count_v055(db, QStringLiteral("SELECT COUNT(*) FROM %1").arg(tmp), &ok);
    if (!ok || copied + orphans != before)
        return Result<void>::err(QStringLiteral("v055: copy verification failed (%1 rows, %2 copied, %3 orphaned)")
                                     .arg(before)
                                     .arg(copied)
                                     .arg(orphans)
                                     .toStdString());

    r = sql_v055(db, QStringLiteral("DROP TABLE %1").arg(table));
    if (r.is_err())
        return r;
    return sql_v055(db, QStringLiteral("ALTER TABLE %1 RENAME TO %2").arg(tmp, table));
}

Result<void> apply_v055(QSqlDatabase& db) {
    const QString table = QString::fromLatin1(kTxnTableV055);
    const QString ddl = table_sql_v055(db, table);
    if (ddl.isEmpty()) {
        // No table at all (v006 never ran on this file?) — create the new shape.
        auto r = sql_v055(db, new_txn_ddl_v055(table));
        if (r.is_err())
            return r;
    } else if (ddl.contains(QLatin1String("CHECK"), Qt::CaseInsensitive) &&
               !ddl.contains(QLatin1String("'DEPOSIT'"), Qt::CaseInsensitive)) {
        auto r = rebuild_transactions_v055(db);
        if (r.is_err())
            return r;
    }

    // Columns (no-ops after a rebuild; needed when the CHECK was already wide/absent).
    struct NewCol {
        const char* name;
        const char* decl;
    };
    static const NewCol kTxnCols[] = {
        {"fees", "REAL NOT NULL DEFAULT 0"},          {"taxes", "REAL NOT NULL DEFAULT 0"},
        {"currency", "TEXT NOT NULL DEFAULT ''"},     {"fx_rate", "REAL NOT NULL DEFAULT 0"},
        {"lot_id", "TEXT NOT NULL DEFAULT ''"},
    };
    for (const auto& c : kTxnCols) {
        auto r = add_column_if_missing_v055(db, table, QString::fromLatin1(c.name), QString::fromLatin1(c.decl));
        if (r.is_err())
            return r;
    }

    // Indices vanish with a rebuilt table; IF NOT EXISTS keeps this idempotent.
    for (const char* idx : {
             "CREATE INDEX IF NOT EXISTS idx_ptx_portfolio ON portfolio_transactions(portfolio_id)",
             "CREATE INDEX IF NOT EXISTS idx_ptx_date ON portfolio_transactions(transaction_date)",
             "CREATE INDEX IF NOT EXISTS idx_ptx_portfolio_symbol ON portfolio_transactions(portfolio_id, symbol)",
         }) {
        auto r = sql_v055(db, QString::fromLatin1(idx));
        if (r.is_err())
            return r;
    }

    static const NewCol kPortfolioCols[] = {
        {"benchmark", "TEXT DEFAULT ''"},
        {"risk_free_override", "REAL"},
        {"track_cash", "INTEGER NOT NULL DEFAULT 0"},
        {"lot_method", "TEXT NOT NULL DEFAULT 'FIFO'"},
        {"long_term_days", "INTEGER NOT NULL DEFAULT 365"},
    };
    for (const auto& c : kPortfolioCols) {
        auto r = add_column_if_missing_v055(db, QStringLiteral("portfolios"), QString::fromLatin1(c.name),
                                            QString::fromLatin1(c.decl));
        if (r.is_err())
            return r;
    }
    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v055() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({55, "portfolio_ledger", apply_v055});
}

} // namespace fincept
