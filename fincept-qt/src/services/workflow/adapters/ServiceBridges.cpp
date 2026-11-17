#include "services/workflow/adapters/ServiceBridges.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "mcp/server/CallApproval.h"
#include "network/http/HttpClient.h"
#include "python/PythonRunner.h"
#include "services/notifications/NotificationService.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/RiskManager.h"
#include "services/workflow/WorkflowExecutor.h"
#include "services/workflow/adapters/TradingBridgeLogic.h"
#include "services/workflow/adapters/WorkflowDbQuery.h"
#include "storage/repositories/WorkflowRepository.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerRegistry.h"
#include "trading/ExchangeService.h"
#include "trading/ExchangeSession.h"
#include "trading/ExchangeSessionManager.h"
#include "trading/OrderThreadPool.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedTrading.h"
#include "ui/formatting/NumberFormat.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPageSize>
#include <QPainter>
#include <QPointer>
#include <QPrinter>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QTimer>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrent>

#include <optional>

using fincept::python::extract_json;
using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace fincept::workflow {

// ── CSV helpers for the "Convert to File" bridge ──────────────────────

// One CSV cell: full-precision numbers (QString::number() kept 6 significant digits, so
// 1234.5678 was exported as 1234.57 and 12345678 as 1.23457e+07), true/false, empty for
// null, nested values as JSON, and RFC 4180 quoting where needed.
static QString bridge_csv_cell(const QJsonValue& v) {
    QString s = ExpressionEngine::value_to_string(v);
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n')) ||
        s.contains(QLatin1Char('\r')))
        s = QLatin1Char('"') + s.replace(QLatin1Char('"'), QLatin1String("\"\"")) + QLatin1Char('"');
    return s;
}

// Rows -> CSV text. The header is the union of every row's keys (first-seen order) — taking
// it from row 0 alone dropped any column that first appears later.
static QString bridge_csv_from_rows(const QJsonArray& rows) {
    QStringList headers;
    for (const QJsonValue& row : rows) {
        if (!row.isObject())
            continue;
        for (const QString& key : row.toObject().keys()) {
            if (!headers.contains(key))
                headers << key;
        }
    }
    QString csv;
    if (headers.isEmpty())
        return csv;
    QStringList head_cells;
    for (const QString& h : headers)
        head_cells << bridge_csv_cell(h);
    csv += head_cells.join(QLatin1Char(',')) + QLatin1Char('\n');
    for (const QJsonValue& row : rows) {
        if (!row.isObject())
            continue;
        const QJsonObject obj = row.toObject();
        QStringList cells;
        for (const QString& h : headers)
            cells << bridge_csv_cell(obj.value(h));
        csv += cells.join(QLatin1Char(',')) + QLatin1Char('\n');
    }
    return csv;
}

// Semi-Auto gate for headless workflow order nodes. When the account is in
// Semi-Auto mode, queue the (entry) order for manual approval — it surfaces in
// the status-bar Pending Orders popover — report a "queued" result via cb, and
// return true so the caller skips immediate placement. Auto mode returns false
// and the caller places as normal. Only use for entry/add ops; risk-reducing
// ops (close position, protective stops) must stay immediate.
static bool gate_or_queue(const QString& account_id, const fincept::trading::UnifiedOrder& order,
                          const std::function<void(bool, QJsonValue, QString)>& cb) {
    if (!fincept::trading::ActionCenter::instance().should_queue(account_id, "placeorder"))
        return false;
    const QString pid = fincept::trading::ActionCenter::instance().queue_order(
        account_id, "placeorder", fincept::trading::ActionCenter::serialize_unified_order(order));
    QJsonObject out;
    out["status"] = pid.isEmpty() ? "queue_failed" : "queued_for_approval";
    out["pending_id"] = pid;
    cb(!pid.isEmpty(), out, pid.isEmpty() ? QString("Failed to queue order for approval") : QString());
    return true;
}

// UnifiedOrder::source of an order a workflow node creates: "workflow:<id>" (the
// executor passes the workflow's id as _workflow_id to every trading.* node). Entry /
// add orders carry it into UnifiedTrading's approval rule (Semi-Auto -> queued, like
// gate_or_queue). Risk-reducing orders (close position, trailing stop, flatten) carry
// bridge_exit_order_source() instead. Paper orders carry them too (audit only).
static QString bridge_order_source(const QJsonObject& params) {
    const QString id = params.value(QStringLiteral("_workflow_id")).toString().trimmed();
    return id.isEmpty() ? QStringLiteral("workflow") : QStringLiteral("workflow:") + id;
}

namespace {
// UnifiedOrder::source of a risk-reducing order a workflow node creates (close position,
// trailing stop, smart-order flatten): "workflow_exit:<id>". UnifiedTrading places it
// without approval only after verifying, at submission, that it reduces an open position
// of the account (opposite side, quantity <= the position); otherwise the approval rule
// treats it like "workflow:<id>" (Semi-Auto -> queued) — trading/OrderApprovalRule.h.
QString bridge_exit_order_source(const QJsonObject& params) {
    const QString id = params.value(QStringLiteral("_workflow_id")).toString().trimmed();
    return id.isEmpty() ? QStringLiteral("workflow_exit") : QStringLiteral("workflow_exit:") + id;
}
} // namespace

// ── Market Data Bridge ─────────────────────────────────────────────────

void wire_market_data_bridges(NodeRegistry& registry) {
    // market.get_quote and market.get_historical already have real Python-backed
    // executors from MarketDataNodes.cpp — do NOT overwrite them.

    // Crypto Price — uses ExchangeService (Kraken public API, no key needed)
    auto* crypto_price_def = const_cast<NodeTypeDef*>(registry.find("market.get_crypto_price"));
    if (crypto_price_def) {
        crypto_price_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                       std::function<void(bool, QJsonValue, QString)> cb) {
            QString base = params.value("symbol").toString("BTC").toUpper();
            QString quote = params.value("quote").toString("USD").toUpper();
            QString symbol = base + "/" + quote; // CCXT unified symbol, e.g. BTC/USD
            // The node's own exchange. This used to read the Crypto Trading screen's active
            // exchange instead (and label the result "kraken" whatever it was).
            const QString exchange = params.value("exchange").toString(QStringLiteral("kraken")).trimmed().toLower();

            (void)QtConcurrent::run([symbol, exchange, cb]() {
                trading::ExchangeSession* session = trading::ExchangeSessionManager::instance().session(exchange);
                if (!session) {
                    cb(false, {}, QString("Unknown exchange: %1").arg(exchange));
                    return;
                }
                trading::TickerData t = session->fetch_ticker(symbol);

                if (t.last <= 0.0) {
                    cb(false, {},
                       QString("No price for %1 on %2 — check that the pair is listed there (e.g. USDT, not USD, "
                               "on Binance)")
                           .arg(symbol, exchange));
                    return;
                }

                QJsonObject out;
                out["symbol"] = symbol;
                out["price"] = t.last;
                out["bid"] = t.bid;
                out["ask"] = t.ask;
                out["high"] = t.high;
                out["low"] = t.low;
                out["change"] = t.change;
                out["change_pct"] = t.percentage;
                out["volume"] = t.base_volume;
                out["exchange"] = exchange;
                out["timestamp"] = static_cast<qint64>(t.timestamp);
                cb(true, out, {});
                LOG_DEBUG("MarketBridge", QString("Crypto price fetched: %1 = %2").arg(symbol).arg(t.last));
            });
        };
    }

    // market.get_news already has a real executor from MarketDataNodes.cpp.
    // Remaining market nodes (depth, stats, fundamentals, economics) are wired
    // in MarketDataNodes.cpp directly — no stub overwrite needed here.

    LOG_DEBUG("ServiceBridges", "Market data bridges wired");
}

// ── Trading Bridge ─────────────────────────────────────────────────────

// Helper: find the first active account for a given broker_id, return its account_id
static QString find_account_for_broker(const QString& broker_id) {
    using namespace fincept::trading;
    auto accounts = AccountManager::instance().list_accounts(broker_id);
    for (const auto& acct : accounts)
        if (acct.is_active)
            return acct.account_id;
    return accounts.isEmpty() ? QString{} : accounts[0].account_id;
}

static QString ensure_paper_portfolio() {
    using namespace fincept::trading;
    auto existing = pt_find_portfolio("workflow_default", "");
    if (existing.has_value())
        return existing->id;
    // A new book's cash is in the user's display currency (Settings > Currency &
    // Number Format); orders in other currencies convert at a real rate.
    auto p = pt_create_portfolio("workflow_default", 1000000.0, fincept::ui::formatting::display_currency());
    return p.id;
}

// A paper order of the workflow paper portfolio, by id (the paper engine has no by-id accessor).
static std::optional<fincept::trading::PtOrder> wfb_find_paper_order(const QString& order_id) {
    using namespace fincept::trading;
    for (const auto& o : pt_get_orders(ensure_paper_portfolio())) {
        if (o.id == order_id)
            return o;
    }
    return std::nullopt;
}

// Resolve an account_id for a trading node: prefer the explicit account_id param,
// otherwise fall back to the first active account for the given broker. Returns
// empty when neither resolves (caller reports a friendly "no account" error).
static QString resolve_account_id(const QString& account_id_param, const QString& broker_id) {
    using namespace fincept::trading;
    QString aid = account_id_param.trimmed();
    if (!aid.isEmpty() && AccountManager::instance().has_account(aid))
        return aid;
    return find_account_for_broker(broker_id);
}

// Map a node's product param ("delivery"/"intraday"/"margin"/"mtf", or "CNC"/"MIS"/"NRML"/...)
// to ProductType with the trading layer's own mapping, product_from_broker_str() — the one the
// equity trading screen and the broker close paths use. Blank = the node default, Delivery.
// A position's product as the BROKER reports it must go through IBroker::product_from_wire()
// instead (Angel One's "MARGIN" is MTF).
static fincept::trading::ProductType parse_product(const QString& p) {
    using namespace fincept::trading;
    if (p.trimmed().isEmpty())
        return ProductType::Delivery;
    return product_from_broker_str(p);
}

// The open position a close / protective-stop node acts on: exact symbol match
// (normalised — never a substring, so RELIANCE cannot hit RELIANCEPP), narrowed by
// product when several products hold the same symbol. `error` explains a miss.
static bool wfb_pick_position(const QVector<fincept::trading::BrokerPosition>& positions, const QString& symbol,
                              const QString& product_hint, fincept::trading::BrokerPosition& out, QString& error) {
    QVector<fincept::trading::BrokerPosition> matches;
    for (const auto& p : positions) {
        if (bridge_logic::position_direction(p.quantity, p.side) != 0 && bridge_logic::symbols_match(p.symbol, symbol))
            matches.append(p);
    }
    if (matches.size() > 1 && !product_hint.isEmpty()) {
        QVector<fincept::trading::BrokerPosition> by_product;
        for (const auto& p : matches) {
            if (parse_product(p.product_type) == parse_product(product_hint))
                by_product.append(p);
        }
        if (!by_product.isEmpty())
            matches = by_product;
    }
    if (matches.isEmpty()) {
        error = QString("No open position in %1 — nothing to act on").arg(symbol);
        return false;
    }
    if (matches.size() > 1) {
        error = QString("%1 has %2 open positions in different products — set Product Type to choose one")
                    .arg(symbol)
                    .arg(matches.size());
        return false;
    }
    out = matches.first();
    return true;
}

void wire_trading_bridges(NodeRegistry& registry) {
    using namespace fincept::trading;

    // ── Place Order ───────────────────────────────────────────────
    auto* place_def = const_cast<NodeTypeDef*>(registry.find("trading.place_order"));
    if (place_def) {
        place_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString exchange = params.value("exchange").toString("NSE");
            QString side = params.value("side").toString("buy");
            QString product_str = params.value("product_type").toString("delivery");
            QString order_type_str = params.value("order_type").toString("market");
            double qty = params.value("quantity").toDouble(1);
            double price = params.value("price").toDouble(0);
            const double stop_price = params.value("stop_price").toDouble(0);
            QString validity = params.value("validity").toString("DAY");
            bool paper = (mode == "paper");

            // Refuse an order that cannot be what the node says, before anything is sent. A stop
            // order used to go out with trigger 0 (live) or always fail (paper), and an unknown
            // order type was sent live as a MARKET order.
            const bool is_stop = order_type_str == "stop" || order_type_str == "stop_limit";
            const bool needs_limit = order_type_str == "limit" || order_type_str == "stop_limit";
            if (!is_stop && !needs_limit && order_type_str != "market") {
                cb(false, {}, QString("Unknown order type '%1' — use market, limit, stop or stop_limit").arg(order_type_str));
                return;
            }
            if (is_stop && !(stop_price > 0)) {
                cb(false, {}, QString("Trigger Price is required for %1 orders").arg(order_type_str));
                return;
            }
            if (needs_limit && !(price > 0)) {
                cb(false, {}, QString("Limit Price is required for %1 orders").arg(order_type_str));
                return;
            }

            AuditLogger::instance().log(
                AuditAction::OrderPlaced, {}, {}, symbol,
                QString("[%1] %2 %3 %4 x%5 via %6").arg(mode.toUpper(), product_str, side, symbol).arg(qty).arg(broker),
                params, paper);

            if (paper) {
                try {
                    // A paper MARKET order fills at a real reference price: the node's
                    // price, else the price carried by its input (quote / risk-check
                    // payload). It used to fill at a made-up 100.00 when neither existed.
                    const double ref_price = price > 0 ? price : bridge_logic::reference_price(inputs);
                    if (order_type_str == "market" && ref_price <= 0) {
                        cb(false, {},
                           QString("Paper market order for %1 has no reference price to fill at — connect a quote "
                                   "node upstream or set Limit Price")
                               .arg(symbol));
                        return;
                    }
                    QString portfolio_id = ensure_paper_portfolio();
                    // A plain stop is a stop-MARKET order: no limit price, only the trigger.
                    std::optional<double> opt_price =
                        (price > 0 && order_type_str != "stop") ? std::optional(price) : std::nullopt;
                    std::optional<double> opt_stop = is_stop ? std::optional(stop_price) : std::nullopt;
                    // The product goes with the order (it used to be dropped, so every workflow
                    // paper position was intraday/MIS — auto-squared at the close — whatever
                    // Product Type said, and never a holding).
                    const QString paper_product = QString::fromLatin1(product_to_broker_str(parse_product(product_str)));
                    // Cash is booked in the workflow book's currency at the per-symbol
                    // factor: a symbol priced in another currency needs a real rate
                    // first — never 1:1.
                    QString fx_reason;
                    if (!trading::PaperMarkService::prepare_symbol_fx(portfolio_id, symbol, exchange, &fx_reason)) {
                        cb(false, {}, fx_reason);
                        return;
                    }
                    auto pt_order = pt_place_order(portfolio_id, symbol, side, order_type_str, qty, opt_price, opt_stop,
                                                   /*reduce_only=*/false, exchange, paper_product,
                                                   bridge_order_source(params));

                    if (order_type_str == "market")
                        pt_fill_order(pt_order.id, ref_price);

                    QJsonObject out;
                    out["order_id"] = pt_order.id;
                    out["symbol"] = symbol;
                    out["side"] = side;
                    out["quantity"] = qty;
                    out["status"] = order_type_str == "market" ? QString("filled") : pt_order.status;
                    if (order_type_str == "market")
                        out["fill_price"] = ref_price;
                    out["mode"] = "paper";
                    cb(true, out, {});
                } catch (const std::exception& ex) {
                    LOG_ERROR("ServiceBridges", logf("Workflow paper order failed", {{"symbol", symbol},
                                                                                     {"side", side},
                                                                                     {"qty", qty},
                                                                                     {"error", ex.what()}}));
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            const QString order_source = bridge_order_source(params);
            (void)QtConcurrent::run(&order_thread_pool(), [cb, symbol, exchange, side, product_str, order_type_str, qty,
                                                           price, stop_price, validity, broker, order_source]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                UnifiedOrder order;
                order.symbol = symbol;
                order.exchange = exchange;
                order.side = (side == "sell") ? OrderSide::Sell : OrderSide::Buy;
                if (order_type_str == "limit")
                    order.order_type = OrderType::Limit;
                else if (order_type_str == "stop")
                    order.order_type = OrderType::StopLoss;
                else if (order_type_str == "stop_limit")
                    order.order_type = OrderType::StopLossLimit;
                else
                    order.order_type = OrderType::Market;
                order.quantity = qty;
                // SL-M (stop) carries only the trigger; SL (stop_limit) carries both.
                order.price = order_type_str == "stop" ? 0.0 : price;
                if (order_type_str == "stop" || order_type_str == "stop_limit")
                    order.stop_price = stop_price;
                order.validity = validity;
                order.product_type = parse_product(product_str);
                order.source = order_source;

                if (gate_or_queue(account_id, order, cb))
                    return;

                auto response = UnifiedTrading::instance().place_order(account_id, order);

                if (!response.success) {
                    QString friendly = response.message;
                    if (friendly.contains("Margin Shortfall", Qt::CaseInsensitive))
                        friendly += "\n→ Insufficient margin. Add funds or reduce order size.";
                    else if (friendly.contains("No Holdings uploaded", Qt::CaseInsensitive))
                        friendly += "\n→ Authorize TPIN via your broker app, then retry.";
                    cb(false, {}, friendly);
                    return;
                }

                // Poll order status to catch async rejections (TPIN, risk checks).
                // The order is already out, so the 2 s wait + poll hops to the
                // blocking-jobs pool and this order-pool thread is free at once.
                (void)QtConcurrent::run(&blocking_thread_pool(), [cb, response, account_id, symbol, exchange, side,
                                                                  product_str, qty, broker]() {
                    QThread::msleep(2000);

                    auto creds = AccountManager::instance().load_credentials(account_id);
                    auto* ibk = AccountManager::instance().broker_for(account_id);
                    QString final_status = "submitted";
                    QString reject_reason;

                    if (ibk) {
                        auto orders_result = ibk->get_orders(creds);
                        if (orders_result.success) {
                            for (const auto& o : orders_result.data.value()) {
                                if (o.order_id == response.order_id) {
                                    final_status = o.status.toLower();
                                    if (final_status == "rejected" || final_status == "cancelled") {
                                        reject_reason = o.message;
                                    }
                                    break;
                                }
                            }
                        }
                    }

                    QJsonObject out;
                    out["order_id"] = response.order_id;
                    out["symbol"] = symbol;
                    out["exchange"] = exchange;
                    out["side"] = side;
                    out["product_type"] = product_str;
                    out["quantity"] = qty;
                    out["broker"] = broker;
                    out["status"] = final_status;
                    out["mode"] = "live";

                    if (final_status == "rejected" || final_status == "cancelled") {
                        QString friendly = reject_reason;
                        if (reject_reason.contains("Margin Shortfall", Qt::CaseInsensitive))
                            friendly += "\n→ Insufficient margin. Add funds or reduce order size.";
                        else if (reject_reason.contains("No Holdings uploaded", Qt::CaseInsensitive) ||
                                 reject_reason.contains("TPIN", Qt::CaseInsensitive))
                            friendly += "\n→ Authorize TPIN in your broker app to sell from holdings.";
                        else if (reject_reason.contains("market", Qt::CaseInsensitive) &&
                                 reject_reason.contains("close", Qt::CaseInsensitive))
                            friendly += "\n→ Market is closed. Retry during trading hours or enable AMO.";
                        else if (reject_reason.contains("RMS", Qt::CaseInsensitive) ||
                                 reject_reason.contains("RISK", Qt::CaseInsensitive))
                            friendly += "\n→ Broker risk check failed. Verify your account limits.";
                        out["error"] = friendly;
                        cb(false, out, friendly);
                    } else {
                        cb(true, out, {});
                    }
                });
            });
        };
    }

    // ── Cancel Order ──────────────────────────────────────────────
    auto* cancel_def = const_cast<NodeTypeDef*>(registry.find("trading.cancel_order"));
    if (cancel_def) {
        cancel_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QString order_id = params.value("order_id").toString();
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");

            if (order_id.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                order_id = inputs[0].toObject().value("order_id").toString();
            if (order_id.isEmpty()) {
                cb(false, {}, "Order ID is required");
                return;
            }

            if (mode == "paper") {
                // Look the order up first: pt_cancel_order() is silent for an unknown id and for
                // an order that is no longer resting, and the node reported "cancelled" for both.
                try {
                    const std::optional<PtOrder> order = wfb_find_paper_order(order_id);
                    if (!order) {
                        cb(false, {}, QString("No paper order %1 in the workflow paper portfolio").arg(order_id));
                        return;
                    }
                    if (order->status != "pending" && order->status != "partial") {
                        cb(false, {}, QString("Paper order %1 is %2 — nothing to cancel").arg(order_id, order->status));
                        return;
                    }
                    pt_cancel_order(order_id);
                    QJsonObject out;
                    out["order_id"] = order_id;
                    out["symbol"] = order->symbol;
                    out["side"] = order->side;
                    out["cancelled_quantity"] = order->quantity - order->filled_qty;
                    out["filled_qty"] = order->filled_qty;
                    out["status"] = "cancelled";
                    out["mode"] = "paper";
                    cb(true, out, {});
                } catch (const std::exception& ex) {
                    LOG_ERROR("ServiceBridges",
                              logf("Workflow paper cancel failed", {{"order_id", order_id}, {"error", ex.what()}}));
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            (void)QtConcurrent::run(&order_thread_pool(), [cb, order_id, broker]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto response = UnifiedTrading::instance().cancel_order(account_id, order_id);
                QJsonObject out;
                out["order_id"] = order_id;
                out["broker"] = broker;
                out["status"] = response.success ? "cancelled" : "failed";
                if (!response.success)
                    out["error"] = response.message;
                cb(response.success, out, response.success ? QString{} : response.message);
            });
        };
    }

    // ── Modify Order ──────────────────────────────────────────────
    auto* modify_def = const_cast<NodeTypeDef*>(registry.find("trading.modify_order"));
    if (modify_def) {
        modify_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QString order_id = params.value("order_id").toString();
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");
            double new_qty = params.value("quantity").toDouble(0);
            double new_price = params.value("price").toDouble(0);

            if (order_id.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                order_id = inputs[0].toObject().value("order_id").toString();
            if (order_id.isEmpty()) {
                cb(false, {}, "Order ID is required");
                return;
            }

            if (mode == "paper") {
                // The paper engine has no modify: cancel the resting order and place its
                // replacement with the new quantity / price (same symbol, side and type). This
                // used to answer "modified" and change nothing.
                if (!(new_qty > 0) && !(new_price > 0)) {
                    cb(false, {}, "Set New Quantity or New Price to modify the order");
                    return;
                }
                try {
                    const std::optional<PtOrder> order = wfb_find_paper_order(order_id);
                    if (!order) {
                        cb(false, {}, QString("No paper order %1 in the workflow paper portfolio").arg(order_id));
                        return;
                    }
                    if (order->status != "pending") {
                        cb(false, {},
                           QString("Paper order %1 is %2 — only a pending order can be modified").arg(order_id, order->status));
                        return;
                    }
                    const double qty = new_qty > 0 ? new_qty : order->quantity;
                    std::optional<double> price = order->price;
                    if (new_price > 0) {
                        if (order->order_type == "stop") {
                            cb(false, {}, "A stop (market) order has no limit price to modify");
                            return;
                        }
                        price = new_price;
                    }
                    // The replacement blocks margin at the per-symbol factor: check the
                    // rate BEFORE the original is cancelled, so a missing rate leaves it intact.
                    if (QString fx_reason; !trading::PaperMarkService::prepare_symbol_fx(order->portfolio_id, order->symbol,
                                                                                order->exchange, &fx_reason)) {
                        cb(false, {}, fx_reason);
                        return;
                    }
                    pt_cancel_order(order->id);
                    PtOrder replacement;
                    try {
                        replacement = pt_place_order(order->portfolio_id, order->symbol, order->side, order->order_type,
                                                     qty, price, order->stop_price, order->reduce_only, order->exchange,
                                                     order->product);
                    } catch (const std::exception& ex) {
                        // Put the original back so a refused modify does not silently drop the order.
                        QString restored;
                        try {
                            restored = pt_place_order(order->portfolio_id, order->symbol, order->side,
                                                      order->order_type, order->quantity, order->price,
                                                      order->stop_price, order->reduce_only, order->exchange,
                                                      order->product)
                                           .id;
                        } catch (const std::exception& restore_ex) {
                            LOG_ERROR("ServiceBridges",
                                      logf("Paper modify refused and the original order could not be restored",
                                           {{"order_id", order_id},
                                            {"symbol", order->symbol},
                                            {"reason", ex.what()},
                                            {"error", restore_ex.what()}}));
                        }
                        if (!restored.isEmpty())
                            LOG_WARN("ServiceBridges", logf("Paper modify refused, original order re-placed",
                                                            {{"order_id", order_id},
                                                             {"new_order_id", restored},
                                                             {"symbol", order->symbol},
                                                             {"reason", ex.what()}}));
                        cb(false, {},
                           restored.isEmpty()
                               ? QString("Paper modify refused (%1); the original order %2 was cancelled")
                                     .arg(ex.what(), order_id)
                               : QString("Paper modify refused (%1); the original order was re-placed as %2")
                                     .arg(ex.what(), restored));
                        return;
                    }
                    QJsonObject out;
                    out["order_id"] = replacement.id;
                    out["replaced_order_id"] = order_id;
                    out["symbol"] = replacement.symbol;
                    out["side"] = replacement.side;
                    out["quantity"] = replacement.quantity;
                    if (replacement.price)
                        out["price"] = *replacement.price;
                    out["status"] = "modified";
                    out["note"] = "Paper modify = cancel + replace: the order has a new id";
                    out["mode"] = "paper";
                    cb(true, out, {});
                } catch (const std::exception& ex) {
                    LOG_ERROR("ServiceBridges",
                              logf("Workflow paper modify failed", {{"order_id", order_id}, {"error", ex.what()}}));
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            (void)QtConcurrent::run(&order_thread_pool(), [cb, order_id, broker, new_qty, new_price]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                QJsonObject mods;
                if (new_qty > 0)
                    mods["quantity"] = new_qty;
                if (new_price > 0)
                    mods["price"] = new_price;

                auto response = UnifiedTrading::instance().modify_order(account_id, order_id, mods);
                QJsonObject out;
                out["order_id"] = order_id;
                out["broker"] = broker;
                out["status"] = response.success ? "modified" : "failed";
                if (!response.success)
                    out["error"] = response.message;
                cb(response.success, out, response.success ? QString{} : response.message);
            });
        };
    }

    // ── Get Orders ────────────────────────────────────────────────
    auto* orders_def = const_cast<NodeTypeDef*>(registry.find("trading.get_orders"));
    if (orders_def) {
        orders_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");
            QString status_filter = params.value("status").toString("all");

            if (mode == "paper") {
                QString portfolio_id = ensure_paper_portfolio();
                // Filter in C++ with the paper engine's own statuses: "open" was passed straight
                // to SQL ("AND status = 'open'") and matched nothing — resting paper orders are
                // "pending" / "partial".
                auto orders = pt_get_orders(portfolio_id);
                QJsonArray arr;
                for (const auto& o : orders) {
                    if (status_filter == "open" && o.status != "pending" && o.status != "partial")
                        continue;
                    if (status_filter == "filled" && o.status != "filled")
                        continue;
                    if (status_filter == "cancelled" && o.status != "cancelled" && o.status != "rejected")
                        continue;
                    if (status_filter != "all" && status_filter != "open" && status_filter != "filled" &&
                        status_filter != "cancelled" && o.status != status_filter)
                        continue;
                    QJsonObject obj;
                    obj["order_id"] = o.id;
                    obj["symbol"] = o.symbol;
                    obj["side"] = o.side;
                    obj["order_type"] = o.order_type;
                    obj["quantity"] = o.quantity;
                    obj["filled_qty"] = o.filled_qty;
                    obj["status"] = o.status;
                    obj["mode"] = "paper";
                    if (o.price)
                        obj["price"] = *o.price;
                    if (o.avg_price)
                        obj["avg_price"] = *o.avg_price;
                    arr.append(obj);
                }
                cb(true, arr, {});
                return;
            }

            (void)QtConcurrent::run([cb, broker, status_filter]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto creds = AccountManager::instance().load_credentials(account_id);
                auto* ibk = AccountManager::instance().broker_for(account_id);
                if (!ibk) {
                    cb(false, {}, "Broker not found in registry");
                    return;
                }

                auto result = ibk->get_orders(creds);
                if (!result.success) {
                    cb(false, {}, result.error);
                    return;
                }

                QJsonArray arr;
                for (const auto& o : result.data.value()) {
                    if (status_filter != "all") {
                        QString s = o.status.toLower();
                        if (status_filter == "open" && s != "pending" && s != "open" && s != "trigger_pending")
                            continue;
                        if (status_filter == "filled" && s != "complete" && s != "filled" && s != "traded")
                            continue;
                        if (status_filter == "cancelled" && s != "cancelled" && s != "rejected")
                            continue;
                    }
                    QJsonObject obj;
                    obj["order_id"] = o.order_id;
                    obj["symbol"] = o.symbol;
                    obj["exchange"] = o.exchange;
                    obj["side"] = o.side;
                    obj["order_type"] = o.order_type;
                    obj["quantity"] = o.quantity;
                    obj["price"] = o.price;
                    obj["filled_qty"] = o.filled_qty;
                    obj["avg_price"] = o.avg_price;
                    obj["status"] = o.status;
                    obj["timestamp"] = o.timestamp;
                    arr.append(obj);
                }
                cb(true, arr, {});
            });
        };
    }

    // ── Get Positions ─────────────────────────────────────────────
    auto* pos_def = const_cast<NodeTypeDef*>(registry.find("trading.get_positions"));
    if (pos_def) {
        pos_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                              std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");

            if (mode == "paper") {
                QString portfolio_id = ensure_paper_portfolio();
                auto positions = pt_get_positions(portfolio_id);
                QJsonArray arr;
                for (const auto& p : positions) {
                    QJsonObject obj;
                    obj["symbol"] = p.symbol;
                    obj["side"] = p.side;
                    obj["quantity"] = p.quantity;
                    obj["entry_price"] = p.entry_price;
                    obj["current_price"] = p.current_price;
                    obj["unrealized_pnl"] = p.unrealized_pnl;
                    obj["realized_pnl"] = p.realized_pnl;
                    obj["mode"] = "paper";
                    arr.append(obj);
                }
                cb(true, arr, {});
                return;
            }

            (void)QtConcurrent::run([cb, broker]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto creds = AccountManager::instance().load_credentials(account_id);
                auto* ibk = AccountManager::instance().broker_for(account_id);
                if (!ibk) {
                    cb(false, {}, "Broker not found in registry");
                    return;
                }

                auto result = ibk->get_positions(creds);
                if (!result.success) {
                    cb(false, {}, result.error);
                    return;
                }

                QJsonArray arr;
                for (const auto& p : result.data.value()) {
                    QJsonObject obj;
                    obj["symbol"] = p.symbol;
                    obj["exchange"] = p.exchange;
                    obj["side"] = p.side;
                    obj["quantity"] = p.quantity;
                    obj["avg_price"] = p.avg_price;
                    obj["ltp"] = p.ltp;
                    obj["pnl"] = p.pnl;
                    obj["pnl_pct"] = p.pnl_pct;
                    obj["product_type"] = p.product_type;
                    obj["mode"] = "live";
                    arr.append(obj);
                }
                cb(true, arr, {});
            });
        };
    }

    // ── Get Holdings ──────────────────────────────────────────────
    auto* hold_def = const_cast<NodeTypeDef*>(registry.find("trading.get_holdings"));
    if (hold_def) {
        hold_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");

            if (mode == "paper") {
                // Delivery (CNC) paper positions are the holdings, as on the Equity Trading
                // screen. This used to return an empty list whatever the paper book held.
                try {
                    QJsonArray arr;
                    for (const auto& p : pt_get_positions(ensure_paper_portfolio())) {
                        if (!product_is_delivery(p.product))
                            continue;
                        const double ltp = p.current_price > 0.0 ? p.current_price : p.entry_price;
                        const double invested = p.entry_price * p.quantity;
                        const double current = ltp * p.quantity;
                        const double pnl = p.side.compare("short", Qt::CaseInsensitive) == 0 ? invested - current
                                                                                              : current - invested;
                        QJsonObject obj;
                        obj["symbol"] = p.symbol;
                        obj["quantity"] = p.quantity;
                        obj["avg_price"] = p.entry_price;
                        obj["ltp"] = ltp;
                        obj["pnl"] = pnl;
                        obj["pnl_pct"] = invested > 0.0 ? pnl / invested * 100.0 : 0.0;
                        obj["invested_value"] = invested;
                        obj["current_value"] = current;
                        obj["mode"] = "paper";
                        arr.append(obj);
                    }
                    cb(true, arr, {});
                } catch (const std::exception& ex) {
                    LOG_ERROR("ServiceBridges", logf("Workflow paper holdings read failed", {{"error", ex.what()}}));
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            (void)QtConcurrent::run([cb, broker]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto creds = AccountManager::instance().load_credentials(account_id);
                auto* ibk = AccountManager::instance().broker_for(account_id);
                if (!ibk) {
                    cb(false, {}, "Broker not found in registry");
                    return;
                }

                auto result = ibk->get_holdings(creds);
                if (!result.success) {
                    cb(false, {}, result.error);
                    return;
                }

                QJsonArray arr;
                for (const auto& h : result.data.value()) {
                    QJsonObject obj;
                    obj["symbol"] = h.symbol;
                    obj["exchange"] = h.exchange;
                    obj["quantity"] = h.quantity;
                    obj["avg_price"] = h.avg_price;
                    obj["ltp"] = h.ltp;
                    obj["pnl"] = h.pnl;
                    obj["pnl_pct"] = h.pnl_pct;
                    obj["invested_value"] = h.invested_value;
                    obj["current_value"] = h.current_value;
                    arr.append(obj);
                }
                cb(true, arr, {});
            });
        };
    }

    // ── Get Balance ───────────────────────────────────────────────
    auto* balance_def = const_cast<NodeTypeDef*>(registry.find("trading.get_balance"));
    if (balance_def) {
        balance_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                  std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");

            if (mode == "paper") {
                // The workflow paper portfolio's real funds (computed as the Equity Trading
                // screen's paper funds card does) — not a constant 1,000,000.
                try {
                    const QString portfolio_id = ensure_paper_portfolio();
                    const PtPortfolio portfolio = pt_get_portfolio(portfolio_id);
                    double used_margin = 0.0, intraday_unrealized = 0.0;
                    double holdings_value = 0.0, holdings_unrealized = 0.0;
                    for (const auto& p : pt_get_positions(portfolio_id)) {
                        if (product_is_delivery(p.product)) {
                            const double ltp = p.current_price > 0.0 ? p.current_price : p.entry_price;
                            const double invested = p.entry_price * p.quantity;
                            const double current = ltp * p.quantity;
                            holdings_value += current;
                            holdings_unrealized += p.side.compare("short", Qt::CaseInsensitive) == 0
                                                       ? invested - current
                                                       : current - invested;
                        } else {
                            used_margin += p.held_margin;
                            intraday_unrealized += p.unrealized_pnl;
                        }
                    }
                    QJsonObject out;
                    out["available_balance"] = portfolio.balance;
                    out["used_margin"] = used_margin;
                    out["holdings_value"] = holdings_value;
                    out["unrealized_pnl"] = intraday_unrealized + holdings_unrealized;
                    out["total_balance"] = portfolio.balance + used_margin + holdings_value + intraday_unrealized;
                    out["opening_balance"] = portfolio.initial_balance;
                    out["currency"] = portfolio.currency;
                    out["broker"] = "paper";
                    out["mode"] = "paper";
                    cb(true, out, {});
                } catch (const std::exception& ex) {
                    LOG_ERROR("ServiceBridges", logf("Workflow paper balance read failed", {{"error", ex.what()}}));
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            (void)QtConcurrent::run([cb, broker]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto creds = AccountManager::instance().load_credentials(account_id);
                auto* ibk = AccountManager::instance().broker_for(account_id);
                if (!ibk) {
                    cb(false, {}, "Broker not found in registry");
                    return;
                }

                auto result = ibk->get_funds(creds);
                if (!result.success) {
                    cb(false, {}, result.error);
                    return;
                }

                auto& f = result.data.value();
                QJsonObject out;
                out["available_balance"] = f.available_balance;
                out["used_margin"] = f.used_margin;
                out["total_balance"] = f.total_balance;
                out["collateral"] = f.collateral;
                out["broker"] = broker;
                cb(true, out, {});
            });
        };
    }

    // ── Close Position ────────────────────────────────────────────
    auto* close_def = const_cast<NodeTypeDef*>(registry.find("trading.close_position"));
    if (close_def) {
        close_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString exchange = params.value("exchange").toString("NSE");
            QString product_str = params.value("product_type").toString();
            double qty = params.value("quantity").toDouble(0);

            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }

            if (mode == "paper") {
                // Close the paper position for real (opposite side, capped at its size)
                // instead of reporting "closed" without touching the paper book.
                try {
                    const QString portfolio_id = ensure_paper_portfolio();
                    const PtPosition* found = nullptr;
                    const auto positions = pt_get_positions(portfolio_id);
                    for (const auto& p : positions) {
                        if (p.quantity > 0 && bridge_logic::symbols_match(p.symbol, symbol)) {
                            found = &p;
                            break;
                        }
                    }
                    if (!found) {
                        cb(false, {}, QString("No open paper position in %1 — nothing to close").arg(symbol));
                        return;
                    }
                    const double close_qty = bridge_logic::close_quantity(qty, found->quantity);
                    const QString close_side = found->side == QLatin1String("short") ? "buy" : "sell";
                    const double px = found->current_price > 0 ? found->current_price : found->entry_price;
                    // Realized P&L is booked at the per-symbol factor (unset after a
                    // restart): the exit rule — never 1:1 (trading::PaperMarkService::prepare_close_fx).
                    QString fx_reason;
                    if (!trading::PaperMarkService::prepare_close_fx(portfolio_id, found->symbol, QString(),
                                                            trading::PaperMarkService::kCloseFxWaitMs, &fx_reason)) {
                        cb(false, {}, fx_reason);
                        return;
                    }
                    auto order = pt_place_order(portfolio_id, found->symbol, close_side, "market", close_qty, std::nullopt,
                                                std::nullopt, /*reduce_only=*/true, QString(), QString(),
                                                bridge_exit_order_source(params)); // paper: audit provenance only
                    pt_fill_order(order.id, px);
                    QJsonObject out;
                    out["symbol"] = found->symbol;
                    out["side"] = close_side;
                    out["quantity_closed"] = close_qty;
                    out["position_quantity"] = found->quantity;
                    out["fill_price"] = px;
                    out["order_id"] = order.id;
                    out["status"] = close_qty < found->quantity ? "partially_closed" : "closed";
                    out["mode"] = "paper";
                    cb(true, out, {});
                } catch (const std::exception& ex) {
                    LOG_ERROR("ServiceBridges", logf("Workflow paper close failed",
                                                     {{"symbol", symbol}, {"qty", qty}, {"error", ex.what()}}));
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            const QString exit_source = bridge_exit_order_source(params);
            (void)QtConcurrent::run(&order_thread_pool(), [cb, symbol, exchange, product_str, qty, broker,
                                                           exit_source]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto creds = AccountManager::instance().load_credentials(account_id);
                auto* ibk = AccountManager::instance().broker_for(account_id);
                if (!ibk) {
                    cb(false, {}, "Broker not found in registry");
                    return;
                }

                // Always read the position first: the close goes on the OPPOSITE side
                // of what is actually held (a short is bought back), in the position's
                // own product, and never for more than is held — the old code always
                // SOLD, so closing a short added to it.
                auto pos_result = ibk->get_positions(creds);
                if (!pos_result.success || !pos_result.data.has_value()) {
                    cb(false, {}, QString("Could not read positions to close %1: %2").arg(symbol, pos_result.error));
                    return;
                }
                BrokerPosition pos;
                QString pick_error;
                if (!wfb_pick_position(pos_result.data.value(), symbol, product_str, pos, pick_error)) {
                    cb(false, {}, pick_error);
                    return;
                }
                const int direction = bridge_logic::position_direction(pos.quantity, pos.side);
                const double held = std::abs(pos.quantity);
                const double close_qty = bridge_logic::close_quantity(qty, held);

                UnifiedOrder order;
                order.symbol = symbol;
                order.exchange = pos.exchange.isEmpty() ? exchange : pos.exchange;
                order.side = direction > 0 ? OrderSide::Sell : OrderSide::Buy;
                order.order_type = OrderType::Market;
                order.quantity = close_qty;
                order.validity = "DAY";
                // The position's product as THIS broker means it (as the equity screen's close does).
                order.product_type = pos.product_type.isEmpty() ? parse_product(product_str)
                                                                : ibk->product_from_wire(pos.product_type);
                // Risk-reducing: "workflow_exit:<id>". UnifiedTrading places it without
                // approval only after re-reading the account's positions and verifying it
                // reduces one; otherwise it is treated like an entry (Semi-Auto: queued).
                order.source = exit_source;

                auto response = UnifiedTrading::instance().place_order(account_id, order);
                QJsonObject out;
                out["symbol"] = symbol;
                out["side"] = direction > 0 ? "sell" : "buy";
                out["position_quantity"] = held;
                out["quantity_closed"] = close_qty;
                out["order_id"] = response.order_id;
                out["broker"] = broker;
                out["mode"] = "live";
                out["status"] = response.success ? (close_qty < held ? "partially_closed" : "closed") : "failed";
                if (!response.success)
                    out["error"] = response.message;
                cb(response.success, out, response.success ? QString{} : response.message);
            });
        };
    }

    // ── Bracket Order ─────────────────────────────────────────────
    auto* bracket_def = const_cast<NodeTypeDef*>(registry.find("trading.bracket_order"));
    if (bracket_def) {
        bracket_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                  std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString exchange = params.value("exchange").toString("NSE");
            QString side = params.value("side").toString("buy");
            double qty = params.value("quantity").toDouble(1);
            double entry = params.value("entry_price").toDouble(0);
            double sl = params.value("stop_loss").toDouble(0);
            double tp = params.value("take_profit").toDouble(0);

            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }
            if (sl <= 0) {
                cb(false, {}, "Stop loss is required for bracket orders");
                return;
            }
            if (tp <= 0) {
                cb(false, {}, "Take profit is required for bracket orders");
                return;
            }

            if (mode == "paper") {
                QJsonObject out;
                // Not booked in the paper portfolio — say so instead of looking like a real fill.
                out["simulated_only"] = true;
                out["mode"] = "paper";
                out["order_id"] = "PAPER-BKT-" + QString::number(QDateTime::currentMSecsSinceEpoch());
                out["symbol"] = symbol;
                out["side"] = side;
                out["quantity"] = qty;
                out["entry_price"] = entry;
                out["stop_loss"] = sl;
                out["take_profit"] = tp;
                out["status"] = "submitted";
                out["broker"] = "paper";
                cb(true, out, {});
                return;
            }

            const QString order_source = bridge_order_source(params);
            (void)QtConcurrent::run(&order_thread_pool(), [cb, symbol, exchange, side, qty, entry, sl, tp, broker,
                                                           order_source]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                UnifiedOrder order;
                order.symbol = symbol;
                order.exchange = exchange; // was never set — the order went out with no exchange
                order.side = (side == "sell") ? OrderSide::Sell : OrderSide::Buy;
                order.order_type = entry > 0 ? OrderType::Limit : OrderType::Market;
                order.quantity = qty;
                order.price = entry;
                order.stop_loss = sl;
                order.take_profit = tp;
                order.product_type = ProductType::BracketOrder;
                order.validity = "DAY";
                order.source = order_source; // an entry order: under the approval rule

                if (gate_or_queue(account_id, order, cb))
                    return;

                auto response = UnifiedTrading::instance().place_order(account_id, order);
                QJsonObject out;
                out["order_id"] = response.order_id;
                out["symbol"] = symbol;
                out["side"] = side;
                out["quantity"] = qty;
                out["entry_price"] = entry;
                out["stop_loss"] = sl;
                out["take_profit"] = tp;
                out["broker"] = broker;
                out["status"] = response.success ? "submitted" : "failed";
                if (!response.success)
                    out["error"] = response.message;
                cb(response.success, out, response.success ? QString{} : response.message);
            });
        };
    }

    // ── Trailing Stop ─────────────────────────────────────────────
    auto* trail_def = const_cast<NodeTypeDef*>(registry.find("trading.trailing_stop"));
    if (trail_def) {
        trail_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString exchange = params.value("exchange").toString("NSE");
            QString product_str = params.value("product_type").toString();
            QString trail_type = params.value("trail_type").toString("percent");
            double trail_value = params.value("trail_value").toDouble(5.0);
            double qty = params.value("quantity").toDouble(0);

            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }
            if (!(trail_value > 0)) {
                cb(false, {}, "Trail value must be greater than 0");
                return;
            }

            if (mode == "paper") {
                // A real paper stop order on the paper position, priced from its last
                // price — not a fake "active" id that protected nothing.
                try {
                    const QString portfolio_id = ensure_paper_portfolio();
                    const auto positions = pt_get_positions(portfolio_id);
                    const PtPosition* found = nullptr;
                    for (const auto& p : positions) {
                        if (p.quantity > 0 && bridge_logic::symbols_match(p.symbol, symbol)) {
                            found = &p;
                            break;
                        }
                    }
                    if (!found) {
                        cb(false, {}, QString("No open paper position in %1 to protect").arg(symbol));
                        return;
                    }
                    const int direction = found->side == QLatin1String("short") ? -1 : 1;
                    const double last = found->current_price > 0 ? found->current_price : found->entry_price;
                    double stop = bridge_logic::trailing_stop_price(last, direction, trail_type, trail_value);
                    if (stop <= 0) {
                        cb(false, {}, QString("Cannot place a stop for %1: no usable price / trail").arg(symbol));
                        return;
                    }
                    stop = bridge_logic::round_to_tick(stop, exchange, direction);
                    const double stop_qty = bridge_logic::close_quantity(qty, found->quantity);
                    const QString stop_side = direction > 0 ? "sell" : "buy";
                    auto order = pt_place_order(portfolio_id, found->symbol, stop_side, "stop", stop_qty, std::nullopt,
                                                stop, /*reduce_only=*/true, QString(), QString(),
                                                bridge_exit_order_source(params)); // paper: audit provenance only
                    QJsonObject out;
                    out["order_id"] = order.id;
                    out["symbol"] = found->symbol;
                    out["side"] = stop_side;
                    out["quantity"] = stop_qty;
                    out["reference_price"] = last;
                    out["stop_price"] = stop;
                    out["trail_type"] = trail_type;
                    out["trail_value"] = trail_value;
                    out["status"] = order.status;
                    out["note"] = "Static stop placed from the last price; it does not move with the market";
                    out["mode"] = "paper";
                    cb(true, out, {});
                } catch (const std::exception& ex) {
                    LOG_ERROR("ServiceBridges",
                              logf("Workflow paper stop order failed", {{"symbol", symbol}, {"error", ex.what()}}));
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            QThreadPool* const pool = &order_thread_pool();
            const QString exit_source = bridge_exit_order_source(params);
            (void)QtConcurrent::run(pool, [cb, symbol, exchange, product_str, trail_type, trail_value, qty, broker,
                                           exit_source]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }
                auto creds = AccountManager::instance().load_credentials(account_id);
                auto* ibk = AccountManager::instance().broker_for(account_id);
                if (!ibk) {
                    cb(false, {}, "Broker not found in registry");
                    return;
                }

                // The trail used to be sent as an ABSOLUTE stop price (a 5% trail became a
                // stop at 5.00), with quantity 0 and always on the SELL side. Now: read the
                // position (side + size), read the last price, and place the stop the trail
                // away from it on the protective side.
                auto pos_result = ibk->get_positions(creds);
                if (!pos_result.success || !pos_result.data.has_value()) {
                    cb(false, {}, QString("Could not read positions for %1: %2").arg(symbol, pos_result.error));
                    return;
                }
                BrokerPosition pos;
                QString pick_error;
                if (!wfb_pick_position(pos_result.data.value(), symbol, product_str, pos, pick_error)) {
                    cb(false, {}, pick_error);
                    return;
                }
                const int direction = bridge_logic::position_direction(pos.quantity, pos.side);
                const double held = std::abs(pos.quantity);
                const QString venue = pos.exchange.isEmpty() ? exchange : pos.exchange;

                double last = 0;
                QVector<QPair<QString, QString>> syms{{symbol, venue}};
                auto quote = UnifiedTrading::instance().get_multi_quotes(account_id, syms);
                if (quote.success && quote.data.has_value() && !quote.data->isEmpty())
                    last = quote.data->first().ltp;
                if (!(last > 0))
                    last = pos.ltp;
                double stop = bridge_logic::trailing_stop_price(last, direction, trail_type, trail_value);
                if (stop <= 0) {
                    cb(false, {}, QString("Cannot place a stop for %1: no live price (or the trail is too wide)")
                                      .arg(symbol));
                    return;
                }
                stop = bridge_logic::round_to_tick(stop, venue, direction);

                UnifiedOrder order;
                order.symbol = symbol;
                order.exchange = venue;
                order.side = direction > 0 ? OrderSide::Sell : OrderSide::Buy;
                order.order_type = OrderType::StopLoss;
                order.quantity = bridge_logic::close_quantity(qty, held);
                order.stop_price = stop;
                order.validity = "DAY";
                // The position's product as THIS broker means it (as the equity screen's close does).
                order.product_type = pos.product_type.isEmpty() ? parse_product(product_str)
                                                                : ibk->product_from_wire(pos.product_type);
                // Protective stop: "workflow_exit:<id>". UnifiedTrading places it without
                // approval only after re-reading the account's positions and verifying it
                // reduces one; otherwise it is treated like an entry (Semi-Auto: queued).
                order.source = exit_source;

                auto response = UnifiedTrading::instance().place_order(account_id, order);
                QJsonObject out;
                out["order_id"] = response.order_id;
                out["symbol"] = symbol;
                out["side"] = direction > 0 ? "sell" : "buy";
                out["quantity"] = order.quantity;
                out["reference_price"] = last;
                out["stop_price"] = stop;
                out["trail_type"] = trail_type;
                out["trail_value"] = trail_value;
                out["broker"] = broker;
                out["mode"] = "live";
                out["note"] = "Static stop placed from the last price; the broker does not trail it";
                out["status"] = response.success ? "active" : "failed";
                if (!response.success)
                    out["error"] = response.message;
                cb(response.success, out, response.success ? QString{} : response.message);
            });
        };
    }

    // ── Scale In ──────────────────────────────────────────────────
    auto* scale_def = const_cast<NodeTypeDef*>(registry.find("trading.scale_in"));
    if (scale_def) {
        scale_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString mode = params.value("mode").toString("paper");
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString exchange = params.value("exchange").toString("NSE");
            QString product_str = params.value("product_type").toString("delivery");
            QString side = params.value("side").toString("buy");
            double total_qty = params.value("total_quantity").toDouble(100);
            int tranches = static_cast<int>(params.value("tranches").toDouble(4));
            int interval_sec = static_cast<int>(params.value("interval_sec").toDouble(60));
            const QString run_token = params.value("_run_token").toString();

            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }
            if (tranches < 1)
                tranches = 1;

            double per_tranche = std::floor(total_qty / tranches);
            double remainder = total_qty - (per_tranche * tranches);

            if (mode == "paper") {
                QJsonArray orders;
                for (int i = 0; i < tranches; ++i) {
                    double q = per_tranche + (i == tranches - 1 ? remainder : 0);
                    QJsonObject o;
                    o["order_id"] = QString("PAPER-SCALE-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(i);
                    o["tranche"] = i + 1;
                    o["quantity"] = q;
                    o["scheduled_sec"] = i * interval_sec;
                    o["status"] = "scheduled";
                    orders.append(o);
                }
                QJsonObject out;
                out["symbol"] = symbol;
                out["side"] = side;
                out["total_quantity"] = total_qty;
                out["tranches"] = tranches;
                out["orders"] = orders;
                out["broker"] = "paper";
                out["mode"] = "paper";
                out["simulated_only"] = true; // tranches are a schedule, not paper orders
                cb(true, out, {});
                return;
            }

            // Sleeps interval_sec between tranches (minutes by default), so it
            // lives on the blocking-jobs pool, never the global or order pool.
            const QString order_source = bridge_order_source(params);
            (void)QtConcurrent::run(&blocking_thread_pool(), [cb, symbol, exchange, product_str, side, per_tranche,
                                                              remainder, tranches, interval_sec, broker, run_token,
                                                              order_source]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                // Semi-Auto gate (headless entry): scale-in adds to a position,
                // so queue the full size for one-shot approval rather than placing
                // individual tranches. Auto mode falls through to the loop below.
                {
                    UnifiedOrder gate_order;
                    gate_order.symbol = symbol;
                    gate_order.exchange = exchange;
                    gate_order.side = (side == "sell") ? OrderSide::Sell : OrderSide::Buy;
                    gate_order.order_type = OrderType::Market;
                    gate_order.quantity = per_tranche * tranches + remainder;
                    gate_order.validity = "DAY";
                    // Same mapping as Place Order: "mtf" used to fall through to Delivery.
                    gate_order.product_type = parse_product(product_str);
                    gate_order.source = order_source;
                    if (gate_or_queue(account_id, gate_order, cb))
                        return;
                }

                // Stop / abort of the workflow must halt the tranches: check the run before
                // every order and wake up every second while waiting between tranches.
                auto run_alive = [&run_token]() {
                    return run_token.isEmpty() || WorkflowExecutor::is_run_active(run_token);
                };
                QJsonArray results;
                bool stopped = false;
                for (int i = 0; i < tranches; ++i) {
                    if (i > 0) {
                        for (int waited = 0; waited < interval_sec && run_alive(); ++waited)
                            QThread::sleep(1);
                    }
                    if (!run_alive()) {
                        stopped = true;
                        break;
                    }

                    double q = per_tranche + (i == tranches - 1 ? remainder : 0);
                    UnifiedOrder order;
                    order.symbol = symbol;
                    order.exchange = exchange;
                    order.side = (side == "sell") ? OrderSide::Sell : OrderSide::Buy;
                    order.order_type = OrderType::Market;
                    order.quantity = q;
                    order.validity = "DAY";
                    order.product_type = parse_product(product_str);
                    order.source = order_source;

                    auto response = UnifiedTrading::instance().place_order(account_id, order);
                    QJsonObject r;
                    r["tranche"] = i + 1;
                    r["quantity"] = q;
                    r["order_id"] = response.order_id;
                    r["status"] = response.success ? "submitted" : "failed";
                    if (!response.success)
                        r["error"] = response.message;
                    results.append(r);

                    if (!response.success)
                        break;
                }

                QJsonObject out;
                out["symbol"] = symbol;
                out["side"] = side;
                out["total_quantity"] = per_tranche * tranches + remainder;
                out["tranches_executed"] = results.size();
                out["orders"] = results;
                out["broker"] = broker;
                out["mode"] = "live";
                if (stopped) {
                    out["stopped_early"] = true;
                    LOG_WARN("ServiceBridges", QString("Scale-in %1 stopped after %2 of %3 tranches (workflow stopped)")
                                                   .arg(symbol)
                                                   .arg(results.size())
                                                   .arg(tranches));
                }
                cb(true, out, {});
            });
        };
    }

    // ── Phase 3 §6: Get Quote ─────────────────────────────────────
    auto* quote_def = const_cast<NodeTypeDef*>(registry.find("trading.get_quote"));
    if (quote_def) {
        quote_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString account_id_param = params.value("account_id").toString();
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString exchange = params.value("exchange").toString("NSE");
            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }

            (void)QtConcurrent::run([cb, account_id_param, broker, symbol, exchange]() {
                QString account_id = resolve_account_id(account_id_param, broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                QVector<QPair<QString, QString>> syms{{symbol, exchange}};
                auto resp = UnifiedTrading::instance().get_multi_quotes(account_id, syms);
                if (!resp.success || !resp.data.has_value() || resp.data->isEmpty()) {
                    cb(false, {}, resp.error.isEmpty() ? QString("No quote for %1").arg(symbol) : resp.error);
                    return;
                }

                const BrokerQuote& q = resp.data->first();
                QJsonObject out;
                out["symbol"] = symbol;
                out["exchange"] = exchange;
                out["ltp"] = q.ltp;
                out["bid"] = q.bid;
                out["ask"] = q.ask;
                out["volume"] = q.volume;
                out["oi"] = static_cast<double>(q.oi);
                out["open"] = q.open;
                out["high"] = q.high;
                out["low"] = q.low;
                out["close"] = q.close;
                out["change"] = q.change;
                out["change_pct"] = q.change_pct;
                cb(true, out, {});
            });
        };
    }

    // ── Phase 3 §6: Get Position (single symbol) ──────────────────
    auto* single_pos_def = const_cast<NodeTypeDef*>(registry.find("trading.get_position"));
    if (single_pos_def) {
        single_pos_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                     std::function<void(bool, QJsonValue, QString)> cb) {
            QString account_id_param = params.value("account_id").toString();
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString product = params.value("product").toString();
            const QString exchange = params.value("exchange").toString().trimmed();
            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }

            (void)QtConcurrent::run([cb, account_id_param, broker, symbol, product, exchange]() {
                QString account_id = resolve_account_id(account_id_param, broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto creds = AccountManager::instance().load_credentials(account_id);
                auto* ibk = AccountManager::instance().broker_for(account_id);
                if (!ibk) {
                    cb(false, {}, "Broker not found in registry");
                    return;
                }

                auto result = ibk->get_positions(creds);
                if (!result.success) {
                    cb(false, {}, result.error);
                    return;
                }

                // Exact, normalised symbol match on OPEN rows only (as the close / stop bridges
                // match). A substring match let "RELIANCE" return RELIANCEPP or a RELIANCE
                // option, "NIFTY" any NIFTY contract, and a closed (quantity 0) row win.
                // Product is a filter, read as this broker means it; Exchange narrows several
                // hits (it is not a hard filter: the select always holds a value, NSE by default).
                QVector<BrokerPosition> open;
                for (const auto& p : result.data.value()) {
                    if (bridge_logic::position_direction(p.quantity, p.side) == 0 ||
                        !bridge_logic::symbols_match(p.symbol, symbol))
                        continue;
                    if (!product.isEmpty() && ibk->product_from_wire(p.product_type) != parse_product(product))
                        continue;
                    open.append(p);
                }
                if (open.size() > 1 && !exchange.isEmpty()) {
                    QVector<BrokerPosition> same_venue;
                    for (const auto& p : open) {
                        if (p.exchange.compare(exchange, Qt::CaseInsensitive) == 0)
                            same_venue.append(p);
                    }
                    if (!same_venue.isEmpty())
                        open = same_venue;
                }
                if (open.size() > 1) {
                    cb(false, {},
                       QString("%1 has %2 open positions in different products — set Product to choose one")
                           .arg(symbol)
                           .arg(open.size()));
                    return;
                }
                if (open.size() == 1) {
                    const BrokerPosition& p = open.first();
                    QJsonObject out;
                    out["symbol"] = p.symbol;
                    out["exchange"] = p.exchange;
                    out["product"] = p.product_type;
                    out["quantity"] = p.quantity;
                    out["avg_price"] = p.avg_price;
                    out["ltp"] = p.ltp;
                    out["pnl"] = p.pnl;
                    out["pnl_pct"] = p.pnl_pct;
                    out["side"] = p.side;
                    cb(true, out, {});
                    return;
                }

                // No open position — return a flat (zero) position rather than error.
                QJsonObject out;
                out["symbol"] = symbol;
                out["quantity"] = 0.0;
                out["avg_price"] = 0.0;
                out["pnl"] = 0.0;
                cb(true, out, {});
            });
        };
    }

    // ── Phase 3 §6: Smart Order ───────────────────────────────────
    auto* smart_def = const_cast<NodeTypeDef*>(registry.find("trading.smart_order"));
    if (smart_def) {
        smart_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString account_id_param = params.value("account_id").toString();
            QString broker = params.value("broker").toString("fyers");
            QString symbol = params.value("symbol").toString();
            QString exchange = params.value("exchange").toString("NSE");
            double position_size = params.value("position_size").toDouble(0);
            QString order_type_str = params.value("order_type").toString("market");
            double price = params.value("price").toDouble(0);
            QString product = params.value("product").toString("intraday");
            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }

            // A non-zero target opens / adjusts a position: "workflow:<id>" (approval rule —
            // queued on a Semi-Auto account). A flatten (target 0, no quantity) is risk-reducing:
            // "workflow_exit:<id>" — UnifiedTrading places it without approval as the engine's
            // own close path (it sizes the order from the live position), like Close Position.
            const QString order_source =
                position_size != 0.0 ? bridge_order_source(params) : bridge_exit_order_source(params);
            (void)QtConcurrent::run(&order_thread_pool(), [cb, account_id_param, broker, symbol, exchange,
                                                           position_size, order_type_str, price, product,
                                                           order_source]() {
                QString account_id = resolve_account_id(account_id_param, broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                SmartOrder so;
                so.symbol = symbol;
                so.exchange = exchange;
                so.position_size = position_size;
                so.action = position_size >= 0 ? OrderSide::Buy : OrderSide::Sell;
                so.order_type = (order_type_str == "limit") ? OrderType::Limit : OrderType::Market;
                so.price = price;
                so.product_type = parse_product(product);
                so.source = order_source;

                auto resp = UnifiedTrading::instance().place_smart_order(account_id, so);
                if (!resp.success || !resp.data.has_value()) {
                    cb(false, {}, resp.error.isEmpty() ? "Smart order failed" : resp.error);
                    return;
                }

                const SmartOrderResult& r = resp.data.value();
                QJsonObject out;
                out["symbol"] = symbol;
                out["exchange"] = exchange;
                out["order_id"] = r.order_id;
                out["action_taken"] = r.action_taken;
                out["executed_action"] = QString(order_side_str(r.executed_action));
                out["executed_quantity"] = r.executed_quantity;
                out["message"] = r.message;
                out["broker"] = broker;
                cb(true, out, {});
            });
        };
    }

    // ── Phase 3 §6: Cancel All Orders ─────────────────────────────
    auto* cancel_all_def = const_cast<NodeTypeDef*>(registry.find("trading.cancel_all"));
    if (cancel_all_def) {
        cancel_all_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                     std::function<void(bool, QJsonValue, QString)> cb) {
            QString account_id_param = params.value("account_id").toString();
            QString broker = params.value("broker").toString("fyers");

            (void)QtConcurrent::run(&order_thread_pool(), [cb, account_id_param, broker]() {
                QString account_id = resolve_account_id(account_id_param, broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto resp = UnifiedTrading::instance().cancel_all_orders(account_id);
                if (!resp.success || !resp.data.has_value()) {
                    cb(false, {}, resp.error.isEmpty() ? "Cancel all failed" : resp.error);
                    return;
                }

                const CancelAllResult& r = resp.data.value();
                QJsonObject out;
                out["canceled_count"] = r.canceled_order_ids.size();
                out["failed_count"] = r.failed.size();
                out["total_attempted"] = r.total_attempted;
                QJsonArray ids;
                for (const auto& id : r.canceled_order_ids)
                    ids.append(id);
                out["canceled_ids"] = ids;
                out["broker"] = broker;
                cb(true, out, {});
            });
        };
    }

    // ── Phase 3 §6: Close All Positions ───────────────────────────
    auto* close_all_def = const_cast<NodeTypeDef*>(registry.find("trading.close_all"));
    if (close_all_def) {
        close_all_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                    std::function<void(bool, QJsonValue, QString)> cb) {
            QString account_id_param = params.value("account_id").toString();
            QString broker = params.value("broker").toString("fyers");

            (void)QtConcurrent::run(&order_thread_pool(), [cb, account_id_param, broker]() {
                QString account_id = resolve_account_id(account_id_param, broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                auto resp = UnifiedTrading::instance().close_all_positions(account_id);
                if (!resp.success || !resp.data.has_value()) {
                    cb(false, {}, resp.error.isEmpty() ? "Close all failed" : resp.error);
                    return;
                }

                const CloseAllResult& r = resp.data.value();
                QJsonObject out;
                out["closed_count"] = r.closed_symbols.size();
                out["failed_count"] = r.failed.size();
                out["total_positions"] = r.total_positions;
                QJsonArray syms;
                for (const auto& s : r.closed_symbols)
                    syms.append(s);
                out["closed_symbols"] = syms;
                out["broker"] = broker;
                cb(true, out, {});
            });
        };
    }

    // ── Phase 3 §6: Trading Alert ─────────────────────────────────
    auto* alert_def = const_cast<NodeTypeDef*>(registry.find("trading.alert"));
    if (alert_def) {
        alert_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            using namespace fincept::notifications;
            QString message = params.value("message").toString();
            QString channel = params.value("channel").toString("notification");
            if (message.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                message = inputs[0].toObject().value("message").toString();
            if (message.isEmpty()) {
                cb(false, {}, "Alert message is required");
                return;
            }

            const QJsonValue pass_through = inputs.isEmpty() ? QJsonValue{} : inputs[0];

            if (channel == "log") {
                LOG_INFO("TradingAlert", message);
                cb(true, pass_through, {});
                return;
            }

            NotificationRequest req;
            req.title = "Trading Alert";
            req.message = message;
            req.level = NotifLevel::Alert;
            req.trigger = NotifTrigger::WorkflowNode;
            NotificationService::instance().send(req);
            cb(true, pass_through, {});
        };
    }

    LOG_DEBUG("ServiceBridges", "Trading bridges wired (17 nodes)");
}

// ── Agent Bridge ───────────────────────────────────────────────────────

void wire_agent_bridges(NodeRegistry& /*registry*/) {
    // agent.run and agent.tool_picker have real executors from AgentNodes.cpp.
    // agent.single/agent.multi/agent.mediator are legacy stubs — leave their
    // executors to the fallback pass-through in wire_all_bridges if still null.
    LOG_DEBUG("ServiceBridges", "Agent bridges wired");
}

// ── Wire All ───────────────────────────────────────────────────────────

static void wire_utility_bridges(NodeRegistry& registry) {
    // ── HTTP Request — via HttpClient ──────────────────────────────
    auto* http_def = const_cast<NodeTypeDef*>(registry.find("utility.http_request"));
    if (http_def && !http_def->execute) {
        http_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString url = params.value("url").toString();
            QString method = params.value("method").toString("GET");
            if (url.isEmpty()) {
                cb(false, {}, "URL is required");
                return;
            }

            // HttpClient only does GET/POST and has no header support: PUT and DELETE used to be
            // sent as GET and the Headers parameter was dropped. The API Call node already does
            // all of that (methods, headers, timeout) — route those requests through it.
            if (method == "PUT" || method == "DELETE" || !params.value("headers").toObject().isEmpty()) {
                const auto* api = NodeRegistry::instance().find("utility.api_call");
                if (api && api->execute) {
                    QJsonObject api_params = params;
                    api_params["auth_type"] = "none";
                    api->execute(api_params, inputs, cb);
                    return;
                }
            }

            // A user-entered URL can answer with megabytes: the large forms parse the body on
            // HttpClient's worker, never on the GUI thread. The answer is delivered to the run
            // that launched this node (its executor) and dropped once that run is gone. Every
            // result is what post() / get() gave: a non-JSON 2xx is still a failure carrying
            // the parse text, an HTTP error still "HTTP_<status>: <message>". Outside a run
            // (no executor) the client itself is the context, as before.
            const QObject* run_context = WorkflowExecutor::launching_executor();
            auto& http = HttpClient::instance();
            if (method == "POST") {
                QJsonObject body = params.value("body").toObject();
                if (body.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                    body = inputs[0].toObject();
                // timeout 0: the client-wide transfer timeout, as post() has.
                http.post_large(url, body, 0, [cb, url](Result<QJsonDocument> result) {
                    if (result.is_err()) {
                        cb(false, {}, QString::fromStdString(result.error()));
                        return;
                    }
                    // No status_code: HttpClient's JSON callback does not carry it (a constant 200
                    // was reported here); any non-2xx reply already arrives as a failure.
                    QJsonObject out;
                    out["url"] = url;
                    out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                            : QJsonValue(result.value().array());
                    cb(true, out, {});
                }, run_context);
            } else {
                http.get_large(url, [cb, url](Result<QJsonDocument> result) {
                    if (result.is_err()) {
                        cb(false, {}, QString::fromStdString(result.error()));
                        return;
                    }
                    QJsonObject out;
                    out["url"] = url;
                    out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                            : QJsonValue(result.value().array());
                    cb(true, out, {});
                }, run_context);
            }
            LOG_DEBUG("UtilityBridge", QString("HTTP %1 %2").arg(method, log_url(QUrl(url))));
        };
    }

    // ── Code node — via PythonRunner ──────────────────────────────
    auto* code_def = const_cast<NodeTypeDef*>(registry.find("utility.code"));
    if (code_def && !code_def->execute) {
        code_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString language = params.value("language").toString("python");
            QString code = params.value("code").toString();
            if (code.isEmpty()) {
                cb(false, {}, "Code is empty");
                return;
            }
            // The Language select offers JavaScript, but only a Python runtime is wired: the old
            // code ran JavaScript source through Python and reported a baffling SyntaxError.
            if (language.compare("python", Qt::CaseInsensitive) != 0) {
                cb(false, {}, QString("The Code node can only run Python — '%1' is not supported").arg(language));
                return;
            }

            // Inject the upstream input as `_input`. It travels base64-encoded: pasting the JSON
            // text into a single-quoted Python literal broke on any backslash, quote or newline
            // inside a string value (the \n escape became a raw newline -> invalid JSON), and a
            // scalar input arrived as [].
            if (!inputs.isEmpty() && !inputs[0].isNull() && !inputs[0].isUndefined()) {
                const QByteArray encoded =
                    QJsonDocument(QJsonArray{inputs[0]}).toJson(QJsonDocument::Compact).toBase64();
                code = QString("import json, base64\n_input = json.loads(base64.b64decode('%1').decode('utf-8'))[0]\n")
                           .arg(QString::fromLatin1(encoded)) +
                       code;
            }

            // Append a JSON output wrapper if user doesn't print JSON themselves
            if (!code.contains("json.dumps") && !code.contains("print(")) {
                code += "\nimport json as _json\ntry:\n    print(_json.dumps({'result': result}))\nexcept NameError:\n "
                        "   print(_json.dumps({'executed': True}))\n";
            }

            PythonRunner::instance().run_code(code, [cb](const PythonResult& res) {
                if (!res.success) {
                    cb(false, {}, res.error.isEmpty() ? "Code execution failed" : res.error);
                    return;
                }
                QString json_str = extract_json(res.output).trimmed();
                if (!json_str.isEmpty()) {
                    auto doc = QJsonDocument::fromJson(json_str.toUtf8());
                    if (!doc.isNull()) {
                        cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
                        return;
                    }
                }
                // Return raw output as text
                QJsonObject out;
                out["output"] = res.output;
                cb(true, out, {});
            });
        };
    }

    // ── Analytics nodes already have executors from AnalyticsNodes.cpp — skip

    // ── File operations — via Qt file I/O ─────────────────────────
    auto* file_def = const_cast<NodeTypeDef*>(registry.find("file.operations"));
    if (file_def && !file_def->execute) {
        file_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            QString operation = params.value("operation").toString("read");
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "File path is required");
                return;
            }

            if (operation == "read") {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    cb(false, {}, QString("Cannot open file: %1").arg(file.errorString()));
                    return;
                }
                QJsonObject out;
                out["path"] = path;
                out["content"] = QString::fromUtf8(file.readAll());
                out["size"] = file.size();
                cb(true, out, {});
            } else if (operation == "write" || operation == "append") {
                QDir().mkpath(QFileInfo(path).absolutePath()); // "output/x.csv" used to fail when output/ was missing
                QFile file(path);
                auto mode = QIODevice::WriteOnly | QIODevice::Text;
                if (operation == "append")
                    mode |= QIODevice::Append;
                if (!file.open(mode)) {
                    cb(false, {}, QString("Cannot open file for writing: %1").arg(file.errorString()));
                    return;
                }
                QString content;
                if (!inputs.isEmpty()) {
                    if (inputs[0].isString())
                        content = inputs[0].toString();
                    else
                        content =
                            QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                                 : QJsonDocument(inputs[0].toArray()))
                                                  .toJson(QJsonDocument::Indented));
                }
                if (const QByteArray bytes_out = content.toUtf8(); file.write(bytes_out) != bytes_out.size())
                    LOG_ERROR("ServiceBridges", logf("Workflow file node could not write", {{"path", log_path(path)},
                                                                                           {"bytes", bytes_out.size()},
                                                                                           {"error", file.errorString()}}));
                QJsonObject out;
                out["path"] = path;
                out["operation"] = operation;
                out["bytes_written"] = content.toUtf8().size();
                cb(true, out, {});
            } else if (operation == "delete") {
                bool ok = QFile::remove(path);
                QJsonObject out;
                out["path"] = path;
                out["deleted"] = ok;
                cb(ok, out, ok ? QString{} : "Failed to delete file");
            } else if (operation == "exists") {
                QJsonObject out;
                out["path"] = path;
                out["exists"] = QFile::exists(path);
                cb(true, out, {});
            } else {
                cb(false, {}, "Unknown file operation: " + operation);
            }
        };
    }

    // ── File convert — export data as JSON/CSV ────────────────────
    auto* convert_def = const_cast<NodeTypeDef*>(registry.find("file.convert"));
    if (convert_def && !convert_def->execute) {
        convert_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                  std::function<void(bool, QJsonValue, QString)> cb) {
            QString format = params.value("format").toString("json");
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "Output path is required");
                return;
            }
            if (inputs.isEmpty()) {
                cb(false, {}, "No input data to convert");
                return;
            }

            // Only JSON and CSV are produced here. xlsx / xml / pdf used to fall into a branch that
            // wrote compact JSON under the requested name — a "report.pdf" that no PDF reader
            // can open, reported as a success.
            if (format != "json" && format != "csv") {
                cb(false, {},
                   QString("Convert to File cannot write '%1' — use the Spreadsheet node for xlsx, the PDF Report "
                           "node for pdf, or the XML node for xml")
                       .arg(format));
                return;
            }

            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                cb(false, {}, QString("Cannot open: %1").arg(file.errorString()));
                return;
            }

            if (format == "json") {
                QJsonDocument doc =
                    inputs[0].isObject() ? QJsonDocument(inputs[0].toObject()) : QJsonDocument(inputs[0].toArray());
                if (const QByteArray bytes_out = doc.toJson(QJsonDocument::Indented);
                    file.write(bytes_out) != bytes_out.size())
                    LOG_ERROR("ServiceBridges", logf("Workflow file node could not write", {{"path", log_path(path)},
                                                                                           {"bytes", bytes_out.size()},
                                                                                           {"error", file.errorString()}}));
            } else {
                // CSV from an array of objects
                if (const QByteArray bytes_out = bridge_csv_from_rows(inputs[0].toArray()).toUtf8();
                    file.write(bytes_out) != bytes_out.size())
                    LOG_ERROR("ServiceBridges", logf("Workflow file node could not write", {{"path", log_path(path)},
                                                                                           {"bytes", bytes_out.size()},
                                                                                           {"error", file.errorString()}}));
            }
            file.close();

            QJsonObject out;
            out["path"] = path;
            out["format"] = format;
            out["size"] = QFileInfo(path).size();
            cb(true, out, {});
        };
    }

    // ── File binary — read/write raw bytes ────────────────────────
    auto* binary_def = const_cast<NodeTypeDef*>(registry.find("file.binary"));
    if (binary_def && !binary_def->execute) {
        binary_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QString op = params.value("operation").toString("read");
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "File path is required");
                return;
            }

            if (op == "read") {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) {
                    cb(false, {}, "Cannot open file: " + file.errorString());
                    return;
                }
                QByteArray data = file.readAll();
                QJsonObject out;
                out["path"] = path;
                out["size"] = data.size();
                out["base64"] = QString::fromLatin1(data.toBase64());
                cb(true, out, {});
            } else {
                // write: decode base64 from input and write to path
                QByteArray bytes;
                if (!inputs.isEmpty()) {
                    if (inputs[0].isObject()) {
                        QString b64 = inputs[0].toObject().value("base64").toString();
                        if (!b64.isEmpty())
                            bytes = QByteArray::fromBase64(b64.toLatin1());
                        else
                            bytes = QJsonDocument(inputs[0].toObject()).toJson(QJsonDocument::Compact);
                    } else if (inputs[0].isString()) {
                        bytes = QByteArray::fromBase64(inputs[0].toString().toLatin1());
                    }
                }
                if (bytes.isEmpty()) {
                    cb(false, {}, "No binary data to write (pass {\"base64\": \"...\"} or base64 string as input)");
                    return;
                }

                QFileInfo fi(path);
                QDir().mkpath(fi.absolutePath());
                QFile file(path);
                if (!file.open(QIODevice::WriteOnly)) {
                    cb(false, {}, "Cannot open file for writing: " + file.errorString());
                    return;
                }
                if (file.write(bytes) != bytes.size())
                    LOG_ERROR("ServiceBridges", logf("Workflow file node could not write", {{"path", log_path(path)},
                                                                                           {"bytes", bytes.size()},
                                                                                           {"error", file.errorString()}}));
                file.close();

                QJsonObject out;
                out["path"] = path;
                out["operation"] = "write";
                out["bytes_written"] = bytes.size();
                cb(true, out, {});
            }
        };
    }

    // ── File compress — compress/decompress ─────────────────────────
    auto* compress_def = const_cast<NodeTypeDef*>(registry.find("file.compress"));
    if (compress_def && !compress_def->execute) {
        compress_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                                   std::function<void(bool, QJsonValue, QString)> cb) {
            QString op = params.value("operation").toString("compress");
            QString path = params.value("path").toString();
            if (path.isEmpty()) {
                cb(false, {}, "File path is required");
                return;
            }

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                cb(false, {}, "Cannot open file: " + file.errorString());
                return;
            }
            QByteArray data = file.readAll();
            file.close();

            if (op == "decompress") {
                QByteArray decompressed = qUncompress(data);
                if (decompressed.isEmpty()) {
                    cb(false, {}, "Decompression failed — file may not be qCompress format");
                    return;
                }
                QString out_path = path.endsWith(".qz") ? path.chopped(3) : path + ".decompressed";
                QFile out_file(out_path);
                if (!out_file.open(QIODevice::WriteOnly)) {
                    cb(false, {}, "Cannot write decompressed file: " + out_file.errorString());
                    return;
                }
                if (out_file.write(decompressed) != decompressed.size())
                    LOG_ERROR("ServiceBridges", logf("Workflow file node could not write", {{"path", log_path(out_path)},
                                                                                           {"bytes", decompressed.size()},
                                                                                           {"error", out_file.errorString()}}));

                QJsonObject out;
                out["input_path"] = path;
                out["output_path"] = out_path;
                out["compressed_size"] = data.size();
                out["original_size"] = decompressed.size();
                out["operation"] = "decompress";
                out["format_note"] = "Qt qCompress container (.qz) — not a .zip / .gz / .tar.gz archive";
                cb(true, out, {});
            } else {
                QByteArray compressed = qCompress(data);
                QString out_path = path + ".qz";
                QFile out_file(out_path);
                if (!out_file.open(QIODevice::WriteOnly)) {
                    cb(false, {}, "Cannot write compressed file: " + out_file.errorString());
                    return;
                }
                if (out_file.write(compressed) != compressed.size())
                    LOG_ERROR("ServiceBridges", logf("Workflow file node could not write", {{"path", log_path(out_path)},
                                                                                           {"bytes", compressed.size()},
                                                                                           {"error", out_file.errorString()}}));

                QJsonObject out;
                out["input_path"] = path;
                out["output_path"] = out_path;
                out["original_size"] = data.size();
                out["compressed_size"] = compressed.size();
                out["ratio"] = data.size() > 0 ? static_cast<double>(compressed.size()) / data.size() : 0.0;
                out["operation"] = "compress";
                // The Format select offers zip / gzip / tar.gz, but the file written is Qt's own
                // qCompress container: other tools cannot open it. Say so.
                out["format_note"] = "Qt qCompress container (.qz) — not a .zip / .gz / .tar.gz archive";
                cb(true, out, {});
            }
        };
    }

    // ── HTML Extract — regex-based extraction ─────────────────────
    auto* html_def = const_cast<NodeTypeDef*>(registry.find("format.html_extract"));
    if (html_def && !html_def->execute) {
        html_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                               std::function<void(bool, QJsonValue, QString)> cb) {
            if (inputs.isEmpty()) {
                cb(false, {}, "No HTML input");
                return;
            }
            QString html;
            if (inputs[0].isString())
                html = inputs[0].toString();
            else if (inputs[0].isObject())
                html = inputs[0].toObject().value("content").toString(inputs[0].toObject().value("data").toString());
            if (html.isEmpty()) {
                cb(false, {}, "No HTML content found in input");
                return;
            }

            QString selector = params.value("selector").toString();
            const QString attr = params.value("attribute").toString("text").trimmed();

            // The element the selector targets is its LAST compound ("div.price > span" ->
            // span), with its tag, .class and #id filters. The old code kept only the tag
            // name before the first '.' / '#' ("div.price > span" returned every <div>) and
            // ignored Attribute. Ancestor parts of a selector are not checked.
            QString compound;
            {
                static const QRegularExpression comb_re(QStringLiteral(R"([\s>+~]+)"));
                const QStringList parts = selector.trimmed().split(comb_re, Qt::SkipEmptyParts);
                compound = parts.isEmpty() ? QString() : parts.last();
            }
            QString tag;
            QString id;
            QStringList classes;
            static const QRegularExpression tok_re(QStringLiteral(R"(([.#]?)([A-Za-z0-9_-]+))"));
            for (auto tit = tok_re.globalMatch(compound); tit.hasNext();) {
                const auto t = tit.next();
                if (t.captured(1) == QLatin1String("."))
                    classes << t.captured(2);
                else if (t.captured(1) == QLatin1String("#"))
                    id = t.captured(2);
                else if (tag.isEmpty())
                    tag = t.captured(2);
            }
            if (tag.isEmpty() && classes.isEmpty() && id.isEmpty()) {
                cb(false, {}, "Set a selector: a tag (span), .class, #id or tag.class");
                return;
            }

            auto attr_value = [](const QString& attrs, const QString& name) -> std::optional<QString> {
                const QRegularExpression a_re(
                    QString(R"re((?:^|\s)%1\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'>]+)))re").arg(QRegularExpression::escape(name)),
                    QRegularExpression::CaseInsensitiveOption);
                const auto m = a_re.match(attrs);
                if (!m.hasMatch())
                    return std::nullopt;
                for (int g = 1; g <= 3; ++g) {
                    if (m.capturedStart(g) >= 0)
                        return m.captured(g);
                }
                return QString();
            };
            auto passes_filters = [&](const QString& attrs) {
                if (!id.isEmpty() && attr_value(attrs, QStringLiteral("id")).value_or(QString()) != id)
                    return false;
                if (!classes.isEmpty()) {
                    static const QRegularExpression ws_re(QStringLiteral("\\s+"));
                    const QStringList have =
                        attr_value(attrs, QStringLiteral("class")).value_or(QString()).split(ws_re, Qt::SkipEmptyParts);
                    for (const QString& c : classes) {
                        if (!have.contains(c))
                            return false;
                    }
                }
                return true;
            };

            // "text" = inner text, "html" = inner HTML, anything else = that attribute's value.
            const bool want_attr =
                !attr.isEmpty() && attr.compare("text", Qt::CaseInsensitive) != 0 && attr.compare("html", Qt::CaseInsensitive) != 0;
            const QString tag_pat = tag.isEmpty() ? QStringLiteral("[A-Za-z][A-Za-z0-9-]*") : QRegularExpression::escape(tag);
            const QRegularExpression open_re(QString("<(%1)(\\s[^>]*)?/?>").arg(tag_pat),
                                             QRegularExpression::CaseInsensitiveOption);
            static const QRegularExpression strip_tags_re(QStringLiteral("<[^>]*>"));
            QJsonArray results;
            for (auto it = open_re.globalMatch(html); it.hasNext();) {
                const auto m = it.next();
                const QString attrs = m.captured(2);
                if (!passes_filters(attrs))
                    continue;
                if (want_attr) {
                    if (const auto v = attr_value(attrs, attr))
                        results.append(*v);
                    continue;
                }
                // Content up to the first closing tag of the same name (same-name nesting is
                // not balanced — this is a regex reader, not a DOM).
                const qsizetype content_start = m.capturedEnd(0);
                const QRegularExpression close_re(QString("</%1\\s*>").arg(QRegularExpression::escape(m.captured(1))),
                                                  QRegularExpression::CaseInsensitiveOption);
                const auto cm = close_re.match(html, content_start);
                if (!cm.hasMatch())
                    continue;
                QString inner = html.mid(content_start, cm.capturedStart(0) - content_start);
                if (attr.compare("html", Qt::CaseInsensitive) != 0)
                    inner = inner.remove(strip_tags_re).simplified();
                results.append(inner.trimmed());
            }

            QJsonObject out;
            out["selector"] = selector;
            out["matches"] = results;
            out["count"] = results.size();
            cb(true, out, {});
        };
    }

    // ── RSS Read — fetch and parse RSS/Atom XML ───────────────────
    auto* rss_def = const_cast<NodeTypeDef*>(registry.find("utility.rss_read"));
    if (rss_def && !rss_def->execute) {
        rss_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
                              std::function<void(bool, QJsonValue, QString)> cb) {
            QString url = params.value("url").toString();
            int limit = static_cast<int>(params.value("limit").toDouble(20));
            if (url.isEmpty()) {
                cb(false, {}, "RSS URL is required");
                return;
            }

            static QNetworkAccessManager* rss_nam = nullptr;
            if (!rss_nam)
                rss_nam = new QNetworkAccessManager;

            QNetworkRequest request{QUrl{url}};
            request.setRawHeader("Accept", "application/rss+xml, application/atom+xml, application/xml, text/xml");
            request.setTransferTimeout(15000);
            QNetworkReply* reply = rss_nam->get(request);

            QObject::connect(reply, &QNetworkReply::finished, [reply, cb, url, limit]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    cb(false, {}, QString("RSS fetch failed: %1").arg(reply->errorString()));
                    return;
                }

                QByteArray xml_data = reply->readAll();
                QXmlStreamReader xml(xml_data);
                QJsonArray items;
                bool is_atom = false;

                while (!xml.atEnd() && !xml.hasError()) {
                    xml.readNext();
                    if (xml.isStartElement()) {
                        if (xml.name() == u"feed")
                            is_atom = true;

                        bool is_item = is_atom ? (xml.name() == u"entry") : (xml.name() == u"item");
                        if (is_item) {
                            QJsonObject item;
                            // Stop at end-of-input / a parse error as well: on a truncated or malformed
                            // feed the end tag never arrives and this loop used to spin forever —
                            // on the UI thread.
                            while (!(xml.isEndElement() && (xml.name() == u"item" || xml.name() == u"entry")) &&
                                   !xml.atEnd() && !xml.hasError()) {
                                xml.readNext();
                                if (xml.isStartElement()) {
                                    QString tag = xml.name().toString();
                                    if (tag == "title") {
                                        item["title"] = xml.readElementText();
                                    } else if (tag == "link") {
                                        if (is_atom) {
                                            item["link"] = xml.attributes().value("href").toString();
                                            xml.skipCurrentElement();
                                        } else {
                                            item["link"] = xml.readElementText();
                                        }
                                    } else if (tag == "description" || tag == "summary" || tag == "content") {
                                        item["description"] = xml.readElementText();
                                    } else if (tag == "pubDate" || tag == "published" || tag == "updated") {
                                        item["pubDate"] = xml.readElementText();
                                    } else if (tag == "author" || tag == "creator") {
                                        item["author"] = xml.readElementText();
                                    } else if (tag == "category") {
                                        QString cat = xml.attributes().value("term").toString();
                                        if (cat.isEmpty())
                                            cat = xml.readElementText();
                                        else
                                            xml.skipCurrentElement();
                                        if (!cat.isEmpty())
                                            item["category"] = cat;
                                    } else {
                                        xml.skipCurrentElement();
                                    }
                                }
                            }
                            if (!item.isEmpty()) {
                                items.append(item);
                                if (items.size() >= limit)
                                    break;
                            }
                        }
                    }
                }

                if (xml.hasError() && items.isEmpty()) {
                    cb(false, {}, QString("XML parse error: %1").arg(xml.errorString()));
                    return;
                }

                QJsonObject out;
                out["url"] = url;
                out["format"] = is_atom ? "atom" : "rss";
                out["count"] = items.size();
                out["items"] = items;
                cb(true, out, {});
            });
        };
    }

    // ── Crypto/Hash — QCryptographicHash ──────────────────────────
    auto* crypto_def = const_cast<NodeTypeDef*>(registry.find("utility.crypto"));
    if (crypto_def && !crypto_def->execute) {
        crypto_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                 std::function<void(bool, QJsonValue, QString)> cb) {
            QString op = params.value("operation").toString("sha256");
            QString input_str;
            if (!inputs.isEmpty()) {
                if (inputs[0].isString())
                    input_str = inputs[0].toString();
                else
                    input_str =
                        QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                             : QJsonDocument(inputs[0].toArray()))
                                              .toJson(QJsonDocument::Compact));
            }

            QCryptographicHash::Algorithm algo = QCryptographicHash::Sha256;
            if (op == "md5")
                algo = QCryptographicHash::Md5;
            else if (op == "sha1")
                algo = QCryptographicHash::Sha1;
            else if (op == "sha512")
                algo = QCryptographicHash::Sha512;

            QByteArray hash = QCryptographicHash::hash(input_str.toUtf8(), algo);

            QJsonObject out;
            out["algorithm"] = op;
            out["hash"] = QString::fromLatin1(hash.toHex());
            out["input_length"] = input_str.length();
            cb(true, out, {});
        };
    }

    // ── Database — SQLite via Qt SQL ──────────────────────────────
    auto* db_def = const_cast<NodeTypeDef*>(registry.find("utility.database"));
    if (db_def && !db_def->execute) {
        db_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                             std::function<void(bool, QJsonValue, QString)> cb) {
            QString op = params.value("operation").toString("select");
            QString query = params.value("query").toString();
            QString db_source = params.value("database").toString("local");
            if (query.isEmpty()) {
                cb(false, {}, "SQL query is required");
                return;
            }

            // The policy, the private per-run connection and the masking live in
            // WorkflowDbQuery.h: the application's database is read-only for workflows
            // (SQL console guard + SQLITE_OPEN_READONLY + query_only, secrets masked);
            // "select" is read-only on any file; nothing reuses the app's connections.
            const QString app_db_path = fincept::Database::instance().path();
            QString db_path;
            db_node::Source source = db_node::Source::AppDatabase;
            if (db_source == "custom") {
                // The node's Database File first, else an upstream {"database_path"}. (The SQL
                // text used to be tried as a file path as a last resort.)
                db_path = params.value("database_path").toString().trimmed();
                if (db_path.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                    db_path = inputs[0].toObject().value("database_path").toString();
                if (db_path.isEmpty() || !QFile::exists(db_path)) {
                    cb(false, {},
                       db_path.isEmpty() ? QString("Choose the Database File for a custom database")
                                         : QString("Database file not found: %1").arg(db_path));
                    return;
                }
                // Pointing "custom" at fincept.db itself gets the application-database rules.
                if (!db_node::same_file(db_path, app_db_path))
                    source = db_node::Source::CustomFile;
            } else {
                if (app_db_path.isEmpty()) {
                    cb(false, {}, "No database connection available");
                    return;
                }
                db_path = app_db_path;
            }

            // Off the GUI thread; the connection is created, used and removed on the
            // worker. The executor accepts the callback from any thread.
            (void)QtConcurrent::run([cb, op, query, db_path, source]() {
                const db_node::Outcome r = db_node::run(db_path, source, op, query);
                if (!r.log_detail.isEmpty())
                    LOG_WARN("WorkflowDbNode", QString("%1 — %2").arg(r.error, r.log_detail));
                cb(r.ok, r.ok ? QJsonValue(r.output) : QJsonValue(), r.error);
            });
        };
    }

    // ── API Call — via QNetworkAccessManager with auth, headers, timeout, retry
    auto* api_def = const_cast<NodeTypeDef*>(registry.find("utility.api_call"));
    if (api_def && !api_def->execute) {
        struct ApiCallState {
            QNetworkRequest request;
            QByteArray body_bytes;
            QString method;
            QString url;
            int retries_left;
            std::function<void(bool, QJsonValue, QString)> cb;
        };

        static std::function<void(std::shared_ptr<ApiCallState>)> execute_request;
        execute_request = [](std::shared_ptr<ApiCallState> state) {
            static QNetworkAccessManager* api_nam = nullptr;
            if (!api_nam)
                api_nam = new QNetworkAccessManager;

            QNetworkReply* reply = nullptr;
            if (state->method == "POST")
                reply = api_nam->post(state->request, state->body_bytes);
            else if (state->method == "PUT")
                reply = api_nam->put(state->request, state->body_bytes);
            else if (state->method == "PATCH")
                reply = api_nam->sendCustomRequest(state->request, "PATCH", state->body_bytes);
            else if (state->method == "DELETE")
                reply = api_nam->deleteResource(state->request);
            else
                reply = api_nam->get(state->request);

            QObject::connect(reply, &QNetworkReply::finished, [reply, state]() {
                reply->deleteLater();
                int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

                if (reply->error() != QNetworkReply::NoError) {
                    if (state->retries_left > 0) {
                        state->retries_left--;
                        QTimer::singleShot(1000 * (3 - state->retries_left), [state]() { execute_request(state); });
                        return;
                    }
                    state->cb(
                        false, {},
                        QString("HTTP %1 failed (%2): %3").arg(state->method).arg(status).arg(reply->errorString()));
                    return;
                }

                QByteArray data = reply->readAll();

                // Include response headers
                QJsonObject resp_headers;
                for (const auto& pair : reply->rawHeaderPairs())
                    resp_headers[QString::fromUtf8(pair.first)] = QString::fromUtf8(pair.second);

                // The body of a user-entered URL can be megabytes: it is parsed (and the
                // output built — the same output) on a worker, never on the GUI thread. The
                // executor accepts the callback from any thread, and drops it once the run
                // is gone. This is also the HTTP Request node's PUT / DELETE / headers path.
                (void)QtConcurrent::run([state, data = std::move(data), status, resp_headers]() {
                    QJsonDocument doc = QJsonDocument::fromJson(data);

                    QJsonObject out;
                    out["url"] = state->url;
                    out["method"] = state->method;
                    out["status_code"] = status;
                    if (!doc.isNull())
                        out["data"] = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
                    else
                        out["data"] = QString::fromUtf8(data);
                    out["headers"] = resp_headers;

                    state->cb(true, out, {});
                });
            });
        };

        api_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                              std::function<void(bool, QJsonValue, QString)> cb) {
            QString url = params.value("url").toString();
            QString method = params.value("method").toString("GET");
            if (url.isEmpty()) {
                cb(false, {}, "URL is required");
                return;
            }

            QString auth_type = params.value("auth_type").toString("none");
            QString auth_value = params.value("auth_value").toString();
            QJsonObject custom_headers = params.value("headers").toObject();
            QJsonObject body = params.value("body").toObject();
            int timeout_sec = static_cast<int>(params.value("timeout_sec").toDouble(30));
            int retry_count = static_cast<int>(params.value("retry_count").toDouble(0));

            if (body.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                body = inputs[0].toObject();

            QNetworkRequest request{QUrl{url}};
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setTransferTimeout(timeout_sec * 1000);

            if (auth_type == "bearer" && !auth_value.isEmpty()) {
                request.setRawHeader("Authorization", QByteArray("Bearer ") + auth_value.toUtf8());
            } else if (auth_type == "api_key" && !auth_value.isEmpty()) {
                request.setRawHeader("X-API-Key", auth_value.toUtf8());
            } else if (auth_type == "basic" && !auth_value.isEmpty()) {
                request.setRawHeader("Authorization", QByteArray("Basic ") + auth_value.toUtf8().toBase64());
            } else if (auth_type == "oauth2" && !auth_value.isEmpty()) {
                request.setRawHeader("Authorization", QByteArray("Bearer ") + auth_value.toUtf8());
            }

            for (auto it = custom_headers.begin(); it != custom_headers.end(); ++it)
                request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());

            auto state = std::make_shared<ApiCallState>();
            state->request = request;
            state->body_bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
            state->method = method;
            state->url = url;
            state->retries_left = retry_count;
            state->cb = cb;

            execute_request(state);
        };
    }

    // ── PDF Generate — via QPrinter/QPainter ─────────────────────
    auto* pdf_def = const_cast<NodeTypeDef*>(registry.find("file.pdf_generate"));
    if (pdf_def && !pdf_def->execute) {
        pdf_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                              std::function<void(bool, QJsonValue, QString)> cb) {
            QString title = params.value("title").toString("Report");
            QString tmpl = params.value("template").toString("default");
            QString path = params.value("path").toString("output/report.pdf");
            if (path.isEmpty()) {
                cb(false, {}, "Output path is required");
                return;
            }

            QFileInfo fi(path);
            QDir().mkpath(fi.absolutePath());

            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(path);
            printer.setPageSize(QPageSize(QPageSize::A4));

            QPainter painter;
            if (!painter.begin(&printer)) {
                cb(false, {}, "Cannot open PDF file for writing");
                return;
            }

            int dpi = printer.resolution();
            int page_w = printer.pageLayout().paintRectPixels(dpi).width();
            int margin = dpi / 2; // 0.5 inch margin
            int y = margin;

            // The PDF is a printed document, not terminal UI: white paper, dark
            // ink and its own document fonts and point sizes, whatever the theme.

            // Title
            QFont title_font("Segoe UI", 18, QFont::Bold);
            if (tmpl == "financial")
                title_font = QFont("Georgia", 20, QFont::Bold);
            else if (tmpl == "research")
                title_font = QFont("Palatino", 18, QFont::Bold);
            painter.setFont(title_font);
            painter.drawText(margin, y + painter.fontMetrics().ascent(), title);
            y += painter.fontMetrics().height() + dpi / 4;

            // Subtitle line
            QFont sub_font("Segoe UI", 9);
            painter.setFont(sub_font);
            painter.setPen(QColor(100, 100, 100));
            painter.drawText(margin, y + painter.fontMetrics().ascent(),
                             QString("Generated: %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")));
            y += painter.fontMetrics().height() + dpi / 6;
            painter.setPen(Qt::black);

            // Separator line
            painter.drawLine(margin, y, page_w - margin, y);
            y += dpi / 4;

            // Body: render input data as a table
            QFont body_font("Consolas", 9);
            painter.setFont(body_font);
            int line_h = painter.fontMetrics().height();
            int page_h = printer.pageLayout().paintRectPixels(dpi).height();

            auto new_page = [&]() {
                printer.newPage();
                y = margin;
            };

            if (!inputs.isEmpty() && inputs[0].isArray()) {
                QJsonArray arr = inputs[0].toArray();
                // Table header from first object's keys
                if (!arr.isEmpty() && arr[0].isObject()) {
                    QStringList headers = arr[0].toObject().keys();
                    int col_w = (page_w - 2 * margin) / qMax(1, headers.size());

                    // Header row
                    QFont hdr_font("Segoe UI", 9, QFont::Bold);
                    painter.setFont(hdr_font);
                    for (int c = 0; c < headers.size(); ++c)
                        painter.drawText(margin + c * col_w, y, col_w, line_h, Qt::AlignLeft, headers[c]);
                    y += line_h;
                    painter.drawLine(margin, y, page_w - margin, y);
                    y += 4;

                    // Data rows
                    painter.setFont(body_font);
                    for (const QJsonValue& row_val : arr) {
                        if (y + line_h > page_h - margin)
                            new_page();
                        QJsonObject row = row_val.toObject();
                        for (int c = 0; c < headers.size(); ++c) {
                            QJsonValue v = row.value(headers[c]);
                            QString text = v.isString() ? v.toString() : QString::number(v.toDouble(), 'f', 4);
                            painter.drawText(margin + c * col_w, y, col_w - 8, line_h, Qt::AlignLeft, text);
                        }
                        y += line_h;
                    }
                }
            } else if (!inputs.isEmpty() && inputs[0].isObject()) {
                // Key-value pairs
                QJsonObject obj = inputs[0].toObject();
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    if (y + line_h > page_h - margin)
                        new_page();
                    QString val =
                        it.value().isString() ? it.value().toString() : QString::number(it.value().toDouble(), 'f', 4);
                    painter.drawText(margin, y, it.key() + ": " + val);
                    y += line_h;
                }
            } else if (!inputs.isEmpty() && inputs[0].isString()) {
                // Plain text
                QStringList lines = inputs[0].toString().split('\n');
                for (const QString& line : lines) {
                    if (y + line_h > page_h - margin)
                        new_page();
                    painter.drawText(margin, y + painter.fontMetrics().ascent(), line);
                    y += line_h;
                }
            }

            painter.end();

            QJsonObject out;
            out["path"] = path;
            out["title"] = title;
            out["template"] = tmpl;
            out["size"] = QFileInfo(path).size();
            cb(true, out, {});
        };
    }

    // ── Image Chart — via PythonRunner + matplotlib ──────────────
    auto* chart_def = const_cast<NodeTypeDef*>(registry.find("file.image_chart"));
    if (chart_def && !chart_def->execute) {
        chart_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            QString chart_type = params.value("chart_type").toString("line");
            static const QSet<QString> kChartTypes = {"line", "bar", "candlestick", "scatter", "pie", "heatmap"};
            if (!kChartTypes.contains(chart_type))
                chart_type = "line"; // it is spliced into the script text — only known names get through
            QString title = params.value("title").toString("");
            int width = static_cast<int>(params.value("width").toDouble(800));
            int height = static_cast<int>(params.value("height").toDouble(600));
            QString path = params.value("path").toString("output/chart.png");
            if (path.isEmpty()) {
                cb(false, {}, "Output path is required");
                return;
            }

            QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            QString data_json =
                QString::fromUtf8(data.isObject() ? QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact)
                                                  : QJsonDocument(data.toArray()).toJson(QJsonDocument::Compact));

            // The chart is an exported PNG file with its own fixed colour scheme;
            // it is not drawn by the terminal and does not follow the theme.
            QString code = QString(R"PY(
import json, os, sys
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import matplotlib.dates as mdates
except ImportError:
    print(json.dumps({"error": "matplotlib not installed"}))
    sys.exit(0)

import base64 as _b64
# Text values arrive base64-encoded: quoting them into the script broke on quotes and backslashes.
data = json.loads(_b64.b64decode('%1').decode('utf-8'))
chart_type = '%2'
title = _b64.b64decode('%3').decode('utf-8')
width = %4
height = %5
path = _b64.b64decode('%6').decode('utf-8')

os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)

fig, ax = plt.subplots(figsize=(width/100, height/100), dpi=100)
fig.patch.set_facecolor('#1a1a2e')
ax.set_facecolor('#16213e')
ax.tick_params(colors='#a0a0a0')
ax.spines['bottom'].set_color('#404060')
ax.spines['left'].set_color('#404060')
ax.spines['top'].set_visible(False)
ax.spines['right'].set_visible(False)

if isinstance(data, list) and len(data) > 0:
    if isinstance(data[0], dict):
        keys = list(data[0].keys())
        x_key = keys[0]
        y_keys = [k for k in keys[1:] if isinstance(data[0].get(k), (int, float))]
        x_vals = [row.get(x_key, i) for i, row in enumerate(data)]
        if not y_keys:
            y_keys = [x_key]
            x_vals = list(range(len(data)))

        if chart_type == 'bar':
            import numpy as np
            x_pos = np.arange(len(x_vals))
            bar_w = 0.8 / max(1, len(y_keys))
            for i, yk in enumerate(y_keys):
                vals = [row.get(yk, 0) for row in data]
                ax.bar(x_pos + i * bar_w, vals, bar_w, label=yk, alpha=0.85)
            ax.set_xticks(x_pos + bar_w * (len(y_keys)-1) / 2)
            ax.set_xticklabels([str(v)[:12] for v in x_vals], rotation=45, ha='right', fontsize=7)
        elif chart_type == 'scatter':
            if len(y_keys) >= 2:
                xs = [row.get(y_keys[0], 0) for row in data]
                ys = [row.get(y_keys[1], 0) for row in data]
                ax.scatter(xs, ys, alpha=0.7, c='#00d4aa', s=30)
                ax.set_xlabel(y_keys[0], color='#a0a0a0')
                ax.set_ylabel(y_keys[1], color='#a0a0a0')
            else:
                vals = [row.get(y_keys[0], 0) for row in data]
                ax.scatter(range(len(vals)), vals, alpha=0.7, c='#00d4aa', s=30)
        elif chart_type == 'pie':
            vals = [row.get(y_keys[0], 0) for row in data]
            labels = [str(v)[:15] for v in x_vals]
            ax.set_facecolor('#1a1a2e')
            ax.pie(vals, labels=labels, autopct=lambda pct: '{:.1f}%'.format(pct), textprops={'color': '#d0d0d0', 'fontsize': 8})
        elif chart_type == 'heatmap':
            import numpy as np
            matrix = [[row.get(k, 0) for k in y_keys] for row in data]
            im = ax.imshow(matrix, aspect='auto', cmap='viridis')
            ax.set_xticks(range(len(y_keys)))
            ax.set_xticklabels(y_keys, rotation=45, ha='right', fontsize=7)
            ax.set_yticks(range(len(x_vals)))
            ax.set_yticklabels([str(v)[:12] for v in x_vals], fontsize=7)
            fig.colorbar(im, ax=ax)
        elif chart_type == 'candlestick':
            ohlc_keys = {'open': None, 'high': None, 'low': None, 'close': None}
            for k in keys:
                kl = k.lower()
                for ok in ohlc_keys:
                    if ok in kl:
                        ohlc_keys[ok] = k
            if all(ohlc_keys.values()):
                for i, row in enumerate(data):
                    o = row.get(ohlc_keys['open'], 0)
                    h = row.get(ohlc_keys['high'], 0)
                    l = row.get(ohlc_keys['low'], 0)
                    c = row.get(ohlc_keys['close'], 0)
                    color = '#00d4aa' if c >= o else '#ff6b6b'
                    ax.plot([i, i], [l, h], color=color, linewidth=0.8)
                    ax.plot([i, i], [o, c], color=color, linewidth=3.5)
            else:
                vals = [row.get(y_keys[0], 0) for row in data]
                ax.plot(vals, color='#00d4aa', linewidth=1.2)
        else:  # line
            for yk in y_keys:
                vals = [row.get(yk, 0) for row in data]
                ax.plot(vals, label=yk, linewidth=1.2)
            if len(y_keys) > 1:
                ax.legend(fontsize=8, facecolor='#1a1a2e', edgecolor='#404060', labelcolor='#d0d0d0')
    else:
        ax.plot(data, color='#00d4aa', linewidth=1.2)

if title:
    ax.set_title(title, color='#e0e0e0', fontsize=12, pad=10)

plt.tight_layout()
plt.savefig(path, dpi=100, facecolor=fig.get_facecolor(), bbox_inches='tight')
plt.close()

print(json.dumps({"path": path, "chart_type": chart_type, "size": os.path.getsize(path)}))
)PY")
                               .arg(QString::fromLatin1(data_json.toUtf8().toBase64()))
                               .arg(chart_type)
                               .arg(QString::fromLatin1(title.toUtf8().toBase64()))
                               .arg(width)
                               .arg(height)
                               .arg(QString::fromLatin1(path.toUtf8().toBase64()));

            PythonRunner::instance().run_code(code, [cb, path, chart_type](const PythonResult& res) {
                if (!res.success) {
                    cb(false, {}, res.error.isEmpty() ? "Chart generation failed" : res.error);
                    return;
                }
                QString json_str = extract_json(res.output).trimmed();
                if (!json_str.isEmpty()) {
                    auto doc = QJsonDocument::fromJson(json_str.toUtf8());
                    if (!doc.isNull() && doc.isObject()) {
                        QJsonObject out = doc.object();
                        if (out.contains("error")) {
                            cb(false, {}, out.value("error").toString());
                            return;
                        }
                        cb(true, out, {});
                        return;
                    }
                }
                QJsonObject out;
                out["path"] = path;
                out["chart_type"] = chart_type;
                cb(true, out, {});
            });
        };
    }

    // ── Execute Workflow — run another saved workflow as a sub-workflow ──
    auto* subwf_def = const_cast<NodeTypeDef*>(registry.find("control.execute_workflow"));
    if (subwf_def && !subwf_def->execute) {
        subwf_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                                std::function<void(bool, QJsonValue, QString)> cb) {
            const QString wf_id = params.value("workflow_id").toString().trimmed();
            if (wf_id.isEmpty()) {
                cb(false, {}, "Workflow ID is required");
                return;
            }

            // Recursion / cycle guard across nested execute_workflow nodes. Runs on the
            // UI thread; executors are signal-driven on the same thread, so a plain set
            // is sufficient. Blocks A→A and A→B→A; A→B→C is allowed.
            static QSet<QString> in_flight;
            if (in_flight.contains(wf_id)) {
                cb(false, {}, QString("Cyclic sub-workflow execution detected for '%1'").arg(wf_id));
                return;
            }

            auto loaded = fincept::WorkflowRepository::instance().load(wf_id);
            if (loaded.is_err()) {
                cb(false, {},
                   QString("Failed to load workflow '%1': %2").arg(wf_id, QString::fromStdString(loaded.error())));
                return;
            }

            WorkflowDef sub = loaded.value();
            if (sub.nodes.isEmpty()) {
                cb(false, {}, QString("Sub-workflow '%1' has no executable nodes").arg(wf_id));
                return;
            }

            // Expose this node's inbound payload to the sub-workflow via static_data.
            if (!inputs.isEmpty() && !inputs[0].isNull() && !inputs[0].isUndefined()) {
                QJsonObject sd = sub.static_data;
                sd["parent_input"] = inputs[0];
                sub.static_data = sd;
            }

            in_flight.insert(wf_id);
            auto* exec = new WorkflowExecutor();
            QObject::connect(exec, &WorkflowExecutor::execution_finished, exec,
                             [cb, wf_id, exec](const WorkflowExecutionResult& result) {
                                 in_flight.remove(wf_id);

                                 QJsonObject out;
                                 out["workflow_id"] = result.workflow_id;
                                 out["success"] = result.success;
                                 out["duration_ms"] = result.total_duration_ms;
                                 QJsonObject node_outputs;
                                 for (const auto& nr : result.node_results)
                                     node_outputs[nr.node_id] = nr.output;
                                 out["nodes"] = node_outputs;
                                 if (!result.error.isEmpty())
                                     out["error"] = result.error;

                                 if (result.success)
                                     cb(true, out, {});
                                 else
                                     cb(false, out, result.error.isEmpty() ? "Sub-workflow failed" : result.error);

                                 exec->deleteLater();
                             });
            exec->execute(sub);
        };
        LOG_DEBUG("ServiceBridges", "control.execute_workflow bridge wired");
    }

    LOG_DEBUG("ServiceBridges", "Utility bridges wired");
}

static void wire_mcp_bridges(NodeRegistry& registry) {
    auto* def = const_cast<NodeTypeDef*>(registry.find("mcp.tool_call"));
    if (!def)
        return;

    def->execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
        // An empty Tool param is legitimate: a Tool Picker upstream names the tool. The
        // old early "tool is required" return made that path unreachable.
        QString tool = params.value("tool").toString().trimmed();

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

        // Set by WorkflowExecutor::enforce_trading_mode when the run is in PAPER mode.
        const bool paper_run = params.value("_paper_run").toBool();

        (void)QtConcurrent::run([tool, args, cb, paper_run]() {
            auto& svc = mcp::McpService::instance();
            mcp::ToolResult result;

            // PAPER: the core tools refuse anything that would place, change or
            // cancel a real order or move money (no approval prompt is offered).
            std::optional<mcp::ScopedPaperRun> paper_guard;
            if (paper_run)
                paper_guard.emplace();

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
            QJsonValue out =
                result.data.isNull() || result.data.isUndefined() ? QJsonValue(result.to_json()) : result.data;
            cb(true, out, {});
            LOG_DEBUG("McpBridge", QString("Tool '%1' executed successfully").arg(tool));
        });
    };

    LOG_DEBUG("ServiceBridges", "MCP bridge wired");
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

    LOG_DEBUG("ServiceBridges", QString("All bridges wired (%1 pass-through fallbacks)").arg(wired_count));
}

} // namespace fincept::workflow
