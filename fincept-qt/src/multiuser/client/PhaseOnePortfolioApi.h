#pragma once

#include "auth/AuthTypes.h"
#include "multiuser/contracts/PhaseOnePortfolioTypes.h"

#include <QObject>

#include <functional>

namespace fincept::multiuser {

class PhaseOnePortfolioApi : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(fincept::auth::ApiResponse)>;

    static PhaseOnePortfolioApi& instance();

    void list_portfolios(Callback cb);
    void fetch_portfolio(const QString& portfolio_id, Callback cb);
    void create_portfolio(const PhaseOneCreatePortfolioRequest& request, Callback cb);
    void update_portfolio(const PhaseOneUpdatePortfolioRequest& request, Callback cb);
    void delete_portfolio(const QString& portfolio_id, Callback cb);

    void list_holdings(const QString& portfolio_id, Callback cb);
    void create_holding(const PhaseOneCreateHoldingRequest& request, Callback cb);
    void update_holding(const PhaseOneUpdateHoldingRequest& request, Callback cb);
    void remove_holding(const PhaseOneRemoveHoldingRequest& request, Callback cb);

  private:
    PhaseOnePortfolioApi() = default;
};

} // namespace fincept::multiuser
