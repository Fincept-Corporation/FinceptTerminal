#include "services/workflow/adapters/ServiceBridges.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ConfirmationService.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/RiskManager.h"
#include "trading/ExchangeService.h"

#include <QDateTime>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>

namespace fincept::workflow {

// ── Market Data Bridge ─────────────────────────────────────────────────

void wire_market_data_bridges(NodeRegistry& registry) {
    // Get Quote — uses MarketDataService via PythonRunner
    auto* quote_def = const_cast<NodeTypeDef*>(registry.find("market.get_quote"));
    if (quote_def) {
        quote_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString symbol = params.value("symbol").toString();
            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }
            // Delegate to MarketDataService (async, returns via callback)
            // For now, return placeholder — real wiring requires MarketDataService::fetch_quote
            QJsonObject out;
            out["symbol"] = symbol;
            out["source"] = params.value("source").toString("yahoo");
            out["status"] = "pending_integration";
            cb(true, out, {});
            LOG_DEBUG("MarketBridge", QString("Quote requested: %1").arg(symbol));
        };
    }

    // Historical Data
    auto* hist_def = const_cast<NodeTypeDef*>(registry.find("market.get_historical"));
    if (hist_def) {
        hist_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString symbol = params.value("symbol").toString();
            QJsonObject out;
            out["symbol"] = symbol;
            out["period"] = params.value("period").toString("1y");
            out["interval"] = params.value("interval").toString("1d");
            out["status"] = "pending_integration";
            cb(true, out, {});
        };
    }

    // Crypto Price — uses ExchangeService (Kraken public API, no key needed)
    auto* crypto_price_def = const_cast<NodeTypeDef*>(registry.find("market.get_crypto_price"));
    if (crypto_price_def) {
        crypto_price_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                       std::function<void(bool, QJsonValue, QString)> cb) {
            QString base   = params.value("symbol").toString("BTC").toUpper();
            QString quote  = params.value("quote").toString("USD").toUpper();
            QString symbol = base + "/" + quote;   // Kraken format: BTC/USD

            (void)QtConcurrent::run([symbol, cb]() {
                auto& svc = trading::ExchangeService::instance();
                trading::TickerData t = svc.fetch_ticker(symbol);

                if (t.last <= 0.0) {
                    cb(false, {}, QString("No price data for %1").arg(symbol));
                    return;
                }

                QJsonObject out;
                out["symbol"]       = symbol;
                out["price"]        = t.last;
                out["bid"]          = t.bid;
                out["ask"]          = t.ask;
                out["high"]         = t.high;
                out["low"]          = t.low;
                out["change"]       = t.change;
                out["change_pct"]   = t.percentage;
                out["volume"]       = t.base_volume;
                out["exchange"]     = "kraken";
                out["timestamp"]    = static_cast<qint64>(t.timestamp);
                cb(true, out, {});
                LOG_DEBUG("MarketBridge", QString("Crypto price fetched: %1 = %2").arg(symbol).arg(t.last));
            });
        };
    }

    // Wire remaining market data nodes with placeholder executors
    for (const auto& id : {"market.get_depth", "market.get_stats", "market.get_fundamentals", "market.get_economics",
                           "market.get_news"}) {
        auto* def = const_cast<NodeTypeDef*>(registry.find(id));
        if (def && !def->execute) {
            def->execute = [id_str = QString(id)](const QJsonObject& params, const QVector<QJsonValue>&,
                                                  std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["node_type"] = id_str;
                out["symbol"] = params.value("symbol").toString();
                out["status"] = "pending_integration";
                cb(true, out, {});
            };
        }
    }

    LOG_INFO("ServiceBridges", "Market data bridges wired");
}

// ── Trading Bridge ─────────────────────────────────────────────────────

void wire_trading_bridges(NodeRegistry& registry) {
    // Place Order — with confirmation and audit
    auto* place_def = const_cast<NodeTypeDef*>(registry.find("trading.place_order"));
    if (place_def) {
        place_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString symbol = params.value("symbol").toString();
            QString side = params.value("side").toString("buy");
            double qty = params.value("quantity").toDouble(1);
            QString broker = params.value("broker").toString("paper");
            bool paper = (broker == "paper");

            ConfirmationRequest req;
            req.type = ConfirmationType::Trade;
            req.risk = paper ? RiskLevel::Low : RiskLevel::High;
            req.title = QString("%1 %2 x%3").arg(side.toUpper(), symbol).arg(qty);
            req.message =
                QString("Place %1 order for %2 shares of %3 via %4").arg(side, QString::number(qty), symbol, broker);
            req.details = params;
            req.paper_trading = paper;

            ConfirmationService::instance().request(
                req, [cb, params, symbol, side, qty, broker, paper](bool approved, const QString& notes) {
                    if (!approved) {
                        cb(false, {}, "Order rejected by user: " + notes);
                        return;
                    }

                    // Log the order
                    AuditLogger::instance().log(AuditAction::OrderPlaced, {}, {}, symbol,
                                                QString("%1 %2 x%3 via %4").arg(side, symbol).arg(qty).arg(broker),
                                                params, paper);

                    QJsonObject out;
                    out["order_id"] = "ORD-" + QString::number(QDateTime::currentMSecsSinceEpoch());
                    out["symbol"] = symbol;
                    out["side"] = side;
                    out["quantity"] = qty;
                    out["broker"] = broker;
                    out["status"] = "submitted";
                    cb(true, out, {});
                });
        };
    }

    // Wire remaining trading nodes with placeholder executors
    for (const auto& id :
         {"trading.cancel_order", "trading.modify_order", "trading.get_orders", "trading.get_positions",
          "trading.get_holdings", "trading.get_balance", "trading.close_position"}) {
        auto* def = const_cast<NodeTypeDef*>(registry.find(id));
        if (def && !def->execute) {
            def->execute = [id_str = QString(id)](const QJsonObject& params, const QVector<QJsonValue>&,
                                                  std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["node_type"] = id_str;
                out["broker"] = params.value("broker").toString("paper");
                out["status"] = "pending_integration";
                cb(true, out, {});
            };
        }
    }

    LOG_INFO("ServiceBridges", "Trading bridges wired");
}

// ── Agent Bridge ───────────────────────────────────────────────────────

void wire_agent_bridges(NodeRegistry& registry) {
    auto* agent_def = const_cast<NodeTypeDef*>(registry.find("agent.single"));
    if (agent_def) {
        agent_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString agent_type = params.value("agent_type").toString("general");
            QString prompt = params.value("prompt").toString();

            // Build context from inputs
            QJsonObject context;
            if (!inputs.isEmpty())
                context["input"] = inputs[0];
            context["agent_type"] = agent_type;
            context["prompt"] = prompt;

            // Delegate to AgentService — placeholder for now
            QJsonObject out;
            out["agent_type"] = agent_type;
            out["prompt"] = prompt;
            out["status"] = "pending_integration";
            out["response"] = "Agent execution requires AgentService integration";
            cb(true, out, {});
            LOG_DEBUG("AgentBridge", QString("Agent requested: %1").arg(agent_type));
        };
    }

    // Wire multi-agent and mediator similarly
    for (const auto& id : {"agent.multi", "agent.mediator"}) {
        auto* def = const_cast<NodeTypeDef*>(registry.find(id));
        if (def && !def->execute) {
            def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                              std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["mode"] = params.value("mode").toString("sequential");
                out["status"] = "pending_integration";
                cb(true, out, {});
            };
        }
    }

    LOG_INFO("ServiceBridges", "Agent bridges wired");
}

// ── Wire All ───────────────────────────────────────────────────────────

static void wire_utility_bridges(NodeRegistry& registry) {
    // ── HTTP Request — via HttpClient ──────────────────────────────
    auto* http_def = const_cast<NodeTypeDef*>(registry.find("utility.http_request"));
    if (http_def) {
        http_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString url = params.value("url").toString();
            QString method = params.value("method").toString("GET");
            if (url.isEmpty()) {
                cb(false, {}, "URL is required");
                return;
            }

            // Build output — real HttpClient integration point
            QJsonObject out;
            out["url"] = url;
            out["method"] = method;
            out["status"] = "pending_http_integration";
            if (!inputs.isEmpty())
                out["input"] = inputs[0];
            cb(true, out, {});
            LOG_DEBUG("UtilityBridge", QString("HTTP %1 %2").arg(method, url));
        };
    }

    // ── Code node — via PythonRunner ──────────────────────────────
    auto* code_def = const_cast<NodeTypeDef*>(registry.find("utility.code"));
    if (code_def) {
        code_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString language = params.value("language").toString("python");
            QString code = params.value("code").toString();
            if (code.isEmpty()) {
                cb(false, {}, "Code is empty");
                return;
            }

            QJsonObject out;
            out["language"] = language;
            out["code_length"] = code.length();
            out["status"] = "pending_python_integration";
            if (!inputs.isEmpty())
                out["input"] = inputs[0];
            cb(true, out, {});
            LOG_DEBUG("UtilityBridge", QString("Code exec (%1, %2 chars)").arg(language).arg(code.length()));
        };
    }

    // ── Analytics nodes — via PythonRunner scripts ────────────────
    for (const auto& id :
         {"analytics.technical_indicators", "analytics.backtest", "analytics.portfolio_optimization",
          "analytics.performance_metrics", "analytics.correlation_matrix", "analytics.risk_analysis"}) {
        auto* def = const_cast<NodeTypeDef*>(registry.find(id));
        if (def && !def->execute) {
            def->execute = [id_str = QString(id)](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                                  std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["node_type"] = id_str;
                out["status"] = "pending_python_integration";
                if (!inputs.isEmpty())
                    out["input"] = inputs[0];
                for (auto it = params.constBegin(); it != params.constEnd(); ++it)
                    out[it.key()] = it.value();
                cb(true, out, {});
            };
        }
    }

    // ── File nodes — via Qt file I/O ─────────────────────────────
    for (const auto& id : {"file.operations", "file.spreadsheet", "file.binary", "file.convert", "file.compress"}) {
        auto* def = const_cast<NodeTypeDef*>(registry.find(id));
        if (def && !def->execute) {
            def->execute = [id_str = QString(id)](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                                  std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["node_type"] = id_str;
                out["operation"] = params.value("operation").toString();
                out["path"] = params.value("path").toString();
                out["status"] = "pending_file_integration";
                if (!inputs.isEmpty())
                    out["input"] = inputs[0];
                cb(true, out, {});
            };
        }
    }

    // ── HTML Extract — stub ──────────────────────────────────────
    auto* html_def = const_cast<NodeTypeDef*>(registry.find("format.html_extract"));
    if (html_def && !html_def->execute) {
        html_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QJsonObject out;
            out["selector"] = params.value("selector").toString();
            out["status"] = "pending_integration";
            if (!inputs.isEmpty())
                out["input"] = inputs[0];
            cb(true, out, {});
        };
    }

    // ── RSS Read — stub ──────────────────────────────────────────
    auto* rss_def = const_cast<NodeTypeDef*>(registry.find("utility.rss_read"));
    if (rss_def && !rss_def->execute) {
        rss_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                              std::function<void(bool, QJsonValue, QString)> cb) {
            QJsonObject out;
            out["url"] = params.value("url").toString();
            out["status"] = "pending_integration";
            cb(true, out, {});
        };
    }

    // ── Crypto/Hash — stub ───────────────────────────────────────
    auto* crypto_def = const_cast<NodeTypeDef*>(registry.find("utility.crypto"));
    if (crypto_def && !crypto_def->execute) {
        crypto_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QJsonObject out;
            out["operation"] = params.value("operation").toString();
            out["status"] = "pending_integration";
            if (!inputs.isEmpty())
                out["input"] = inputs[0];
            cb(true, out, {});
        };
    }

    // ── Database — stub ──────────────────────────────────────────
    auto* db_def = const_cast<NodeTypeDef*>(registry.find("utility.database"));
    if (db_def && !db_def->execute) {
        db_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                             std::function<void(bool, QJsonValue, QString)> cb) {
            QJsonObject out;
            out["operation"] = params.value("operation").toString();
            out["status"] = "pending_integration";
            cb(true, out, {});
        };
    }

    LOG_INFO("ServiceBridges", "Utility bridges wired");
}

static void wire_mcp_bridges(NodeRegistry& registry) {
    auto* def = const_cast<NodeTypeDef*>(registry.find("mcp.tool_call"));
    if (!def) return;

    def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
        QString tool = params.value("tool").toString().trimmed();
        if (tool.isEmpty()) {
            cb(false, {}, "MCP Tool: 'tool' parameter is required");
            return;
        }

        // If input is from a Tool Picker node it carries { "tool": "...", "args": {...} }
        // — use those values when the param isn't explicitly set.
        QJsonObject input_obj;
        if (!inputs.isEmpty() && inputs[0].isObject())
            input_obj = inputs[0].toObject();

        if (tool.isEmpty())
            tool = input_obj.value("tool").toString().trimmed();
        if (tool.isEmpty()) {
            cb(false, {}, "MCP Tool: no tool selected — set the 'Tool' param or connect a Tool Picker node");
            return;
        }

        // Args: prefer "args" sub-object from Tool Picker, else pass the whole input object
        QJsonObject args;
        if (input_obj.contains("args") && input_obj.value("args").isObject())
            args = input_obj.value("args").toObject();
        else if (!input_obj.contains("tool"))
            args = input_obj; // plain data input, not a Tool Picker output

        (void)QtConcurrent::run([tool, args, cb]() {
            auto& svc = mcp::McpService::instance();
            mcp::ToolResult result;

            // Support both "serverId__toolName" and bare tool name
            if (tool.contains("__")) {
                result = svc.execute_openai_function(tool, args);
            } else {
                result = svc.execute_tool(mcp::INTERNAL_SERVER_ID, tool, args);
            }

            if (!result.success) {
                cb(false, {}, result.error.isEmpty() ? "MCP tool failed" : result.error);
                return;
            }

            // Return the result data; wrap primitive values in an object
            QJsonValue out = result.data.isNull() || result.data.isUndefined()
                                 ? QJsonValue(result.to_json())
                                 : result.data;
            cb(true, out, {});
            LOG_DEBUG("McpBridge", QString("Tool '%1' executed successfully").arg(tool));
        });
    };

    LOG_INFO("ServiceBridges", "MCP bridge wired");
}

void wire_all_bridges(NodeRegistry& registry) {
    wire_mcp_bridges(registry);
    wire_market_data_bridges(registry);
    wire_trading_bridges(registry);
    wire_agent_bridges(registry);
    wire_utility_bridges(registry);

    // Wire any remaining nullptr executors with pass-through
    int wired_count = 0;
    for (const auto& def : registry.all()) {
        if (!def.execute) {
            auto* mutable_def = const_cast<NodeTypeDef*>(registry.find(def.type_id));
            if (mutable_def) {
                mutable_def->execute = [type_id = def.type_id](const QJsonObject&, const QVector<QJsonValue>& inputs,
                                                               std::function<void(bool, QJsonValue, QString)> cb) {
                    auto data = inputs.isEmpty() ? QJsonValue(QJsonObject{{"node", type_id}}) : inputs[0];
                    cb(true, data, {});
                };
                wired_count++;
            }
        }
    }

    LOG_INFO("ServiceBridges", QString("All bridges wired (%1 pass-through fallbacks)").arg(wired_count));
}

} // namespace fincept::workflow
