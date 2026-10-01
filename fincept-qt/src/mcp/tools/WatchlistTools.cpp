// WatchlistTools.cpp — Watchlist MCP tools
//
// Thread-safety note (2026-05-01):
// These tools mutate fincept::Database via WatchlistRepository. The main DB
// connection is a single QSqlDatabase opened on the main thread. Qt's docs
// state that a QSqlDatabase connection "can only be used from within the
// thread that created it." MCP tool handlers run on the LlmService worker
// thread (QtConcurrent::run), so we MUST marshal every DB call to the main
// thread via run_async_wait — otherwise SQL writes are silently dropped /
// orphaned in the WAL of an off-thread connection and don't survive a
// restart. (This was the actual bug behind "watchlist resets on restart".)

#include "mcp/tools/WatchlistTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "mcp/tools/ThreadHelper.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QVariantMap>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "WatchlistTools";

std::vector<ToolDef> get_watchlist_tools() {
    std::vector<ToolDef> tools;

    // ── get_watchlists ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_watchlists";
        t.description = "Get all watchlists with their symbols.";
        t.category = "watchlist";
        t.handler = [](const QJsonObject&) -> ToolResult {
            QJsonArray result;
            QString error;
            detail::run_async_wait(QCoreApplication::instance(), [&](auto signal_done) {
                auto& repo = WatchlistRepository::instance();
                auto lists = repo.list_all();
                if (lists.is_err()) {
                    error = "Failed to load watchlists: " + QString::fromStdString(lists.error());
                    signal_done();
                    return;
                }
                for (const auto& wl : lists.value()) {
                    auto stocks = repo.get_stocks(wl.id);
                    QJsonArray symbols;
                    if (stocks.is_ok()) {
                        for (const auto& s : stocks.value()) {
                            symbols.append(
                                QJsonObject{{"symbol", s.symbol}, {"name", s.name}, {"added_at", s.added_at}});
                        }
                    }
                    result.append(QJsonObject{{"id", wl.id},
                                              {"name", wl.name},
                                              {"color", wl.color},
                                              {"description", wl.description},
                                              {"symbols", symbols}});
                }
                signal_done();
            });
            if (!error.isEmpty())
                return ToolResult::fail(error);
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── create_watchlist ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_watchlist";
        t.description = "Create a new watchlist.";
        t.category = "watchlist";
        t.is_destructive = true; // persistent write; later add_to_watchlist calls depend on it existing
        t.input_schema.properties =
            QJsonObject{{"name", QJsonObject{{"type", "string"}, {"description", "Watchlist name"}}},
                        {"description", QJsonObject{{"type", "string"}, {"description", "Optional description"}}},
                        {"color", QJsonObject{{"type", "string"}, {"description", "Hex color (optional)"}}}};
        t.input_schema.required = {"name"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString name = args["name"].toString().trimmed();
            if (name.isEmpty())
                return ToolResult::fail("Missing 'name'");

            // `color` and `description` are declared in the schema but the handler used
            // to ignore both — a requested colour silently became the default orange.
            const QString color = args["color"].toString().trimmed();
            const QString description = args["description"].toString().trimmed();
            static const QRegularExpression hex_color(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
            if (!color.isEmpty() && !hex_color.match(color).hasMatch())
                return ToolResult::fail("Invalid 'color' — use a hex colour like #FF6600");

            QString new_id;
            QString error;
            detail::run_async_wait(QCoreApplication::instance(), [&](auto signal_done) {
                auto& repo = WatchlistRepository::instance();
                auto r = color.isEmpty() ? repo.create(name) : repo.create(name, color);
                if (r.is_err()) {
                    error = "Failed to create watchlist: " + QString::fromStdString(r.error());
                } else {
                    new_id = r.value().id;
                    if (!description.isEmpty()) {
                        Watchlist w = r.value();
                        w.description = description;
                        auto u = repo.update(w);
                        if (u.is_err())
                            LOG_WARN(TAG, "Created watchlist but could not save its description: " +
                                              QString::fromStdString(u.error()));
                    }
                }
                signal_done();
            });
            if (!error.isEmpty())
                return ToolResult::fail(error);

            EventBus::instance().publish("watchlist.created", QVariantMap{{"id", new_id}, {"name", name}});
            LOG_INFO(TAG, "Created watchlist: " + name);
            return ToolResult::ok("Watchlist created", QJsonObject{{"id", new_id}, {"name", name}});
        };
        tools.push_back(std::move(t));
    }

    // ── delete_watchlist ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "delete_watchlist";
        t.description = "Delete a watchlist by ID.";
        t.category = "watchlist";
        t.is_destructive = true; // mutation tool — penalise on read-style queries
        t.input_schema.properties =
            QJsonObject{{"watchlist_id", QJsonObject{{"type", "string"}, {"description", "Watchlist ID"}}}};
        t.input_schema.required = {"watchlist_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["watchlist_id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'watchlist_id'");

            QString error;
            detail::run_async_wait(QCoreApplication::instance(), [&](auto signal_done) {
                auto r = WatchlistRepository::instance().remove(id);
                if (r.is_err())
                    error = "Failed to delete watchlist: " + QString::fromStdString(r.error());
                signal_done();
            });
            if (!error.isEmpty())
                return ToolResult::fail(error);

            EventBus::instance().publish("watchlist.deleted", QVariantMap{{"id", id}});
            return ToolResult::ok("Watchlist deleted", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── add_to_watchlist ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_to_watchlist";
        t.description = "Add a ticker symbol to a watchlist. The 'symbol' field MUST be the exact "
                        "exchange-suffixed Yahoo ticker (e.g. 'RITES.NS' for RITES Limited on NSE, "
                        "'AAPL' for Apple on NASDAQ, '7203.T' for Toyota on Tokyo). "
                        "If the user gave a company NAME instead of a ticker — or you are uncertain "
                        "about the exchange suffix — call lookup_symbol(query=<company name>) FIRST "
                        "to resolve the correct ticker, then pass that ticker here. "
                        "Do NOT guess tickers from prior knowledge: cross-listed names and Indian/Asian "
                        "tickers are easy to get wrong, and the watchlist stores whatever you pass. "
                        "If watchlist_id is omitted, the first available watchlist is used.";
        t.category = "watchlist";
        // Persistent write. Also an ordering hazard: two concurrent calls with no
        // watchlist_id both saw "no watchlists" and each created its own "Default".
        t.is_destructive = true;
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol to add"}}},
            {"watchlist_id",
             QJsonObject{{"type", "string"}, {"description", "Watchlist ID (optional — uses first if omitted)"}}},
            {"name", QJsonObject{{"type", "string"}, {"description", "Company/asset name (optional)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            QString watchlist_id = args["watchlist_id"].toString();
            QString name = args["name"].toString();
            QString error;
            detail::run_async_wait(QCoreApplication::instance(), [&](auto signal_done) {
                auto& repo = WatchlistRepository::instance();
                if (watchlist_id.isEmpty()) {
                    auto lists = repo.list_all();
                    if (lists.is_err()) {
                        error = "Failed to load watchlists: " + QString::fromStdString(lists.error());
                        signal_done();
                        return;
                    }
                    if (lists.value().isEmpty()) {
                        auto created = repo.create("Default");
                        if (created.is_err()) {
                            error = "Failed to create watchlist";
                            signal_done();
                            return;
                        }
                        watchlist_id = created.value().id;
                    } else {
                        watchlist_id = lists.value().first().id;
                    }
                } else if (repo.get(watchlist_id).is_err()) {
                    // add_stock() is INSERT OR IGNORE: an id that matches no watchlist was
                    // reported as "Added" while the row went nowhere (or orphaned).
                    error = "Watchlist not found: " + watchlist_id + " (use get_watchlists to list ids)";
                    signal_done();
                    return;
                }

                auto r = repo.add_stock(watchlist_id, symbol, name);
                if (r.is_err())
                    error = "Failed to add symbol: " + QString::fromStdString(r.error());
                signal_done();
            });
            if (!error.isEmpty())
                return ToolResult::fail(error);

            LOG_INFO(TAG, "Added " + symbol + " to watchlist " + watchlist_id);
            EventBus::instance().publish(
                "watchlist.updated",
                QVariantMap{{"watchlist_id", watchlist_id}, {"action", "add"}, {"symbol", symbol}});

            return ToolResult::ok("Added " + symbol + " to watchlist",
                                  QJsonObject{{"symbol", symbol}, {"watchlist_id", watchlist_id}});
        };
        tools.push_back(std::move(t));
    }

    // ── remove_from_watchlist ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "remove_from_watchlist";
        t.description = "Remove a symbol from a watchlist.";
        t.category = "watchlist";
        t.is_destructive = true; // mutation tool — penalise on read-style queries
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol to remove"}}},
            {"watchlist_id", QJsonObject{{"type", "string"},
                                         {"description", "Watchlist ID (optional — removes from all if omitted)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            QString watchlist_id = args["watchlist_id"].toString();
            QStringList removed_from;
            QString error;
            detail::run_async_wait(QCoreApplication::instance(), [&](auto signal_done) {
                auto& repo = WatchlistRepository::instance();
                // Only touch watchlists that actually hold the symbol, and keep every
                // repository error. The old version discarded the Result of each delete
                // and answered "Removed" for a missing symbol, a bad id, or a failed write.
                auto remove_from = [&](const QString& wl_id) {
                    auto stocks = repo.get_stocks(wl_id);
                    if (stocks.is_err())
                        return;
                    bool present = false;
                    for (const auto& s : stocks.value()) {
                        if (s.symbol.compare(symbol, Qt::CaseInsensitive) == 0) {
                            present = true;
                            break;
                        }
                    }
                    if (!present)
                        return;
                    auto r = repo.remove_stock(wl_id, symbol);
                    if (r.is_err())
                        error = "Failed to remove symbol: " + QString::fromStdString(r.error());
                    else
                        removed_from.append(wl_id);
                };
                if (watchlist_id.isEmpty()) {
                    auto lists = repo.list_all();
                    if (lists.is_ok()) {
                        for (const auto& wl : lists.value())
                            remove_from(wl.id);
                    } else {
                        error = "Failed to load watchlists: " + QString::fromStdString(lists.error());
                    }
                } else {
                    remove_from(watchlist_id);
                }
                signal_done();
            });
            if (!error.isEmpty())
                return ToolResult::fail(error);
            if (removed_from.isEmpty())
                return ToolResult::fail(symbol + (watchlist_id.isEmpty() ? QStringLiteral(" is not in any watchlist")
                                                                         : " is not in watchlist " + watchlist_id));

            EventBus::instance().publish("watchlist.updated", QVariantMap{{"action", "remove"}, {"symbol", symbol}});
            return ToolResult::ok("Removed " + symbol + " from " + QString::number(removed_from.size()) +
                                      (removed_from.size() == 1 ? " watchlist" : " watchlists"),
                                  QJsonObject{{"symbol", symbol}, {"watchlist_ids", QJsonArray::fromStringList(removed_from)}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
