#pragma once
// M&A Analytics — data types for deals, valuations, and financial models

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <nlohmann/json.hpp>

namespace fincept::mna {

using json = nlohmann::json;

// ============================================================================
// Deal Database
// ============================================================================
struct MADeal {
    std::string deal_id;
    std::string target_name;
    std::string acquirer_name;
    double deal_value         = 0;
    double offer_price_per_share = 0;
    double premium_1day       = 0;
    std::string payment_method;
    double payment_cash_pct   = 0;
    double payment_stock_pct  = 0;
    double ev_revenue         = 0;
    double ev_ebitda          = 0;
    double synergies          = 0;
    std::string status        = "Unknown";
    std::string deal_type;
    std::string industry      = "General";
    std::string announced_date;
    std::string expected_close_date;
    double breakup_fee        = 0;
    int hostile_flag          = 0;
    int tender_offer_flag     = 0;
    int cross_border_flag     = 0;
    std::string acquirer_country;
    std::string target_country;
    std::string created_at;
    std::string updated_at;

    json to_json() const {
        return json{
            {"deal_id", deal_id}, {"target_name", target_name},
            {"acquirer_name", acquirer_name}, {"deal_value", deal_value},
            {"offer_price_per_share", offer_price_per_share},
            {"premium_1day", premium_1day}, {"payment_method", payment_method},
            {"payment_cash_pct", payment_cash_pct}, {"payment_stock_pct", payment_stock_pct},
            {"ev_revenue", ev_revenue}, {"ev_ebitda", ev_ebitda},
            {"synergies", synergies}, {"status", status},
            {"deal_type", deal_type}, {"industry", industry},
            {"announced_date", announced_date}, {"expected_close_date", expected_close_date},
            {"breakup_fee", breakup_fee}, {"hostile_flag", hostile_flag},
            {"tender_offer_flag", tender_offer_flag}, {"cross_border_flag", cross_border_flag},
            {"acquirer_country", acquirer_country}, {"target_country", target_country},
        };
    }

    static MADeal from_json(const json& j) {
        MADeal d;
        d.deal_id              = j.value("deal_id", "");
        d.target_name          = j.value("target_name", "");
        d.acquirer_name        = j.value("acquirer_name", "");
        d.deal_value           = j.value("deal_value", 0.0);
        d.offer_price_per_share= j.value("offer_price_per_share", 0.0);
        d.premium_1day         = j.value("premium_1day", 0.0);
        d.payment_method       = j.value("payment_method", "");
        d.payment_cash_pct     = j.value("payment_cash_pct", 0.0);
        d.payment_stock_pct    = j.value("payment_stock_pct", 0.0);
        d.ev_revenue           = j.value("ev_revenue", 0.0);
        d.ev_ebitda            = j.value("ev_ebitda", 0.0);
        d.synergies            = j.value("synergies", 0.0);
        d.status               = j.value("status", std::string("Unknown"));
        d.deal_type            = j.value("deal_type", "");
        d.industry             = j.value("industry", std::string("General"));
        d.announced_date       = j.value("announced_date", "");
        d.expected_close_date  = j.value("expected_close_date", "");
        d.breakup_fee          = j.value("breakup_fee", 0.0);
        d.hostile_flag         = j.value("hostile_flag", 0);
        d.tender_offer_flag    = j.value("tender_offer_flag", 0);
        d.cross_border_flag    = j.value("cross_border_flag", 0);
        d.acquirer_country     = j.value("acquirer_country", "");
        d.target_country       = j.value("target_country", "");
        d.created_at           = j.value("created_at", "");
        d.updated_at           = j.value("updated_at", "");
        return d;
    }
};

// ============================================================================
// Valuation Types
// ============================================================================
struct WACCInputs {
    double risk_free_rate     = 0.045;
    double market_risk_premium= 0.06;
    double beta               = 1.2;
    double cost_of_debt       = 0.05;
    double tax_rate           = 0.21;
    double market_value_equity= 5000;
    double market_value_debt  = 1000;
};

struct FCFInputs {
    double ebit               = 500;
    double tax_rate           = 0.21;
    double depreciation       = 100;
    double capex              = 150;
    double change_in_nwc      = 20;
};

struct DCFInputs {
    WACCInputs wacc;
    FCFInputs  fcf;
    std::vector<double> growth_rates;
    double terminal_growth    = 0.025;
    double cash               = 500;
    double debt               = 1000;
    double shares_outstanding = 100;
};

struct LBOInputs {
    // Company data
    std::string company_name  = "Target";
    double revenue            = 500;
    double ebitda             = 100;
    double ebit               = 80;
    double capex              = 25;
    double nwc                = 50;
    // Transaction assumptions
    double purchase_price     = 800;
    double entry_multiple     = 8.0;
    double exit_multiple      = 9.0;
    double debt_percent       = 0.60;
    double equity_percent     = 0.40;
    double revenue_growth     = 0.05;
    double ebitda_margin      = 0.20;
    double capex_pct_revenue  = 0.03;
    double nwc_pct_revenue    = 0.05;
    double tax_rate           = 0.21;
    double hurdle_irr         = 0.20;
    int projection_years      = 5;
};

struct MergerModelInputs {
    // Acquirer
    double acq_revenue        = 10000;
    double acq_ebitda         = 2000;
    double acq_ebit           = 1600;
    double acq_net_income     = 1000;
    double acq_shares         = 100;
    double acq_eps            = 10;
    double acq_market_cap     = 0;
    // Target
    double tgt_revenue        = 3000;
    double tgt_ebitda         = 600;
    double tgt_ebit           = 480;
    double tgt_net_income     = 300;
    double tgt_shares         = 0;
    double tgt_eps            = 0;
    double tgt_market_cap     = 0;
    // Deal structure
    double purchase_price     = 5000;
    double cash_pct           = 50;
    double stock_pct          = 50;
    double cost_synergies     = 0;
    double revenue_synergies  = 0;
    double integration_costs  = 0;
    double tax_rate           = 0.21;
};

struct StartupScenario {
    char name[64]             = "";
    double probability        = 0;
    int exit_year             = 5;
    double exit_value         = 0;
    char description[128]     = "";
};

struct ValuationResult {
    std::string method;
    double valuation          = 0;
    double range_low          = 0;
    double range_high         = 0;
};

// ============================================================================
// Sort criteria for deal comparison
// ============================================================================
enum class DealSortCriteria { Premium, DealValue, EVRevenue, EVEBITDA, Synergies };

inline const char* sort_label(DealSortCriteria c) {
    switch (c) {
        case DealSortCriteria::Premium:   return "premium";
        case DealSortCriteria::DealValue: return "deal_value";
        case DealSortCriteria::EVRevenue: return "ev_revenue";
        case DealSortCriteria::EVEBITDA:  return "ev_ebitda";
        case DealSortCriteria::Synergies: return "synergies";
    }
    return "premium";
}

} // namespace fincept::mna
