#include "multiuser/server/PhaseOnePortfolioServer.h"

#include "core/logging/Logger.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "storage/repositories/PortfolioHoldingsRepository.h"
#include "storage/repositories/PortfolioRepository.h"

namespace fincept::multiuser {

namespace {

constexpr auto kLogTag = "Phase1PortfolioServer";

PhaseOnePortfolioRecord to_record(const fincept::portfolio::Portfolio& portfolio) {
    PhaseOnePortfolioRecord record;
    record.id = portfolio.id;
    record.name = portfolio.name;
    record.owner = portfolio.owner;
    record.currency = portfolio.currency;
    record.description = portfolio.description;
    record.created_at = portfolio.created_at;
    record.updated_at = portfolio.updated_at;
    record.broker_account_id = portfolio.broker_account_id;
    return record;
}

PhaseOneHoldingRecord to_record(const fincept::portfolio::PortfolioAsset& asset) {
    PhaseOneHoldingRecord record;
    record.id = asset.id;
    record.portfolio_id = asset.portfolio_id;
    record.symbol = asset.symbol;
    record.name = asset.symbol;
    record.shares = asset.quantity;
    record.avg_cost = asset.avg_buy_price;
    record.active = true;
    record.added_at = asset.first_purchase_date;
    record.updated_at = asset.last_updated;
    record.sector = asset.sector;
    record.broker_symbol = asset.broker_symbol;
    record.exchange = asset.exchange;
    return record;
}

PhaseOneHoldingRecord to_record(const fincept::PortfolioHolding& holding) {
    PhaseOneHoldingRecord record;
    record.id = holding.id;
    record.symbol = holding.symbol;
    record.name = holding.name;
    record.shares = holding.shares;
    record.avg_cost = holding.avg_cost;
    record.active = holding.active;
    record.added_at = holding.added_at;
    record.updated_at = holding.updated_at;
    return record;
}

QString portfolio_target(const QString& portfolio_id) {
    return QStringLiteral("portfolio:%1").arg(portfolio_id);
}

QString holding_target(int holding_id, const QString& portfolio_id) {
    if (!portfolio_id.isEmpty())
        return QStringLiteral("holding:%1:%2").arg(portfolio_id).arg(holding_id);
    return QStringLiteral("holding:%1").arg(holding_id);
}

} // namespace

PhaseOnePortfolioServer::PhaseOnePortfolioServer()
    : PhaseOnePortfolioServer(&fincept::PortfolioRepository::instance(), &fincept::PortfolioHoldingsRepository::instance(),
                              []() -> PhaseOneAuditRepository* {
                                  static PhaseOneAuditRepository repository;
                                  return &repository;
                              }()) {}

PhaseOnePortfolioServer::PhaseOnePortfolioServer(fincept::PortfolioRepository* portfolio_repository,
                                                 fincept::PortfolioHoldingsRepository* holdings_repository,
                                                 PhaseOneAuditRepository* audit_repository)
    : portfolio_repository_(portfolio_repository), holdings_repository_(holdings_repository), audit_repository_(audit_repository) {}

fincept::Result<PhaseOnePortfolioListResponse> PhaseOnePortfolioServer::list_portfolios() const {
    const auto result = portfolio_repository_->list_portfolios();
    if (result.is_err())
        return fincept::Result<PhaseOnePortfolioListResponse>::err(result.error());

    PhaseOnePortfolioListResponse response;
    response.portfolios.reserve(result.value().size());
    for (const auto& portfolio : result.value())
        response.portfolios.append(to_record(portfolio));
    return fincept::Result<PhaseOnePortfolioListResponse>::ok(response);
}

fincept::Result<PhaseOnePortfolioRecord> PhaseOnePortfolioServer::fetch_portfolio(const QString& portfolio_id) const {
    const auto result = portfolio_repository_->get_portfolio(portfolio_id);
    if (result.is_err())
        return fincept::Result<PhaseOnePortfolioRecord>::err(result.error());
    return fincept::Result<PhaseOnePortfolioRecord>::ok(to_record(result.value()));
}

fincept::Result<PhaseOnePortfolioRecord> PhaseOnePortfolioServer::create_portfolio(
    const PhaseOneSessionInfo& actor, const PhaseOneCreatePortfolioRequest& request) const {
    const auto create = portfolio_repository_->create_portfolio(request.name, request.owner, request.currency, request.description,
                                                                request.broker_account_id);
    if (create.is_err()) {
        write_audit_event(actor.username, QStringLiteral("portfolio_create"), portfolio_target(QStringLiteral("pending")),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOnePortfolioRecord>::err(create.error());
    }

    const auto created = fetch_portfolio(create.value());
    write_audit_event(actor.username, QStringLiteral("portfolio_create"), portfolio_target(create.value()),
                      created.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return created;
}

fincept::Result<PhaseOnePortfolioRecord> PhaseOnePortfolioServer::update_portfolio(
    const PhaseOneSessionInfo& actor, const PhaseOneUpdatePortfolioRequest& request) const {
    const auto existing = portfolio_repository_->get_portfolio(request.id);
    if (existing.is_err()) {
        write_audit_event(actor.username, QStringLiteral("portfolio_update"), portfolio_target(request.id),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOnePortfolioRecord>::err(existing.error());
    }

    const auto update = portfolio_repository_->update_portfolio(request.id, request.name, request.owner, request.currency,
                                                                request.description);
    if (update.is_err()) {
        write_audit_event(actor.username, QStringLiteral("portfolio_update"), portfolio_target(request.id),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOnePortfolioRecord>::err(update.error());
    }

    const auto updated = fetch_portfolio(request.id);
    write_audit_event(actor.username, QStringLiteral("portfolio_update"), portfolio_target(request.id),
                      updated.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return updated;
}

fincept::Result<void> PhaseOnePortfolioServer::delete_portfolio(const PhaseOneSessionInfo& actor,
                                                                const QString& portfolio_id) const {
    const auto existing = portfolio_repository_->get_portfolio(portfolio_id);
    if (existing.is_err()) {
        write_audit_event(actor.username, QStringLiteral("portfolio_delete"), portfolio_target(portfolio_id),
                          QStringLiteral("failure"));
        return fincept::Result<void>::err(existing.error());
    }

    const auto remove = portfolio_repository_->delete_portfolio(portfolio_id);
    write_audit_event(actor.username, QStringLiteral("portfolio_delete"), portfolio_target(portfolio_id),
                      remove.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return remove;
}

fincept::Result<PhaseOneHoldingsListResponse> PhaseOnePortfolioServer::list_holdings(const QString& portfolio_id) const {
    PhaseOneHoldingsListResponse response;
    if (portfolio_id.isEmpty())
        return fincept::Result<PhaseOneHoldingsListResponse>::err("portfolio_id_required");

    const auto portfolio = portfolio_repository_->get_portfolio(portfolio_id);
    if (portfolio.is_err())
        return fincept::Result<PhaseOneHoldingsListResponse>::err("portfolio_not_found");

    const auto result = portfolio_repository_->get_assets(portfolio_id);
    if (result.is_err())
        return fincept::Result<PhaseOneHoldingsListResponse>::err(result.error());
    response.holdings.reserve(result.value().size());
    for (const auto& asset : result.value())
        response.holdings.append(to_record(asset));
    return fincept::Result<PhaseOneHoldingsListResponse>::ok(response);
}

fincept::Result<PhaseOneHoldingRecord> PhaseOnePortfolioServer::create_holding(
    const PhaseOneSessionInfo& actor, const PhaseOneCreateHoldingRequest& request) const {
    if (request.portfolio_id.isEmpty()) {
        write_audit_event(actor.username, QStringLiteral("holding_create"), holding_target(-1, {}), QStringLiteral("failure"));
        return fincept::Result<PhaseOneHoldingRecord>::err("portfolio_id_required");
    }

    const auto portfolio = portfolio_repository_->get_portfolio(request.portfolio_id);
    if (portfolio.is_err()) {
        write_audit_event(actor.username, QStringLiteral("holding_create"), holding_target(-1, request.portfolio_id),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOneHoldingRecord>::err("portfolio_not_found");
    }

    const auto create = portfolio_repository_->add_asset(request.portfolio_id, request.symbol, request.shares,
                                                         request.avg_cost, request.acquired_at, request.sector,
                                                         request.broker_symbol, request.exchange);
    if (create.is_err()) {
        write_audit_event(actor.username, QStringLiteral("holding_create"),
                          holding_target(-1, request.portfolio_id), QStringLiteral("failure"));
        return fincept::Result<PhaseOneHoldingRecord>::err(create.error());
    }
    const auto holding = fetch_portfolio_holding(static_cast<int>(create.value()));
    write_audit_event(actor.username, QStringLiteral("holding_create"),
                      holding_target(static_cast<int>(create.value()), request.portfolio_id),
                      holding.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return holding;
}

fincept::Result<PhaseOneHoldingRecord> PhaseOnePortfolioServer::update_holding(
    const PhaseOneSessionInfo& actor, const PhaseOneUpdateHoldingRequest& request) const {
    if (request.portfolio_id.isEmpty()) {
        write_audit_event(actor.username, QStringLiteral("holding_update"), holding_target(request.id, {}),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOneHoldingRecord>::err("portfolio_id_required");
    }

    const auto existing = portfolio_repository_->get_asset_by_id(request.id);
    if (existing.is_err()) {
        write_audit_event(actor.username, QStringLiteral("holding_update"), holding_target(request.id, request.portfolio_id),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOneHoldingRecord>::err(existing.error());
    }
    if (existing.value().portfolio_id != request.portfolio_id) {
        write_audit_event(actor.username, QStringLiteral("holding_update"), holding_target(request.id, request.portfolio_id),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOneHoldingRecord>::err("Not found");
    }

    const auto update = portfolio_repository_->update_asset_by_id(request.id, request.shares, request.avg_cost,
                                                                  request.sector);
    if (update.is_err()) {
        write_audit_event(actor.username, QStringLiteral("holding_update"), holding_target(request.id, request.portfolio_id),
                          QStringLiteral("failure"));
        return fincept::Result<PhaseOneHoldingRecord>::err(update.error());
    }

    const auto holding = fetch_portfolio_holding(request.id);
    write_audit_event(actor.username, QStringLiteral("holding_update"), holding_target(request.id, request.portfolio_id),
                      holding.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return holding;
}

fincept::Result<void> PhaseOnePortfolioServer::remove_holding(const PhaseOneSessionInfo& actor,
                                                              const PhaseOneRemoveHoldingRequest& request) const {
    if (request.portfolio_id.isEmpty()) {
        write_audit_event(actor.username, QStringLiteral("holding_delete"), holding_target(request.id, {}),
                          QStringLiteral("failure"));
        return fincept::Result<void>::err("portfolio_id_required");
    }

    const auto existing = portfolio_repository_->get_asset_by_id(request.id);
    if (existing.is_err()) {
        write_audit_event(actor.username, QStringLiteral("holding_delete"), holding_target(request.id, request.portfolio_id),
                          QStringLiteral("failure"));
        return fincept::Result<void>::err(existing.error());
    }
    if (existing.value().portfolio_id != request.portfolio_id) {
        write_audit_event(actor.username, QStringLiteral("holding_delete"), holding_target(request.id, request.portfolio_id),
                          QStringLiteral("failure"));
        return fincept::Result<void>::err("Not found");
    }

    const auto remove = portfolio_repository_->remove_asset_by_id(request.id);
    write_audit_event(actor.username, QStringLiteral("holding_delete"), holding_target(request.id, request.portfolio_id),
                      remove.is_ok() ? QStringLiteral("success") : QStringLiteral("failure"));
    return remove;
}

fincept::Result<void> PhaseOnePortfolioServer::write_audit_event(const QString& actor, const QString& action_type,
                                                                 const QString& target,
                                                                 const QString& result_status) const {
    const auto result = audit_repository_->append_event(actor.isEmpty() ? QStringLiteral("anonymous") : actor,
                                                        action_type, target, result_status);
    if (result.is_err()) {
        LOG_WARN(kLogTag, QStringLiteral("Failed to write portfolio audit event: %1")
                              .arg(QString::fromStdString(result.error())));
    }
    return result;
}

fincept::Result<PhaseOneHoldingRecord> PhaseOnePortfolioServer::fetch_flat_holding(int id) const {
    const auto result = holdings_repository_->get_by_id(id);
    if (result.is_err())
        return fincept::Result<PhaseOneHoldingRecord>::err(result.error());
    return fincept::Result<PhaseOneHoldingRecord>::ok(to_record(result.value()));
}

fincept::Result<PhaseOneHoldingRecord> PhaseOnePortfolioServer::fetch_portfolio_holding(int id) const {
    const auto result = portfolio_repository_->get_asset_by_id(id);
    if (result.is_err())
        return fincept::Result<PhaseOneHoldingRecord>::err(result.error());
    return fincept::Result<PhaseOneHoldingRecord>::ok(to_record(result.value()));
}

} // namespace fincept::multiuser
