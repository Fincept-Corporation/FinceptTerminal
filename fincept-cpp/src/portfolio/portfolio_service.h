#pragma once
// Portfolio Service — CRUD for investment portfolio tracking
// Direct port of Rust portfolio_management.rs + portfolio.rs

#include "portfolio_types.h"
#include <vector>
#include <optional>
#include <map>

namespace fincept::portfolio {

// --- Portfolio CRUD ---
Portfolio create_portfolio(const std::string& name, const std::string& owner,
                           const std::string& currency = "USD",
                           const std::string& description = "");
std::vector<Portfolio> get_all_portfolios();
std::optional<Portfolio> get_portfolio_by_id(const std::string& id);
void delete_portfolio(const std::string& id);

// --- Asset operations ---
void add_asset(const std::string& portfolio_id, const std::string& symbol,
               double quantity, double price);
void sell_asset(const std::string& portfolio_id, const std::string& symbol,
                double quantity, double price);
std::vector<PortfolioAsset> get_assets(const std::string& portfolio_id);
void update_asset_symbol(const std::string& asset_id, const std::string& new_symbol);

// --- Transactions ---
std::vector<Transaction> get_transactions(const std::string& portfolio_id, int limit = -1);
std::optional<Transaction> get_transaction_by_id(const std::string& id);
void update_transaction(const std::string& id, double quantity, double price,
                        const std::string& date, const std::string& notes = "");
void delete_transaction(const std::string& id);

// --- Summary (enriched with market data) ---
// current_prices: symbol -> (current_price, previous_close)
PortfolioSummary compute_summary(
    const Portfolio& portfolio,
    const std::vector<PortfolioAsset>& assets,
    const std::map<std::string, std::pair<double, double>>& current_prices);

// --- Export ---
std::string export_portfolio_json(const std::string& portfolio_id);
std::string export_portfolio_csv(const std::string& portfolio_id);

// --- Import ---
struct ImportResult {
    std::string portfolio_id;
    int transactions_imported = 0;
    int transactions_skipped = 0;
};

// mode: "new" creates a new portfolio, "merge" adds to existing
ImportResult import_portfolio_json(const std::string& json_data,
                                    const std::string& mode = "new",
                                    const std::string& merge_target_id = "");

} // namespace fincept::portfolio
