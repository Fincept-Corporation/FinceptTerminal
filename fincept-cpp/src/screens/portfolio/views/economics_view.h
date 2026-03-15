#pragma once
#include "portfolio/portfolio_types.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

// Economics Panel — Business cycle, equity risk premium
class EconomicsView {
public:
    void render(const PortfolioSummary& summary);

private:
    int sub_tab_ = 0; // 0=Business Cycle, 1=ERP
    nlohmann::json cycle_result_;
    nlohmann::json erp_result_;
    bool cycle_loaded_ = false;
    bool erp_loaded_ = false;
    bool loading_ = false;
    std::string error_;

    void fetch_business_cycle();
    void fetch_erp();
};

} // namespace fincept::portfolio
