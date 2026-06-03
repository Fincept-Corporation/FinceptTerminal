#include "services/workflow/adapters/ServiceBridges.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"
#include "network/http/HttpClient.h"
#include "python/PythonRunner.h"
#include "services/notifications/NotificationService.h"
#include "services/workflow/AuditLogger.h"
#include "services/workflow/ConfirmationService.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/RiskManager.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerRegistry.h"
#include "trading/ExchangeService.h"
#include "trading/PaperTrading.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedTrading.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPageSize>
#include <QPainter>
#include <QPrinter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QThread>
#include <QTimer>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrent>

using fincept::python::extract_json;
using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace fincept::workflow {

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
            QString symbol = base + "/" + quote; // Kraken format: BTC/USD

            (void)QtConcurrent::run([symbol, cb]() {
                auto& svc = trading::ExchangeService::instance();
                trading::TickerData t = svc.fetch_ticker(symbol);

                if (t.last <= 0.0) {
                    cb(false, {}, QString("No price data for %1").arg(symbol));
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
                out["exchange"] = "kraken";
                out["timestamp"] = static_cast<qint64>(t.timestamp);
                cb(true, out, {});
                LOG_DEBUG("MarketBridge", QString("Crypto price fetched: %1 = %2").arg(symbol).arg(t.last));
            });
        };
    }

    // market.get_news already has a real executor from MarketDataNodes.cpp.
    // Remaining market nodes (depth, stats, fundamentals, economics) are wired
    // in MarketDataNodes.cpp directly — no stub overwrite needed here.

    LOG_INFO("ServiceBridges", "Market data bridges wired");
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
    auto p = pt_create_portfolio("workflow_default", 1000000.0, "USD");
    return p.id;
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

// Map a product param string ("MIS"/"CNC"/"NRML" or "intraday"/"delivery"/...) to ProductType.
static fincept::trading::ProductType parse_product(const QString& p) {
    using namespace fincept::trading;
    QString s = p.toLower();
    if (s == "mis" || s == "intraday")
        return ProductType::Intraday;
    if (s == "nrml" || s == "margin")
        return ProductType::Margin;
    if (s == "mtf")
        return ProductType::MTF;
    return ProductType::Delivery; // "cnc" / "delivery" / default
}

void wire_trading_bridges(NodeRegistry& registry) {
    using namespace fincept::trading;

    // ── Place Order ───────────────────────────────────────────────
    auto* place_def = const_cast<NodeTypeDef*>(registry.find("trading.place_order"));
    if (place_def) {
        place_def->execute = [](const QJsonObject& params, const QVector<QJsonValue>&,
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
            QString validity = params.value("validity").toString("DAY");
            bool paper = (mode == "paper");

            AuditLogger::instance().log(AuditAction::OrderPlaced, {}, {}, symbol,
                                        QString("[%1] %2 %3 %4 x%5 via %6").arg(mode.toUpper(), product_str, side, symbol).arg(qty).arg(broker),
                                        params, paper);

            if (paper) {
                try {
                    QString portfolio_id = ensure_paper_portfolio();
                    std::optional<double> opt_price = price > 0 ? std::optional(price) : std::nullopt;
                    auto pt_order = pt_place_order(portfolio_id, symbol, side, order_type_str, qty, opt_price);

                    if (order_type_str == "market") {
                        double fill_price = price > 0 ? price : 100.0;
                        pt_fill_order(pt_order.id, fill_price);
                    }

                    QJsonObject out;
                    out["order_id"] = pt_order.id;
                    out["symbol"] = symbol;
                    out["side"] = side;
                    out["quantity"] = qty;
                    out["status"] = pt_order.status;
                    out["mode"] = "paper";
                    cb(true, out, {});
                } catch (const std::exception& ex) {
                    cb(false, {}, QString("Paper trading error: %1").arg(ex.what()));
                }
                return;
            }

            (void)QtConcurrent::run([cb, symbol, exchange, side, product_str, order_type_str, qty, price, validity, broker]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                UnifiedOrder order;
                order.symbol = symbol;
                order.exchange = exchange;
                order.side = (side == "sell") ? OrderSide::Sell : OrderSide::Buy;
                if (order_type_str == "limit") order.order_type = OrderType::Limit;
                else if (order_type_str == "stop") order.order_type = OrderType::StopLoss;
                else if (order_type_str == "stop_limit") order.order_type = OrderType::StopLossLimit;
                else order.order_type = OrderType::Market;
                order.quantity = qty;
                order.price = price;
                order.validity = validity;
                if (product_str == "intraday") order.product_type = ProductType::Intraday;
                else if (product_str == "margin") order.product_type = ProductType::Margin;
                else if (product_str == "mtf") order.product_type = ProductType::MTF;
                else order.product_type = ProductType::Delivery;

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

                // Poll order status to catch async rejections (TPIN, risk checks)
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
                pt_cancel_order(order_id);
                QJsonObject out;
                out["order_id"] = order_id;
                out["status"] = "cancelled";
                out["mode"] = "paper";
                cb(true, out, {});
                return;
            }

            (void)QtConcurrent::run([cb, order_id, broker]() {
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
                QJsonObject out;
                out["order_id"] = order_id;
                out["status"] = "modified";
                out["mode"] = "paper";
                if (new_qty > 0) out["new_quantity"] = new_qty;
                if (new_price > 0) out["new_price"] = new_price;
                cb(true, out, {});
                return;
            }

            (void)QtConcurrent::run([cb, order_id, broker, new_qty, new_price]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                QJsonObject mods;
                if (new_qty > 0) mods["quantity"] = new_qty;
                if (new_price > 0) mods["price"] = new_price;

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
                auto orders = pt_get_orders(portfolio_id, status_filter == "all" ? "" : status_filter);
                QJsonArray arr;
                for (const auto& o : orders) {
                    QJsonObject obj;
                    obj["order_id"] = o.id;
                    obj["symbol"] = o.symbol;
                    obj["side"] = o.side;
                    obj["order_type"] = o.order_type;
                    obj["quantity"] = o.quantity;
                    obj["filled_qty"] = o.filled_qty;
                    obj["status"] = o.status;
                    obj["mode"] = "paper";
                    if (o.price) obj["price"] = *o.price;
                    if (o.avg_price) obj["avg_price"] = *o.avg_price;
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
                cb(true, QJsonArray{}, {});
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
                QJsonObject out;
                out["available_balance"] = 1000000.0;
                out["used_margin"] = 0.0;
                out["total_balance"] = 1000000.0;
                out["broker"] = "paper";
                cb(true, out, {});
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
            QString product_str = params.value("product_type").toString("delivery");
            double qty = params.value("quantity").toDouble(0);

            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }

            if (mode == "paper") {
                QJsonObject out;
                out["symbol"] = symbol;
                out["status"] = "closed";
                out["mode"] = "paper";
                cb(true, out, {});
                return;
            }

            (void)QtConcurrent::run([cb, symbol, exchange, product_str, qty, broker]() {
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

                        // Get positions to find current quantity if qty==0
                        double close_qty = qty;
                        if (close_qty <= 0) {
                            auto pos_result = ibk->get_positions(creds);
                            if (pos_result.success) {
                                for (const auto& p : pos_result.data.value()) {
                                    if (p.symbol.contains(symbol, Qt::CaseInsensitive) && p.quantity > 0) {
                                        close_qty = p.quantity;
                                        break;
                                    }
                                }
                            }
                        }
                        if (close_qty <= 0) {
                            cb(false, {}, QString("No open position found for %1").arg(symbol));
                            return;
                        }

                        UnifiedOrder order;
                        order.symbol = symbol;
                        order.exchange = exchange;
                        order.side = OrderSide::Sell;
                        order.order_type = OrderType::Market;
                        order.quantity = close_qty;
                        order.validity = "DAY";
                        if (product_str == "intraday") order.product_type = ProductType::Intraday;
                        else if (product_str == "margin") order.product_type = ProductType::Margin;
                        else order.product_type = ProductType::Delivery;

                        // Risk-reducing op — always immediate, never gated for
                        // approval (see ActionCenter::immediate_ops).
                        auto response = UnifiedTrading::instance().place_order(account_id, order);
                        QJsonObject out;
                        out["symbol"] = symbol;
                        out["quantity_closed"] = close_qty;
                        out["order_id"] = response.order_id;
                        out["broker"] = broker;
                        out["status"] = response.success ? "closed" : "failed";
                        if (!response.success) out["error"] = response.message;
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
            QString side = params.value("side").toString("buy");
            double qty = params.value("quantity").toDouble(1);
            double entry = params.value("entry_price").toDouble(0);
            double sl = params.value("stop_loss").toDouble(0);
            double tp = params.value("take_profit").toDouble(0);

            if (symbol.isEmpty()) { cb(false, {}, "Symbol is required"); return; }
            if (sl <= 0) { cb(false, {}, "Stop loss is required for bracket orders"); return; }
            if (tp <= 0) { cb(false, {}, "Take profit is required for bracket orders"); return; }

            if (mode == "paper") {
                QJsonObject out;
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

            (void)QtConcurrent::run([cb, symbol, side, qty, entry, sl, tp, broker]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                UnifiedOrder order;
                order.symbol = symbol;
                order.side = (side == "sell") ? OrderSide::Sell : OrderSide::Buy;
                order.order_type = entry > 0 ? OrderType::Limit : OrderType::Market;
                order.quantity = qty;
                order.price = entry;
                order.stop_loss = sl;
                order.take_profit = tp;
                order.product_type = ProductType::BracketOrder;
                order.validity = "DAY";

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
                if (!response.success) out["error"] = response.message;
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
            QString product_str = params.value("product_type").toString("intraday");
            QString trail_type = params.value("trail_type").toString("percent");
            double trail_value = params.value("trail_value").toDouble(5.0);
            double qty = params.value("quantity").toDouble(0);

            if (symbol.isEmpty()) { cb(false, {}, "Symbol is required"); return; }

            if (mode == "paper") {
                QJsonObject out;
                out["order_id"] = "PAPER-TSL-" + QString::number(QDateTime::currentMSecsSinceEpoch());
                out["symbol"] = symbol;
                out["trail_type"] = trail_type;
                out["trail_value"] = trail_value;
                out["status"] = "active";
                out["mode"] = "paper";
                cb(true, out, {});
                return;
            }

            (void)QtConcurrent::run([cb, symbol, exchange, product_str, trail_type, trail_value, qty, broker]() {
                QString account_id = find_account_for_broker(broker);
                if (account_id.isEmpty()) {
                    cb(false, {}, QString("No account configured for broker: %1").arg(broker));
                    return;
                }

                UnifiedOrder order;
                order.symbol = symbol;
                order.exchange = exchange;
                order.side = OrderSide::Sell;
                order.order_type = OrderType::StopLoss;
                order.quantity = qty;
                order.stop_price = trail_value;
                order.validity = "DAY";
                if (product_str == "intraday") order.product_type = ProductType::Intraday;
                else if (product_str == "margin") order.product_type = ProductType::Margin;
                else order.product_type = ProductType::Delivery;

                // Protective stop — risk-reducing, always immediate, never gated
                // for approval (see ActionCenter::immediate_ops).
                auto response = UnifiedTrading::instance().place_order(account_id, order);
                QJsonObject out;
                out["order_id"] = response.order_id;
                out["symbol"] = symbol;
                out["trail_type"] = trail_type;
                out["trail_value"] = trail_value;
                out["broker"] = broker;
                out["status"] = response.success ? "active" : "failed";
                if (!response.success) out["error"] = response.message;
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

            if (symbol.isEmpty()) { cb(false, {}, "Symbol is required"); return; }
            if (tranches < 1) tranches = 1;

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
                cb(true, out, {});
                return;
            }

            (void)QtConcurrent::run([cb, symbol, exchange, product_str, side, per_tranche, remainder, tranches, interval_sec, broker]() {
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
                    if (product_str == "intraday") gate_order.product_type = ProductType::Intraday;
                    else if (product_str == "margin") gate_order.product_type = ProductType::Margin;
                    else gate_order.product_type = ProductType::Delivery;
                    if (gate_or_queue(account_id, gate_order, cb))
                        return;
                }

                QJsonArray results;
                for (int i = 0; i < tranches; ++i) {
                    if (i > 0)
                        QThread::sleep(interval_sec);

                    double q = per_tranche + (i == tranches - 1 ? remainder : 0);
                    UnifiedOrder order;
                    order.symbol = symbol;
                    order.exchange = exchange;
                    order.side = (side == "sell") ? OrderSide::Sell : OrderSide::Buy;
                    order.order_type = OrderType::Market;
                    order.quantity = q;
                    order.validity = "DAY";
                    if (product_str == "intraday") order.product_type = ProductType::Intraday;
                    else if (product_str == "margin") order.product_type = ProductType::Margin;
                    else order.product_type = ProductType::Delivery;

                    auto response = UnifiedTrading::instance().place_order(account_id, order);
                    QJsonObject r;
                    r["tranche"] = i + 1;
                    r["quantity"] = q;
                    r["order_id"] = response.order_id;
                    r["status"] = response.success ? "filled" : "failed";
                    if (!response.success) r["error"] = response.message;
                    results.append(r);

                    if (!response.success) break;
                }

                QJsonObject out;
                out["symbol"] = symbol;
                out["side"] = side;
                out["total_quantity"] = per_tranche * tranches + remainder;
                out["tranches_executed"] = results.size();
                out["orders"] = results;
                out["broker"] = broker;
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
            if (symbol.isEmpty()) {
                cb(false, {}, "Symbol is required");
                return;
            }

            (void)QtConcurrent::run([cb, account_id_param, broker, symbol, product]() {
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

                for (const auto& p : result.data.value()) {
                    if (!p.symbol.contains(symbol, Qt::CaseInsensitive))
                        continue;
                    if (!product.isEmpty() && !p.product_type.contains(product, Qt::CaseInsensitive))
                        continue;
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

                // No matching position — return a flat (zero) position rather than error.
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

            (void)QtConcurrent::run([cb, account_id_param, broker, symbol, exchange, position_size, order_type_str,
                                     price, product]() {
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

            (void)QtConcurrent::run([cb, account_id_param, broker]() {
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

            (void)QtConcurrent::run([cb, account_id_param, broker]() {
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

    LOG_INFO("ServiceBridges", "Trading bridges wired (17 nodes)");
}

// ── Agent Bridge ───────────────────────────────────────────────────────

void wire_agent_bridges(NodeRegistry& /*registry*/) {
    // agent.run and agent.tool_picker have real executors from AgentNodes.cpp.
    // agent.single/agent.multi/agent.mediator are legacy stubs — leave their
    // executors to the fallback pass-through in wire_all_bridges if still null.
    LOG_INFO("ServiceBridges", "Agent bridges wired");
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

            auto& http = HttpClient::instance();
            if (method == "POST") {
                QJsonObject body = params.value("body").toObject();
                if (body.isEmpty() && !inputs.isEmpty() && inputs[0].isObject())
                    body = inputs[0].toObject();
                http.post(url, body, [cb, url](Result<QJsonDocument> result) {
                    if (result.is_err()) {
                        cb(false, {}, QString::fromStdString(result.error()));
                        return;
                    }
                    QJsonObject out;
                    out["url"] = url;
                    out["status_code"] = 200;
                    out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                            : QJsonValue(result.value().array());
                    cb(true, out, {});
                });
            } else {
                http.get(url, [cb, url](Result<QJsonDocument> result) {
                    if (result.is_err()) {
                        cb(false, {}, QString::fromStdString(result.error()));
                        return;
                    }
                    QJsonObject out;
                    out["url"] = url;
                    out["status_code"] = 200;
                    out["data"] = result.value().isObject() ? QJsonValue(result.value().object())
                                                            : QJsonValue(result.value().array());
                    cb(true, out, {});
                });
            }
            LOG_DEBUG("UtilityBridge", QString("HTTP %1 %2").arg(method, url));
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

            // Inject upstream input as a variable the script can access
            if (!inputs.isEmpty() && !inputs[0].isNull()) {
                QString input_json =
                    QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                         : QJsonDocument(inputs[0].toArray()))
                                          .toJson(QJsonDocument::Compact));
                code = QString("import json\n_input = json.loads('%1')\n").arg(input_json.replace("'", "\\'")) + code;
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
                file.write(content.toUtf8());
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

            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                cb(false, {}, QString("Cannot open: %1").arg(file.errorString()));
                return;
            }

            if (format == "json") {
                QJsonDocument doc =
                    inputs[0].isObject() ? QJsonDocument(inputs[0].toObject()) : QJsonDocument(inputs[0].toArray());
                file.write(doc.toJson(QJsonDocument::Indented));
            } else if (format == "csv") {
                // Convert JSON array of objects to CSV
                QString csv;
                if (inputs[0].isArray()) {
                    QJsonArray arr = inputs[0].toArray();
                    if (!arr.isEmpty() && arr[0].isObject()) {
                        QStringList headers = arr[0].toObject().keys();
                        csv += headers.join(",") + "\n";
                        for (const QJsonValue& row : arr) {
                            QStringList vals;
                            for (const QString& h : headers) {
                                QJsonValue v = row.toObject().value(h);
                                QString s = v.isString() ? "\"" + v.toString().replace("\"", "\"\"") + "\""
                                                         : QString::number(v.toDouble());
                                vals << s;
                            }
                            csv += vals.join(",") + "\n";
                        }
                    }
                }
                file.write(csv.toUtf8());
            } else {
                file.write(QString::fromUtf8(QJsonDocument(inputs[0].isObject() ? QJsonDocument(inputs[0].toObject())
                                                                                : QJsonDocument(inputs[0].toArray()))
                                                 .toJson(QJsonDocument::Compact))
                               .toUtf8());
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
                file.write(bytes);
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
                out_file.write(decompressed);

                QJsonObject out;
                out["input_path"] = path;
                out["output_path"] = out_path;
                out["compressed_size"] = data.size();
                out["original_size"] = decompressed.size();
                out["operation"] = "decompress";
                cb(true, out, {});
            } else {
                QByteArray compressed = qCompress(data);
                QString out_path = path + ".qz";
                QFile out_file(out_path);
                if (!out_file.open(QIODevice::WriteOnly)) {
                    cb(false, {}, "Cannot write compressed file: " + out_file.errorString());
                    return;
                }
                out_file.write(compressed);

                QJsonObject out;
                out["input_path"] = path;
                out["output_path"] = out_path;
                out["original_size"] = data.size();
                out["compressed_size"] = compressed.size();
                out["ratio"] = data.size() > 0 ? static_cast<double>(compressed.size()) / data.size() : 0.0;
                out["operation"] = "compress";
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
            QString attr = params.value("attribute").toString("text");

            // Basic tag extraction — match tags by element name from selector
            // e.g. selector "div" extracts all <div>...</div> content
            QString tag_name = selector.section('.', 0, 0).section('#', 0, 0).trimmed();
            if (tag_name.isEmpty())
                tag_name = "div";

            QRegularExpression re(QString("<%1[^>]*>(.*?)</%1>").arg(QRegularExpression::escape(tag_name)),
                                  QRegularExpression::DotMatchesEverythingOption);
            auto it = re.globalMatch(html);
            QJsonArray results;
            while (it.hasNext()) {
                auto m = it.next();
                results.append(m.captured(1).trimmed());
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
                            while (!(xml.isEndElement() && (xml.name() == u"item" || xml.name() == u"entry"))) {
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

            QSqlDatabase db;
            QString conn_name;
            if (db_source == "custom") {
                QString db_path;
                if (!inputs.isEmpty() && inputs[0].isObject())
                    db_path = inputs[0].toObject().value("database_path").toString();
                if (db_path.isEmpty())
                    db_path = params.value("query").toString(); // fallback: maybe path in query for raw ops
                if (db_path.isEmpty() || !QFile::exists(db_path)) {
                    cb(false, {}, "Custom database path is required (pass {\"database_path\": \"...\"} as input)");
                    return;
                }
                conn_name = "workflow_custom_" + QString::number(qHash(db_path));
                if (QSqlDatabase::contains(conn_name)) {
                    db = QSqlDatabase::database(conn_name);
                } else {
                    db = QSqlDatabase::addDatabase("QSQLITE", conn_name);
                    db.setDatabaseName(db_path);
                }
                if (!db.isOpen() && !db.open()) {
                    cb(false, {}, "Cannot open database: " + db.lastError().text());
                    return;
                }
            } else {
                db = QSqlDatabase::database();
                if (!db.isOpen()) {
                    cb(false, {}, "No database connection available");
                    return;
                }
            }

            QSqlQuery q(db);

            if (op == "insert" || op == "update" || op == "delete") {
                if (!q.exec(query)) {
                    cb(false, {}, "SQL error: " + q.lastError().text());
                    return;
                }
                QJsonObject out;
                out["query"] = query;
                out["operation"] = op;
                out["rows_affected"] = q.numRowsAffected();
                cb(true, out, {});
                return;
            }

            if (op == "raw") {
                if (!q.exec(query)) {
                    cb(false, {}, "SQL error: " + q.lastError().text());
                    return;
                }
                QJsonObject out;
                out["query"] = query;
                out["operation"] = "raw";
                out["success"] = true;
                out["rows_affected"] = q.numRowsAffected();
                cb(true, out, {});
                return;
            }

            // select (default)
            if (!q.exec(query)) {
                cb(false, {}, "SQL error: " + q.lastError().text());
                return;
            }

            QJsonArray rows;
            QSqlRecord rec = q.record();
            while (q.next()) {
                QJsonObject row;
                for (int i = 0; i < rec.count(); ++i) {
                    QVariant val = q.value(i);
                    if (val.typeId() == QMetaType::Double || val.typeId() == QMetaType::Int ||
                        val.typeId() == QMetaType::LongLong)
                        row[rec.fieldName(i)] = val.toDouble();
                    else if (val.typeId() == QMetaType::Bool)
                        row[rec.fieldName(i)] = val.toBool();
                    else if (val.isNull())
                        row[rec.fieldName(i)] = QJsonValue::Null;
                    else
                        row[rec.fieldName(i)] = val.toString();
                }
                rows.append(row);
            }

            QJsonObject out;
            out["query"] = query;
            out["operation"] = op;
            out["row_count"] = rows.size();
            out["rows"] = rows;
            cb(true, out, {});
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
                        QTimer::singleShot(1000 * (3 - state->retries_left), [state]() {
                            execute_request(state);
                        });
                        return;
                    }
                    state->cb(false, {},
                              QString("HTTP %1 failed (%2): %3").arg(state->method).arg(status).arg(reply->errorString()));
                    return;
                }

                QByteArray data = reply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);

                QJsonObject out;
                out["url"] = state->url;
                out["method"] = state->method;
                out["status_code"] = status;
                if (!doc.isNull())
                    out["data"] = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
                else
                    out["data"] = QString::fromUtf8(data);

                // Include response headers
                QJsonObject resp_headers;
                for (const auto& pair : reply->rawHeaderPairs())
                    resp_headers[QString::fromUtf8(pair.first)] = QString::fromUtf8(pair.second);
                out["headers"] = resp_headers;

                state->cb(true, out, {});
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
                    QString val = it.value().isString() ? it.value().toString()
                                                        : QString::number(it.value().toDouble(), 'f', 4);
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
            QString title = params.value("title").toString("");
            int width = static_cast<int>(params.value("width").toDouble(800));
            int height = static_cast<int>(params.value("height").toDouble(600));
            QString path = params.value("path").toString("output/chart.png");
            if (path.isEmpty()) {
                cb(false, {}, "Output path is required");
                return;
            }

            QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            QString data_json = QString::fromUtf8(
                data.isObject() ? QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact)
                                : QJsonDocument(data.toArray()).toJson(QJsonDocument::Compact));

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

data = json.loads(r'''%1''')
chart_type = '%2'
title = '%3'
width = %4
height = %5
path = r'%6'

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
            ax.pie(vals, labels=labels, autopct='%%1.1f%%%%', textprops={'color': '#d0d0d0', 'fontsize': 8})
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
                                        .arg(data_json.replace("\\", "\\\\").replace("'", "\\'"))
                                        .arg(chart_type)
                                        .arg(title.replace("'", "\\'"))
                                        .arg(width)
                                        .arg(height)
                                        .arg(path.replace("\\", "/").replace("'", "\\'"));

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

    LOG_INFO("ServiceBridges", "Utility bridges wired");
}

static void wire_mcp_bridges(NodeRegistry& registry) {
    auto* def = const_cast<NodeTypeDef*>(registry.find("mcp.tool_call"));
    if (!def)
        return;

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
            QJsonValue out =
                result.data.isNull() || result.data.isUndefined() ? QJsonValue(result.to_json()) : result.data;
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
