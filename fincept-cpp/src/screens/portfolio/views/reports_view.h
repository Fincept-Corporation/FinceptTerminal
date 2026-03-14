#pragma once
#include "portfolio/portfolio_types.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

// Reports & PME Panel — Tax calculator, PME analysis
class ReportsView {
public:
    void render(const PortfolioSummary& summary);

private:
    int sub_tab_ = 0; // 0=Tax Report, 1=PME Analysis
    nlohmann::json tax_result_;
    nlohmann::json pme_result_;
    bool tax_loaded_ = false;
    bool pme_loaded_ = false;
    bool loading_ = false;
    std::string loaded_for_portfolio_;

    void fetch_tax_report(const PortfolioSummary& summary);
    void fetch_pme(const PortfolioSummary& summary);
};

} // namespace fincept::portfolio
