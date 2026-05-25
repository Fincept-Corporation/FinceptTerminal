#pragma once

#include "core/result/Result.h"
#include "multiuser/contracts/PhaseOneAuthTypes.h"
#include "multiuser/contracts/PhaseOnePortfolioTypes.h"

namespace fincept {
class PortfolioHoldingsRepository;
class PortfolioRepository;
}

namespace fincept::multiuser {

class PhaseOneAuditRepository;

class PhaseOnePortfolioServer {
  public:
    PhaseOnePortfolioServer();
    PhaseOnePortfolioServer(fincept::PortfolioRepository* portfolio_repository,
                            fincept::PortfolioHoldingsRepository* holdings_repository,
                            PhaseOneAuditRepository* audit_repository);

    fincept::Result<PhaseOnePortfolioListResponse> list_portfolios() const;
    fincept::Result<PhaseOnePortfolioRecord> fetch_portfolio(const QString& portfolio_id) const;
    fincept::Result<PhaseOnePortfolioRecord> create_portfolio(const PhaseOneSessionInfo& actor,
                                                              const PhaseOneCreatePortfolioRequest& request) const;
    fincept::Result<PhaseOnePortfolioRecord> update_portfolio(const PhaseOneSessionInfo& actor,
                                                              const PhaseOneUpdatePortfolioRequest& request) const;
    fincept::Result<void> delete_portfolio(const PhaseOneSessionInfo& actor, const QString& portfolio_id) const;

    fincept::Result<PhaseOneHoldingsListResponse> list_holdings(const QString& portfolio_id) const;
    fincept::Result<PhaseOneHoldingRecord> create_holding(const PhaseOneSessionInfo& actor,
                                                          const PhaseOneCreateHoldingRequest& request) const;
    fincept::Result<PhaseOneHoldingRecord> update_holding(const PhaseOneSessionInfo& actor,
                                                          const PhaseOneUpdateHoldingRequest& request) const;
    fincept::Result<void> remove_holding(const PhaseOneSessionInfo& actor,
                                         const PhaseOneRemoveHoldingRequest& request) const;

  private:
    fincept::Result<void> write_audit_event(const QString& actor, const QString& action_type, const QString& target,
                                            const QString& result_status) const;

    fincept::Result<PhaseOneHoldingRecord> fetch_flat_holding(int id) const;
    fincept::Result<PhaseOneHoldingRecord> fetch_portfolio_holding(int id) const;

    fincept::PortfolioRepository* portfolio_repository_ = nullptr;
    fincept::PortfolioHoldingsRepository* holdings_repository_ = nullptr;
    PhaseOneAuditRepository* audit_repository_ = nullptr;
};

} // namespace fincept::multiuser
