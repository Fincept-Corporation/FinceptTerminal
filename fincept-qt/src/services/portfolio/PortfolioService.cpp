// src/services/portfolio/PortfolioService.cpp
//
// Core: singleton, portfolio CRUD (load/create/delete), asset CRUD
// (add/sell), transactions, dividends, invalidate_cache. Split concerns:
//   - PortfolioService_Summary.cpp      — load/build/finalize summary
//   - PortfolioService_Metrics.cpp      — analytics + history + snapshots
//   - PortfolioService_ImportExport.cpp — CSV/JSON round-trip
#include "services/portfolio/PortfolioService.h"

#include "core/logging/Logger.h"
#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOneEndpointProvider.h"
#include "multiuser/client/PhaseOnePortfolioApi.h"
#include "multiuser/contracts/PhaseOnePortfolioTypes.h"
#include "python/PythonRunner.h"
#include "services/sectors/SectorResolver.h"
#include "storage/repositories/PortfolioRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {

QString portfolio_response_error(const fincept::auth::ApiResponse& response, const QString& fallback) {
    if (!response.error.isEmpty())
        return response.error;
    const QString message = response.data.value(QStringLiteral("message")).toString();
    return message.isEmpty() ? fallback : message;
}

fincept::portfolio::Portfolio portfolio_from_record(const fincept::multiuser::PhaseOnePortfolioRecord& record) {
    return {record.id, record.name, record.owner, record.currency, record.description,
            record.created_at, record.updated_at, record.broker_account_id};
}

} // namespace


namespace fincept::services {

PortfolioService& PortfolioService::instance() {
    static PortfolioService s;
    return s;
}

PortfolioService::PortfolioService() : QObject(nullptr) {
    // When the SectorResolver lands a fresh sector for a symbol, persist it
    // onto whichever portfolios hold it and invalidate their summary caches
    // so the Sectors tab refreshes on next refresh.
    connect(&SectorResolver::instance(), &SectorResolver::sector_resolved, this,
            [this](QString symbol, QString sector) {
                if (symbol.isEmpty() || sector.isEmpty())
                    return;
                auto& repo = PortfolioRepository::instance();
                auto ports = repo.list_portfolios();
                if (ports.is_err())
                    return;
                for (const auto& p : ports.value()) {
                    auto assets = repo.get_assets(p.id);
                    if (assets.is_err())
                        continue;
                    bool touched = false;
                    for (const auto& a : assets.value()) {
                        if (a.symbol == symbol && a.sector != sector) {
                            repo.set_asset_sector(p.id, symbol, sector);
                            touched = true;
                        }
                    }
                    if (touched) {
                        invalidate_cache(p.id);
                        // Refresh the active portfolio view so sectors appear
                        // without the user having to hit refresh manually.
                        load_summary(p.id);
                    }
                }
            });
}

// ── Portfolio CRUD ───────────────────────────────────────────────────────────

void PortfolioService::load_portfolios() {
    if (uses_phase_one_server_authority()) {
        QPointer<PortfolioService> self = this;
        fincept::multiuser::PhaseOnePortfolioApi::instance().list_portfolios([self](fincept::auth::ApiResponse response) {
            if (!self)
                return;
            if (!response.success) {
                emit self->portfolios_failed(
                    portfolio_response_error(response, QStringLiteral("Failed to load connected portfolios.")));
                return;
            }

            QVector<portfolio::Portfolio> portfolios;
            const auto parsed = fincept::multiuser::PhaseOnePortfolioListResponse::from_json(response.data);
            portfolios.reserve(parsed.portfolios.size());
            for (const auto& record : parsed.portfolios)
                portfolios.append(portfolio_from_record(record));
            emit self->portfolios_loaded(portfolios);
        });
        return;
    }

    auto r = PortfolioRepository::instance().list_portfolios();
    if (r.is_ok()) {
        emit portfolios_loaded(r.value());
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to load portfolios: " + QString::fromStdString(r.error()));
    }
}

void PortfolioService::create_portfolio(const QString& name, const QString& owner, const QString& currency,
                                        const QString& description, const QString& broker_account_id) {
    if (uses_phase_one_server_authority()) {
        fincept::multiuser::PhaseOneCreatePortfolioRequest request{name, owner, currency, description, broker_account_id};
        QPointer<PortfolioService> self = this;
        fincept::multiuser::PhaseOnePortfolioApi::instance().create_portfolio(
            request, [self](fincept::auth::ApiResponse response) {
                if (!self)
                    return;
                if (!response.success) {
                    emit self->portfolio_mutation_failed(
                        portfolio_response_error(response, QStringLiteral("Failed to create connected portfolio.")));
                    return;
                }

                const auto record = fincept::multiuser::PhaseOnePortfolioRecord::from_json(
                    response.data.value(QStringLiteral("portfolio")).toObject());
                emit self->portfolio_created(portfolio_from_record(record));
                self->load_portfolios();
            });
        return;
    }

    auto r = PortfolioRepository::instance().create_portfolio(name, owner, currency, description, broker_account_id);
    if (r.is_ok()) {
        auto p = PortfolioRepository::instance().get_portfolio(r.value());
        if (p.is_ok())
            emit portfolio_created(p.value());
        load_portfolios();
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to create portfolio: " + QString::fromStdString(r.error()));
    }
}

void PortfolioService::delete_portfolio(const QString& id) {
    if (uses_phase_one_server_authority()) {
        QPointer<PortfolioService> self = this;
        fincept::multiuser::PhaseOnePortfolioApi::instance().delete_portfolio(id, [self, id](fincept::auth::ApiResponse response) {
            if (!self)
                return;
            if (!response.success) {
                emit self->portfolio_mutation_failed(
                    portfolio_response_error(response, QStringLiteral("Failed to delete connected portfolio.")));
                return;
            }

            self->invalidate_cache(id);
            emit self->portfolio_deleted(id);
            self->load_portfolios();
        });
        return;
    }

    invalidate_cache(id);
    auto r = PortfolioRepository::instance().delete_portfolio(id);
    if (r.is_ok()) {
        emit portfolio_deleted(id);
        load_portfolios();
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to delete portfolio: " + QString::fromStdString(r.error()));
    }
}

// ── Summary ──────────────────────────────────────────────────────────────────


void PortfolioService::add_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                   const QString& date,
                                   const QString& broker_symbol, const QString& exchange) {
    if (uses_phase_one_server_authority()) {
        fincept::multiuser::PhaseOneCreateHoldingRequest request;
        request.portfolio_id = portfolio_id;
        request.symbol = symbol;
        request.name = symbol.toUpper();
        request.shares = qty;
        request.avg_cost = price;
        request.acquired_at = date;
        request.broker_symbol = broker_symbol;
        request.exchange = exchange;

        QPointer<PortfolioService> self = this;
        fincept::multiuser::PhaseOnePortfolioApi::instance().create_holding(
            request, [self, portfolio_id](fincept::auth::ApiResponse response) {
                if (!self)
                    return;
                if (!response.success) {
                    emit self->portfolio_mutation_failed(
                        portfolio_response_error(response, QStringLiteral("Failed to add holding to connected portfolio.")));
                    return;
                }
                self->invalidate_cache(portfolio_id);
                emit self->asset_added(portfolio_id);
            });
        return;
    }

    auto& repo = PortfolioRepository::instance();

    // Sector left empty here — SectorResolver fills it asynchronously after
    // the asset lands in the DB. Broker fields pass through verbatim.
    auto r = repo.add_asset(portfolio_id, symbol, qty, price, date, /*sector=*/QString(),
                            broker_symbol, exchange);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to add asset: " + QString::fromStdString(r.error()));
        return;
    }

    // Record transaction
    if (!uses_phase_one_server_authority()) {
        QString txn_date = date.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : date;
        repo.add_transaction(portfolio_id, symbol, "BUY", qty, price, txn_date);
    }

    invalidate_cache(portfolio_id);
    emit asset_added(portfolio_id);
}

void PortfolioService::sell_asset(const QString& portfolio_id, const QString& symbol, double qty, double price,
                                  const QString& date) {
    if (uses_phase_one_server_authority()) {
        QPointer<PortfolioService> self = this;
        fincept::multiuser::PhaseOnePortfolioApi::instance().list_holdings(
            portfolio_id, [self, portfolio_id, symbol, qty, price, date](fincept::auth::ApiResponse response) {
                if (!self)
                    return;
                if (!response.success) {
                    emit self->portfolio_mutation_failed(
                        portfolio_response_error(response, QStringLiteral("Failed to load connected holdings.")));
                    return;
                }

                const auto holdings = fincept::multiuser::PhaseOneHoldingsListResponse::from_json(response.data).holdings;
                const auto it = std::find_if(holdings.begin(), holdings.end(), [&symbol](const auto& holding) {
                    return holding.symbol.compare(symbol, Qt::CaseInsensitive) == 0;
                });
                if (it == holdings.end()) {
                    emit self->portfolio_mutation_failed(QStringLiteral("Asset not found"));
                    return;
                }

                const double remaining = it->shares - qty;
                if (remaining <= 0.0001) {
                    fincept::multiuser::PhaseOneRemoveHoldingRequest request;
                    request.id = it->id;
                    request.portfolio_id = portfolio_id;
                    fincept::multiuser::PhaseOnePortfolioApi::instance().remove_holding(
                        request, [self, portfolio_id](fincept::auth::ApiResponse remove_response) {
                            if (!self)
                                return;
                            if (!remove_response.success) {
                                emit self->portfolio_mutation_failed(portfolio_response_error(
                                    remove_response, QStringLiteral("Failed to sell holding from connected portfolio.")));
                                return;
                            }
                            self->invalidate_cache(portfolio_id);
                            emit self->asset_sold(portfolio_id);
                        });
                    return;
                }

                fincept::multiuser::PhaseOneUpdateHoldingRequest request;
                request.id = it->id;
                request.portfolio_id = portfolio_id;
                request.shares = remaining;
                request.avg_cost = it->avg_cost;
                request.sector = it->sector;
                fincept::multiuser::PhaseOnePortfolioApi::instance().update_holding(
                    request, [self, portfolio_id](fincept::auth::ApiResponse update_response) {
                        if (!self)
                            return;
                        if (!update_response.success) {
                            emit self->portfolio_mutation_failed(portfolio_response_error(
                                update_response, QStringLiteral("Failed to sell holding from connected portfolio.")));
                            return;
                        }
                        self->invalidate_cache(portfolio_id);
                        emit self->asset_sold(portfolio_id);
                    });
            });
        return;
    }

    auto& repo = PortfolioRepository::instance();

    // Get current holdings to update quantity
    auto assets_r = repo.get_assets(portfolio_id);
    if (assets_r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to get assets for sell: " + QString::fromStdString(assets_r.error()));
        return;
    }

    // Find the asset
    const portfolio::PortfolioAsset* found = nullptr;
    for (const auto& a : assets_r.value()) {
        if (a.symbol == symbol.toUpper()) {
            found = &a;
            break;
        }
    }

    if (!found) {
        LOG_ERROR("PortfolioSvc", QString("Asset %1 not found in portfolio %2").arg(symbol, portfolio_id));
        return;
    }

    double remaining = found->quantity - qty;
    Result<void> mutation_result = Result<void>::ok();
    if (remaining <= 0.0001) {
        // Full sell — remove asset
        mutation_result = repo.remove_asset(portfolio_id, symbol);
    } else {
        // Partial sell — keep same avg price
        mutation_result = repo.update_asset(portfolio_id, symbol, remaining, found->avg_buy_price);
    }

    if (mutation_result.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to sell asset: " + QString::fromStdString(mutation_result.error()));
        return;
    }

    // Record transaction
    if (!uses_phase_one_server_authority()) {
        QString txn_date = date.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : date;
        repo.add_transaction(portfolio_id, symbol, "SELL", qty, price, txn_date);
    }

    invalidate_cache(portfolio_id);
    emit asset_sold(portfolio_id);
}

// ── Transactions ─────────────────────────────────────────────────────────────

void PortfolioService::load_transactions(const QString& portfolio_id, int limit) {
    auto r = PortfolioRepository::instance().get_transactions(portfolio_id, limit);
    if (r.is_ok()) {
        emit transactions_loaded(r.value());
    } else {
        LOG_ERROR("PortfolioSvc", "Failed to load transactions: " + QString::fromStdString(r.error()));
    }
}

void PortfolioService::update_transaction(const QString& id, double qty, double price, const QString& date,
                                          const QString& notes) {
    if (uses_phase_one_server_authority()) {
        Q_UNUSED(id)
        Q_UNUSED(qty)
        Q_UNUSED(price)
        Q_UNUSED(date)
        Q_UNUSED(notes)
        LOG_WARN("PortfolioSvc", "Connected mode does not support local transaction edits.");
        return;
    }

    PortfolioRepository::instance().update_transaction(id, qty, price, date, notes);
}

void PortfolioService::delete_transaction(const QString& id, const QString& portfolio_id) {
    if (uses_phase_one_server_authority()) {
        Q_UNUSED(id)
        Q_UNUSED(portfolio_id)
        LOG_WARN("PortfolioSvc", "Connected mode does not support local transaction deletes.");
        return;
    }

    PortfolioRepository::instance().delete_transaction(id);
    invalidate_cache(portfolio_id);
}

// ── Dividend ──────────────────────────────────────────────────────────────────

void PortfolioService::record_dividend(const QString& portfolio_id, const QString& symbol, double qty,
                                       double amount_per_share, double total, const QString& date,
                                       const QString& notes) {
    if (uses_phase_one_server_authority()) {
        Q_UNUSED(portfolio_id)
        Q_UNUSED(symbol)
        Q_UNUSED(qty)
        Q_UNUSED(amount_per_share)
        Q_UNUSED(total)
        Q_UNUSED(date)
        Q_UNUSED(notes)
        LOG_WARN("PortfolioSvc", "Connected mode does not support local dividend transaction writes.");
        return;
    }

    const QString txn_date = date.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : date;
    auto& repo = PortfolioRepository::instance();
    auto r = repo.add_transaction(portfolio_id, symbol, "DIVIDEND", qty, amount_per_share, txn_date, notes);
    if (r.is_err()) {
        LOG_ERROR("PortfolioSvc", "Failed to record dividend: " + QString::fromStdString(r.error()));
        return;
    }
    Q_UNUSED(total)
    invalidate_cache(portfolio_id);
    // Reload transactions so the txn panel updates
    load_transactions(portfolio_id, 50);
}

// ── Historical correlation ────────────────────────────────────────────────────


void PortfolioService::invalidate_cache(const QString& portfolio_id) {
    QMutexLocker lock(&cache_mutex_);
    summary_cache_.remove(portfolio_id);
}

bool PortfolioService::uses_phase_one_server_authority() const {
    return fincept::multiuser::PhaseOneEndpointProvider::instance().resolve().ok;
}

} // namespace fincept::services
