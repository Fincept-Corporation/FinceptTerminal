// PortfolioTools.cpp — Portfolio tab MCP tools
// Merges flat holdings (PortfolioHoldingsRepository) + full CRUD (PortfolioRepository)

#include "mcp/tools/PortfolioTools.h"

#include "core/logging/Logger.h"
#include "storage/repositories/PortfolioHoldingsRepository.h"
#include "storage/repositories/PortfolioRepository.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG = "PortfolioTools";

std::vector<ToolDef> get_portfolio_tools() {
    std::vector<ToolDef> tools;

    // ════════════════════════════════════════════════════════════════════
    // Holdings (flat quick-add list)
    // ════════════════════════════════════════════════════════════════════

    // ── get_holdings ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_holdings";
        t.description = "Get all active portfolio holdings (symbol, shares, avg cost).";
        t.category = "portfolio";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto result = PortfolioHoldingsRepository::instance().get_active();
            if (result.is_err())
                return ToolResult::fail("Failed to load holdings: " + QString::fromStdString(result.error()));

            QJsonArray arr;
            for (const auto& h : result.value()) {
                arr.append(QJsonObject{{"id", h.id},
                                       {"symbol", h.symbol},
                                       {"name", h.name},
                                       {"shares", h.shares},
                                       {"avg_cost", h.avg_cost},
                                       {"added_at", h.added_at},
                                       {"updated_at", h.updated_at}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── add_holding ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_holding";
        t.description = "Add or increase a position in the portfolio holdings list.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol (e.g. AAPL)"}}},
                        {"shares", QJsonObject{{"type", "number"}, {"description", "Number of shares/units"}}},
                        {"avg_cost", QJsonObject{{"type", "number"}, {"description", "Average cost per share/unit"}}},
                        {"name", QJsonObject{{"type", "string"}, {"description", "Company/asset name (optional)"}}}};
        t.input_schema.required = {"symbol", "shares", "avg_cost"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            double shares = args["shares"].toDouble(0.0);
            double avg_cost = args["avg_cost"].toDouble(0.0);
            QString name = args["name"].toString();

            if (symbol.isEmpty() || shares <= 0 || avg_cost <= 0)
                return ToolResult::fail("Missing or invalid: symbol, shares (>0), avg_cost (>0)");

            auto r = PortfolioHoldingsRepository::instance().add(symbol, shares, avg_cost, name);
            if (r.is_err())
                return ToolResult::fail("Failed to add holding: " + QString::fromStdString(r.error()));

            LOG_INFO(TAG, QString("Added holding: %1 x%2 @ %3").arg(symbol).arg(shares).arg(avg_cost));
            return ToolResult::ok("Holding added", QJsonObject{{"id", static_cast<int>(r.value())},
                                                               {"symbol", symbol},
                                                               {"shares", shares},
                                                               {"avg_cost", avg_cost}});
        };
        tools.push_back(std::move(t));
    }

    // ── update_holding ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "update_holding";
        t.description = "Update shares and average cost for an existing holding by ID.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "integer"}, {"description", "Holding ID"}}},
                        {"shares", QJsonObject{{"type", "number"}, {"description", "Updated share count"}}},
                        {"avg_cost", QJsonObject{{"type", "number"}, {"description", "Updated average cost"}}}};
        t.input_schema.required = {"id", "shares", "avg_cost"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args["id"].toInt(-1);
            double shares = args["shares"].toDouble(0.0);
            double avg_cost = args["avg_cost"].toDouble(0.0);

            if (id < 0 || shares <= 0 || avg_cost <= 0)
                return ToolResult::fail("Missing or invalid: id, shares (>0), avg_cost (>0)");

            auto r = PortfolioHoldingsRepository::instance().update(id, shares, avg_cost);
            if (r.is_err())
                return ToolResult::fail("Failed to update holding: " + QString::fromStdString(r.error()));

            return ToolResult::ok("Holding updated", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── remove_holding ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "remove_holding";
        t.description = "Remove a holding from the portfolio by ID.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "integer"}, {"description", "Holding ID"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args["id"].toInt(-1);
            if (id < 0)
                return ToolResult::fail("Missing or invalid 'id'");

            auto r = PortfolioHoldingsRepository::instance().remove(id);
            if (r.is_err())
                return ToolResult::fail("Failed to remove holding: " + QString::fromStdString(r.error()));

            return ToolResult::ok("Holding removed", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // Portfolio CRUD (named portfolios with assets, transactions, snapshots)
    // ════════════════════════════════════════════════════════════════════

    // ── list_portfolios ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_portfolios";
        t.description = "List all named investment portfolios.";
        t.category = "portfolio";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto r = PortfolioRepository::instance().list_portfolios();
            if (r.is_err())
                return ToolResult::fail("Failed to list portfolios: " + QString::fromStdString(r.error()));

            QJsonArray arr;
            for (const auto& p : r.value()) {
                arr.append(QJsonObject{{"id", p.id},
                                       {"name", p.name},
                                       {"owner", p.owner},
                                       {"currency", p.currency},
                                       {"description", p.description},
                                       {"created_at", p.created_at},
                                       {"updated_at", p.updated_at}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── get_portfolio ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_portfolio";
        t.description = "Get a single portfolio by ID.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            auto r = PortfolioRepository::instance().get_portfolio(id);
            if (r.is_err())
                return ToolResult::fail("Portfolio not found: " + QString::fromStdString(r.error()));

            const auto& p = r.value();
            return ToolResult::ok_data(QJsonObject{{"id", p.id},
                                                   {"name", p.name},
                                                   {"owner", p.owner},
                                                   {"currency", p.currency},
                                                   {"description", p.description},
                                                   {"created_at", p.created_at},
                                                   {"updated_at", p.updated_at}});
        };
        tools.push_back(std::move(t));
    }

    // ── create_portfolio ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_portfolio";
        t.description = "Create a new named investment portfolio.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"name", QJsonObject{{"type", "string"}, {"description", "Portfolio name"}}},
                        {"owner", QJsonObject{{"type", "string"}, {"description", "Owner name"}}},
                        {"currency", QJsonObject{{"type", "string"}, {"description", "Currency code (default: USD)"}}},
                        {"description", QJsonObject{{"type", "string"}, {"description", "Optional description"}}}};
        t.input_schema.required = {"name", "owner"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString name = args["name"].toString().trimmed();
            QString owner = args["owner"].toString().trimmed();
            if (name.isEmpty() || owner.isEmpty())
                return ToolResult::fail("Missing 'name' or 'owner'");

            QString currency = args["currency"].toString("USD");
            QString description = args["description"].toString();

            auto r = PortfolioRepository::instance().create_portfolio(name, owner, currency, description);
            if (r.is_err())
                return ToolResult::fail("Failed to create portfolio: " + QString::fromStdString(r.error()));

            LOG_INFO(TAG, "Created portfolio: " + name);
            return ToolResult::ok("Portfolio created",
                                  QJsonObject{{"id", r.value()}, {"name", name}, {"currency", currency}});
        };
        tools.push_back(std::move(t));
    }

    // ── delete_portfolio ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "delete_portfolio";
        t.description = "Delete a portfolio and all its assets and transactions.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            auto r = PortfolioRepository::instance().delete_portfolio(id);
            if (r.is_err())
                return ToolResult::fail("Failed to delete portfolio: " + QString::fromStdString(r.error()));

            LOG_INFO(TAG, "Deleted portfolio: " + id);
            return ToolResult::ok("Portfolio deleted", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_portfolio_assets ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_portfolio_assets";
        t.description = "Get all asset holdings for a named portfolio.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            auto r = PortfolioRepository::instance().get_assets(id);
            if (r.is_err())
                return ToolResult::fail("Failed to load assets: " + QString::fromStdString(r.error()));

            QJsonArray arr;
            for (const auto& a : r.value()) {
                arr.append(QJsonObject{{"id", a.id},
                                       {"symbol", a.symbol},
                                       {"quantity", a.quantity},
                                       {"avg_buy_price", a.avg_buy_price},
                                       {"first_purchase_date", a.first_purchase_date},
                                       {"last_updated", a.last_updated}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── add_portfolio_asset ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_portfolio_asset";
        t.description = "Add a new asset/holding to a named portfolio.";
        t.category = "portfolio";
        t.input_schema.properties = QJsonObject{
            {"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}},
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol (e.g. AAPL)"}}},
            {"quantity", QJsonObject{{"type", "number"}, {"description", "Number of shares/units"}}},
            {"price", QJsonObject{{"type", "number"}, {"description", "Purchase price per unit"}}},
            {"date", QJsonObject{{"type", "string"}, {"description", "Purchase date YYYY-MM-DD (optional)"}}}};
        t.input_schema.required = {"portfolio_id", "symbol", "quantity", "price"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString portfolio_id = args["portfolio_id"].toString().trimmed();
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            double quantity = args["quantity"].toDouble(0.0);
            double price = args["price"].toDouble(0.0);
            QString date = args["date"].toString();

            if (portfolio_id.isEmpty() || symbol.isEmpty() || quantity <= 0 || price <= 0)
                return ToolResult::fail("Missing or invalid: portfolio_id, symbol, quantity (>0), price (>0)");

            auto r = PortfolioRepository::instance().add_asset(portfolio_id, symbol, quantity, price, date);
            if (r.is_err())
                return ToolResult::fail("Failed to add asset: " + QString::fromStdString(r.error()));

            LOG_INFO(TAG, QString("Added asset %1 x%2 to portfolio %3").arg(symbol).arg(quantity).arg(portfolio_id));
            return ToolResult::ok("Asset added", QJsonObject{{"id", static_cast<int>(r.value())},
                                                             {"symbol", symbol},
                                                             {"quantity", quantity},
                                                             {"price", price}});
        };
        tools.push_back(std::move(t));
    }

    // ── remove_portfolio_asset ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "remove_portfolio_asset";
        t.description = "Remove an asset from a named portfolio by symbol.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}},
                        {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol to remove"}}}};
        t.input_schema.required = {"portfolio_id", "symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString portfolio_id = args["portfolio_id"].toString().trimmed();
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (portfolio_id.isEmpty() || symbol.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id' or 'symbol'");

            auto r = PortfolioRepository::instance().remove_asset(portfolio_id, symbol);
            if (r.is_err())
                return ToolResult::fail("Failed to remove asset: " + QString::fromStdString(r.error()));

            return ToolResult::ok("Asset removed", QJsonObject{{"symbol", symbol}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_transactions ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_transactions";
        t.description = "Get transaction history for a portfolio.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}},
                        {"limit", QJsonObject{{"type", "integer"}, {"description", "Max rows (default: 50)"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            int limit = args["limit"].toInt(50);
            auto r = PortfolioRepository::instance().get_transactions(id, limit);
            if (r.is_err())
                return ToolResult::fail("Failed to load transactions: " + QString::fromStdString(r.error()));

            QJsonArray arr;
            for (const auto& tx : r.value()) {
                arr.append(QJsonObject{{"id", tx.id},
                                       {"symbol", tx.symbol},
                                       {"type", tx.transaction_type},
                                       {"quantity", tx.quantity},
                                       {"price", tx.price},
                                       {"total_value", tx.total_value},
                                       {"date", tx.transaction_date},
                                       {"notes", tx.notes},
                                       {"created_at", tx.created_at}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── add_transaction ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_transaction";
        t.description = "Record a transaction (BUY, SELL, DIVIDEND, SPLIT) in a portfolio.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}},
                        {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
                        {"type", QJsonObject{{"type", "string"}, {"description", "BUY, SELL, DIVIDEND, SPLIT"}}},
                        {"quantity", QJsonObject{{"type", "number"}, {"description", "Number of shares/units"}}},
                        {"price", QJsonObject{{"type", "number"}, {"description", "Price per unit"}}},
                        {"date", QJsonObject{{"type", "string"}, {"description", "Transaction date YYYY-MM-DD"}}},
                        {"notes", QJsonObject{{"type", "string"}, {"description", "Optional notes"}}}};
        t.input_schema.required = {"portfolio_id", "symbol", "type", "quantity", "price", "date"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString portfolio_id = args["portfolio_id"].toString().trimmed();
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            QString type = args["type"].toString().trimmed().toUpper();
            double quantity = args["quantity"].toDouble(0.0);
            double price = args["price"].toDouble(0.0);
            QString date = args["date"].toString().trimmed();
            QString notes = args["notes"].toString();

            if (portfolio_id.isEmpty() || symbol.isEmpty() || type.isEmpty() || quantity <= 0 || date.isEmpty())
                return ToolResult::fail("Missing required: portfolio_id, symbol, type, quantity (>0), date");

            static const QStringList valid_types{"BUY", "SELL", "DIVIDEND", "SPLIT"};
            if (!valid_types.contains(type))
                return ToolResult::fail("Invalid type. Must be BUY, SELL, DIVIDEND, or SPLIT");

            auto r = PortfolioRepository::instance().add_transaction(portfolio_id, symbol, type, quantity, price, date,
                                                                     notes);
            if (r.is_err())
                return ToolResult::fail("Failed to add transaction: " + QString::fromStdString(r.error()));

            LOG_INFO(TAG, QString("Transaction %1 %2 x%3").arg(type, symbol).arg(quantity));
            return ToolResult::ok("Transaction recorded",
                                  QJsonObject{{"id", r.value()}, {"symbol", symbol}, {"type", type}});
        };
        tools.push_back(std::move(t));
    }

    // ── delete_transaction ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "delete_transaction";
        t.description = "Delete a transaction by ID.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"transaction_id", QJsonObject{{"type", "string"}, {"description", "Transaction ID"}}}};
        t.input_schema.required = {"transaction_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["transaction_id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'transaction_id'");

            auto r = PortfolioRepository::instance().delete_transaction(id);
            if (r.is_err())
                return ToolResult::fail("Failed to delete transaction: " + QString::fromStdString(r.error()));

            return ToolResult::ok("Transaction deleted", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── get_portfolio_snapshots ─────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_portfolio_snapshots";
        t.description = "Get historical performance snapshots for a portfolio.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}},
                        {"days", QJsonObject{{"type", "integer"}, {"description", "Days of history (default: 365)"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            int days = args["days"].toInt(365);
            auto r = PortfolioRepository::instance().get_snapshots(id, days);
            if (r.is_err())
                return ToolResult::fail("Failed to load snapshots: " + QString::fromStdString(r.error()));

            QJsonArray arr;
            for (const auto& s : r.value()) {
                arr.append(QJsonObject{{"id", s.id},
                                       {"total_value", s.total_value},
                                       {"total_cost_basis", s.total_cost_basis},
                                       {"total_pnl", s.total_pnl},
                                       {"total_pnl_percent", s.total_pnl_percent},
                                       {"date", s.snapshot_date}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
