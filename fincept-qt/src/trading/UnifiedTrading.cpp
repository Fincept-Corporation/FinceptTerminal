// Unified Trading — routes orders to live broker or paper trading engine

#include "trading/UnifiedTrading.h"

#include "core/logging/Logger.h"
#include "services/workflow/RiskManager.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderMatcher.h"
#include "trading/OrderThreadPool.h"
#include "trading/OrderValidator.h"
#include "trading/PaperTrading.h"
#include "trading/RateLimiter.h"
#include "trading/SmartOrderEngine.h"
#include "trading/StrategyPortfolio.h"
#include "trading/TradingEvents.h"
#include "trading/brokers/BrokerClientOrderId.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QMutexLocker>
#include <QPointer>
#include <QSqlQuery>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace fincept::trading {

namespace {
constexpr const char* kUtLog = "UnifiedTrading";

/// Risk-limit gate shared by every UnifiedTrading path that can reach a broker.
/// Returns an empty string when the order is allowed, otherwise the reason it was
/// blocked.
///
/// Every RiskLimits field defaults to "unlimited" (0) with short selling allowed,
/// so an unconfigured install behaves exactly as it did before this gate existed;
/// only an operator who has set a limit can be rejected by it.
///
/// Fails CLOSED: if the check itself throws, the order is refused. A risk gate
/// that could not be evaluated must never read as a pass.
QString risk_block_reason(const QString& symbol, OrderSide side, double quantity, double price, bool paper) {
    try {
        QString reason;
        if (workflow::RiskManager::instance().is_order_allowed(symbol, QString::fromLatin1(order_side_str(side)),
                                                               quantity, price, paper, &reason))
            return {};
        return reason.isEmpty() ? QStringLiteral("Blocked by risk limits") : reason;
    } catch (const std::exception& e) {
        return QStringLiteral("Risk check failed: ") + QString::fromUtf8(e.what());
    } catch (...) {
        return QStringLiteral("Risk check failed (unknown error)");
    }
}

/// Broker order-rate gate, applied immediately before a live broker call.
/// OrderRateLimiter::acquire() sleeps (QThread::msleep) to space orders out, so it
/// must NEVER run on the GUI thread (P1). The batch paths that actually need it —
/// basket, split, broadcast, algo deployment — all run under QtConcurrent::run.
/// The two known main-thread callers are a single user-initiated approval
/// (ActionCenter) and the MCP tool handler; there we skip the throttle rather than
/// freeze the UI, which is safe because those are one order at a time.
void rate_limit_broker(const QString& broker_id) {
    const auto id = parse_broker_id(broker_id);
    if (!id)
        return; // unknown broker string — nothing registered to throttle
    auto* app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread()) {
        LOG_DEBUG(kUtLog, QString("Order rate-limit skipped for %1 — call is on the GUI thread").arg(broker_id));
        return;
    }
    OrderRateLimiter::instance().acquire(*id);
}

/// Stamp an order's idempotency reference exactly ONCE, at the point it enters
/// the routing layer. A non-empty value is never overwritten, so re-submitting
/// the SAME UnifiedOrder after BrokerHttp's 8 s client-side timeout reaches the
/// broker with the reference it already used — the broker rejects the retry as a
/// duplicate instead of opening a second live position. Minting inside each
/// broker (what make_client_order_ref() does on its own) cannot achieve that,
/// because it produces a fresh value on every attempt.
void stamp_client_order_id(UnifiedOrder& order) {
    if (order.client_order_id.isEmpty())
        order.client_order_id = make_client_order_ref(20);
}

/// Fill the caller-owned identity fields of a modify request from the RESTING
/// order, then return an empty string. On failure, returns the reason the modify
/// must be refused.
///
/// Why this exists: Kotak and Tradejini implement modify as a FULL REPLACE —
/// every field they transmit overwrites the resting order. They used to default
/// the missing ones, and the worst of those defaults was `side` ("B" / "buy"):
/// re-pricing a resting SELL or stop-loss rewrote it as a BUY, turning an exit
/// into a new long. Both now hard-fail instead of guessing. Every caller in the
/// terminal (order blotter, workflow node) sends only {quantity, price}, so
/// without this hydration those two brokers can no longer modify anything.
///
/// Caller-supplied values ALWAYS win — this only fills gaps.
///
/// The same concept is read under different key names across the 22 brokers, so
/// each value is written under every spelling some broker reads. Brokers ignore
/// keys they don't read, so the duplicate spellings are inert:
///   side       -> "side"                              (kotak, tradejini)
///   symbol     -> "symbol"                            (flattrade, kotak, shoonya, tradejini)
///   exchange   -> "exchange", "exchange_code"         (flattrade/groww/kotak/shoonya, icicidirect)
///   product    -> "product", "productType"            (kotak, iifl)
///   order type -> "order_type", "orderType", "type"   (see the three families below)
///
/// Deliberately NOT written:
///   "variety" — it looks like a product slot but is the Kite / AngelOne order
///     VARIETY (regular|amo|co, NORMAL|STOPLOSS|AMO). Zerodha puts it in the
///     request path, so writing a product there would issue
///     PUT /orders/MIS/<id>; AngelOne already recovers the real variety from its
///     own order book.
///   "segment" — Groww's CASH/FNO segment, not an exchange. Groww already
///     derives it from "exchange" when absent, which this does supply.
QString hydrate_modify_fields(IBroker& broker, const BrokerCredentials& creds, const QString& order_id,
                              QJsonObject& mods) {
    // Every key some broker reads for one of the five hydratable concepts.
    static const char* const kHydratedKeys[] = {"side",    "symbol",      "exchange",   "exchange_code",
                                                "product", "productType", "order_type", "orderType",
                                                "type"};
    auto blank = [&mods](const char* key) {
        return mods.value(QString::fromLatin1(key)).toString().trimmed().isEmpty();
    };

    // If the caller already supplied everything, skip the round trip entirely.
    bool needs_fill = false;
    for (const char* k : kHydratedKeys) {
        if (blank(k)) {
            needs_fill = true;
            break;
        }
    }
    if (!needs_fill)
        return {};

    const QString broker_name = QString::fromLatin1(broker.name());
    auto resp = broker.get_orders(creds);
    if (!resp.success || !resp.data.has_value()) {
        // Refuse rather than dispatch a half-populated map — a partially
        // populated full-replace modify is the exact failure mode this guards.
        return QString("Cannot modify order %1: %2's order book could not be read (%3)")
            .arg(order_id, broker_name, resp.error.isEmpty() ? QStringLiteral("no detail") : resp.error);
    }

    const BrokerOrderInfo* resting = nullptr;
    for (const auto& o : *resp.data) {
        if (o.order_id == order_id || (!o.exchange_order_id.isEmpty() && o.exchange_order_id == order_id)) {
            resting = &o;
            break;
        }
    }
    if (!resting) {
        return QString("Cannot modify order %1: it is not in %2's order book (already filled or cancelled?)")
            .arg(order_id, broker_name);
    }

    auto fill = [&mods, &blank](const char* key, const QString& value) {
        if (!value.trimmed().isEmpty() && blank(key))
            mods[QString::fromLatin1(key)] = value;
    };
    fill("side", resting->side);
    fill("symbol", resting->symbol);
    fill("exchange", resting->exchange);
    fill("exchange_code", resting->exchange);
    fill("product", resting->product_type);
    fill("productType", resting->product_type);
    fill("order_type", resting->order_type);
    fill("orderType", resting->order_type);
    fill("type", resting->order_type);

    LOG_DEBUG(kUtLog, QString("Modify %1 on %2 hydrated from the resting order: %3 %4 %5 %6/%7")
                          .arg(order_id, broker_name, resting->side, resting->symbol, resting->exchange,
                               resting->product_type, resting->order_type));
    return {};
}

/// True when `account` is in PAPER mode AND that PAPER mode runs on the broker's own paper
/// venue (Alpaca paper-api) instead of the local pt_* simulator: the broker offers native
/// paper and the account's stored credentials are positively that venue's paper environment
/// (IBroker::is_paper_environment).
///
/// Fails CLOSED. BrokerProfile::has_native_paper only says the broker OFFERS paper; the
/// credentials may be live ones (a new account defaults to PAPER mode whatever keys it
/// holds), and sending those a "paper" ticket would place a real order. Anything uncertain
/// — live keys, unidentifiable environment, a broker that has not opted in — answers false,
/// which keeps the local simulator, exactly as before this existed.
bool native_paper_venue(const BrokerAccount& account) {
    if (account.trading_mode != "paper")
        return false;
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker || !broker->profile().has_native_paper)
        return false; // cheap exit before touching secure storage
    const auto creds = AccountManager::instance().load_credentials(account.account_id);
    return !creds.api_key.isEmpty() && broker->is_paper_environment(creds);
}

/// PAPER mode served by the local pt_* simulator. Everything else — LIVE, or PAPER on a
/// native broker paper venue — goes through the broker adapter.
bool local_paper(const BrokerAccount& account) {
    return account.trading_mode == "paper" && !native_paper_venue(account);
}

/// Guard for every broker call made on behalf of a PAPER-mode account. native_paper_venue()
/// already decided this, but credentials can change between that decision and the call, and
/// a PAPER ticket must never reach a live exchange — so re-verify on the exact credentials
/// about to be used. Returns an empty string when the call may proceed.
QString paper_venue_refusal(const BrokerAccount& account, const IBroker& broker, const BrokerCredentials& creds) {
    if (account.trading_mode != "paper")
        return {};
    if (!creds.api_key.isEmpty() && broker.is_paper_environment(creds))
        return {};
    return QStringLiteral("Refusing to send a PAPER order: this account's credentials are not verified as the "
                          "broker's paper environment");
}

/// Mode tag for a response that went through the broker adapter: "paper" for a PAPER account
/// on the broker's paper venue (the UI mode is preserved), "live" otherwise.
QString broker_mode_tag(const BrokerAccount& account) {
    return account.trading_mode == "paper" ? QStringLiteral("paper") : QStringLiteral("live");
}
} // namespace

UnifiedTrading& UnifiedTrading::instance() {
    static UnifiedTrading ut;
    return ut;
}

// ============================================================================
// Session Management
// ============================================================================

TradingSession UnifiedTrading::init_session(const QString& broker, const QString& mode,
                                            const QString& paper_portfolio_id) {
    QMutexLocker lock(&mutex_);

    TradingSession session;
    session.broker = broker;
    session.mode = (mode == "live") ? "live" : "paper";
    session.is_connected = false;

    if (session.mode == "paper") {
        if (!paper_portfolio_id.isEmpty()) {
            session.paper_portfolio_id = paper_portfolio_id;
        } else {
            auto portfolio = pt_create_portfolio(broker + " Paper Trading", 1000000.0, "INR", 1.0, "cross", 0.0003);
            session.paper_portfolio_id = portfolio.id;
        }
    }

    session_ = session;
    return session;
}

std::optional<TradingSession> UnifiedTrading::get_session() const {
    QMutexLocker lock(&mutex_);
    return session_;
}

TradingSession UnifiedTrading::switch_mode(const QString& mode) {
    QMutexLocker lock(&mutex_);

    if (!session_) {
        session_ = TradingSession{};
        session_->broker = "fyers";
    }

    session_->mode = (mode == "live") ? "live" : "paper";

    if (session_->mode == "paper" && session_->paper_portfolio_id.isEmpty()) {
        auto portfolio =
            pt_create_portfolio(session_->broker + " Paper Trading", 1000000.0, "INR", 1.0, "cross", 0.0003);
        session_->paper_portfolio_id = portfolio.id;
    }

    return *session_;
}

// ============================================================================
// Order Routing
// ============================================================================

UnifiedOrderResponse UnifiedTrading::place_order(const UnifiedOrder& order) {
    QMutexLocker lock(&mutex_);

    if (!session_) {
        return {false, "", "No active trading session. Call init_session first.", ""};
    }

    UnifiedOrder routed = order;
    stamp_client_order_id(routed);

    if (session_->mode == "paper") {
        return place_paper_order(*session_, routed);
    }
    return place_live_order(*session_, routed);
}

UnifiedOrderResponse UnifiedTrading::place_paper_order(const TradingSession& session, const UnifiedOrder& order) {
    if (session.paper_portfolio_id.isEmpty()) {
        return {false, "", "No paper portfolio configured", "paper"};
    }

    QString symbol = order.exchange.isEmpty() ? order.symbol : order.exchange + ":" + order.symbol;

    QString side_str = order_side_str(order.side);
    QString type_str = order_type_str(order.order_type);

    // Fill/reference price = caller-supplied LTP (order.price). The old code used
    // a hardcoded 1000.0 for market orders, fabricating P&L. Reject a market fill
    // with no usable price instead, matching the account-based place_order path.
    std::optional<double> price_opt;
    if (order.price > 0)
        price_opt = order.price;

    std::optional<double> stop_opt;
    if (order.stop_price > 0)
        stop_opt = order.stop_price;

    if (order.order_type == OrderType::Market && !price_opt)
        return {false, "", "No live price yet for a market fill — wait for quotes to load", "paper"};

    try {
        auto paper_order = pt_place_order(session.paper_portfolio_id, symbol, side_str, type_str, order.quantity,
                                          price_opt, stop_opt, false);

        if (order.order_type == OrderType::Market)
            pt_fill_order(paper_order.id, *price_opt);

        return {true, paper_order.id, "Paper order placed", "paper"};
    } catch (const std::exception& e) {
        return {false, "", QString("Paper order failed: %1").arg(e.what()), "paper"};
    }
}

UnifiedOrderResponse UnifiedTrading::place_live_order(const TradingSession& session, const UnifiedOrder& order) {
    auto* broker = BrokerRegistry::instance().get(session.broker);
    if (!broker) {
        return {false, "", "Broker not found: " + session.broker, "live"};
    }

    auto creds = broker->load_credentials();
    if (creds.access_token.isEmpty()) {
        return {false, "", "No credentials for " + session.broker + ". Please authenticate.", "live"};
    }

    // Legacy session path bypasses the account-aware place_order, so it carries
    // its own risk gate — this reaches a real broker.
    const QString blocked = risk_block_reason(order.symbol, order.side, order.quantity, order.price, /*paper=*/false);
    if (!blocked.isEmpty()) {
        LOG_INFO(kUtLog, QString("Risk limit rejected %1 %2 x%3 on %4: %5")
                             .arg(QString::fromLatin1(order_side_str(order.side)), order.symbol)
                             .arg(order.quantity)
                             .arg(session.broker, blocked));
        return {false, "", "Risk check failed: " + blocked, "live"};
    }
    // No account id on the legacy path: user limits + sanity check without the
    // per-account snapshot (unknown P&L fails a configured daily-loss limit closed).
    if (const auto gate = OrderValidator::pre_trade_risk(QString(), order, /*broker_routed=*/true); !gate.valid)
        return {false, "", "Risk check failed: " + gate.errors.join("; "), "live"};

    rate_limit_broker(session.broker);
    auto result = broker->place_order(creds, order);
    return {result.success, result.order_id, result.error, "live"};
}

UnifiedOrderResponse UnifiedTrading::cancel_order(const QString& order_id) {
    QMutexLocker lock(&mutex_);

    if (!session_) {
        return {false, "", "No active trading session", ""};
    }

    if (session_->mode == "paper") {
        try {
            pt_cancel_order(order_id);
            return {true, order_id, "Paper order cancelled", "paper"};
        } catch (const std::exception& e) {
            return {false, "", QString("Cancel failed: %1").arg(e.what()), "paper"};
        }
    }

    auto* broker = BrokerRegistry::instance().get(session_->broker);
    if (!broker)
        return {false, "", "Broker not found", "live"};

    auto creds = broker->load_credentials();
    rate_limit_broker(session_->broker);
    auto result = broker->cancel_order(creds, order_id);
    return {result.success, order_id, result.error, "live"};
}

// ============================================================================
// Account-Aware Order Routing (new multi-account API)
// ============================================================================

bool UnifiedTrading::uses_native_paper(const QString& account_id) const {
    return native_paper_venue(AccountManager::instance().get_account(account_id));
}

UnifiedOrderResponse UnifiedTrading::place_order(const QString& account_id, const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, "", "Account not found: " + account_id, ""};

    // Pre-flight validation (Phase 3 §9).
    auto vr = OrderValidator::validate(order);
    if (!vr.valid) {
        const QString err = vr.errors.join("; ");
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, err, account.trading_mode});
        return {false, "", "Validation failed: " + err, account.trading_mode};
    }

    // Risk limits. This is the terminal's single order choke point — every
    // broadcast, approval and workflow order funnels through here — so the gate
    // lives here rather than in each caller. Defaults are wide open (see
    // RiskLimits), so nothing that works today starts failing; a rejection only
    // happens when an operator has actually configured a limit, and is logged so
    // they can see which one bit.
    const QString risk_err =
        risk_block_reason(order.symbol, order.side, order.quantity, order.price, account.trading_mode == "paper");
    if (!risk_err.isEmpty()) {
        LOG_INFO(kUtLog, QString("Risk limit rejected %1 %2 x%3 on account %4: %5")
                             .arg(QString::fromLatin1(order_side_str(order.side)), order.symbol)
                             .arg(order.quantity)
                             .arg(account_id, risk_err));
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, risk_err, account.trading_mode});
        return {false, "", "Risk check failed: " + risk_err, account.trading_mode};
    }

    // Settings-driven pre-trade gate (OrderValidator): user risk limits + the
    // absurd-order sanity check. Same choke point, so it covers the order ticket,
    // broadcasts, approvals, MCP tools, workflows, algos and the F&O screen.
    const bool is_local_paper = local_paper(account);
    const auto gate = OrderValidator::pre_trade_risk(account_id, order, /*broker_routed=*/!is_local_paper);
    if (!gate.valid) {
        const QString err = gate.errors.join("; ");
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, err, account.trading_mode});
        return {false, "", "Risk check failed: " + err, account.trading_mode};
    }

    // Quantity freeze check (Phase 3 §17). Exchanges cap the max quantity per
    // single order (e.g. NSE NIFTY futures = 1800). place_order is synchronous;
    // an auto-split is inherently async (place_split_orders runs on a worker
    // thread with a callback). To keep place_order's sync contract, we REJECT
    // over-limit orders here with an actionable message rather than silently
    // turning into an async split. Callers that want the split should use
    // place_order_auto_split() instead.
    const int freeze = qty_freeze_limit(order.symbol, order.exchange);
    if (freeze > 0 && order.quantity > freeze) {
        const QString err = QString("Quantity %1 exceeds freeze limit %2 for %3 on %4 — "
                                    "use Split Order (split size %2) or place_order_auto_split().")
                                .arg(static_cast<qint64>(order.quantity))
                                .arg(freeze)
                                .arg(order.symbol, order.exchange);
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, err, account.trading_mode});
        return {false, "", err, account.trading_mode};
    }

    // Stamp the idempotency reference here — this is the terminal's single order
    // choke point, so every routed order carries one exactly once. Re-submitting
    // the same UnifiedOrder (a retry after an 8 s BrokerHttp timeout) keeps the
    // original reference; a freshly built order gets a new one.
    UnifiedOrder routed = order;
    stamp_client_order_id(routed);

    // PAPER mode normally means the local simulator; for a broker with a verified native
    // paper venue it means that venue, reached through the same adapter as a live order
    // (the mode tag stays "paper"). See native_paper_venue().
    UnifiedOrderResponse resp = is_local_paper ? place_paper_order_for_account(account_id, routed)
                                               : place_live_order_for_account(account_id, routed);

    if (resp.success) {
        publish(OrderPlacedEvent{account_id, resp.order_id, order.symbol, order.exchange, order.side, order.quantity,
                                 "PLACE", account.trading_mode});
    } else {
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, resp.message, account.trading_mode});
    }
    return resp;
}

UnifiedOrderResponse UnifiedTrading::cancel_order(const QString& account_id, const QString& order_id) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, "", "Account not found: " + account_id, ""};

    if (local_paper(account)) {
        try {
            pt_cancel_order(order_id);
            return {true, order_id, "Paper order cancelled", "paper"};
        } catch (const std::exception& e) {
            return {false, "", QString("Cancel failed: %1").arg(e.what()), "paper"};
        }
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, "", "Broker not found: " + account.broker_id, broker_mode_tag(account)};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return {false, order_id, refusal, "paper"};
    rate_limit_broker(account.broker_id);
    auto result = broker->cancel_order(creds, order_id);
    return {result.success, order_id, result.error, broker_mode_tag(account)};
}

UnifiedOrderResponse UnifiedTrading::modify_order(const QString& account_id, const QString& order_id,
                                                  const QJsonObject& modifications) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, "", "Account not found: " + account_id, ""};

    if (local_paper(account))
        return {false, "", "Modify not supported for paper orders", "paper"};

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, "", "Broker not found: " + account.broker_id, broker_mode_tag(account)};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return {false, order_id, refusal, "paper"};

    // Hydrate side/symbol/exchange/product/order_type from the resting order
    // before dispatching. Several brokers replace the whole order on modify, so
    // a map carrying only {quantity, price} either gets rejected (Kotak,
    // Tradejini) or silently rewrites the fields the caller never touched
    // (IIFL's MIS/MARKET defaults, AliceBlue's LIMIT default).
    //
    // This costs one extra broker round trip per modify. Both call sites
    // (EquityTradingScreen::async_modify_order, the workflow modify-order node)
    // already run under QtConcurrent::run, so it never blocks the GUI thread.
    QJsonObject mods = modifications;
    const QString hydrate_err = hydrate_modify_fields(*broker, creds, order_id, mods);
    if (!hydrate_err.isEmpty()) {
        LOG_ERROR(kUtLog, QString("Modify refused on account %1: %2").arg(account_id, hydrate_err));
        return {false, order_id, hydrate_err, broker_mode_tag(account)};
    }

    rate_limit_broker(account.broker_id);
    auto result = broker->modify_order(creds, order_id, mods);
    return {result.success, order_id, result.error, broker_mode_tag(account)};
}

UnifiedOrderResponse UnifiedTrading::place_paper_order_for_account(const QString& account_id,
                                                                   const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.paper_portfolio_id.isEmpty())
        return {false, "", "No paper portfolio for this account", "paper"};

    // Store the BARE symbol (no exchange prefix). Positions mark to market against
    // the live quote feed published under the bare symbol (broker:<id>:<acct>:quote:
    // <bare>), and this matches the Equity screen's convention. Re-prefixing here as
    // "NFO:NIFTY…" was why F&O positions never marked (the prefix never matched a
    // quote topic) and showed a frozen/garbage price.
    const QString symbol = order.symbol;
    const QString side_str = order_side_str(order.side);
    const QString type_str = order_type_str(order.order_type);

    // Forward the broker product (MIS/CNC/NRML) and exchange so the engine applies
    // per-product leverage and TAGS the position with its real product. Without it
    // the product defaulted to CNC (delivery), which made square-off-all skip the
    // position entirely (it filters to intraday) — the "doesn't exit" bug.
    const QString product = product_to_broker_str(order.product_type);

    // Resolve the order's reference / fill price. Limit & stop-limit carry their own
    // trigger; market and plain-stop fill at the caller-supplied live price
    // (order.price = LTP at click time). A market order with no usable price is
    // rejected with a clear message rather than the old hardcoded 1000.0 sentinel.
    std::optional<double> price_opt;
    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        if (order.price > 0)
            price_opt = order.price;
    } else if (order.price > 0) {
        price_opt = order.price;
    }
    std::optional<double> stop_opt;
    if (order.stop_price > 0)
        stop_opt = order.stop_price;

    const bool is_market = (order.order_type == OrderType::Market);
    if (is_market && !price_opt)
        return {false, "", "No live price yet for a market fill — wait for quotes to load", "paper"};

    try {
        auto paper_order = pt_place_order(account.paper_portfolio_id, symbol, side_str, type_str, order.quantity,
                                          price_opt, stop_opt, /*reduce_only=*/false, order.exchange, product);
        if (is_market) {
            pt_fill_order(paper_order.id, *price_opt);
        } else {
            // Limit / stop / stop-limit: hand to the matcher so it fills on a real
            // market touch (driven centrally off the quote feed). Previously these
            // were placed and then left "pending" forever — never filled.
            OrderMatcher::instance().add_order(paper_order);
        }
        return {true, paper_order.id, "Paper order placed", "paper"};
    } catch (const std::exception& e) {
        return {false, "", QString("Paper order failed: %1").arg(e.what()), "paper"};
    }
}

UnifiedOrderResponse UnifiedTrading::place_live_order_for_account(const QString& account_id,
                                                                  const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    const QString mode = broker_mode_tag(account);
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, "", "Broker not found: " + account.broker_id, mode};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (creds.access_token.isEmpty())
        return {false, "", "No credentials for account " + account.display_name + ". Please authenticate.", mode};

    // The one place a single order reaches a broker. A PAPER-mode account only gets here
    // via a verified native paper venue — re-check on the exact credentials about to be used.
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        LOG_ERROR(kUtLog, QString("%1 (account %2)").arg(refusal, account_id));
        return {false, "", refusal, mode};
    }

    // Space orders out to the broker's published per-second cap. Without this a
    // broadcast / basket / algo loop fires as fast as the network allows and the
    // API key gets throttled or banned mid-batch.
    rate_limit_broker(account.broker_id);
    auto result = broker->place_order(creds, order);
    return {result.success, result.order_id, result.error, mode};
}

// ============================================================================
// Broadcast Order (multi-account simultaneous placement)
// ============================================================================

QVector<UnifiedTrading::BroadcastResult> UnifiedTrading::broadcast_order(const QStringList& account_ids,
                                                                         const UnifiedOrder& order) {
    QVector<BroadcastResult> results;
    results.reserve(account_ids.size());

    for (const auto& acct_id : account_ids) {
        auto account = AccountManager::instance().get_account(acct_id);
        if (account.account_id.isEmpty()) {
            results.append({acct_id, "Unknown", {false, "", "Account not found", ""}});
            continue;
        }
        auto response = place_order(acct_id, order);
        results.append({acct_id, account.display_name, response});
    }

    return results;
}

// ============================================================================
// Phase 1: Bulk Operations (OpenAlgo bridge)
// ============================================================================

ApiResponse<CancelAllResult> UnifiedTrading::cancel_all_orders(const QString& account_id) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    if (local_paper(account)) {
        CancelAllResult result;
        auto pending = pt_get_orders(account.paper_portfolio_id, "pending");
        for (const auto& order : pending) {
            try {
                pt_cancel_order(order.id);
                result.canceled_order_ids.append(order.id);
            } catch (...) {
                result.failed.append({order.id, "Cancel failed"});
            }
            result.total_attempted++;
        }
        publish(AllOrdersCancelledEvent{account_id, (int)result.canceled_order_ids.size(), (int)result.failed.size(),
                                        "paper"});
        return {true, result, {}};
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return {false, std::nullopt, refusal};
    auto resp = broker->cancel_all_orders(creds);
    if (resp.success && resp.data.has_value()) {
        publish(AllOrdersCancelledEvent{account_id, (int)resp.data->canceled_order_ids.size(),
                                        (int)resp.data->failed.size(), broker_mode_tag(account)});
    }
    return resp;
}

ApiResponse<CloseAllResult> UnifiedTrading::close_all_positions(const QString& account_id) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    if (local_paper(account)) {
        CloseAllResult result;
        auto positions = pt_get_positions(account.paper_portfolio_id);
        for (const auto& pos : positions) {
            if (pos.quantity == 0)
                continue;
            result.total_positions++;
            // Close with the OPPOSITE side. Paper positions store quantity as a
            // positive magnitude with side="long"/"short", so the quantity sign is
            // unreliable (shorts are +) — derive the direction from `side`. Using
            // the sign sold a short instead of buying it back, so it never closed.
            const QString side_str =
                position_is_short(pos.side, pos.quantity) ? QStringLiteral("buy") : QStringLiteral("sell");
            const double fill = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
            try {
                // reduce_only: a close may only shrink the position, never flip it.
                auto paper_order = pt_place_order(account.paper_portfolio_id, pos.symbol, side_str, "market",
                                                  std::abs(pos.quantity), fill, std::nullopt, /*reduce_only=*/true,
                                                  QString(), pos.product);
                pt_fill_order(paper_order.id, fill);
                result.closed_symbols.append(pos.symbol);
                LOG_INFO("UnifiedTrading", QString("close_all paper: %1 %2 x%3 @ %4 (was %5)")
                                               .arg(side_str, pos.symbol)
                                               .arg(std::abs(pos.quantity))
                                               .arg(fill)
                                               .arg(pos.side));
            } catch (const std::exception& e) {
                result.failed.append({pos.symbol, e.what()});
                LOG_WARN("UnifiedTrading", QString("close_all paper failed for %1: %2").arg(pos.symbol, e.what()));
            }
        }
        publish(
            AllPositionsClosedEvent{account_id, (int)result.closed_symbols.size(), (int)result.failed.size(), "paper"});
        return {true, result, {}};
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return {false, std::nullopt, refusal};
    auto resp = broker->close_all_positions(creds);
    if (resp.success && resp.data.has_value()) {
        publish(AllPositionsClosedEvent{account_id, (int)resp.data->closed_symbols.size(),
                                        (int)resp.data->failed.size(), broker_mode_tag(account)});
    }
    return resp;
}

ApiResponse<OrderPlaceResponse> UnifiedTrading::close_position(const QString& account_id, const QString& symbol,
                                                               const QString& exchange, const QString& product_type) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    if (local_paper(account)) {
        auto positions = pt_get_positions(account.paper_portfolio_id);
        for (const auto& pos : positions) {
            QString pos_sym = pos.symbol;
            if (pos_sym == symbol || pos_sym == exchange + ":" + symbol) {
                if (pos.quantity == 0)
                    continue;
                // Close by the OPPOSITE of the position's `side` (quantity is a
                // positive magnitude; shorts are +, so the sign is unreliable).
                const QString side_str =
                    position_is_short(pos.side, pos.quantity) ? QStringLiteral("buy") : QStringLiteral("sell");
                const double fill = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
                try {
                    // reduce_only: a close may only shrink the position, never flip it.
                    auto paper_order = pt_place_order(account.paper_portfolio_id, pos.symbol, side_str, "market",
                                                      std::abs(pos.quantity), fill, std::nullopt,
                                                      /*reduce_only=*/true, QString(), pos.product);
                    pt_fill_order(paper_order.id, fill);
                    return {true, OrderPlaceResponse{true, paper_order.id, {}}, {}};
                } catch (const std::exception& e) {
                    return {false, std::nullopt, QString("Close failed: %1").arg(e.what())};
                }
            }
        }
        return {false, std::nullopt, "Position not found"};
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return {false, std::nullopt, refusal};
    return broker->close_position(creds, symbol, exchange, product_type);
}

ApiResponse<SmartOrderResult> UnifiedTrading::place_smart_order(const QString& account_id, const SmartOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    // Pre-flight validation. Smart orders previously reached the broker with no
    // checks at all — only the plain UnifiedOrder overload was wired.
    auto vr = OrderValidator::validate_smart(order);
    if (!vr.valid)
        return {false, std::nullopt, "Validation failed: " + vr.errors.join("; ")};

    // Risk gate on the TARGET exposure: the engine computes the delta from the
    // live book, so the size the account will be carrying is the meaningful
    // quantity to check (falling back to the explicit quantity when the target is
    // a flatten). Defaults are unlimited — see risk_block_reason.
    const double risk_qty = (order.position_size != 0.0) ? std::abs(order.position_size) : order.quantity;
    const QString risk_err = risk_block_reason(order.symbol, order.action, risk_qty, order.price,
                                               account.trading_mode == "paper");
    if (!risk_err.isEmpty()) {
        LOG_INFO(kUtLog, QString("Risk limit rejected smart order %1 (target %2) on account %3: %4")
                             .arg(order.symbol)
                             .arg(order.position_size)
                             .arg(account_id, risk_err));
        return {false, std::nullopt, "Risk check failed: " + risk_err};
    }
    {
        // Settings-driven gate on the same target exposure (see place_order).
        UnifiedOrder probe;
        probe.symbol = order.symbol;
        probe.exchange = order.exchange;
        probe.side = order.action;
        probe.quantity = risk_qty;
        probe.order_type = order.order_type;
        probe.price = order.price;
        probe.stop_price = order.trigger_price;
        probe.product_type = order.product_type;
        if (probe.quantity > 0) {
            const auto gate = OrderValidator::pre_trade_risk(account_id, probe, !local_paper(account));
            if (!gate.valid)
                return {false, std::nullopt, "Risk check failed: " + gate.errors.join("; ")};
        }
    }

    if (local_paper(account)) {
        // Paper mode: get paper positions, calculate delta, place paper order
        auto positions = pt_get_positions(account.paper_portfolio_id);
        double current = 0;
        QString paper_sym = order.exchange.isEmpty() ? order.symbol : order.exchange + ":" + order.symbol;
        for (const auto& p : positions) {
            if (p.symbol == paper_sym) {
                current = p.quantity;
                break;
            }
        }

        double target = order.position_size;
        OrderSide action;
        double quantity;

        if (target == 0 && current == 0) {
            action = order.action;
            quantity = order.quantity;
            if (quantity <= 0)
                return {true, SmartOrderResult{false, {}, {}, 0, "No action needed. Position is zero."}, {}};
        } else if (target == 0 && current > 0) {
            action = OrderSide::Sell;
            quantity = std::abs(current);
        } else if (target == 0 && current < 0) {
            action = OrderSide::Buy;
            quantity = std::abs(current);
        } else if (current == 0) {
            action = (target > 0) ? OrderSide::Buy : OrderSide::Sell;
            quantity = std::abs(target);
        } else if (target > current) {
            action = OrderSide::Buy;
            quantity = target - current;
        } else if (target < current) {
            action = OrderSide::Sell;
            quantity = current - target;
        } else {
            return {true, SmartOrderResult{false, {}, {}, 0, "No action needed. Position already at target."}, {}};
        }

        // Fill at the caller-supplied live price (SmartOrder.price = LTP), NOT a
        // hardcoded 1000.0 sentinel — that fabricated P&L (closing a 250-cost
        // long booked (1000-250)·qty of phantom profit, and a net-new position
        // opened at 1000). Reject a paper market fill with no usable price, the
        // same way the regular paper place_order path does.
        if (order.price <= 0.0)
            return {false, std::nullopt,
                    QString("No live price for a paper smart-order fill on %1 (supply SmartOrder.price = LTP)")
                        .arg(paper_sym)};
        const auto side_str = (action == OrderSide::Buy) ? QString("buy") : QString("sell");
        try {
            auto paper_order = pt_place_order(account.paper_portfolio_id, paper_sym, side_str, "market", quantity,
                                              order.price, std::nullopt, false);
            pt_fill_order(paper_order.id, order.price);
            return {true,
                    SmartOrderResult{true, paper_order.id, action, quantity,
                                     QString("Paper: adjusted from %1 to %2").arg(current).arg(target)},
                    {}};
        } catch (const std::exception& e) {
            return {false, std::nullopt, QString("Paper smart order failed: %1").arg(e.what())};
        }
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return {false, std::nullopt, refusal};
    rate_limit_broker(account.broker_id);
    return SmartOrderEngine::instance().execute(broker, creds, order);
}

ApiResponse<QVector<BrokerQuote>> UnifiedTrading::get_multi_quotes(const QString& account_id,
                                                                   const QVector<QPair<QString, QString>>& symbols) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    return broker->get_multi_quotes(creds, symbols);
}

ApiResponse<MarketDepth> UnifiedTrading::get_market_depth(const QString& account_id, const QString& symbol,
                                                          const QString& exchange) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    return broker->get_market_depth(creds, symbol, exchange);
}

// ============================================================================
// Basket & Split Orders
// ============================================================================

void UnifiedTrading::place_basket_orders(const QString& account_id, const BasketOrderRequest& basket,
                                         std::function<void(const BasketOrderResult&)> callback) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty()) {
        BasketOrderResult result;
        result.total = basket.orders.size();
        for (const auto& order : basket.orders) {
            result.results.append({order.symbol, order.exchange, false, {}, "Account not found: " + account_id});
            result.failed++;
        }
        if (callback)
            callback(result);
        return;
    }

    // Pre-flight validation. validate_basket had zero call sites, so a basket
    // reached the broker with no checks whatsoever — a leg with a bad exchange or
    // a zero quantity was only ever rejected by the broker itself, one leg at a
    // time, after the earlier legs had already filled.
    auto bvr = OrderValidator::validate_basket(basket);
    if (!bvr.valid) {
        const QString err = bvr.errors.join("; ");
        BasketOrderResult result;
        result.total = basket.orders.size();
        for (const auto& order : basket.orders) {
            result.results.append({order.symbol, order.exchange, false, {}, "Validation failed: " + err});
            result.failed++;
        }
        LOG_WARN(kUtLog, QString("Basket rejected by validation for %1: %2").arg(account_id, err));
        if (callback)
            callback(result);
        return;
    }

    // is_paper = served by the LOCAL simulator. A PAPER account on a verified native broker
    // paper venue goes through the broker like a live one, yet stays "paper" for the risk
    // gate and the completion event (paper_mode).
    const bool paper_mode = (account.trading_mode == "paper");
    const bool is_paper = local_paper(account);

    // Resolve broker + credentials up front on the calling thread (consistent
    // with the existing place_live_order_for_account helper).
    IBroker* broker = nullptr;
    BrokerCredentials creds;
    QString paper_portfolio_id = account.paper_portfolio_id;
    if (!is_paper) {
        broker = BrokerRegistry::instance().get(account.broker_id);
        if (!broker) {
            BasketOrderResult result;
            result.total = basket.orders.size();
            for (const auto& order : basket.orders) {
                result.results.append(
                    {order.symbol, order.exchange, false, {}, "Broker not found: " + account.broker_id});
                result.failed++;
            }
            if (callback)
                callback(result);
            return;
        }
        creds = AccountManager::instance().load_credentials(account_id);
        if (creds.access_token.isEmpty()) {
            BasketOrderResult result;
            result.total = basket.orders.size();
            for (const auto& order : basket.orders) {
                result.results.append(
                    {order.symbol,
                     order.exchange,
                     false,
                     {},
                     "No credentials for account " + account.display_name + ". Please authenticate."});
                result.failed++;
            }
            if (callback)
                callback(result);
            return;
        }
        if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
            BasketOrderResult result;
            result.total = basket.orders.size();
            for (const auto& order : basket.orders) {
                result.results.append({order.symbol, order.exchange, false, {}, refusal});
                result.failed++;
            }
            if (callback)
                callback(result);
            return;
        }
    }

    // Order BUY legs first so that a basket which both buys and sells uses the
    // bought collateral before selling. stable_partition keeps relative order.
    QVector<UnifiedOrder> orders = basket.orders;
    std::stable_partition(orders.begin(), orders.end(), [](const UnifiedOrder& o) { return o.side == OrderSide::Buy; });

    // Basket legs bypass place_order, so stamp each leg's idempotency reference
    // here. Every leg is a distinct order and therefore gets its own reference —
    // sharing one would make the broker dedupe legs 2..N away.
    for (auto& o : orders)
        stamp_client_order_id(o);

    // Paper MARKET legs need a fill price, but basket builders (BasketOrdersDialog,
    // options strategies) leave price = 0 on market legs. Backfill from the account
    // stream's quote cache HERE on the calling (GUI) thread — cached_quote() is
    // main-thread-only — mirroring how the single-order path injects the screen's
    // live current_price_. Legs with no cached quote keep price 0 and fail with the
    // existing "No live price for market fill" message.
    if (is_paper) {
        if (auto* stream = DataStreamManager::instance().stream_for(account_id)) {
            for (auto& o : orders) {
                if (o.order_type == OrderType::Market && o.price <= 0.0) {
                    const double ltp = stream->cached_quote(o.symbol).ltp;
                    if (ltp > 0.0)
                        o.price = ltp;
                }
            }
        }
    }

    QPointer<UnifiedTrading> self = this;
    const QString basket_strategy = basket.strategy_name;
    const QString broker_id = account.broker_id;
    // The batch sleeps between legs (rate-limit spacing), so it runs on the
    // blocking-jobs pool: never queued behind the global pool, and never holding
    // an order-pool thread a single BUY/CANCEL needs.
    (void)QtConcurrent::run(&blocking_thread_pool(), [self, account_id, basket_strategy, orders, is_paper, paper_mode,
                                                      broker, broker_id, creds, paper_portfolio_id, callback]() {
        BasketOrderResult result;
        result.total = orders.size();

        constexpr int kBatchSize = 10;
        for (int i = 0; i < orders.size(); ++i) {
            // Pause between batches to respect broker rate limits.
            if (i > 0 && (i % kBatchSize) == 0)
                QThread::msleep(1000);

            const UnifiedOrder& order = orders.at(i);
            BasketOrderResult::OrderResult r;
            r.symbol = order.symbol;
            r.exchange = order.exchange;

            // Basket legs bypass place_order, so the risk gate is applied per leg
            // here. A blocked leg fails on its own; the rest of the basket still
            // goes out, matching how a broker-rejected leg already behaves.
            QString leg_risk = risk_block_reason(order.symbol, order.side, order.quantity, order.price, paper_mode);
            if (leg_risk.isEmpty()) {
                const auto gate = OrderValidator::pre_trade_risk(account_id, order, /*broker_routed=*/!is_paper);
                if (!gate.valid)
                    leg_risk = gate.errors.join("; ");
            }
            if (!leg_risk.isEmpty()) {
                LOG_INFO(kUtLog, QString("Risk limit rejected basket leg %1 on account %2: %3")
                                     .arg(order.symbol, account_id, leg_risk));
                r.success = false;
                r.error = "Risk check failed: " + leg_risk;
                result.failed++;
                result.results.append(r);
                continue;
            }

            if (is_paper) {
                // Store the BARE symbol + forward exchange/product (same contract as
                // place_paper_order_for_account) so each leg marks to market and
                // squares off correctly. Market legs fill at the supplied price (no
                // 1000.0 sentinel — a market leg with no price fails cleanly); limit/
                // stop legs go to the matcher to fill on a real touch.
                const QString side_str = order_side_str(order.side);
                const QString type_str = order_type_str(order.order_type);
                const QString product = product_to_broker_str(order.product_type);
                const bool is_market = (order.order_type == OrderType::Market);
                std::optional<double> price_opt;
                if (order.price > 0)
                    price_opt = order.price;
                std::optional<double> stop_opt;
                if (order.stop_price > 0)
                    stop_opt = order.stop_price;
                if (is_market && !price_opt) {
                    r.success = false;
                    r.error = "No live price for market fill";
                } else {
                    try {
                        auto paper_order =
                            pt_place_order(paper_portfolio_id, order.symbol, side_str, type_str, order.quantity,
                                           price_opt, stop_opt, false, order.exchange, product);
                        if (is_market)
                            pt_fill_order(paper_order.id, *price_opt);
                        else
                            OrderMatcher::instance().add_order(paper_order);
                        r.success = true;
                        r.order_id = paper_order.id;
                    } catch (const std::exception& e) {
                        r.success = false;
                        r.error = QString("Paper order failed: %1").arg(e.what());
                    }
                }
            } else {
                // Worker thread — safe to block on the broker's per-second cap.
                rate_limit_broker(broker_id);
                OrderPlaceResponse resp = broker->place_order(creds, order);
                r.success = resp.success;
                r.order_id = resp.order_id;
                r.error = resp.error;
            }

            if (r.success)
                result.successful++;
            else
                result.failed++;
            result.results.append(r);
        }

        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, account_id, basket_strategy, paper_mode, result, callback]() {
                // Notify listeners (e.g. TradingNotificationBridge) that the basket finished.
                publish(BasketCompletedEvent{account_id, basket_strategy, result.successful, result.failed,
                                             result.total,
                                             paper_mode ? QStringLiteral("paper") : QStringLiteral("live")});
                if (callback)
                    callback(result);
            },
            Qt::QueuedConnection);
    });
}

void UnifiedTrading::place_split_orders(const QString& account_id, const SplitOrderRequest& request,
                                        std::function<void(const SplitOrderResult&)> callback) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty()) {
        SplitOrderResult result;
        result.results.append(
            {request.base_order.symbol, request.base_order.exchange, false, {}, "Account not found: " + account_id});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    if (request.split_size <= 0) {
        SplitOrderResult result;
        result.results.append(
            {request.base_order.symbol, request.base_order.exchange, false, {}, "Invalid split size"});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    // UnifiedOrder::quantity is a double. Truncating it to int made any fractional
    // quantity below 1 collapse to total = 0: the chunk loop never ran, and the
    // callback fired with chunks_successful == 0 AND chunks_failed == 0 — reported
    // as neither success nor failure, so the user believed the order had gone out.
    // Reject anything that is not a whole quantity of at least 1, and ROUND (not
    // truncate) the conversion so a 9.9999 carried in from a float calculation is
    // 10 chunks' worth, not 9.
    const double raw_qty = request.base_order.quantity;
    const long long rounded_qty = std::llround(raw_qty);
    if (raw_qty < 1.0 || std::fabs(raw_qty - static_cast<double>(rounded_qty)) > 1e-6) {
        SplitOrderResult result;
        result.results.append({request.base_order.symbol, request.base_order.exchange, false, {},
                               QString("Invalid quantity %1 for a split order — it must be a whole number of at "
                                       "least 1.")
                                   .arg(raw_qty)});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    // Risk gate on the FULL quantity (a split is one intent, not N independent
    // orders) — split chunks bypass place_order entirely. Defaults are unlimited.
    QString split_risk = risk_block_reason(request.base_order.symbol, request.base_order.side, raw_qty,
                                           request.base_order.price, account.trading_mode == "paper");
    if (split_risk.isEmpty()) {
        // Settings-driven gate on the whole split (one intent). Sanity / value
        // checks see the full quantity, not one chunk.
        const auto gate = OrderValidator::pre_trade_risk(account_id, request.base_order, !local_paper(account));
        if (!gate.valid)
            split_risk = gate.errors.join("; ");
    }
    if (!split_risk.isEmpty()) {
        LOG_INFO(kUtLog, QString("Risk limit rejected split order %1 on account %2: %3")
                             .arg(request.base_order.symbol, account_id, split_risk));
        SplitOrderResult result;
        result.results.append({request.base_order.symbol, request.base_order.exchange, false, {},
                               "Risk check failed: " + split_risk});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    const int total = static_cast<int>(rounded_qty);
    const int split_size = request.split_size;
    const int num_full = total / split_size;
    const int remainder = total % split_size;
    const int total_orders = num_full + (remainder > 0 ? 1 : 0);

    if (total_orders > 100) {
        SplitOrderResult result;
        result.results.append({request.base_order.symbol,
                               request.base_order.exchange,
                               false,
                               {},
                               QString("Too many chunks (%1). Maximum is 100.").arg(total_orders)});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    // is_paper = served by the LOCAL simulator; a PAPER account on a verified native broker
    // paper venue goes through the broker adapter like a live one (the risk gate above
    // already used the account's actual mode).
    const bool is_paper = local_paper(account);

    IBroker* broker = nullptr;
    BrokerCredentials creds;
    QString paper_portfolio_id = account.paper_portfolio_id;
    if (!is_paper) {
        broker = BrokerRegistry::instance().get(account.broker_id);
        if (!broker) {
            SplitOrderResult result;
            result.results.append({request.base_order.symbol,
                                   request.base_order.exchange,
                                   false,
                                   {},
                                   "Broker not found: " + account.broker_id});
            result.chunks_failed++;
            if (callback)
                callback(result);
            return;
        }
        creds = AccountManager::instance().load_credentials(account_id);
        if (creds.access_token.isEmpty()) {
            SplitOrderResult result;
            result.results.append({request.base_order.symbol,
                                   request.base_order.exchange,
                                   false,
                                   {},
                                   "No credentials for account " + account.display_name + ". Please authenticate."});
            result.chunks_failed++;
            if (callback)
                callback(result);
            return;
        }
        if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
            SplitOrderResult result;
            result.results.append({request.base_order.symbol, request.base_order.exchange, false, {}, refusal});
            result.chunks_failed++;
            if (callback)
                callback(result);
            return;
        }
    }

    // Build the per-chunk quantity list.
    QVector<int> chunk_qtys;
    chunk_qtys.reserve(total_orders);
    for (int i = 0; i < num_full; ++i)
        chunk_qtys.append(split_size);
    if (remainder > 0)
        chunk_qtys.append(remainder);

    UnifiedOrder base = request.base_order;
    // Same paper MARKET-leg price backfill as place_basket_orders: resolve the live
    // LTP from the account stream's quote cache on the calling (GUI) thread; falls
    // through to the existing "No live price" error if no quote is cached yet.
    if (is_paper && base.order_type == OrderType::Market && base.price <= 0.0) {
        if (auto* stream = DataStreamManager::instance().stream_for(account_id)) {
            const double ltp = stream->cached_quote(base.symbol).ltp;
            if (ltp > 0.0)
                base.price = ltp;
        }
    }
    const int delay_ms = request.delay_between_ms;
    const QString broker_id = account.broker_id;

    QPointer<UnifiedTrading> self = this;
    // Sleeps delay_ms between chunks — blocking-jobs pool (see place_basket_orders).
    (void)QtConcurrent::run(
        &blocking_thread_pool(),
        [self, base, chunk_qtys, delay_ms, is_paper, broker, broker_id, creds, paper_portfolio_id, callback]() {
            SplitOrderResult result;

            for (int i = 0; i < chunk_qtys.size(); ++i) {
                if (i > 0)
                    QThread::msleep(delay_ms);

                UnifiedOrder chunk = base;
                chunk.quantity = chunk_qtys.at(i);
                // Each chunk is a SEPARATE live order, so it needs its own
                // idempotency reference. Inheriting the base order's would make
                // the broker dedupe chunks 2..N and only the first would fill.
                chunk.client_order_id = make_client_order_ref(20);

                BasketOrderResult::OrderResult r;
                r.symbol = chunk.symbol;
                r.exchange = chunk.exchange;

                if (is_paper) {
                    // Bare symbol + forwarded exchange/product (same contract as the
                    // keystone) so split chunks mark + square off; no 1000.0 sentinel.
                    const QString side_str = order_side_str(chunk.side);
                    const QString type_str = order_type_str(chunk.order_type);
                    const QString product = product_to_broker_str(chunk.product_type);
                    const bool is_market = (chunk.order_type == OrderType::Market);
                    std::optional<double> price_opt;
                    if (chunk.price > 0)
                        price_opt = chunk.price;
                    std::optional<double> stop_opt;
                    if (chunk.stop_price > 0)
                        stop_opt = chunk.stop_price;
                    if (is_market && !price_opt) {
                        r.success = false;
                        r.error = "No live price for market fill";
                    } else {
                        try {
                            auto paper_order =
                                pt_place_order(paper_portfolio_id, chunk.symbol, side_str, type_str, chunk.quantity,
                                               price_opt, stop_opt, false, chunk.exchange, product);
                            if (is_market)
                                pt_fill_order(paper_order.id, *price_opt);
                            else
                                OrderMatcher::instance().add_order(paper_order);
                            r.success = true;
                            r.order_id = paper_order.id;
                        } catch (const std::exception& e) {
                            r.success = false;
                            r.error = QString("Paper order failed: %1").arg(e.what());
                        }
                    }
                } else {
                    // Worker thread — safe to block on the broker's per-second cap.
                    rate_limit_broker(broker_id);
                    OrderPlaceResponse resp = broker->place_order(creds, chunk);
                    r.success = resp.success;
                    r.order_id = resp.order_id;
                    r.error = resp.error;
                }

                if (r.success) {
                    result.chunks_successful++;
                    result.total_quantity_placed += static_cast<int>(chunk.quantity);
                } else {
                    result.chunks_failed++;
                }
                result.results.append(r);
            }

            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, result, callback]() {
                    if (callback)
                        callback(result);
                },
                Qt::QueuedConnection);
        });
}

// ============================================================================
// Basket unwind
// ============================================================================

namespace {
bool ut_status_is_open(const QString& raw) {
    const QString s = raw.trimmed().toLower();
    return s == "open" || s == "pending" || s == "new" || s == "trigger pending" || s == "trigger_pending" ||
           s == "ordered" || s == "transit" || s == "accepted" || s == "partially_filled" || s == "partial" ||
           s == "working" || s == "open pending";
}
bool ut_status_is_filled(const QString& raw) {
    const QString s = raw.trimmed().toLower();
    return s == "filled" || s == "complete" || s == "completed" || s == "executed" || s == "traded";
}
const BrokerOrderInfo* ut_find_order(const QVector<BrokerOrderInfo>& book, const QString& id) {
    for (const auto& o : book)
        if (o.order_id == id || (!o.exchange_order_id.isEmpty() && o.exchange_order_id == id))
            return &o;
    return nullptr;
}
} // namespace

QVector<UnifiedTrading::UnwindLegResult>
UnifiedTrading::unwind_basket_legs(const QString& account_id, const QVector<BasketOrderResult::OrderResult>& legs) {
    QVector<UnwindLegResult> out;
    QVector<BasketOrderResult::OrderResult> placed;
    for (const auto& l : legs)
        if (l.success && !l.order_id.isEmpty())
            placed.append(l);
    auto fail_all = [&](const QString& why) {
        for (const auto& l : placed)
            out.append({l.symbol, l.order_id, false, {}, why});
        return out;
    };
    if (placed.isEmpty())
        return out;

    const auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return fail_all("Account not found: " + account_id);

    if (local_paper(account)) {
        const auto book = pt_get_orders(account.paper_portfolio_id);
        for (const auto& l : placed) {
            UnwindLegResult r{l.symbol, l.order_id, false, {}, {}};
            const PtOrder* o = nullptr;
            for (const auto& po : book)
                if (po.id == l.order_id)
                    o = &po;
            if (!o) {
                r.error = "paper order not found — check the order book";
                out.append(r);
                continue;
            }
            QStringList did;
            try {
                if (o->status == QLatin1String("pending") || o->status == QLatin1String("partial")) {
                    pt_cancel_order(o->id);
                    OrderMatcher::instance().remove_order(o->id);
                    did << QStringLiteral("cancelled remainder");
                }
                if (o->filled_qty > 0.0) {
                    const double px = o->avg_price.value_or(o->price.value_or(0.0));
                    if (px <= 0.0)
                        throw std::runtime_error("no fill price to reverse at");
                    const QString opp = (o->side == QLatin1String("buy")) ? QStringLiteral("sell") : QStringLiteral("buy");
                    auto rev = pt_place_order(account.paper_portfolio_id, o->symbol, opp, "market", o->filled_qty, px,
                                              std::nullopt, /*reduce_only=*/true, l.exchange, QString());
                    pt_fill_order(rev.id, px);
                    did << QString("reversed %1 @ %2 (paper)").arg(o->filled_qty).arg(px);
                }
                r.ok = true;
                r.action = did.isEmpty() ? QStringLiteral("nothing filled — nothing to unwind") : did.join("; ");
            } catch (const std::exception& e) {
                r.error = QString::fromUtf8(e.what());
                r.action = did.join("; ");
            }
            out.append(r);
        }
        return out;
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return fail_all("Broker not found: " + account.broker_id);
    const auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return fail_all(refusal);

    auto book_resp = broker->get_orders(creds);
    if (!book_resp.success || !book_resp.data.has_value())
        return fail_all("order book unreadable (" + book_resp.error + ") — nothing unwound; check the broker");

    // Pass 1: cancel unfilled remainders.
    QHash<QString, QString> cancel_note;
    bool cancelled_any = false;
    for (const auto& l : placed) {
        const BrokerOrderInfo* info = ut_find_order(*book_resp.data, l.order_id);
        if (info && ut_status_is_open(info->status)) {
            rate_limit_broker(account.broker_id);
            const auto c = broker->cancel_order(creds, info->order_id);
            cancel_note.insert(l.order_id, c.success ? QStringLiteral("cancelled remainder")
                                                     : QStringLiteral("cancel failed: ") + c.error);
            cancelled_any = true;
        }
    }
    // Pass 2: re-read so the reversal uses the FINAL filled quantity.
    if (cancelled_any) {
        auto again = broker->get_orders(creds);
        if (!again.success || !again.data.has_value()) {
            for (const auto& l : placed)
                out.append({l.symbol, l.order_id, false, cancel_note.value(l.order_id),
                            "order book unreadable after cancelling — fills unknown, NOT reversed; check the broker"});
            return out;
        }
        book_resp = again;
    }

    for (const auto& l : placed) {
        UnwindLegResult r{l.symbol, l.order_id, false, cancel_note.value(l.order_id), {}};
        const BrokerOrderInfo* info = ut_find_order(*book_resp.data, l.order_id);
        if (!info) {
            r.error = "not in the broker order book — cannot verify the fill, NOT reversed";
            out.append(r);
            continue;
        }
        double filled = info->filled_qty;
        if (filled <= 0.0 && ut_status_is_filled(info->status))
            filled = info->quantity; // brokers that report status but not filled_qty
        if (ut_status_is_open(info->status) && !cancel_note.value(l.order_id).startsWith("cancelled")) {
            r.error = "still open at the broker (cancel failed) — NOT reversed";
            out.append(r);
            continue;
        }
        if (filled <= 0.0) {
            r.ok = true;
            r.action = (r.action.isEmpty() ? QString() : r.action + "; ") + QStringLiteral("nothing filled");
            out.append(r);
            continue;
        }
        const auto side = order_side_from_str(info->side);
        if (!side) {
            r.error = QString("unrecognised side '%1' — NOT reversed").arg(info->side);
            out.append(r);
            continue;
        }
        UnifiedOrder rev;
        rev.symbol = l.symbol;
        rev.exchange = l.exchange.isEmpty() ? info->exchange : l.exchange;
        rev.side = (*side == OrderSide::Buy) ? OrderSide::Sell : OrderSide::Buy;
        rev.quantity = filled;
        rev.order_type = OrderType::Market;
        rev.product_type = broker->product_from_wire(info->product_type);
        const auto resp = place_order(account_id, rev);
        r.ok = resp.success;
        const QString step = QString("reversed %1 (%2 %3)")
                                 .arg(filled)
                                 .arg(QString::fromLatin1(order_side_str(rev.side)).toUpper(),
                                      QString::fromLatin1(product_to_broker_str(rev.product_type)));
        r.action = (r.action.isEmpty() ? QString() : r.action + "; ") + (resp.success ? step : QString());
        if (!resp.success)
            r.error = "reversal failed: " + resp.message;
        out.append(r);
    }
    return out;
}

void UnifiedTrading::place_order_auto_split(const QString& account_id, const UnifiedOrder& order,
                                            std::function<void(const SplitOrderResult&)> callback) {
    const int freeze = qty_freeze_limit(order.symbol, order.exchange);

    if (freeze <= 0 || order.quantity <= freeze) {
        // No freeze limit (or within it) — place a single order synchronously
        // via the normal path, then deliver a one-element SplitOrderResult so
        // callers see a uniform shape. place_order's own freeze check is a
        // no-op here because quantity <= freeze.
        UnifiedOrderResponse resp = place_order(account_id, order);
        SplitOrderResult result;
        BasketOrderResult::OrderResult r;
        r.symbol = order.symbol;
        r.exchange = order.exchange;
        r.success = resp.success;
        r.order_id = resp.order_id;
        r.error = resp.success ? QString() : resp.message;
        result.results.append(r);
        if (resp.success) {
            result.chunks_successful = 1;
            result.total_quantity_placed = static_cast<int>(order.quantity);
        } else {
            result.chunks_failed = 1;
        }
        if (callback)
            callback(result);
        return;
    }

    // Over the freeze limit — auto-split into chunks of `freeze`.
    LOG_INFO("UnifiedTrading", QString("Auto-splitting %1 %2 (qty %3 > freeze %4) into chunks of %4")
                                   .arg(account_id, order.symbol)
                                   .arg(static_cast<qint64>(order.quantity))
                                   .arg(freeze));
    SplitOrderRequest req;
    req.base_order = order;
    req.split_size = freeze;
    req.delay_between_ms = 100;
    place_split_orders(account_id, req, std::move(callback));
}

} // namespace fincept::trading
