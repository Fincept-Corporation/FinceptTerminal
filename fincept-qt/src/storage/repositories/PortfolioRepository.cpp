// src/storage/repositories/PortfolioRepository.cpp
#include "storage/repositories/PortfolioRepository.h"

#include "core/logging/Logger.h"
#include "storage/sync/SyncOutbox.h"

#include <QDateTime>
#include <QUuid>

#include <cmath>

namespace fincept {

namespace {

// Column lists shared by every SELECT so the mappers can rely on the positions.
// COALESCE keeps rows written before v055 (or by a sync pull) readable.
constexpr const char* kPortfolioColumns =
    "SELECT id, name, owner, currency, description, created_at, updated_at, "
    "COALESCE(broker_account_id, ''), COALESCE(benchmark, ''), risk_free_override, "
    "COALESCE(track_cash, 0), COALESCE(lot_method, 'FIFO'), COALESCE(long_term_days, 365) "
    "FROM portfolios ";

constexpr const char* kTransactionColumns =
    "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
    "transaction_date, notes, created_at, COALESCE(fees, 0), COALESCE(taxes, 0), COALESCE(currency, ''), "
    "COALESCE(fx_rate, 0), COALESCE(lot_id, '') "
    "FROM portfolio_transactions ";

double portfolio_repo_finite_or_zero(double v) {
    return std::isfinite(v) ? v : 0.0;
}

/// Text bound into a NOT NULL ... DEFAULT '' column. The SQLite driver binds a null
/// QString (QString(), or the .trimmed() of one) as SQL NULL, which the column's
/// DEFAULT does not cover: every BUY without an explicit currency / lot id failed
/// with "NOT NULL constraint failed: portfolio_transactions.currency" (LOAD DEMO,
/// imports, the BUY dialog). An empty value is stored as ''.
QString portfolio_repo_text(const QString& s) {
    return s.isNull() ? QStringLiteral("") : s;
}

} // namespace

PortfolioRepository& PortfolioRepository::instance() {
    static PortfolioRepository s;
    return s;
}

// ── Row mappers ──────────────────────────────────────────────────────────────

portfolio::Portfolio PortfolioRepository::map_portfolio(QSqlQuery& q) {
    portfolio::Portfolio p;
    p.id = q.value(0).toString();
    p.name = q.value(1).toString();
    p.owner = q.value(2).toString();
    p.currency = q.value(3).toString();
    p.description = q.value(4).toString();
    p.created_at = q.value(5).toString();
    p.updated_at = q.value(6).toString();
    p.broker_account_id = q.value(7).toString(); // v022
    // v055 analytics settings
    p.benchmark = q.value(8).toString().trimmed();
    if (!q.value(9).isNull()) {
        bool ok = false;
        const double rf = q.value(9).toDouble(&ok);
        if (ok && std::isfinite(rf))
            p.risk_free_override = rf;
    }
    p.track_cash = q.value(10).toInt() != 0;
    const QString lm = q.value(11).toString().trimmed().toUpper();
    p.lot_method = lm.isEmpty() ? QStringLiteral("FIFO") : lm;
    const int ltd = q.value(12).toInt();
    p.long_term_days = ltd > 0 ? ltd : 365;
    return p;
}

portfolio::PortfolioAsset PortfolioRepository::map_asset(QSqlQuery& q) {
    return {
        q.value(0).toInt(),    // id
        q.value(1).toString(), // portfolio_id
        q.value(2).toString(), // symbol (yfinance-format)
        q.value(3).toDouble(), // quantity
        q.value(4).toDouble(), // avg_buy_price
        q.value(5).toString(), // first_purchase_date
        q.value(6).toString(), // last_updated
        q.value(7).toString(), // sector
        q.value(8).toString(), // broker_symbol (v022)
        q.value(9).toString(), // exchange (v022)
    };
}

portfolio::Transaction PortfolioRepository::map_transaction(QSqlQuery& q) {
    portfolio::Transaction t;
    t.id = q.value(0).toString();
    t.portfolio_id = q.value(1).toString();
    t.symbol = q.value(2).toString();
    t.transaction_type = q.value(3).toString();
    t.quantity = q.value(4).toDouble();
    t.price = q.value(5).toDouble();
    t.total_value = q.value(6).toDouble();
    t.transaction_date = q.value(7).toString();
    t.notes = q.value(8).toString();
    t.created_at = q.value(9).toString();
    // v055
    t.fees = q.value(10).toDouble();
    t.taxes = q.value(11).toDouble();
    t.currency = q.value(12).toString().trimmed();
    t.fx_rate = q.value(13).toDouble();
    t.lot_id = q.value(14).toString();
    return t;
}

portfolio::PortfolioSnapshot PortfolioRepository::map_snapshot(QSqlQuery& q) {
    return {
        q.value(0).toInt(),    // id
        q.value(1).toString(), // portfolio_id
        q.value(2).toDouble(), // total_value
        q.value(3).toDouble(), // total_cost_basis
        q.value(4).toDouble(), // total_pnl
        q.value(5).toDouble(), // total_pnl_percent
        q.value(6).toString(), // snapshot_date
    };
}

// ── Portfolios CRUD ──────────────────────────────────────────────────────────

Result<QVector<portfolio::Portfolio>> PortfolioRepository::list_portfolios() {
    return query_list(QString::fromLatin1(kPortfolioColumns) + "ORDER BY name", {}, map_portfolio);
}

Result<portfolio::Portfolio> PortfolioRepository::get_portfolio(const QString& id) {
    return query_one(QString::fromLatin1(kPortfolioColumns) + "WHERE id = ?", {id}, map_portfolio);
}

Result<QString> PortfolioRepository::create_portfolio(const QString& name, const QString& owner,
                                                      const QString& currency, const QString& description,
                                                      const QString& broker_account_id) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write("INSERT INTO portfolios (id, name, owner, currency, description, broker_account_id) "
                        "VALUES (?, ?, ?, ?, ?, ?)",
                        {id, portfolio_repo_text(name), portfolio_repo_text(owner), portfolio_repo_text(currency),
                         portfolio_repo_text(description), portfolio_repo_text(broker_account_id)});
    if (r.is_err())
        return Result<QString>::err(r.error());
    LOG_INFO(
        "PortfolioRepo",
        QString("Created portfolio '%1' (%2)%3")
            .arg(name, id, broker_account_id.isEmpty() ? QString() : QString(" [broker:%1]").arg(broker_account_id)));
    SyncOutbox::record_unique("portfolio", id, "sync");
    return Result<QString>::ok(id);
}

Result<void> PortfolioRepository::update_portfolio(const QString& id, const QString& name, const QString& owner,
                                                   const QString& currency, const QString& description) {
    auto r = exec_write("UPDATE portfolios SET name = ?, owner = ?, currency = ?, description = ?, "
                        "updated_at = datetime('now') WHERE id = ?",
                        {portfolio_repo_text(name), portfolio_repo_text(owner), portfolio_repo_text(currency),
                         portfolio_repo_text(description), id});
    if (r.is_ok())
        SyncOutbox::record_unique("portfolio", id, "sync");
    return r;
}

Result<void> PortfolioRepository::update_portfolio_settings(const QString& id, const QString& benchmark,
                                                            std::optional<double> risk_free_override, bool track_cash,
                                                            const QString& lot_method, int long_term_days) {
    const QVariant rf = (risk_free_override && std::isfinite(*risk_free_override)) ? QVariant(*risk_free_override)
                                                                                   : QVariant(QMetaType(QMetaType::Double));
    auto r = exec_write("UPDATE portfolios SET benchmark = ?, risk_free_override = ?, track_cash = ?, "
                        "lot_method = ?, long_term_days = ?, updated_at = datetime('now') WHERE id = ?",
                        {portfolio_repo_text(benchmark.trimmed()), rf, track_cash ? 1 : 0,
                         lot_method.trimmed().isEmpty() ? QStringLiteral("FIFO") : lot_method.trimmed().toUpper(),
                         long_term_days > 0 ? long_term_days : 365, id});
    // Settings are device-local analytics preferences: not part of the cloud body.
    return r;
}

Result<void> PortfolioRepository::delete_portfolio(const QString& id) {
    LOG_INFO("PortfolioRepo", QString("Deleting portfolio %1").arg(id));
    auto r = exec_write("DELETE FROM portfolios WHERE id = ?", {id});
    if (r.is_ok())
        SyncOutbox::record("portfolio", id, "delete");
    return r;
}

// ── Assets CRUD ──────────────────────────────────────────────────────────────

Result<QVector<portfolio::PortfolioAsset>> PortfolioRepository::get_assets(const QString& portfolio_id) {
    return query_list_as<portfolio::PortfolioAsset>(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated, "
        "COALESCE(sector, ''), COALESCE(broker_symbol, ''), COALESCE(exchange, '') "
        "FROM portfolio_assets WHERE portfolio_id = ? ORDER BY symbol",
        {portfolio_id}, map_asset);
}

Result<qint64> PortfolioRepository::add_asset(const QString& portfolio_id, const QString& symbol, double qty,
                                              double price, const QString& date, const QString& sector,
                                              const QString& broker_symbol, const QString& exchange) {
    QString purchase_date = date.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : date;

    // Upsert: if symbol already exists in portfolio, update quantity and avg price.
    // Preserve existing sector / broker_symbol / exchange unless the caller
    // provides non-empty values (manual editors don't pass broker fields, so
    // preserving prior values keeps a re-imported broker portfolio working).
    auto existing = query_list_as<portfolio::PortfolioAsset>(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated, "
        "COALESCE(sector, ''), COALESCE(broker_symbol, ''), COALESCE(exchange, '') "
        "FROM portfolio_assets WHERE portfolio_id = ? AND symbol = ?",
        {portfolio_id, symbol.toUpper()}, map_asset);

    if (existing.is_ok() && !existing.value().isEmpty()) {
        auto& asset = existing.value().first();
        double new_qty = asset.quantity + qty;
        // Guard the weighted-average against a zero/near-zero total quantity — a
        // 0/0 here would write NaN into avg_buy_price and poison every portfolio
        // total downstream.
        double new_avg =
            (new_qty > 1e-9) ? ((asset.avg_buy_price * asset.quantity) + (price * qty)) / new_qty : asset.avg_buy_price;
        QString merged_sector = sector.isEmpty() ? asset.sector : sector;
        QString merged_broker_symbol = broker_symbol.isEmpty() ? asset.broker_symbol : broker_symbol;
        QString merged_exchange = exchange.isEmpty() ? asset.exchange : exchange;
        auto r = exec_write("UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, sector = ?, "
                            "broker_symbol = ?, exchange = ?, last_updated = datetime('now') WHERE id = ?",
                            {new_qty, new_avg, merged_sector, merged_broker_symbol, merged_exchange, asset.id});
        if (r.is_err())
            return Result<qint64>::err(r.error());
        SyncOutbox::record_unique("portfolio", portfolio_id, "sync");
        return Result<qint64>::ok(static_cast<qint64>(asset.id));
    }

    auto ins =
        exec_insert("INSERT INTO portfolio_assets (portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, "
                    "sector, broker_symbol, exchange) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                    {portfolio_id, symbol.toUpper(), qty, price, purchase_date, sector, broker_symbol, exchange});
    if (ins.is_ok())
        SyncOutbox::record_unique("portfolio", portfolio_id, "sync");
    return ins;
}

Result<void> PortfolioRepository::set_asset_sector(const QString& portfolio_id, const QString& symbol,
                                                   const QString& sector) {
    auto r = exec_write("UPDATE portfolio_assets SET sector = ? "
                        "WHERE portfolio_id = ? AND symbol = ?",
                        {sector, portfolio_id, symbol.toUpper()});
    if (r.is_ok())
        SyncOutbox::record_unique("portfolio", portfolio_id, "sync");
    return r;
}

Result<void> PortfolioRepository::update_asset(const QString& portfolio_id, const QString& symbol, double qty,
                                               double avg_price) {
    auto r = exec_write("UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, "
                        "last_updated = datetime('now') WHERE portfolio_id = ? AND symbol = ?",
                        {qty, avg_price, portfolio_id, symbol.toUpper()});
    if (r.is_ok())
        SyncOutbox::record_unique("portfolio", portfolio_id, "sync");
    return r;
}

Result<void> PortfolioRepository::remove_asset(const QString& portfolio_id, const QString& symbol) {
    auto r = exec_write("DELETE FROM portfolio_assets WHERE portfolio_id = ? AND symbol = ?",
                        {portfolio_id, symbol.toUpper()});
    if (r.is_ok())
        SyncOutbox::record_unique("portfolio", portfolio_id, "sync");
    return r;
}

// ── Transactions ─────────────────────────────────────────────────────────────

Result<QVector<portfolio::Transaction>> PortfolioRepository::get_transactions(const QString& portfolio_id, int limit) {
    return query_list_as<portfolio::Transaction>(
        QString::fromLatin1(kTransactionColumns) +
            "WHERE portfolio_id = ? ORDER BY transaction_date DESC, created_at DESC LIMIT ?",
        {portfolio_id, limit}, map_transaction);
}

Result<QVector<portfolio::Transaction>> PortfolioRepository::get_symbol_transactions(const QString& portfolio_id,
                                                                                     const QString& symbol) {
    return query_list_as<portfolio::Transaction>(QString::fromLatin1(kTransactionColumns) +
                                                     "WHERE portfolio_id = ? AND symbol = ? "
                                                     "ORDER BY transaction_date DESC, created_at DESC",
                                                 {portfolio_id, symbol.toUpper()}, map_transaction);
}

Result<QString> PortfolioRepository::add_transaction(const QString& portfolio_id, const QString& symbol,
                                                     const QString& type, double qty, double price, const QString& date,
                                                     const QString& notes) {
    portfolio::Transaction t;
    t.portfolio_id = portfolio_id;
    t.symbol = symbol;
    t.transaction_type = type;
    t.quantity = qty;
    t.price = price;
    t.transaction_date = date;
    t.notes = notes;
    return add_transaction(t);
}

Result<QString> PortfolioRepository::add_transaction(const portfolio::Transaction& t) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write("INSERT INTO portfolio_transactions "
                        "(id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
                        " transaction_date, notes, fees, taxes, currency, fx_rate, lot_id) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        {id, t.portfolio_id, portfolio_repo_text(t.symbol.trimmed().toUpper()),
                         portfolio_repo_text(t.transaction_type.trimmed().toUpper()), t.quantity, t.price,
                         t.quantity * t.price, portfolio_repo_text(t.transaction_date), portfolio_repo_text(t.notes),
                         portfolio_repo_finite_or_zero(t.fees), portfolio_repo_finite_or_zero(t.taxes),
                         portfolio_repo_text(t.currency.trimmed()), portfolio_repo_finite_or_zero(t.fx_rate),
                         portfolio_repo_text(t.lot_id)});
    if (r.is_err())
        return Result<QString>::err(r.error());
    SyncOutbox::record_unique("portfolio", t.portfolio_id, "sync");
    return Result<QString>::ok(id);
}

Result<void> PortfolioRepository::update_transaction(const QString& id, double qty, double price, const QString& date,
                                                     const QString& notes) {
    auto r = exec_write("UPDATE portfolio_transactions SET quantity = ?, price = ?, total_value = ?, "
                        "transaction_date = ?, notes = ? WHERE id = ?",
                        {qty, price, qty * price, portfolio_repo_text(date), portfolio_repo_text(notes), id});
    if (r.is_ok()) {
        auto pr = db().execute("SELECT portfolio_id FROM portfolio_transactions WHERE id = ?", {id});
        if (pr.is_ok() && pr.value().next())
            SyncOutbox::record_unique("portfolio", pr.value().value(0).toString(), "sync");
    }
    return r;
}

Result<void> PortfolioRepository::update_transaction_costs(const QString& id, double fees, double taxes,
                                                           const QString& currency, double fx_rate,
                                                           const QString& lot_id) {
    auto r = exec_write("UPDATE portfolio_transactions SET fees = ?, taxes = ?, currency = ?, fx_rate = ?, "
                        "lot_id = ? WHERE id = ?",
                        {portfolio_repo_finite_or_zero(fees), portfolio_repo_finite_or_zero(taxes),
                         portfolio_repo_text(currency.trimmed()), portfolio_repo_finite_or_zero(fx_rate),
                         portfolio_repo_text(lot_id), id});
    if (r.is_ok()) {
        auto pr = db().execute("SELECT portfolio_id FROM portfolio_transactions WHERE id = ?", {id});
        if (pr.is_ok() && pr.value().next())
            SyncOutbox::record_unique("portfolio", pr.value().value(0).toString(), "sync");
    }
    return r;
}

Result<void> PortfolioRepository::delete_transaction(const QString& id) {
    QString pid; // resolve owning portfolio before the row is gone (for the sync marker)
    {
        auto pr = db().execute("SELECT portfolio_id FROM portfolio_transactions WHERE id = ?", {id});
        if (pr.is_ok() && pr.value().next())
            pid = pr.value().value(0).toString();
    }
    auto r = exec_write("DELETE FROM portfolio_transactions WHERE id = ?", {id});
    if (r.is_ok() && !pid.isEmpty())
        SyncOutbox::record_unique("portfolio", pid, "sync");
    return r;
}

// ── Snapshots ────────────────────────────────────────────────────────────────

Result<void> PortfolioRepository::save_snapshot(const QString& portfolio_id, double value, double cost_basis,
                                                double pnl, double pnl_pct, const QString& date) {
    // A non-finite value is "not known": never persisted (it would read back as 0).
    if (!std::isfinite(value) || !std::isfinite(cost_basis) || !std::isfinite(pnl) || !std::isfinite(pnl_pct))
        return Result<void>::err("snapshot values must be finite");
    auto r = exec_write("INSERT OR REPLACE INTO portfolio_snapshots "
                        "(portfolio_id, total_value, total_cost_basis, total_pnl, total_pnl_percent, snapshot_date) "
                        "VALUES (?, ?, ?, ?, ?, ?)",
                        {portfolio_id, value, cost_basis, pnl, pnl_pct, date});
    if (r.is_ok())
        SyncOutbox::record_unique("portfolio", portfolio_id, "sync");
    return r;
}

Result<QVector<portfolio::PortfolioSnapshot>> PortfolioRepository::get_snapshots(const QString& portfolio_id,
                                                                                 int days) {
    if (days <= 0) {
        return query_list_as<portfolio::PortfolioSnapshot>("SELECT id, portfolio_id, total_value, total_cost_basis, "
                                                           "total_pnl, total_pnl_percent, snapshot_date "
                                                           "FROM portfolio_snapshots WHERE portfolio_id = ? "
                                                           "ORDER BY snapshot_date ASC",
                                                           {portfolio_id}, map_snapshot);
    }
    return query_list_as<portfolio::PortfolioSnapshot>(
        "SELECT id, portfolio_id, total_value, total_cost_basis, total_pnl, "
        "total_pnl_percent, snapshot_date "
        "FROM portfolio_snapshots WHERE portfolio_id = ? "
        "AND snapshot_date >= date('now', '-' || ? || ' days') "
        "ORDER BY snapshot_date ASC",
        {portfolio_id, days}, map_snapshot);
}

} // namespace fincept
