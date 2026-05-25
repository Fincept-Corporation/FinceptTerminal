// src/storage/repositories/PortfolioRepository.cpp
#include "storage/repositories/PortfolioRepository.h"

#include "core/logging/Logger.h"
#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOnePortfolioApi.h"
#include "multiuser/contracts/PhaseOnePortfolioTypes.h"

#include <QDateTime>
#include <QEventLoop>
#include <QUuid>

namespace fincept {

namespace {

using fincept::auth::ApiResponse;
using fincept::multiuser::PhaseOneCreateHoldingRequest;
using fincept::multiuser::PhaseOneCreatePortfolioRequest;
using fincept::multiuser::PhaseOneHoldingRecord;
using fincept::multiuser::PhaseOneHoldingsListResponse;
using fincept::multiuser::PhaseOnePortfolioApi;
using fincept::multiuser::PhaseOnePortfolioListResponse;
using fincept::multiuser::PhaseOnePortfolioRecord;
using fincept::multiuser::PhaseOneRemoveHoldingRequest;
using fincept::multiuser::PhaseOneUpdateHoldingRequest;
using fincept::multiuser::PhaseOneUpdatePortfolioRequest;

bool use_phase_one_server() {
    return !fincept::multiuser::PhaseOneClientTransport::instance().session_id().isEmpty();
}

template <typename T>
Result<T> run_phase_one_request(const std::function<void(std::function<void(ApiResponse)>)>& start,
                                const std::function<Result<T>(const ApiResponse&)>& convert) {
    QEventLoop loop;
    bool completed = false;
    Result<T> result = Result<T>::err("phase_one_request_not_started");

    start([&](ApiResponse response) {
        result = convert(response);
        completed = true;
        loop.quit();
    });

    if (!completed)
        loop.exec();
    return result;
}

std::string response_error(const ApiResponse& response) {
    const QString message = !response.error.isEmpty() ? response.error : response.data.value("message").toString();
    return (message.isEmpty() ? QStringLiteral("Phase one portfolio request failed.") : message).toStdString();
}

portfolio::Portfolio from_record(const PhaseOnePortfolioRecord& record) {
    return {record.id, record.name, record.owner, record.currency, record.description,
            record.created_at, record.updated_at, record.broker_account_id};
}

portfolio::PortfolioAsset from_record(const PhaseOneHoldingRecord& record) {
    return {record.id,
            record.portfolio_id,
            record.symbol,
            record.shares,
            record.avg_cost,
            record.added_at,
            record.updated_at,
            record.sector,
            record.broker_symbol,
            record.exchange};
}

} // namespace

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
        q.value(7).toString(), // broker_account_id (v022)
    };
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
    if (use_phase_one_server()) {
        return run_phase_one_request<QVector<portfolio::Portfolio>>(
            [](auto cb) { PhaseOnePortfolioApi::instance().list_portfolios(std::move(cb)); },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<QVector<portfolio::Portfolio>>::err(response_error(response));

                const auto parsed = PhaseOnePortfolioListResponse::from_json(response.data);
                QVector<portfolio::Portfolio> portfolios;
                portfolios.reserve(parsed.portfolios.size());
                for (const auto& record : parsed.portfolios)
                    portfolios.append(from_record(record));
                return Result<QVector<portfolio::Portfolio>>::ok(portfolios);
            });
    }

    return query_list("SELECT id, name, owner, currency, description, created_at, updated_at, "
                      "COALESCE(broker_account_id, '') "
                      "FROM portfolios ORDER BY name",
                      {}, map_portfolio);
}

Result<portfolio::Portfolio> PortfolioRepository::get_portfolio(const QString& id) {
    if (use_phase_one_server()) {
        return run_phase_one_request<portfolio::Portfolio>(
            [id](auto cb) { PhaseOnePortfolioApi::instance().fetch_portfolio(id, std::move(cb)); },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<portfolio::Portfolio>::err(response_error(response));
                return Result<portfolio::Portfolio>::ok(
                    from_record(PhaseOnePortfolioRecord::from_json(response.data.value("portfolio").toObject())));
            });
    }

    return query_one("SELECT id, name, owner, currency, description, created_at, updated_at, "
                     "COALESCE(broker_account_id, '') "
                     "FROM portfolios WHERE id = ?",
                     {id}, map_portfolio);
}

Result<QString> PortfolioRepository::create_portfolio(const QString& name, const QString& owner,
                                                      const QString& currency, const QString& description,
                                                      const QString& broker_account_id) {
    if (use_phase_one_server()) {
        return run_phase_one_request<QString>(
            [name, owner, currency, description, broker_account_id](auto cb) {
                PhaseOneCreatePortfolioRequest request;
                request.name = name;
                request.owner = owner;
                request.currency = currency;
                request.description = description;
                request.broker_account_id = broker_account_id;
                PhaseOnePortfolioApi::instance().create_portfolio(request, std::move(cb));
            },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<QString>::err(response_error(response));
                return Result<QString>::ok(
                    PhaseOnePortfolioRecord::from_json(response.data.value("portfolio").toObject()).id);
            });
    }

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write(
        "INSERT INTO portfolios (id, name, owner, currency, description, broker_account_id) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        {id, name, owner, currency, description, broker_account_id});
    if (r.is_err())
        return Result<QString>::err(r.error());
    LOG_INFO("PortfolioRepo", QString("Created portfolio '%1' (%2)%3")
                                  .arg(name, id,
                                       broker_account_id.isEmpty() ? QString()
                                                                   : QString(" [broker:%1]").arg(broker_account_id)));
    return Result<QString>::ok(id);
}

Result<void> PortfolioRepository::update_portfolio(const QString& id, const QString& name, const QString& owner,
                                                   const QString& currency, const QString& description) {
    if (use_phase_one_server()) {
        return run_phase_one_request<void>(
            [id, name, owner, currency, description](auto cb) {
                PhaseOneUpdatePortfolioRequest request;
                request.id = id;
                request.name = name;
                request.owner = owner;
                request.currency = currency;
                request.description = description;
                PhaseOnePortfolioApi::instance().update_portfolio(request, std::move(cb));
            },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<void>::err(response_error(response));
                return Result<void>::ok();
            });
    }

    return exec_write("UPDATE portfolios SET name = ?, owner = ?, currency = ?, description = ?, "
                      "updated_at = datetime('now') WHERE id = ?",
                      {name, owner, currency, description, id});
}

Result<void> PortfolioRepository::delete_portfolio(const QString& id) {
    if (use_phase_one_server()) {
        return run_phase_one_request<void>(
            [id](auto cb) { PhaseOnePortfolioApi::instance().delete_portfolio(id, std::move(cb)); },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<void>::err(response_error(response));
                return Result<void>::ok();
            });
    }

    LOG_INFO("PortfolioRepo", QString("Deleting portfolio %1").arg(id));
    return exec_write("DELETE FROM portfolios WHERE id = ?", {id});
}

// ── Assets CRUD ──────────────────────────────────────────────────────────────

Result<QVector<portfolio::PortfolioAsset>> PortfolioRepository::get_assets(const QString& portfolio_id) {
    if (use_phase_one_server()) {
        return run_phase_one_request<QVector<portfolio::PortfolioAsset>>(
            [portfolio_id](auto cb) { PhaseOnePortfolioApi::instance().list_holdings(portfolio_id, std::move(cb)); },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<QVector<portfolio::PortfolioAsset>>::err(response_error(response));

                const auto parsed = PhaseOneHoldingsListResponse::from_json(response.data);
                QVector<portfolio::PortfolioAsset> assets;
                assets.reserve(parsed.holdings.size());
                for (const auto& record : parsed.holdings)
                    assets.append(from_record(record));
                return Result<QVector<portfolio::PortfolioAsset>>::ok(assets);
            });
    }

    return query_list_as<portfolio::PortfolioAsset>(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated, "
        "COALESCE(sector, ''), COALESCE(broker_symbol, ''), COALESCE(exchange, '') "
        "FROM portfolio_assets WHERE portfolio_id = ? ORDER BY symbol",
        {portfolio_id}, map_asset);
}

Result<portfolio::PortfolioAsset> PortfolioRepository::get_asset_by_id(int id) {
    const auto result = query_list_as<portfolio::PortfolioAsset>(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated, "
        "COALESCE(sector, ''), COALESCE(broker_symbol, ''), COALESCE(exchange, '') "
        "FROM portfolio_assets WHERE id = ?",
        {id}, map_asset);
    if (result.is_err())
        return Result<portfolio::PortfolioAsset>::err(result.error());
    if (result.value().isEmpty())
        return Result<portfolio::PortfolioAsset>::err("Not found");
    return Result<portfolio::PortfolioAsset>::ok(result.value().first());
}

Result<qint64> PortfolioRepository::add_asset(const QString& portfolio_id, const QString& symbol, double qty,
                                               double price, const QString& date, const QString& sector,
                                               const QString& broker_symbol, const QString& exchange) {
    if (use_phase_one_server()) {
        return run_phase_one_request<qint64>(
            [portfolio_id, symbol, qty, price, date, sector, broker_symbol, exchange](auto cb) {
                PhaseOneCreateHoldingRequest request;
                request.portfolio_id = portfolio_id;
                request.symbol = symbol;
                request.name = symbol;
                request.shares = qty;
                request.avg_cost = price;
                request.acquired_at = date;
                request.sector = sector;
                request.broker_symbol = broker_symbol;
                request.exchange = exchange;
                PhaseOnePortfolioApi::instance().create_holding(request, std::move(cb));
            },
            [](const ApiResponse& response) {
                if (!response.success)
                    return Result<qint64>::err(response_error(response));
                return Result<qint64>::ok(
                    PhaseOneHoldingRecord::from_json(response.data.value("holding").toObject()).id);
            });
    }

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
        double new_avg = ((asset.avg_buy_price * asset.quantity) + (price * qty)) / new_qty;
        QString merged_sector = sector.isEmpty() ? asset.sector : sector;
        QString merged_broker_symbol = broker_symbol.isEmpty() ? asset.broker_symbol : broker_symbol;
        QString merged_exchange = exchange.isEmpty() ? asset.exchange : exchange;
        auto r = exec_write("UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, sector = ?, "
                            "broker_symbol = ?, exchange = ?, last_updated = datetime('now') WHERE id = ?",
                            {new_qty, new_avg, merged_sector, merged_broker_symbol, merged_exchange, asset.id});
        if (r.is_err())
            return Result<qint64>::err(r.error());
        return Result<qint64>::ok(static_cast<qint64>(asset.id));
    }

    return exec_insert(
        "INSERT INTO portfolio_assets (portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, "
        "sector, broker_symbol, exchange) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        {portfolio_id, symbol.toUpper(), qty, price, purchase_date, sector, broker_symbol, exchange});
}

Result<void> PortfolioRepository::set_asset_sector(const QString& portfolio_id, const QString& symbol,
                                                   const QString& sector) {
    if (use_phase_one_server()) {
        const auto assets = get_assets(portfolio_id);
        if (assets.is_err())
            return Result<void>::err(assets.error());

        for (const auto& asset : assets.value()) {
            if (asset.symbol.compare(symbol, Qt::CaseInsensitive) != 0)
                continue;
            return run_phase_one_request<void>(
                [asset, portfolio_id, sector](auto cb) {
                    PhaseOneUpdateHoldingRequest request;
                    request.id = asset.id;
                    request.portfolio_id = portfolio_id;
                    request.shares = asset.quantity;
                    request.avg_cost = asset.avg_buy_price;
                    request.sector = sector;
                    PhaseOnePortfolioApi::instance().update_holding(request, std::move(cb));
                },
                [](const ApiResponse& response) {
                    if (!response.success)
                        return Result<void>::err(response_error(response));
                    return Result<void>::ok();
                });
        }

        return Result<void>::err("Asset not found");
    }

    return exec_write("UPDATE portfolio_assets SET sector = ? "
                      "WHERE portfolio_id = ? AND symbol = ?",
                      {sector, portfolio_id, symbol.toUpper()});
}

Result<void> PortfolioRepository::update_asset(const QString& portfolio_id, const QString& symbol, double qty,
                                               double avg_price) {
    if (use_phase_one_server()) {
        const auto assets = get_assets(portfolio_id);
        if (assets.is_err())
            return Result<void>::err(assets.error());

        for (const auto& asset : assets.value()) {
            if (asset.symbol.compare(symbol, Qt::CaseInsensitive) != 0)
                continue;
            return run_phase_one_request<void>(
                [asset, portfolio_id, qty, avg_price](auto cb) {
                    PhaseOneUpdateHoldingRequest request;
                    request.id = asset.id;
                    request.portfolio_id = portfolio_id;
                    request.shares = qty;
                    request.avg_cost = avg_price;
                    request.sector = asset.sector;
                    PhaseOnePortfolioApi::instance().update_holding(request, std::move(cb));
                },
                [](const ApiResponse& response) {
                    if (!response.success)
                        return Result<void>::err(response_error(response));
                    return Result<void>::ok();
                });
        }

        return Result<void>::err("Asset not found");
    }

    return exec_write("UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, "
                      "last_updated = datetime('now') WHERE portfolio_id = ? AND symbol = ?",
                      {qty, avg_price, portfolio_id, symbol.toUpper()});
}

Result<void> PortfolioRepository::update_asset_by_id(int id, double qty, double avg_price, const QString& sector) {
    if (sector.isEmpty()) {
        return exec_write("UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, last_updated = datetime('now') "
                          "WHERE id = ?",
                          {qty, avg_price, id});
    }
    return exec_write("UPDATE portfolio_assets SET quantity = ?, avg_buy_price = ?, sector = ?, "
                      "last_updated = datetime('now') WHERE id = ?",
                      {qty, avg_price, sector, id});
}

Result<void> PortfolioRepository::remove_asset(const QString& portfolio_id, const QString& symbol) {
    if (use_phase_one_server()) {
        const auto assets = get_assets(portfolio_id);
        if (assets.is_err())
            return Result<void>::err(assets.error());

        for (const auto& asset : assets.value()) {
            if (asset.symbol.compare(symbol, Qt::CaseInsensitive) != 0)
                continue;
            return run_phase_one_request<void>(
                [asset](auto cb) {
                    PhaseOneRemoveHoldingRequest request;
                    request.id = asset.id;
                    request.portfolio_id = asset.portfolio_id;
                    PhaseOnePortfolioApi::instance().remove_holding(request, std::move(cb));
                },
                [](const ApiResponse& response) {
                    if (!response.success)
                        return Result<void>::err(response_error(response));
                    return Result<void>::ok();
                });
        }

        return Result<void>::err("Asset not found");
    }

    return exec_write("DELETE FROM portfolio_assets WHERE portfolio_id = ? AND symbol = ?",
                      {portfolio_id, symbol.toUpper()});
}

Result<void> PortfolioRepository::remove_asset_by_id(int id) {
    return exec_write("DELETE FROM portfolio_assets WHERE id = ?", {id});
}

// ── Transactions ─────────────────────────────────────────────────────────────

Result<QVector<portfolio::Transaction>> PortfolioRepository::get_transactions(const QString& portfolio_id, int limit) {
    return query_list_as<portfolio::Transaction>(
        "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
        "transaction_date, notes, created_at "
        "FROM portfolio_transactions WHERE portfolio_id = ? "
        "ORDER BY transaction_date DESC, created_at DESC LIMIT ?",
        {portfolio_id, limit}, map_transaction);
}

Result<QVector<portfolio::Transaction>> PortfolioRepository::get_symbol_transactions(const QString& portfolio_id,
                                                                                     const QString& symbol) {
    return query_list_as<portfolio::Transaction>(
        "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
        "transaction_date, notes, created_at "
        "FROM portfolio_transactions WHERE portfolio_id = ? AND symbol = ? "
        "ORDER BY transaction_date DESC",
        {portfolio_id, symbol.toUpper()}, map_transaction);
}

Result<QString> PortfolioRepository::add_transaction(const QString& portfolio_id, const QString& symbol,
                                                     const QString& type, double qty, double price, const QString& date,
                                                     const QString& notes) {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write("INSERT INTO portfolio_transactions "
                        "(id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
                        " transaction_date, notes) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        {id, portfolio_id, symbol.toUpper(), type, qty, price, qty * price, date, notes});
    if (r.is_err())
        return Result<QString>::err(r.error());
    return Result<QString>::ok(id);
}

Result<void> PortfolioRepository::update_transaction(const QString& id, double qty, double price, const QString& date,
                                                     const QString& notes) {
    return exec_write("UPDATE portfolio_transactions SET quantity = ?, price = ?, total_value = ?, "
                      "transaction_date = ?, notes = ? WHERE id = ?",
                      {qty, price, qty * price, date, notes, id});
}

Result<void> PortfolioRepository::delete_transaction(const QString& id) {
    return exec_write("DELETE FROM portfolio_transactions WHERE id = ?", {id});
}

// ── Snapshots ────────────────────────────────────────────────────────────────

Result<void> PortfolioRepository::save_snapshot(const QString& portfolio_id, double value, double cost_basis,
                                                double pnl, double pnl_pct, const QString& date) {
    return exec_write("INSERT OR REPLACE INTO portfolio_snapshots "
                      "(portfolio_id, total_value, total_cost_basis, total_pnl, total_pnl_percent, snapshot_date) "
                      "VALUES (?, ?, ?, ?, ?, ?)",
                      {portfolio_id, value, cost_basis, pnl, pnl_pct, date});
}

Result<QVector<portfolio::PortfolioSnapshot>> PortfolioRepository::get_snapshots(const QString& portfolio_id,
                                                                                 int days) {
    return query_list_as<portfolio::PortfolioSnapshot>(
        "SELECT id, portfolio_id, total_value, total_cost_basis, total_pnl, "
        "total_pnl_percent, snapshot_date "
        "FROM portfolio_snapshots WHERE portfolio_id = ? "
        "AND snapshot_date >= date('now', '-' || ? || ' days') "
        "ORDER BY snapshot_date ASC",
        {portfolio_id, days}, map_snapshot);
}

} // namespace fincept
