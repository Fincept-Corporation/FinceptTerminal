// Portfolio Service — Investment portfolio CRUD operations
// Direct port of Rust portfolio.rs (310 lines) using existing Database singleton

#include "portfolio_service.h"
#include "storage/database.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <sstream>

namespace fincept::portfolio {

// ============================================================================
// Portfolio CRUD
// ============================================================================

Portfolio create_portfolio(const std::string& name, const std::string& owner,
                           const std::string& currency, const std::string& description) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    std::string id = db::generate_uuid();
    auto stmt = db.prepare(
        "INSERT INTO portfolios (id, name, owner, currency, description, created_at, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, name);
    stmt.bind_text(3, owner);
    stmt.bind_text(4, currency);
    if (description.empty()) stmt.bind_null(5);
    else stmt.bind_text(5, description);
    stmt.execute();

    // Read back
    auto q = db.prepare("SELECT id, name, owner, currency, description, created_at, updated_at "
                         "FROM portfolios WHERE id = ?1");
    q.bind_text(1, id);
    q.step();
    return Portfolio{
        q.col_text(0), q.col_text(1), q.col_text(2), q.col_text(3),
        q.col_text_opt(4), q.col_text(5), q.col_text(6)
    };
}

std::vector<Portfolio> get_all_portfolios() {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, name, owner, currency, description, created_at, updated_at "
        "FROM portfolios ORDER BY created_at DESC");
    std::vector<Portfolio> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
            stmt.col_text_opt(4), stmt.col_text(5), stmt.col_text(6)
        });
    }
    return result;
}

std::optional<Portfolio> get_portfolio_by_id(const std::string& id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, name, owner, currency, description, created_at, updated_at "
        "FROM portfolios WHERE id = ?1");
    stmt.bind_text(1, id);
    if (!stmt.step()) return std::nullopt;
    return Portfolio{
        stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
        stmt.col_text_opt(4), stmt.col_text(5), stmt.col_text(6)
    };
}

void delete_portfolio(const std::string& id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare("DELETE FROM portfolios WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// ============================================================================
// Asset Operations
// ============================================================================

void add_asset(const std::string& portfolio_id, const std::string& symbol,
               double quantity, double price) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    db.transaction([&]() {
        // Check for existing asset
        auto check = db.prepare(
            "SELECT id, quantity, avg_buy_price FROM portfolio_assets "
            "WHERE portfolio_id = ?1 AND symbol = ?2");
        check.bind_text(1, portfolio_id);
        check.bind_text(2, symbol);

        if (check.step()) {
            // Average into existing position
            std::string existing_id = check.col_text(0);
            double existing_qty = check.col_double(1);
            double existing_avg = check.col_double(2);

            double total_qty = existing_qty + quantity;
            double new_avg = ((existing_avg * existing_qty) + (price * quantity)) / total_qty;

            auto upd = db.prepare(
                "UPDATE portfolio_assets SET quantity = ?1, avg_buy_price = ?2, "
                "last_updated = CURRENT_TIMESTAMP WHERE id = ?3");
            upd.bind_double(1, total_qty);
            upd.bind_double(2, new_avg);
            upd.bind_text(3, existing_id);
            upd.execute();
        } else {
            // Insert new asset
            std::string id = db::generate_uuid();
            auto ins = db.prepare(
                "INSERT INTO portfolio_assets (id, portfolio_id, symbol, quantity, avg_buy_price, "
                "first_purchase_date, last_updated) VALUES (?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)");
            ins.bind_text(1, id);
            ins.bind_text(2, portfolio_id);
            ins.bind_text(3, symbol);
            ins.bind_double(4, quantity);
            ins.bind_double(5, price);
            ins.execute();
        }

        // Record BUY transaction
        std::string txn_id = db::generate_uuid();
        double total_value = quantity * price;
        auto txn = db.prepare(
            "INSERT INTO portfolio_transactions (id, portfolio_id, symbol, transaction_type, "
            "quantity, price, total_value, notes, transaction_date) "
            "VALUES (?1, ?2, ?3, 'BUY', ?4, ?5, ?6, NULL, CURRENT_TIMESTAMP)");
        txn.bind_text(1, txn_id);
        txn.bind_text(2, portfolio_id);
        txn.bind_text(3, symbol);
        txn.bind_double(4, quantity);
        txn.bind_double(5, price);
        txn.bind_double(6, total_value);
        txn.execute();

        return 0; // transaction() needs a return value
    });
}

void sell_asset(const std::string& portfolio_id, const std::string& symbol,
                double quantity, double price) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    db.transaction([&]() {
        auto check = db.prepare(
            "SELECT id, quantity FROM portfolio_assets "
            "WHERE portfolio_id = ?1 AND symbol = ?2");
        check.bind_text(1, portfolio_id);
        check.bind_text(2, symbol);

        if (!check.step()) {
            throw std::runtime_error("Asset not found in portfolio");
        }

        std::string asset_id = check.col_text(0);
        double existing_qty = check.col_double(1);

        if (quantity >= existing_qty) {
            auto del = db.prepare("DELETE FROM portfolio_assets WHERE id = ?1");
            del.bind_text(1, asset_id);
            del.execute();
        } else {
            double new_qty = existing_qty - quantity;
            auto upd = db.prepare(
                "UPDATE portfolio_assets SET quantity = ?1, last_updated = CURRENT_TIMESTAMP WHERE id = ?2");
            upd.bind_double(1, new_qty);
            upd.bind_text(2, asset_id);
            upd.execute();
        }

        // Record SELL transaction
        std::string txn_id = db::generate_uuid();
        double total_value = quantity * price;
        auto txn = db.prepare(
            "INSERT INTO portfolio_transactions (id, portfolio_id, symbol, transaction_type, "
            "quantity, price, total_value, notes, transaction_date) "
            "VALUES (?1, ?2, ?3, 'SELL', ?4, ?5, ?6, NULL, CURRENT_TIMESTAMP)");
        txn.bind_text(1, txn_id);
        txn.bind_text(2, portfolio_id);
        txn.bind_text(3, symbol);
        txn.bind_double(4, quantity);
        txn.bind_double(5, price);
        txn.bind_double(6, total_value);
        txn.execute();

        return 0;
    });
}

std::vector<PortfolioAsset> get_assets(const std::string& portfolio_id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated "
        "FROM portfolio_assets WHERE portfolio_id = ?1 ORDER BY symbol");
    stmt.bind_text(1, portfolio_id);

    std::vector<PortfolioAsset> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2),
            stmt.col_double(3), stmt.col_double(4),
            stmt.col_text(5), stmt.col_text(6)
        });
    }
    return result;
}

void update_asset_symbol(const std::string& asset_id, const std::string& new_symbol) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "UPDATE portfolio_assets SET symbol = ?1, last_updated = CURRENT_TIMESTAMP WHERE id = ?2");
    stmt.bind_text(1, new_symbol);
    stmt.bind_text(2, asset_id);
    stmt.execute();
}

// ============================================================================
// Transactions
// ============================================================================

std::vector<Transaction> get_transactions(const std::string& portfolio_id, int limit) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    std::string sql =
        "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
        "transaction_date, notes FROM portfolio_transactions WHERE portfolio_id = ?1 "
        "ORDER BY transaction_date DESC";
    if (limit > 0) sql += " LIMIT " + std::to_string(limit);

    auto stmt = db.prepare(sql.c_str());
    stmt.bind_text(1, portfolio_id);

    std::vector<Transaction> result;
    while (stmt.step()) {
        result.push_back({
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
            stmt.col_double(4), stmt.col_double(5), stmt.col_double(6),
            stmt.col_text(7), stmt.col_text_opt(8)
        });
    }
    return result;
}

std::optional<Transaction> get_transaction_by_id(const std::string& id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare(
        "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, "
        "transaction_date, notes FROM portfolio_transactions WHERE id = ?1");
    stmt.bind_text(1, id);
    if (!stmt.step()) return std::nullopt;
    return Transaction{
        stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
        stmt.col_double(4), stmt.col_double(5), stmt.col_double(6),
        stmt.col_text(7), stmt.col_text_opt(8)
    };
}

void update_transaction(const std::string& id, double quantity, double price,
                        const std::string& date, const std::string& notes) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    double total_value = quantity * price;
    auto stmt = db.prepare(
        "UPDATE portfolio_transactions SET quantity = ?1, price = ?2, total_value = ?3, "
        "transaction_date = ?4, notes = ?5 WHERE id = ?6");
    stmt.bind_double(1, quantity);
    stmt.bind_double(2, price);
    stmt.bind_double(3, total_value);
    stmt.bind_text(4, date);
    if (notes.empty()) stmt.bind_null(5);
    else stmt.bind_text(5, notes);
    stmt.bind_text(6, id);
    stmt.execute();
}

void delete_transaction(const std::string& id) {
    auto& db = db::Database::instance();
    std::lock_guard<std::recursive_mutex> lock(db.mutex());

    auto stmt = db.prepare("DELETE FROM portfolio_transactions WHERE id = ?1");
    stmt.bind_text(1, id);
    stmt.execute();
}

// ============================================================================
// Summary — compute from assets + live prices
// ============================================================================

PortfolioSummary compute_summary(
    const Portfolio& portfolio,
    const std::vector<PortfolioAsset>& assets,
    const std::map<std::string, std::pair<double, double>>& current_prices) {

    PortfolioSummary summary;
    summary.portfolio = portfolio;
    summary.total_positions = static_cast<int>(assets.size());

    for (const auto& asset : assets) {
        HoldingWithQuote h;
        h.asset = asset;

        auto it = current_prices.find(asset.symbol);
        if (it != current_prices.end()) {
            double cur_price = it->second.first;
            double prev_close = it->second.second;

            h.current_price = cur_price;
            h.market_value = asset.quantity * cur_price;
            h.cost_basis = asset.quantity * asset.avg_buy_price;
            h.unrealized_pnl = h.market_value - h.cost_basis;
            h.unrealized_pnl_percent = (h.cost_basis > 0.0)
                ? (h.unrealized_pnl / h.cost_basis) * 100.0 : 0.0;
            h.day_change = (cur_price - prev_close) * asset.quantity;
            h.day_change_percent = (prev_close > 0.0)
                ? ((cur_price - prev_close) / prev_close) * 100.0 : 0.0;
        } else {
            h.current_price = asset.avg_buy_price;
            h.market_value = asset.quantity * asset.avg_buy_price;
            h.cost_basis = h.market_value;
        }

        summary.total_market_value += h.market_value;
        summary.total_cost_basis += h.cost_basis;
        summary.total_unrealized_pnl += h.unrealized_pnl;
        summary.total_day_change += h.day_change;
        summary.holdings.push_back(std::move(h));
    }

    // Compute weights
    if (summary.total_market_value > 0.0) {
        for (auto& h : summary.holdings) {
            h.weight = (h.market_value / summary.total_market_value) * 100.0;
        }
    }

    summary.total_unrealized_pnl_percent = (summary.total_cost_basis > 0.0)
        ? (summary.total_unrealized_pnl / summary.total_cost_basis) * 100.0 : 0.0;
    summary.total_day_change_percent = (summary.total_market_value > summary.total_day_change && summary.total_market_value > 0.0)
        ? (summary.total_day_change / (summary.total_market_value - summary.total_day_change)) * 100.0 : 0.0;

    return summary;
}

// ============================================================================
// Export — JSON
// ============================================================================

std::string export_portfolio_json(const std::string& portfolio_id) {
    auto pf_opt = get_portfolio_by_id(portfolio_id);
    if (!pf_opt) throw std::runtime_error("Portfolio not found");
    const auto& pf = *pf_opt;

    auto txns = get_transactions(portfolio_id, -1); // all transactions

    nlohmann::json j;
    j["portfolio"] = {
        {"name", pf.name},
        {"owner", pf.owner},
        {"currency", pf.currency},
        {"description", pf.description.value_or("")},
        {"created_at", pf.created_at}
    };

    nlohmann::json txn_arr = nlohmann::json::array();
    for (const auto& t : txns) {
        txn_arr.push_back({
            {"symbol", t.symbol},
            {"transaction_type", t.transaction_type},
            {"quantity", t.quantity},
            {"price", t.price},
            {"total_value", t.total_value},
            {"transaction_date", t.transaction_date},
            {"notes", t.notes.value_or("")}
        });
    }
    j["transactions"] = txn_arr;
    j["export_version"] = "1.0";
    j["export_date"] = ""; // caller can fill

    return j.dump(2);
}

// ============================================================================
// Export — CSV
// ============================================================================

std::string export_portfolio_csv(const std::string& portfolio_id) {
    auto txns = get_transactions(portfolio_id, -1);

    std::ostringstream oss;
    oss << "Date,Symbol,Type,Quantity,Price,Total,Notes\n";
    for (const auto& t : txns) {
        oss << t.transaction_date << ","
            << t.symbol << ","
            << t.transaction_type << ","
            << t.quantity << ","
            << t.price << ","
            << t.total_value << ","
            << "\"" << t.notes.value_or("") << "\"\n";
    }
    return oss.str();
}

// ============================================================================
// Import — JSON
// ============================================================================

ImportResult import_portfolio_json(const std::string& json_data,
                                    const std::string& mode,
                                    const std::string& merge_target_id) {
    auto j = nlohmann::json::parse(json_data);
    ImportResult result;

    std::string target_id;

    if (mode == "merge" && !merge_target_id.empty()) {
        target_id = merge_target_id;
    } else {
        // Create new portfolio
        auto pf_data = j.at("portfolio");
        auto pf = create_portfolio(
            pf_data.value("name", "Imported Portfolio"),
            pf_data.value("owner", ""),
            pf_data.value("currency", "USD"),
            pf_data.value("description", ""));
        target_id = pf.id;
    }

    result.portfolio_id = target_id;

    // Replay transactions chronologically
    auto txns = j.at("transactions");
    // Sort by date ascending for correct replay
    std::vector<nlohmann::json> sorted_txns(txns.begin(), txns.end());
    std::sort(sorted_txns.begin(), sorted_txns.end(),
        [](const nlohmann::json& a, const nlohmann::json& b) {
            return a.value("transaction_date", "") < b.value("transaction_date", "");
        });

    for (const auto& t : sorted_txns) {
        try {
            std::string type = t.value("transaction_type", "BUY");
            std::string symbol = t.value("symbol", "");
            double qty = t.value("quantity", 0.0);
            double price = t.value("price", 0.0);

            if (symbol.empty() || qty <= 0 || price <= 0) {
                result.transactions_skipped++;
                continue;
            }

            if (type == "BUY") {
                add_asset(target_id, symbol, qty, price);
            } else if (type == "SELL") {
                try {
                    sell_asset(target_id, symbol, qty, price);
                } catch (...) {
                    result.transactions_skipped++;
                    continue;
                }
            }
            // DIVIDEND and SPLIT are recorded but don't affect positions
            result.transactions_imported++;
        } catch (...) {
            result.transactions_skipped++;
        }
    }

    return result;
}

} // namespace fincept::portfolio
