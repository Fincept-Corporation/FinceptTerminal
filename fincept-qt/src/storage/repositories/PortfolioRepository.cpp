// src/storage/repositories/PortfolioRepository.cpp
#include "storage/repositories/PortfolioRepository.h"
#include "core/logging/Logger.h"

#include <QDateTime>
#include <QUuid>

namespace fincept {

PortfolioRepository& PortfolioRepository::instance() {
    static PortfolioRepository s;
    return s;
}

// ── Row mappers ──────────────────────────────────────────────────────────────

portfolio::Portfolio PortfolioRepository::map_portfolio(QSqlQuery& q) {
    return {
        q.value(0).toString(), // id
        q.value(1).toString(), // name
        q.value(2).toString(), // owner
        q.value(3).toString(), // currency
        q.value(4).toString(), // description
        q.value(5).toString(), // created_at
        q.value(6).toString(), // updated_at
    };
}

portfolio::PortfolioAsset PortfolioRepository::map_asset(QSqlQuery& q) {
    return {
        q.value(0).toInt(),    // id
        q.value(1).toString(), // portfolio_id
        q.value(2).toString(), // symbol
        q.value(3).toDouble(), // quantity
        q.value(4).toDouble(), // avg_buy_price
        q.value(5).toString(), // first_purchase_date
        q.value(6).toString(), // last_updated
    };
}

portfolio::Transaction PortfolioRepository::map_transaction(QSqlQuery& q) {
    return {
        q.value(0).toString(), // id
        q.value(1).toString(), // portfolio_id
        q.value(2).toString(), // symbol
        q.value(3).toString(), // transaction_type
        q.value(4).toDouble(), // quantity
        q.value(5).toDouble(), // price
        q.value(6).toDouble(), // total_value
        q.value(7).toString(), // transaction_date
        q.value(8).toString(), // notes
        q.value(9).toString(), // created_at
    };
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
    return query_list(
        "SELECT id, name, owner, currency, description, created_at, updated_at "
        "FROM portfolios ORDER BY name",
        {}, map_portfolio);
}

Result<portfolio::Portfolio> PortfolioRepository::get_portfolio(const QString& id) {
    return query_one(
        "SELECT id, name, owner, currency, description, created_at, updated_at "
        "FROM portfolios WHERE id = ?",
        {id}, map_portfolio);
}

Result<QString> PortfolioRepository::create_portfolio(const QString& name, const QString& owner,
                                                       const QString& currency, const QString& description) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write(
        "INSERT INTO portfolios (id, name, owner, currency, description) VALUES (?, ?, ?, ?, ?)",
        {id, name, owner, currency, description});
    if (r.is_err())
        return Result<QString>::err(r.error());
    LOG_INFO("PortfolioRepo", QString("Created portfolio '%1' (%2)").arg(name, id));
    return Result<QString>::ok(id);
}

Result<void> PortfolioRepository::update_portfolio(const QString& id, const QString& name,
                                                     const QString& owner, const QString& currency,
                                                     const QString& description) {
    return exec_write(
        "UPDATE portfolios SET name = ?, owner = ?, currency = ?, description = ?, "
        "updated_at = datetime('now') WHERE id = ?",
        {name, owner, currency, description, id});
}

Result<void> PortfolioRepository::delete_portfolio(const QString& id) {
    LOG_INFO("PortfolioRepo", QString("Deleting portfolio %1").arg(id));
    return exec_write("DELETE FROM portfolios WHERE id = ?", {id});
}

// ── Assets CRUD ──────────────────────────────────────────────────────────────

Result<QVector<portfolio::PortfolioAsset>> PortfolioRepository::get_assets(const QString& portfolio_id) {
    return query_list_as<portfolio::PortfolioAsset>(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated "
        "FROM portfolio_assets WHERE portfolio_id = ? ORDER BY symbol",
        {portfolio_id}, map_asset);
}

Result<qint64> PortfolioRepository::add_asset(const QString& portfolio_id, const QString& symbol,
                                               double qty, double price, const QString& date) {
    QString purchase_date = date.isEmpty()
        ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        : date;

    // Upsert: if symbol already exists in portfolio, update quantity and avg price
    auto existing = query_list_as<portfolio::PortfolioAsset>(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated "
        "FROM portfolio_assets WHERE portfolio_id = ? AND symbol = ?",
        {portfolio_id, symbol.toUpper()}, map_asset);

    if (existing.is_ok() && !existing.value().isEmpty()) {
        auto& asset = existing.value().first();
        double new_qty = asset.quantity + qty;
        double new_avg = ((asset.avg_buy_price * asset.quantity) + (price * qty)) / new_qty;
        auto r = exec_write(
            "UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, "
            "last_updated = datetime('now') WHERE id = ?",
            {new_qty, new_avg, asset.id});
        if (r.is_err())
            return Result<qint64>::err(r.error());
        return Result<qint64>::ok(static_cast<qint64>(asset.id));
    }

    return exec_insert(
        "INSERT INTO portfolio_assets (portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date) "
        "VALUES (?, ?, ?, ?, ?)",
        {portfolio_id, symbol.toUpper(), qty, price, purchase_date});
}

Result<void> PortfolioRepository::update_asset(const QString& portfolio_id, const QString& symbol,
                                                double qty, double avg_price) {
    return exec_write(
        "UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, "
        "last_updated = datetime('now') WHERE portfolio_id = ? AND symbol = ?",
        {qty, avg_price, portfolio_id, symbol.toUpper()});
}

Result<void> PortfolioRepository::remove_asset(const QString& portfolio_id, const QString& symbol) {
    return exec_write(
        "DELETE FROM portfolio_assets WHERE portfolio_id = ? AND symbol = ?",
        {portfolio_id, symbol.toUpper()});
}

// ── Transactions ─────────────────────────────────────────────────────────────

Result<QVector<portfolio::Transaction>> PortfolioRepository::get_transactions(
    const QString& portfolio_id, int limit) {
    return query_list_as<portfolio::Transaction>(
        "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
        "transaction_date, notes, created_at "
        "FROM portfolio_transactions WHERE portfolio_id = ? "
        "ORDER BY transaction_date DESC, created_at DESC LIMIT ?",
        {portfolio_id, limit}, map_transaction);
}

Result<QVector<portfolio::Transaction>> PortfolioRepository::get_symbol_transactions(
    const QString& portfolio_id, const QString& symbol) {
    return query_list_as<portfolio::Transaction>(
        "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
        "transaction_date, notes, created_at "
        "FROM portfolio_transactions WHERE portfolio_id = ? AND symbol = ? "
        "ORDER BY transaction_date DESC",
        {portfolio_id, symbol.toUpper()}, map_transaction);
}

Result<QString> PortfolioRepository::add_transaction(const QString& portfolio_id, const QString& symbol,
                                                       const QString& type, double qty, double price,
                                                       const QString& date, const QString& notes) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write(
        "INSERT INTO portfolio_transactions "
        "(id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
        " transaction_date, notes) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
        {id, portfolio_id, symbol.toUpper(), type, qty, price, qty * price, date, notes});
    if (r.is_err())
        return Result<QString>::err(r.error());
    return Result<QString>::ok(id);
}

Result<void> PortfolioRepository::update_transaction(const QString& id, double qty, double price,
                                                       const QString& date, const QString& notes) {
    return exec_write(
        "UPDATE portfolio_transactions SET quantity = ?, price = ?, total_value = ?, "
        "transaction_date = ?, notes = ? WHERE id = ?",
        {qty, price, qty * price, date, notes, id});
}

Result<void> PortfolioRepository::delete_transaction(const QString& id) {
    return exec_write("DELETE FROM portfolio_transactions WHERE id = ?", {id});
}

// ── Snapshots ────────────────────────────────────────────────────────────────

Result<void> PortfolioRepository::save_snapshot(const QString& portfolio_id, double value,
                                                  double cost_basis, double pnl, double pnl_pct,
                                                  const QString& date) {
    return exec_write(
        "INSERT OR REPLACE INTO portfolio_snapshots "
        "(portfolio_id, total_value, total_cost_basis, total_pnl, total_pnl_percent, snapshot_date) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        {portfolio_id, value, cost_basis, pnl, pnl_pct, date});
}

Result<QVector<portfolio::PortfolioSnapshot>> PortfolioRepository::get_snapshots(
    const QString& portfolio_id, int days) {
    return query_list_as<portfolio::PortfolioSnapshot>(
        "SELECT id, portfolio_id, total_value, total_cost_basis, total_pnl, "
        "total_pnl_percent, snapshot_date "
        "FROM portfolio_snapshots WHERE portfolio_id = ? "
        "AND snapshot_date >= date('now', '-' || ? || ' days') "
        "ORDER BY snapshot_date ASC",
        {portfolio_id, days}, map_snapshot);
}

} // namespace fincept
