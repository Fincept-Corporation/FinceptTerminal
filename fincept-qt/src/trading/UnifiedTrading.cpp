// Unified Trading — routes orders to live broker or paper trading engine

#include "trading/UnifiedTrading.h"

#include "core/agent/AgentActionScope.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/workflow/RiskManager.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerTopic.h"
#include "trading/DataStreamManager.h"
#include "trading/MarketDataRequests.h"
#include "trading/OrderApprovalRule.h"
#include "trading/OrderMatcher.h"
#include "trading/OrderThreadPool.h"
#include "trading/OrderValidator.h"
#include "trading/PaperBookSymbol.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "trading/RateLimiter.h"
#include "trading/SmartOrderEngine.h"
#include "trading/StrategyPortfolio.h"
#include "trading/TradingEvents.h"
#include "trading/brokers/BrokerClientOrderId.h"
#include "core/market/MarketSessions.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/NumberFormat.h"

#include <QCoreApplication>
#include <QElapsedTimer>
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
QString risk_block_reason(const QString& symbol, OrderSide side, double quantity, double price, bool paper,
                          const QString& exchange = QString()) {
    try {
        // The currency `price` is quoted in, so money limits compare in their own
        // currency: the exchange's venue currency (NSE INR, LSE GBp), a crypto pair's
        // quote asset; empty when unknown (MCX, CDS, FOREX).
        namespace mk = fincept::core::market;
        const QString exch = exchange.trimmed().toUpper();
        QString price_ccy;
        if (exch == QLatin1String("CRYPTO"))
            price_ccy = fincept::ui::formatting::instrument_for_symbol(symbol.trimmed()).currency;
        else if (const mk::Venue* v = exch.isEmpty() ? nullptr : mk::venue_by_code(exch))
            price_ccy = v->currency;
        QString reason;
        if (workflow::RiskManager::instance().is_order_allowed(symbol, QString::fromLatin1(order_side_str(side)),
                                                               quantity, price, paper, &reason, price_ccy))
            return {};
        return reason.isEmpty() ? QStringLiteral("Blocked by risk limits") : reason;
    } catch (const std::exception& e) {
        LOG_ERROR(kUtLog, logf("Risk check threw, order blocked", {{"symbol", symbol},
                                                                   {"side", order_side_str(side)},
                                                                   {"qty", quantity},
                                                                   {"error", e.what()}}));
        return QStringLiteral("Risk check failed: ") + QString::fromUtf8(e.what());
    } catch (...) {
        LOG_ERROR(kUtLog, logf("Risk check threw, order blocked", {{"symbol", symbol},
                                                                   {"side", order_side_str(side)},
                                                                   {"qty", quantity},
                                                                   {"error", "unknown exception"}}));
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

/// The broker adapter of an account-aware call. Every broker-bound path below
/// (orders, cancels, modifies, closes, holdings exits, smart / basket / split /
/// unwind orders, quotes, depth, native-paper detection) resolves its broker HERE.
/// The built-in paper account (BrokerAccount.h) has no broker: it gets nullptr, so
/// each of those paths answers "Broker not found" instead of reaching a broker —
/// and it is local paper anyway (trading mode is always "paper"), so the paper
/// branches serve it before any broker lookup is made.
IBroker* ut_broker_for(const BrokerAccount& account) {
    if (is_builtin_paper_account(account.account_id) || is_builtin_paper_broker(account.broker_id))
        return nullptr;
    return BrokerRegistry::instance().get(account.broker_id);
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
    auto* broker = ut_broker_for(account);
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

/// Live LTP for a LOCAL-paper MARKET fill whose caller supplied no price (basket builders,
/// the BROKERS multi-select / ALL broadcast, Semi-Auto approvals all leave price = 0).
/// 0 when no usable quote is known — the caller then keeps its "No live price" refusal.
/// The account stream's quote cache is GUI-thread-only, so it is read only on the GUI
/// thread; from a worker (broadcast_order runs on the order pool) the DataHub quote
/// snapshot for the same account is read instead — DataHub::peek() is mutex-guarded and
/// TTL-checked, so a stale quote is never used as a fill price.
double ut_paper_market_ltp(const QString& account_id, const QString& broker_id, const QString& symbol) {
    if (symbol.isEmpty())
        return 0.0;
    auto* app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread()) {
        if (auto* stream = DataStreamManager::instance().stream_for(account_id)) {
            const double ltp = stream->cached_quote(symbol).ltp;
            if (std::isfinite(ltp) && ltp > 0.0)
                return ltp;
        }
        return 0.0;
    }
    if (broker_id.isEmpty())
        return 0.0;
    const QVariant v =
        datahub::DataHub::instance().peek(broker_topic(broker_id, account_id, QStringLiteral("quote"), symbol));
    if (!v.isValid() || !v.canConvert<BrokerQuote>())
        return 0.0;
    const double ltp = v.value<BrokerQuote>().ltp;
    return (std::isfinite(ltp) && ltp > 0.0) ? ltp : 0.0;
}

/// Cash currency of a new legacy-session paper book: the broker's account currency
/// (its profile); for a broker id the registry does not know, the user's display
/// currency (Settings > Currency & Number Format) — never a hard-coded code.
QString ut_session_book_currency(const QString& broker_id) {
    if (IBroker* b = BrokerRegistry::instance().get(broker_id)) {
        const QString ccy = b->profile().currency.trimmed().toUpper();
        if (!ccy.isEmpty())
            return ccy;
    }
    const QString ccy = fincept::ui::formatting::FormatSettings::instance().display_currency();
    LOG_WARN(kUtLog, QString("Paper book for unknown broker '%1': cash currency taken from the display currency (%2)")
                         .arg(broker_id, ccy));
    return ccy;
}

/// Closes / reversals of a batch share one FX wait budget: the first symbol whose
/// rate is missing may wait (off the GUI thread) up to PaperMarkService::
/// kCloseFxWaitMs; the rest get what is left, so a book of foreign positions with
/// no rate is refused within seconds instead of N x the wait.
class UtCloseFxBudget {
  public:
    UtCloseFxBudget() { clock_.start(); }
    int remaining_ms() const {
        return static_cast<int>(std::max<qint64>(0, PaperMarkService::kCloseFxWaitMs - clock_.elapsed()));
    }

  private:
    QElapsedTimer clock_;
};

// ── Order audit trail (audit.log; policy B.7) ────────────────────────────────
// Broker-routed orders: "Order submitted" when the request leaves the terminal and
// "Order accepted" / "Order rejected" / "Order outcome unknown" when the answer is
// known, paired by client_id. Orders run by the local paper engine are audited by
// PaperTrading itself (Order accepted / rejected / filled / cancelled, venue=paper);
// this file audits the refusals that stop an order before it reaches the engine or a
// broker.
constexpr const char* kUtAudit = "Orders";

/// Account field of an audit line: the built-in paper account by name, any other by
/// its masked id (never the full id).
QString ut_audit_account(const QString& account_id) {
    return is_builtin_paper_account(account_id) ? account_id : log_mask(account_id);
}

QList<LogField> ut_order_fields(const QString& venue, const QString& account_id, const QString& mode,
                                const UnifiedOrder& o) {
    QList<LogField> f{{"venue", venue},
                      {"account", ut_audit_account(account_id)},
                      {"mode", mode},
                      {"symbol", o.symbol},
                      {"exchange", o.exchange},
                      {"side", order_side_str(o.side)},
                      {"type", order_type_str(o.order_type)},
                      {"product", product_to_broker_str(o.product_type)},
                      {"qty", o.quantity},
                      {"price", o.price}};
    if (o.stop_price > 0.0)
        f.append(LogField("trigger", o.stop_price));
    if (!o.client_order_id.isEmpty())
        f.append(LogField("client_id", o.client_order_id));
    if (!o.source.isEmpty())
        f.append(LogField("source", o.source));
    return f;
}

/// An order refused before it reached a broker or the paper engine (validation,
/// freeze limit, no price, no FX rate, no broker / credentials, …).
void ut_audit_rejected(const QString& venue, const QString& account_id, const QString& mode, const UnifiedOrder& o,
                       const QString& reason) {
    auto f = ut_order_fields(venue, account_id, mode, o);
    f.append(LogField("reason", reason));
    LOG_AUDIT(kUtAudit, logf("Order rejected", f));
}

/// RiskManager limit (risk_block_reason) refused the order. The settings-driven gate
/// (OrderValidator::pre_trade_risk) writes its own audit line.
void ut_audit_risk_rejected(const QString& account_id, const QString& mode, const QString& symbol, OrderSide side,
                            double qty, const QString& reason) {
    LOG_AUDIT(kUtAudit, "Risk limit rejected order",
              {{"account", ut_audit_account(account_id)},
               {"mode", mode},
               {"symbol", symbol},
               {"side", order_side_str(side)},
               {"qty", qty},
               {"reason", reason}});
}

void ut_audit_submitted(const QString& venue, const QString& account_id, const QString& mode, const UnifiedOrder& o) {
    LOG_AUDIT(kUtAudit, logf("Order submitted", ut_order_fields(venue, account_id, mode, o)));
}

/// True when a broker error means the request may have reached the broker (client-side
/// timeout — BrokerHttp says "outcome unknown").
bool ut_outcome_unknown(const QString& error) {
    return error.contains(QLatin1String("outcome unknown"), Qt::CaseInsensitive) ||
           error.contains(QLatin1String("timed out"), Qt::CaseInsensitive);
}

void ut_audit_outcome(const QString& venue, const QString& account_id, const QString& mode, const UnifiedOrder& o,
                      bool ok, const QString& order_id, const QString& error, qint64 elapsed_ms) {
    QList<LogField> f{{"venue", venue},
                      {"account", ut_audit_account(account_id)},
                      {"mode", mode},
                      {"symbol", o.symbol},
                      {"side", order_side_str(o.side)},
                      {"qty", o.quantity},
                      {"client_id", o.client_order_id}};
    if (!o.source.isEmpty())
        f.append(LogField("source", o.source));
    if (ok) {
        f.append(LogField("order_id", order_id));
        f.append(LogField("elapsed_ms", elapsed_ms));
        LOG_AUDIT(kUtAudit, logf("Order accepted", f));
    } else {
        f.append(LogField("reason", error));
        f.append(LogField("elapsed_ms", elapsed_ms));
        LOG_AUDIT(kUtAudit, logf(ut_outcome_unknown(error) ? QStringLiteral("Order outcome unknown")
                                                           : QStringLiteral("Order rejected"),
                                 f));
    }
}

/// One live cancel / modify / bulk action: the request line, then its outcome.
void ut_audit_action(const char* sentence, const QString& venue, const QString& account_id, const QString& mode,
                     const QList<LogField>& extra) {
    QList<LogField> f{{"venue", venue}, {"account", ut_audit_account(account_id)}, {"mode", mode}};
    f.append(extra);
    LOG_AUDIT(kUtAudit, logf(QLatin1StringView(sentence), f));
}

/// Quantity / price of a modify request, whichever spelling the caller used.
QList<LogField> ut_modify_fields(const QString& order_id, const QJsonObject& mods) {
    QList<LogField> f{{"order_id", order_id}};
    for (const char* k : {"quantity", "qty", "price", "trigger_price", "stop_price"}) {
        const QJsonValue v = mods.value(QLatin1String(k));
        if (!v.isUndefined() && !v.isNull())
            f.append(LogField(k, v.toVariant().toString()));
    }
    return f;
}

// ── Approval gate (defence in depth; trading/OrderApprovalRule.h) ────────────
// Callers that know they are automated (algo engine, workflow nodes, broadcast /
// basket dialogs, the F&O chain) consult ActionCenter::should_queue() and queue
// themselves. These entry points make the same decision again from
// UnifiedOrder::source, so an automated order whose caller forgot to route it still
// goes to the approval queue — the same entry (ActionCenter::queue_order, same
// payload, same "queued / not queued" audit lines) — and is never placed directly.

/// Depth of ApprovedOrderScope on this thread (only ActionCenter opens one).
thread_local int ut_approved_depth = 0;

/// The source an order carries from here on: the caller's, or — on the GUI thread,
/// while an agent step is in progress — the agent's (it overrides an empty or
/// screen-set source, never an automated / system one).
QString ut_effective_source(const QString& stated) {
    return approval_rule::effective_source(stated, core::agent::current_agent_order_source());
}

/// The gate's own reduce-only check of an exit-kind order (workflow_exit / algo_exit;
/// approval_rule::ReduceOnly) and why — written to the "Exit order ..." audit line.
struct UtReduceOnlyCheck {
    approval_rule::ReduceOnly verdict = approval_rule::ReduceOnly::Unknown;
    QString reason;
};

/// Does `order` on `account` only reduce an open position, right now? Yes when a
/// position in that symbol is open, the order is on the opposite side and its quantity
/// is at most the position's absolute quantity. The book read is the one the order will
/// meet:
///   local paper — the simulator's book (pt_get_positions). The engine nets an order
///     against the position with the SAME symbol string (the bare symbol this path
///     sends), long / short by side; a row in the other spelling ("EXCH:SYM",
///     trading/PaperBookSymbol.h) is named in the reason, never counted;
///   broker-routed (live, or a verified native paper venue) — the broker's positions,
///     read fresh (one round trip, like close_holding's re-read of the holdings book):
///     symbol as market_symbols_match spells it, and the order's exchange / product
///     where the row carries one. Never on the GUI thread (Unknown): no network wait there.
/// Unknown also when the broker, its credentials or its positions cannot be had, or
/// several open rows match. Resting orders are not counted.
UtReduceOnlyCheck ut_check_reduce_only(const BrokerAccount& account, const UnifiedOrder& order) {
    using RO = approval_rule::ReduceOnly;
    if (!std::isfinite(order.quantity) || !(order.quantity > 0.0) || order.symbol.trimmed().isEmpty())
        return {RO::No, QStringLiteral("the order has no symbol or quantity")};
    // The position this order would reduce: a short for a BUY, a long for a SELL.
    const bool closes_short = order.side == OrderSide::Buy;
    const QString side_txt = QString::fromLatin1(order_side_str(order.side));
    const QString wanted = closes_short ? QStringLiteral("short") : QStringLiteral("long");
    const auto against = [&](const QString& pos_side, double pos_qty, const QString& where) -> UtReduceOnlyCheck {
        const double held = std::abs(pos_qty);
        if (position_is_short(pos_side, pos_qty) != closes_short)
            return {RO::No, QStringLiteral("%1: no open %2 position to reduce — this %3 would add to it")
                                .arg(where, wanted, side_txt)};
        if (order.quantity > held + 1e-9)
            return {RO::No, QStringLiteral("%1: quantity %2 is more than the open %3 position (%4)")
                                .arg(where)
                                .arg(order.quantity)
                                .arg(wanted)
                                .arg(held)};
        return {RO::Yes, QStringLiteral("%1: %2 %3 against an open %4 position of %5")
                             .arg(where, side_txt)
                             .arg(order.quantity)
                             .arg(wanted)
                             .arg(held)};
    };

    if (local_paper(account)) {
        if (account.paper_portfolio_id.isEmpty())
            return {RO::Unknown, QStringLiteral("no paper portfolio for this account")};
        const QString where = QStringLiteral("paper %1").arg(order.symbol);
        const PtPosition* same_side = nullptr;
        const PtPosition* other_spelling = nullptr;
        const auto book = pt_get_positions(account.paper_portfolio_id); // {} when unreadable (logged)
        for (const auto& p : book) {
            if (!(std::abs(p.quantity) > 0.0))
                continue;
            // This order is booked under exactly its own symbol (place_paper_order_for_account)
            // and the engine nets it only against the row stored under that symbol. A row the
            // close / flatten rule reaches in the other spelling ("NSE:X" for an order "X",
            // paper_book::symbol_matches) is NOT reduced by it — the order would open a second
            // position — so such a row never verifies; it only says why.
            if (p.symbol != order.symbol) {
                if (!other_spelling && paper_book::symbol_matches(p.symbol, order.symbol, order.exchange))
                    other_spelling = &p;
                continue;
            }
            if (position_is_short(p.side, p.quantity) == closes_short)
                return against(p.side, p.quantity, where);
            same_side = &p;
        }
        if (same_side)
            return against(same_side->side, same_side->quantity, where);
        if (other_spelling)
            return {RO::No, QStringLiteral("%1: the open position is booked as %2 — an order for %3 does not net "
                                           "against it")
                                .arg(where, other_spelling->symbol, order.symbol)};
        return {RO::No, QStringLiteral("%1: no open position in the paper book").arg(where)};
    }

    IBroker* broker = ut_broker_for(account);
    if (!broker)
        return {RO::Unknown, QStringLiteral("broker not found: %1").arg(account.broker_id)};
    auto* app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread())
        return {RO::Unknown, QStringLiteral("the broker's positions are not read on the GUI thread")};
    const auto creds = AccountManager::instance().load_credentials(account.account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return {RO::Unknown, refusal};
    const auto pr = broker->get_positions(creds);
    if (!pr.success || !pr.data.has_value())
        return {RO::Unknown, QStringLiteral("the broker's positions could not be read (%1)")
                                 .arg(pr.error.isEmpty() ? QStringLiteral("no detail") : pr.error)};
    QVector<BrokerPosition> open;
    for (const auto& p : *pr.data) {
        if (!std::isfinite(p.quantity) || p.quantity == 0.0 || !market_symbols_match(p.symbol, order.symbol))
            continue;
        if (!p.exchange.isEmpty() && !order.exchange.isEmpty() &&
            p.exchange.compare(order.exchange, Qt::CaseInsensitive) != 0)
            continue;
        if (!p.product_type.isEmpty() && broker->product_from_wire(p.product_type) != order.product_type)
            continue;
        open.append(p);
    }
    const QString where = QStringLiteral("%1 %2").arg(account.broker_id, order.symbol);
    if (open.isEmpty())
        return {RO::No, QStringLiteral("%1: no open position at the broker in this symbol / exchange / product")
                            .arg(where)};
    if (open.size() > 1)
        return {RO::Unknown, QStringLiteral("%1: %2 open positions match — cannot tell which one this order reduces")
                                 .arg(where)
                                 .arg(open.size())};
    return against(open.first().side, open.first().quantity, where);
}

/// nullopt = go on and place the order. Otherwise the answer for the caller: queued
/// for approval (nothing sent) or refused. `payload` builds the queue entry's
/// order_data exactly as the explicit callers do. Human / system sources never read
/// the approval mode (no settings read on the ticket path).
///
/// An exit-kind source (workflow_exit / algo_exit) skips approval only when
/// `reduce_only` — this entry point's own check, run here at submission — answers Yes.
/// It runs only where the answer can change the route (Semi-Auto, or no account); in
/// Auto the order is placed either way, so there is no extra position read. Without a
/// `reduce_only` (basket, split) the answer is Unknown: exits are treated like
/// workflow / algo. Either answer is written to the audit trail with its reason.
std::optional<UnifiedOrderResponse> ut_approval_gate(const QString& account_id, const QString& op,
                                                     const QString& source, const QString& mode_tag,
                                                     const std::function<QJsonObject()>& payload,
                                                     const std::function<UtReduceOnlyCheck()>& reduce_only = {}) {
    const bool from_approval = ut_approved_depth > 0;
    if (from_approval || !approval_rule::needs_human_approval(source))
        return std::nullopt;
    const bool mode_known = !account_id.isEmpty();
    const bool semi_auto = mode_known && ActionCenter::instance().should_queue(account_id, op);
    auto verified = approval_rule::ReduceOnly::Unknown;
    if (approval_rule::is_exit_source(source) && (semi_auto || !mode_known)) {
        UtReduceOnlyCheck check;
        if (reduce_only)
            check = reduce_only();
        else
            check.reason = QStringLiteral("this entry point (%1) does not verify reduce-only").arg(op);
        verified = check.verdict;
        QList<LogField> f{{"op", op}, {"mode", mode_tag}, {"source", source}};
        if (mode_known)
            f.prepend(LogField("account", ut_audit_account(account_id)));
        f.append(LogField("reduce_only", approval_rule::reduce_only_name(check.verdict)));
        f.append(LogField("reason", check.reason));
        LOG_AUDIT(kUtAudit, logf(verified == approval_rule::ReduceOnly::Yes
                                     ? QStringLiteral("Exit order verified reduce-only; placed without approval")
                                     : QStringLiteral("Exit order not verified reduce-only; approval rule applies"),
                                 f));
    }
    const auto route = approval_rule::decide(source, semi_auto, from_approval, mode_known, verified);
    if (route == approval_rule::Route::Place)
        return std::nullopt;
    if (route == approval_rule::Route::Refuse) {
        LOG_AUDIT(kUtAudit, "Order rejected",
                  {{"op", op},
                   {"mode", mode_tag},
                   {"source", source},
                   {"reason", "automated order without an account: its approval mode cannot be read"}});
        return UnifiedOrderResponse{false, QString(),
                                    QStringLiteral("Refused: an automated order (%1) needs an account so its approval "
                                                   "mode can be checked — nothing was sent")
                                        .arg(source),
                                    mode_tag};
    }
    // Queue: the caller did not route this automated order itself.
    LOG_WARN(kUtLog, logf("Automated order reached the order path without its approval routing; queued here",
                          {{"account", ut_audit_account(account_id)}, {"op", op}, {"source", source}}));
    const QString pending_id = ActionCenter::instance().queue_order(account_id, op, payload());
    if (pending_id.isEmpty())
        return UnifiedOrderResponse{false, QString(),
                                    QStringLiteral("This account requires approval for %1 orders, and the order could "
                                                   "not be queued for approval — nothing was sent")
                                        .arg(order_source_kind(source)),
                                    mode_tag};
    return UnifiedOrderResponse{false, QString(),
                                QStringLiteral("Queued for approval (Semi-Auto) as %1 — nothing was sent; approve it "
                                               "in the Action Center")
                                    .arg(pending_id),
                                mode_tag};
}
} // namespace

ApprovedOrderScope::ApprovedOrderScope() {
    ++ut_approved_depth;
}

ApprovedOrderScope::~ApprovedOrderScope() {
    --ut_approved_depth;
}

bool ApprovedOrderScope::active() {
    return ut_approved_depth > 0;
}

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
            auto portfolio = pt_create_portfolio(broker + " Paper Trading", 1000000.0, ut_session_book_currency(broker),
                                                 1.0, "cross", 0.0003);
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
        auto portfolio = pt_create_portfolio(session_->broker + " Paper Trading", 1000000.0,
                                             ut_session_book_currency(session_->broker), 1.0, "cross", 0.0003);
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
    routed.source = ut_effective_source(order.source);
    // No account on the legacy session path: an automated order cannot have its
    // approval mode read here and is refused (OrderApprovalRule: REFUSE). An exit-kind
    // order is not verified reduce-only here either (no account to read positions for).
    if (auto gated = ut_approval_gate(
            QString(), QStringLiteral("placeorder"), routed.source, session_->mode,
            [&routed]() { return ActionCenter::serialize_unified_order(routed); },
            []() {
                return UtReduceOnlyCheck{approval_rule::ReduceOnly::Unknown,
                                         QStringLiteral("no account (legacy session path): positions are not read")};
            }))
        return *gated;
    stamp_client_order_id(routed);

    if (session_->mode == "paper") {
        return place_paper_order(*session_, routed);
    }
    return place_live_order(*session_, routed);
}

UnifiedOrderResponse UnifiedTrading::place_paper_order(const TradingSession& session, const UnifiedOrder& order) {
    if (session.paper_portfolio_id.isEmpty()) {
        ut_audit_rejected(QStringLiteral("paper"), QString(), QStringLiteral("paper"), order,
                          QStringLiteral("No paper portfolio configured"));
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

    if (order.order_type == OrderType::Market && !price_opt) {
        ut_audit_rejected(QStringLiteral("paper"), QString(), QStringLiteral("paper"), order,
                          QStringLiteral("no live price for a market fill"));
        return {false, "", "No live price yet for a market fill — wait for quotes to load", "paper"};
    }

    // Cash is booked in the paper book's currency at the per-symbol factor (1.0 until
    // set): a symbol quoted in another currency needs a real rate first, never 1:1.
    QString fx_reason;
    if (!PaperMarkService::prepare_symbol_fx(session.paper_portfolio_id, symbol, order.exchange, &fx_reason))
        return {false, "", fx_reason, "paper"}; // audited by PaperMarkService (missing rate)

    try {
        auto paper_order = pt_place_order(session.paper_portfolio_id, symbol, side_str, type_str, order.quantity,
                                          price_opt, stop_opt, false, QString(), QString(), order.source);

        if (order.order_type == OrderType::Market)
            pt_fill_order(paper_order.id, *price_opt);

        return {true, paper_order.id, "Paper order placed", "paper"};
    } catch (const std::exception& e) {
        // An engine refusal (margin, market hours) is already in the audit trail; this
        // line adds where it came from and covers a failed fill.
        LOG_WARN(kUtLog, logf("Paper order failed", {{"portfolio", log_mask(session.paper_portfolio_id)},
                                                     {"symbol", symbol},
                                                     {"side", side_str},
                                                     {"qty", order.quantity},
                                                     {"error", e.what()}}));
        return {false, "", QString("Paper order failed: %1").arg(e.what()), "paper"};
    }
}

UnifiedOrderResponse UnifiedTrading::place_live_order(const TradingSession& session, const UnifiedOrder& order) {
    const QString live = QStringLiteral("live");
    auto* broker = BrokerRegistry::instance().get(session.broker);
    if (!broker) {
        ut_audit_rejected(session.broker, QString(), live, order, QStringLiteral("broker not found"));
        return {false, "", "Broker not found: " + session.broker, "live"};
    }

    auto creds = broker->load_credentials();
    if (creds.access_token.isEmpty()) {
        ut_audit_rejected(session.broker, QString(), live, order, QStringLiteral("no access token; not signed in"));
        return {false, "", "No credentials for " + session.broker + ". Please authenticate.", "live"};
    }

    // Legacy session path bypasses the account-aware place_order, so it carries
    // its own risk gate — this reaches a real broker.
    const QString blocked = risk_block_reason(order.symbol, order.side, order.quantity, order.price, /*paper=*/false,
                                              order.exchange);
    if (!blocked.isEmpty()) {
        ut_audit_risk_rejected(QString(), live, order.symbol, order.side, order.quantity, blocked);
        return {false, "", "Risk check failed: " + blocked, "live"};
    }
    // No account id on the legacy path: user limits + sanity check without the
    // per-account snapshot (unknown P&L fails a configured daily-loss limit closed).
    // A refusal is audited by pre_trade_risk itself.
    if (const auto gate = OrderValidator::pre_trade_risk(QString(), order, /*broker_routed=*/true); !gate.valid)
        return {false, "", "Risk check failed: " + gate.errors.join("; "), "live"};

    rate_limit_broker(session.broker);
    ut_audit_submitted(session.broker, QString(), live, order);
    QElapsedTimer sent;
    sent.start();
    auto result = broker->place_order(creds, order);
    ut_audit_outcome(session.broker, QString(), live, order, result.success, result.order_id, result.error,
                     sent.elapsed());
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
            LOG_WARN(kUtLog, logf("Paper order cancel failed", {{"order_id", order_id}, {"error", e.what()}}));
            return {false, "", QString("Cancel failed: %1").arg(e.what()), "paper"};
        }
    }

    const QString live = QStringLiteral("live");
    auto* broker = BrokerRegistry::instance().get(session_->broker);
    if (!broker) {
        ut_audit_action("Order cancel rejected", session_->broker, QString(), live,
                        {{"order_id", order_id}, {"reason", "broker not found"}});
        return {false, "", "Broker not found", "live"};
    }

    auto creds = broker->load_credentials();
    rate_limit_broker(session_->broker);
    ut_audit_action("Order cancel requested", session_->broker, QString(), live, {{"order_id", order_id}});
    auto result = broker->cancel_order(creds, order_id);
    if (result.success)
        ut_audit_action("Order cancelled", session_->broker, QString(), live, {{"order_id", order_id}});
    else
        ut_audit_action(ut_outcome_unknown(result.error) ? "Order cancel outcome unknown" : "Order cancel rejected",
                        session_->broker, QString(), live, {{"order_id", order_id}, {"reason", result.error}});
    return {result.success, order_id, result.error, "live"};
}

// ============================================================================
// Account-Aware Order Routing (new multi-account API)
// ============================================================================

bool UnifiedTrading::uses_native_paper(const QString& account_id) const {
    return native_paper_venue(AccountManager::instance().get_account(account_id));
}

UnifiedOrderResponse UnifiedTrading::place_order(const QString& account_id, const UnifiedOrder& order_in) {
    // Provenance first (every audit line below carries it): the caller's source, or the
    // agent's while an agent step is in progress on the GUI thread.
    UnifiedOrder order = order_in;
    order.source = ut_effective_source(order_in.source);

    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty()) {
        ut_audit_rejected(QString(), account_id, QString(), order, QStringLiteral("account not found"));
        return {false, "", "Account not found: " + account_id, ""};
    }

    // Approval gate (live AND paper): an automated order on a Semi-Auto account goes to
    // the approval queue instead of the broker / simulator — unless it is the execution
    // of an entry a person already approved (ApprovedOrderScope), or an exit-kind order
    // this gate verifies reduce-only against the account's positions right now.
    if (auto gated = ut_approval_gate(
            account_id, QStringLiteral("placeorder"), order.source, account.trading_mode,
            [&order]() { return ActionCenter::serialize_unified_order(order); },
            [&account, &order]() { return ut_check_reduce_only(account, order); }))
        return *gated;
    // Venue of the audit lines: the local simulator is "paper", anything else the broker.
    const auto audit_venue = [&account](bool local) {
        return local ? QStringLiteral("paper") : account.broker_id;
    };

    // Pre-flight validation (Phase 3 §9).
    auto vr = OrderValidator::validate(order);
    if (!vr.valid) {
        const QString err = vr.errors.join("; ");
        ut_audit_rejected(audit_venue(account.trading_mode == "paper"), account_id, account.trading_mode, order,
                          "Validation failed: " + err);
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
        risk_block_reason(order.symbol, order.side, order.quantity, order.price, account.trading_mode == "paper",
                          order.exchange);
    if (!risk_err.isEmpty()) {
        ut_audit_risk_rejected(account_id, account.trading_mode, order.symbol, order.side, order.quantity, risk_err);
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, risk_err, account.trading_mode});
        return {false, "", "Risk check failed: " + risk_err, account.trading_mode};
    }

    // Settings-driven pre-trade gate (OrderValidator): user risk limits + the
    // absurd-order sanity check. Same choke point, so it covers the order ticket,
    // broadcasts, approvals, MCP tools, workflows, algos and the F&O screen.
    // A refusal is audited by pre_trade_risk itself.
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
        ut_audit_rejected(audit_venue(is_local_paper), account_id, account.trading_mode, order,
                          QStringLiteral("quantity above the exchange freeze limit %1").arg(freeze));
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, err, account.trading_mode});
        return {false, "", err, account.trading_mode};
    }

    // Stamp the idempotency reference here — this is the terminal's single order
    // choke point, so every routed order carries one exactly once. Re-submitting
    // the same UnifiedOrder (a retry after an 8 s BrokerHttp timeout) keeps the
    // original reference; a freshly built order gets a new one.
    UnifiedOrder routed = order;
    stamp_client_order_id(routed);

    // A local-paper MARKET order fills at a price the caller must supply (order.price =
    // LTP). The order ticket does; the BROKERS multi-select / ALL broadcast and Semi-Auto
    // approvals send price 0, so every paper target of those failed with "No live price"
    // however long the user waited. Backfill from this account's live quote — the same
    // source the basket path uses. Broker-routed orders are never touched.
    if (is_local_paper && routed.order_type == OrderType::Market && !(routed.price > 0.0)) {
        const double ltp = ut_paper_market_ltp(account_id, account.broker_id, routed.symbol);
        if (ltp > 0.0)
            routed.price = ltp;
    }

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
            LOG_WARN(kUtLog, logf("Paper order cancel failed", {{"account", ut_audit_account(account_id)},
                                                                {"order_id", order_id},
                                                                {"error", e.what()}}));
            return {false, "", QString("Cancel failed: %1").arg(e.what()), "paper"};
        }
    }

    const QString mode = broker_mode_tag(account);
    auto* broker = ut_broker_for(account);
    if (!broker) {
        ut_audit_action("Order cancel rejected", account.broker_id, account_id, mode,
                        {{"order_id", order_id}, {"reason", "broker not found"}});
        return {false, "", "Broker not found: " + account.broker_id, broker_mode_tag(account)};
    }

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        ut_audit_action("Order cancel rejected", account.broker_id, account_id, mode,
                        {{"order_id", order_id}, {"reason", refusal}});
        return {false, order_id, refusal, "paper"};
    }
    rate_limit_broker(account.broker_id);
    ut_audit_action("Order cancel requested", account.broker_id, account_id, mode, {{"order_id", order_id}});
    auto result = broker->cancel_order(creds, order_id);
    if (result.success)
        ut_audit_action("Order cancelled", account.broker_id, account_id, mode, {{"order_id", order_id}});
    else
        ut_audit_action(ut_outcome_unknown(result.error) ? "Order cancel outcome unknown" : "Order cancel rejected",
                        account.broker_id, account_id, mode, {{"order_id", order_id}, {"reason", result.error}});
    return {result.success, order_id, result.error, broker_mode_tag(account)};
}

UnifiedOrderResponse UnifiedTrading::modify_order(const QString& account_id, const QString& order_id,
                                                  const QJsonObject& modifications) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, "", "Account not found: " + account_id, ""};

    if (local_paper(account)) {
        auto f = ut_modify_fields(order_id, modifications);
        f.append(LogField("reason", "modify is not supported for paper orders"));
        ut_audit_action("Order modify rejected", QStringLiteral("paper"), account_id, QStringLiteral("paper"), f);
        return {false, "", "Modify not supported for paper orders", "paper"};
    }

    const QString mode = broker_mode_tag(account);
    auto* broker = ut_broker_for(account);
    if (!broker) {
        auto f = ut_modify_fields(order_id, modifications);
        f.append(LogField("reason", "broker not found"));
        ut_audit_action("Order modify rejected", account.broker_id, account_id, mode, f);
        return {false, "", "Broker not found: " + account.broker_id, broker_mode_tag(account)};
    }

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        auto f = ut_modify_fields(order_id, modifications);
        f.append(LogField("reason", refusal));
        ut_audit_action("Order modify rejected", account.broker_id, account_id, mode, f);
        return {false, order_id, refusal, "paper"};
    }

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
        auto f = ut_modify_fields(order_id, modifications);
        f.append(LogField("reason", hydrate_err));
        ut_audit_action("Order modify rejected", account.broker_id, account_id, mode, f);
        return {false, order_id, hydrate_err, broker_mode_tag(account)};
    }

    rate_limit_broker(account.broker_id);
    ut_audit_action("Order modify requested", account.broker_id, account_id, mode,
                    ut_modify_fields(order_id, modifications));
    auto result = broker->modify_order(creds, order_id, mods);
    {
        auto f = ut_modify_fields(order_id, modifications);
        if (!result.success)
            f.append(LogField("reason", result.error));
        ut_audit_action(result.success                        ? "Order modified"
                        : ut_outcome_unknown(result.error) ? "Order modify outcome unknown"
                                                           : "Order modify rejected",
                        account.broker_id, account_id, mode, f);
    }
    return {result.success, order_id, result.error, broker_mode_tag(account)};
}

UnifiedOrderResponse UnifiedTrading::place_paper_order_for_account(const QString& account_id,
                                                                   const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    const QString paper = QStringLiteral("paper");
    if (account.paper_portfolio_id.isEmpty()) {
        ut_audit_rejected(paper, account_id, paper, order, QStringLiteral("no paper portfolio for this account"));
        return {false, "", "No paper portfolio for this account", "paper"};
    }

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
    if (is_market && !price_opt) {
        ut_audit_rejected(paper, account_id, paper, order, QStringLiteral("no live price for a market fill"));
        return {false, "", "No live price yet for a market fill — wait for quotes to load", "paper"};
    }

    // Cash is booked in the paper portfolio's currency at the engine's per-symbol
    // factor, which is 1.0 until set. An instrument quoted in another currency (an
    // LSE share in pence on a USD book) needs a real rate first — never 1:1.
    QString fx_reason;
    if (!PaperMarkService::prepare_symbol_fx(account.paper_portfolio_id, symbol, order.exchange, &fx_reason))
        return {false, "", fx_reason, "paper"}; // audited by PaperMarkService (missing rate)

    try {
        auto paper_order = pt_place_order(account.paper_portfolio_id, symbol, side_str, type_str, order.quantity,
                                          price_opt, stop_opt, /*reduce_only=*/false, order.exchange, product,
                                          order.source);
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
        // An engine refusal (margin, market hours) is already in the audit trail; this
        // line adds the account and covers a failed fill.
        LOG_WARN(kUtLog, logf("Paper order failed", {{"account", ut_audit_account(account_id)},
                                                     {"symbol", symbol},
                                                     {"side", side_str},
                                                     {"qty", order.quantity},
                                                     {"error", e.what()}}));
        return {false, "", QString("Paper order failed: %1").arg(e.what()), "paper"};
    }
}

UnifiedOrderResponse UnifiedTrading::place_live_order_for_account(const QString& account_id,
                                                                  const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    const QString mode = broker_mode_tag(account);
    auto* broker = ut_broker_for(account);
    if (!broker) {
        ut_audit_rejected(account.broker_id, account_id, mode, order, QStringLiteral("broker not found"));
        return {false, "", "Broker not found: " + account.broker_id, mode};
    }

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (creds.access_token.isEmpty()) {
        ut_audit_rejected(account.broker_id, account_id, mode, order,
                          QStringLiteral("no access token; the account is not signed in"));
        return {false, "", "No credentials for account " + account.display_name + ". Please authenticate.", mode};
    }

    // The one place a single order reaches a broker. A PAPER-mode account only gets here
    // via a verified native paper venue — re-check on the exact credentials about to be used.
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        ut_audit_rejected(account.broker_id, account_id, mode, order, refusal);
        return {false, "", refusal, mode};
    }

    // Protective SL / TP ride on the entry order only where the adapter actually sends
    // them and the broker honours them for this product (entry_sl_tp_honoured). Everywhere
    // else they used to be dropped silently: the order went out and the trader believed
    // the position was protected. Refuse instead of sending it stripped.
    if (order.stop_loss > 0.0 || order.take_profit > 0.0) {
        const BrokerProfile prof = broker->profile();
        if (!entry_sl_tp_honoured(prof, order.product_type, order.stop_loss > 0.0, order.take_profit > 0.0)) {
            const QString err = QString("%1 does not accept this stop-loss / take-profit on a %2 entry order — "
                                        "nothing was sent. Clear SL/TP and place a separate SL order (or a GTT) to "
                                        "protect the position.")
                                    .arg(prof.display_name, QString::fromLatin1(product_to_broker_str(order.product_type)));
            ut_audit_rejected(account.broker_id, account_id, mode, order,
                              QStringLiteral("stop-loss / take-profit not honoured by the broker for this product"));
            return {false, "", err, mode};
        }
    }

    // Space orders out to the broker's published per-second cap. Without this a
    // broadcast / basket / algo loop fires as fast as the network allows and the
    // API key gets throttled or banned mid-batch.
    rate_limit_broker(account.broker_id);
    ut_audit_submitted(account.broker_id, account_id, mode, order);
    QElapsedTimer sent;
    sent.start();
    auto result = broker->place_order(creds, order);
    ut_audit_outcome(account.broker_id, account_id, mode, order, result.success, result.order_id, result.error,
                     sent.elapsed());
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
                LOG_WARN(kUtLog, logf("Cancel-all could not cancel a paper order",
                                      {{"account", ut_audit_account(account_id)}, {"order_id", order.id}}));
            }
            result.total_attempted++;
        }
        // Each cancelled order is in the audit trail (PaperTrading); this is the summary.
        ut_audit_action("Cancel all orders finished", QStringLiteral("paper"), account_id, QStringLiteral("paper"),
                        {{"cancelled", static_cast<int>(result.canceled_order_ids.size())},
                         {"cancel_failed", static_cast<int>(result.failed.size())}});
        publish(AllOrdersCancelledEvent{account_id, (int)result.canceled_order_ids.size(), (int)result.failed.size(),
                                        "paper"});
        return {true, result, {}};
    }

    const QString mode = broker_mode_tag(account);
    auto* broker = ut_broker_for(account);
    if (!broker) {
        ut_audit_action("Cancel all orders rejected", account.broker_id, account_id, mode,
                        {{"reason", "broker not found"}});
        return {false, std::nullopt, "Broker not found: " + account.broker_id};
    }

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        ut_audit_action("Cancel all orders rejected", account.broker_id, account_id, mode, {{"reason", refusal}});
        return {false, std::nullopt, refusal};
    }
    ut_audit_action("Cancel all orders requested", account.broker_id, account_id, mode, {});
    auto resp = broker->cancel_all_orders(creds);
    if (resp.success && resp.data.has_value())
        ut_audit_action("Cancel all orders finished", account.broker_id, account_id, mode,
                        {{"cancelled", static_cast<int>(resp.data->canceled_order_ids.size())},
                         {"cancel_failed", static_cast<int>(resp.data->failed.size())}});
    else
        ut_audit_action("Cancel all orders failed", account.broker_id, account_id, mode, {{"reason", resp.error}});
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
        const UtCloseFxBudget fx_budget;
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
            // A foreign-currency position books its P&L at the per-symbol factor, which
            // is unset after a restart: refresh it from a real rate first (see
            // PaperMarkService::prepare_close_fx). No rate -> this one is left open and
            // reported; the others still close.
            QString fx_reason;
            if (!PaperMarkService::prepare_close_fx(account.paper_portfolio_id, pos.symbol, QString(),
                                                    fx_budget.remaining_ms(), &fx_reason)) {
                result.failed.append({pos.symbol, fx_reason}); // audited by PaperMarkService (missing rate)
                continue;
            }
            try {
                // reduce_only: a close may only shrink the position, never flip it.
                auto paper_order = pt_place_order(account.paper_portfolio_id, pos.symbol, side_str, "market",
                                                  std::abs(pos.quantity), fill, std::nullopt, /*reduce_only=*/true,
                                                  QString(), pos.product);
                pt_fill_order(paper_order.id, fill);
                result.closed_symbols.append(pos.symbol);
                // The order and its fill are in the audit trail (PaperTrading).
                LOG_DEBUG("UnifiedTrading", QString("close_all paper: %1 %2 x%3 @ %4 (was %5)")
                                                .arg(side_str, pos.symbol)
                                                .arg(std::abs(pos.quantity))
                                                .arg(fill)
                                                .arg(pos.side));
            } catch (const std::exception& e) {
                result.failed.append({pos.symbol, e.what()});
                LOG_WARN("UnifiedTrading", logf("Paper position close failed", {{"account", ut_audit_account(account_id)},
                                                                                {"symbol", pos.symbol},
                                                                                {"qty", std::abs(pos.quantity)},
                                                                                {"error", e.what()}}));
            }
        }
        ut_audit_action("Close all positions finished", QStringLiteral("paper"), account_id, QStringLiteral("paper"),
                        {{"closed", static_cast<int>(result.closed_symbols.size())},
                         {"close_failed", static_cast<int>(result.failed.size())}});
        publish(
            AllPositionsClosedEvent{account_id, (int)result.closed_symbols.size(), (int)result.failed.size(), "paper"});
        return {true, result, {}};
    }

    const QString mode = broker_mode_tag(account);
    auto* broker = ut_broker_for(account);
    if (!broker) {
        ut_audit_action("Close all positions rejected", account.broker_id, account_id, mode,
                        {{"reason", "broker not found"}});
        return {false, std::nullopt, "Broker not found: " + account.broker_id};
    }

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        ut_audit_action("Close all positions rejected", account.broker_id, account_id, mode, {{"reason", refusal}});
        return {false, std::nullopt, refusal};
    }
    ut_audit_action("Close all positions requested", account.broker_id, account_id, mode, {});
    auto resp = broker->close_all_positions(creds);
    if (resp.success && resp.data.has_value())
        ut_audit_action("Close all positions finished", account.broker_id, account_id, mode,
                        {{"closed", static_cast<int>(resp.data->closed_symbols.size())},
                         {"close_failed", static_cast<int>(resp.data->failed.size())}});
    else
        ut_audit_action("Close all positions failed", account.broker_id, account_id, mode, {{"reason", resp.error}});
    if (resp.success && resp.data.has_value()) {
        publish(AllPositionsClosedEvent{account_id, (int)resp.data->closed_symbols.size(),
                                        (int)resp.data->failed.size(), broker_mode_tag(account)});
    }
    return resp;
}

ApiResponse<OrderPlaceResponse> UnifiedTrading::close_position(const QString& account_id, const QString& symbol,
                                                               const QString& exchange, const QString& product_type,
                                                               const QString& source) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    // No approval gate: this is the engine's own close path — the whole position, on the
    // opposite side (local paper: a reduce_only order; a broker: IBroker::close_position
    // reads its book and sends the counter order), so an exit-kind source is reduce-only
    // by construction (OrderApprovalRule.h) and "closeposition" is an immediate op
    // (ActionCenter::immediate_ops). `source` is provenance: on every audit line below
    // and on the paper order's own lines (PaperTrading).
    const auto close_fields = [&source](QList<LogField> f) {
        if (!source.isEmpty())
            f.append(LogField("source", source));
        return f;
    };

    if (local_paper(account)) {
        auto positions = pt_get_positions(account.paper_portfolio_id);
        for (const auto& pos : positions) {
            QString pos_sym = pos.symbol;
            // The bare symbol or "EXCH:SYM" (paper_book::symbol_matches); the close then
            // goes on the row's own symbol string so it nets against that row.
            if (paper_book::symbol_matches(pos_sym, symbol, exchange)) {
                if (pos.quantity == 0)
                    continue;
                // Close by the OPPOSITE of the position's `side` (quantity is a
                // positive magnitude; shorts are +, so the sign is unreliable).
                const QString side_str =
                    position_is_short(pos.side, pos.quantity) ? QStringLiteral("buy") : QStringLiteral("sell");
                const double fill = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
                // The cash factor of a foreign-currency position lives in memory only
                // (unset after a restart). Refresh it from a real rate; with none, the
                // exit waits briefly off the GUI thread and is otherwise refused with
                // the reason — never booked at 1:1 (PaperMarkService::prepare_close_fx).
                QString fx_reason;
                if (!PaperMarkService::prepare_close_fx(account.paper_portfolio_id, pos.symbol, exchange,
                                                        PaperMarkService::kCloseFxWaitMs, &fx_reason))
                    return {false, std::nullopt, fx_reason}; // audited by PaperMarkService (missing rate)
                try {
                    // reduce_only: a close may only shrink the position, never flip it.
                    auto paper_order = pt_place_order(account.paper_portfolio_id, pos.symbol, side_str, "market",
                                                      std::abs(pos.quantity), fill, std::nullopt,
                                                      /*reduce_only=*/true, QString(), pos.product, source);
                    pt_fill_order(paper_order.id, fill);
                    return {true, OrderPlaceResponse{true, paper_order.id, {}}, {}};
                } catch (const std::exception& e) {
                    LOG_WARN(kUtLog, logf("Paper position close failed", {{"account", ut_audit_account(account_id)},
                                                                          {"symbol", pos.symbol},
                                                                          {"qty", std::abs(pos.quantity)},
                                                                          {"error", e.what()}}));
                    return {false, std::nullopt, QString("Close failed: %1").arg(e.what())};
                }
            }
        }
        return {false, std::nullopt, "Position not found"};
    }

    const QString mode = broker_mode_tag(account);
    auto* broker = ut_broker_for(account);
    if (!broker) {
        ut_audit_action("Position close refused", account.broker_id, account_id, mode,
                        close_fields({{"symbol", symbol}, {"exchange", exchange}, {"reason", "broker not found"}}));
        return {false, std::nullopt, "Broker not found: " + account.broker_id};
    }

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        ut_audit_action("Position close refused", account.broker_id, account_id, mode,
                        close_fields({{"symbol", symbol}, {"exchange", exchange}, {"reason", refusal}}));
        return {false, std::nullopt, refusal};
    }
    ut_audit_action("Position close submitted", account.broker_id, account_id, mode,
                    close_fields({{"symbol", symbol}, {"exchange", exchange}, {"product", product_type}}));
    QElapsedTimer sent;
    sent.start();
    auto closed = broker->close_position(creds, symbol, exchange, product_type);
    if (closed.success && closed.data.has_value())
        ut_audit_action(
            "Position close accepted", account.broker_id, account_id, mode,
            close_fields({{"symbol", symbol}, {"order_id", closed.data->order_id}, {"elapsed_ms", sent.elapsed()}}));
    else
        ut_audit_action(ut_outcome_unknown(closed.error) ? "Position close outcome unknown" : "Position close rejected",
                        account.broker_id, account_id, mode,
                        close_fields({{"symbol", symbol}, {"reason", closed.error}, {"elapsed_ms", sent.elapsed()}}));
    return closed;
}

ApiResponse<OrderPlaceResponse> UnifiedTrading::close_holding(const QString& account_id, const QString& symbol,
                                                              const QString& exchange, double quantity) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    // The local simulator keeps delivery exposure as CNC positions — its own close path
    // already handles them (opposite side, reduce-only, full position).
    if (local_paper(account))
        return close_position(account_id, symbol, exchange, QStringLiteral("CNC"));

    // Every refusal below is an exit the user asked for that was not sent: audited with
    // its reason (the sell itself goes through place_order, which audits it).
    const auto refused = [&](const QString& reason) -> ApiResponse<OrderPlaceResponse> {
        ut_audit_action("Holding exit refused", account.broker_id, account_id, broker_mode_tag(account),
                        {{"symbol", symbol}, {"exchange", exchange}, {"qty", quantity}, {"reason", reason}});
        return {false, std::nullopt, reason};
    };

    if (symbol.trimmed().isEmpty() || exchange.trimmed().isEmpty())
        return refused(QStringLiteral("Holding exit refused: symbol and exchange are required"));
    if (!std::isfinite(quantity) || quantity <= 0.0)
        return refused(
            QString("Holding exit refused for %1: no quantity to sell — refresh holdings and retry").arg(symbol));

    auto* broker = ut_broker_for(account);
    if (!broker)
        return refused("Broker not found: " + account.broker_id);
    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty())
        return refused(refusal);

    // Re-read the broker's holdings book (truth, not the blotter snapshot) so a holding
    // that was sold elsewhere, or shrank, is never oversold — on a broker that allows it
    // an oversell opens a short.
    const auto hr = broker->get_holdings(creds);
    if (!hr.success || !hr.data.has_value())
        return refused(QString("Holding exit refused for %1: the broker's holdings could not be read (%2)")
                           .arg(symbol, hr.error.isEmpty() ? QStringLiteral("no detail") : hr.error));
    const BrokerHolding* held = nullptr;
    for (const auto& h : *hr.data) {
        if (h.symbol != symbol)
            continue;
        // Some adapters (Alpaca) report holdings without an exchange; match on symbol then.
        if (!h.exchange.isEmpty() && h.exchange.compare(exchange, Qt::CaseInsensitive) != 0)
            continue;
        held = &h;
        break;
    }
    if (!held)
        return refused(QString("Holding exit refused: %1 (%2) is no longer in the holdings book — already sold?")
                           .arg(symbol, exchange));
    if (!(held->quantity > 0.0))
        return refused(QString("Holding exit refused: %1 is not a long holding at the broker (qty %2) — exit it from "
                               "the Positions tab")
                           .arg(symbol)
                           .arg(held->quantity));
    if (quantity > held->quantity + 1e-9)
        return refused(QString("Holding exit refused: the broker now reports %1 × %2 held, not the %3 you confirmed — "
                               "refresh holdings and retry")
                           .arg(held->quantity)
                           .arg(symbol)
                           .arg(quantity));

    UnifiedOrder order;
    order.symbol = symbol;
    order.exchange = exchange;
    order.side = OrderSide::Sell;
    order.order_type = OrderType::Market;
    order.quantity = quantity;
    order.product_type = ProductType::Delivery;
    order.validity = QStringLiteral("DAY");
    const UnifiedOrderResponse resp = place_order(account_id, order);
    if (!resp.success)
        return {false, std::nullopt, resp.message.isEmpty() ? QStringLiteral("Holding exit failed") : resp.message};
    return {true, OrderPlaceResponse{true, resp.order_id, {}}, {}};
}

ApiResponse<SmartOrderResult> UnifiedTrading::place_smart_order(const QString& account_id, const SmartOrder& order_in) {
    SmartOrder order = order_in;
    order.source = ut_effective_source(order_in.source);
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    // Approval gate (see place_order). A queued smart order is stored the way a
    // "smartorder" row is executed on approval (ActionCenter::execute_pending).
    // Reduce-only for an exit-kind source: a flatten (target 0, no explicit quantity) is
    // the engine's own close path — it sizes the order from the live position below, so
    // it can only shrink it (with no position it sends nothing). A non-zero target or an
    // explicit quantity can open exposure: not reduce-only.
    if (auto gated = ut_approval_gate(
            account_id, QStringLiteral("smartorder"), order.source, account.trading_mode,
            [&order]() {
                UnifiedOrder u;
                u.symbol = order.symbol;
                u.exchange = order.exchange;
                u.side = order.action;
                u.order_type = order.order_type;
                u.quantity = order.quantity;
                u.price = order.price;
                u.stop_price = order.trigger_price;
                u.product_type = order.product_type;
                u.source = order.source;
                QJsonObject j = ActionCenter::serialize_unified_order(u);
                j[QStringLiteral("position_size")] = order.position_size;
                return j;
            },
            [&order]() {
                if (order.position_size == 0.0 && !(order.quantity > 0.0))
                    return UtReduceOnlyCheck{approval_rule::ReduceOnly::Yes,
                                             QStringLiteral("%1: flatten to 0 — the engine closes the open position "
                                                            "from the live book (reduce-only by construction)")
                                                 .arg(order.symbol)};
                return UtReduceOnlyCheck{approval_rule::ReduceOnly::No,
                                         QStringLiteral("%1: a smart order with target %2 / quantity %3 can open "
                                                        "exposure — only a flatten (target 0, no quantity) is an exit")
                                             .arg(order.symbol)
                                             .arg(order.position_size)
                                             .arg(order.quantity)};
            }))
        return {false, std::nullopt, gated->message};

    // Pre-flight validation. Smart orders previously reached the broker with no
    // checks at all — only the plain UnifiedOrder overload was wired.
    // Fields shared by the smart-order audit lines (the engine computes side / qty).
    const auto smart_fields = [&](const QString& venue, const QString& mode) {
        QList<LogField> f{{"venue", venue},
                          {"account", ut_audit_account(account_id)},
                          {"mode", mode},
                          {"symbol", order.symbol},
                          {"exchange", order.exchange},
                          {"target", order.position_size},
                          {"qty", order.quantity},
                          {"price", order.price}};
        if (!order.source.isEmpty())
            f.append(LogField("source", order.source));
        return f;
    };
    const QString smart_venue = account.trading_mode == "paper" ? QStringLiteral("paper") : account.broker_id;

    auto vr = OrderValidator::validate_smart(order);
    if (!vr.valid) {
        auto f = smart_fields(smart_venue, account.trading_mode);
        f.append(LogField("reason", "Validation failed: " + vr.errors.join("; ")));
        LOG_AUDIT(kUtAudit, logf("Smart order rejected", f));
        return {false, std::nullopt, "Validation failed: " + vr.errors.join("; ")};
    }

    // Risk gate on the TARGET exposure: the engine computes the delta from the
    // live book, so the size the account will be carrying is the meaningful
    // quantity to check (falling back to the explicit quantity when the target is
    // a flatten). Defaults are unlimited — see risk_block_reason.
    const double risk_qty = (order.position_size != 0.0) ? std::abs(order.position_size) : order.quantity;
    const QString risk_err = risk_block_reason(order.symbol, order.action, risk_qty, order.price,
                                               account.trading_mode == "paper", order.exchange);
    if (!risk_err.isEmpty()) {
        ut_audit_risk_rejected(account_id, account.trading_mode, order.symbol, order.action, risk_qty, risk_err);
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
        // With no open position the order is booked as "EXCH:SYM", as before. An open one is
        // found the way the Equity screen's exit finds it (close_position): under the bare
        // symbol — how the account path books every order — or "EXCH:SYM"
        // (paper_book::symbol_matches), and the adjustment then goes on that row's own
        // symbol so it nets against it. Looking for "EXCH:SYM" alone missed every position
        // the account path had opened: a workflow flatten answered "Position is zero" with
        // the position still open.
        QString paper_sym = order.exchange.isEmpty() ? order.symbol : order.exchange + ":" + order.symbol;
        for (const auto& p : positions) {
            if (p.quantity == 0 || !paper_book::symbol_matches(p.symbol, order.symbol, order.exchange))
                continue;
            paper_sym = p.symbol;
            // Paper positions store a positive magnitude with side "long"/"short":
            // a short is a negative net position here.
            current = position_is_short(p.side, p.quantity) ? -std::abs(p.quantity) : std::abs(p.quantity);
            break;
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
        if (order.price <= 0.0) {
            auto f = smart_fields(QStringLiteral("paper"), QStringLiteral("paper"));
            f.append(LogField("reason", "no live price for a market fill"));
            LOG_AUDIT(kUtAudit, logf("Smart order rejected", f));
            return {false, std::nullopt,
                    QString("No live price for a paper smart-order fill on %1 (supply SmartOrder.price = LTP)")
                        .arg(paper_sym)};
        }
        const auto side_str = (action == OrderSide::Buy) ? QString("buy") : QString("sell");
        // Cash is booked at the per-symbol factor: an adjustment that only shrinks the
        // position follows the exit rule (prepare_close_fx), anything that adds
        // exposure the entry rule (prepare_symbol_fx). Neither books at 1:1.
        const bool only_reduces = current != 0 && quantity <= std::abs(current) + 1e-9 &&
                                  ((current > 0) == (action == OrderSide::Sell));
        QString fx_reason;
        const bool fx_ok = only_reduces
                               ? PaperMarkService::prepare_close_fx(account.paper_portfolio_id, paper_sym, order.exchange,
                                                                    PaperMarkService::kCloseFxWaitMs, &fx_reason)
                               : PaperMarkService::prepare_symbol_fx(account.paper_portfolio_id, paper_sym,
                                                                     order.exchange, &fx_reason);
        if (!fx_ok)
            return {false, std::nullopt, fx_reason}; // audited by PaperMarkService (missing rate)
        try {
            auto paper_order = pt_place_order(account.paper_portfolio_id, paper_sym, side_str, "market", quantity,
                                              order.price, std::nullopt, false, QString(), QString(), order.source);
            pt_fill_order(paper_order.id, order.price);
            return {true,
                    SmartOrderResult{true, paper_order.id, action, quantity,
                                     QString("Paper: adjusted from %1 to %2").arg(current).arg(target)},
                    {}};
        } catch (const std::exception& e) {
            LOG_WARN(kUtLog, logf("Paper smart order failed", {{"account", ut_audit_account(account_id)},
                                                               {"symbol", paper_sym},
                                                               {"target", target},
                                                               {"error", e.what()}}));
            return {false, std::nullopt, QString("Paper smart order failed: %1").arg(e.what())};
        }
    }

    const QString mode = broker_mode_tag(account);
    auto* broker = ut_broker_for(account);
    if (!broker) {
        auto f = smart_fields(account.broker_id, mode);
        f.append(LogField("reason", "broker not found"));
        LOG_AUDIT(kUtAudit, logf("Smart order rejected", f));
        return {false, std::nullopt, "Broker not found: " + account.broker_id};
    }

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (const QString refusal = paper_venue_refusal(account, *broker, creds); !refusal.isEmpty()) {
        auto f = smart_fields(account.broker_id, mode);
        f.append(LogField("reason", refusal));
        LOG_AUDIT(kUtAudit, logf("Smart order rejected", f));
        return {false, std::nullopt, refusal};
    }
    rate_limit_broker(account.broker_id);
    // SmartOrderEngine computes the delta and calls the broker itself: one line before,
    // one with the outcome (the order it sent, or why not).
    LOG_AUDIT(kUtAudit, logf("Smart order submitted", smart_fields(account.broker_id, mode)));
    QElapsedTimer sent;
    sent.start();
    auto smart = SmartOrderEngine::instance().execute(broker, creds, order);
    {
        auto f = smart_fields(account.broker_id, mode);
        f.append(LogField("elapsed_ms", sent.elapsed()));
        if (smart.success && smart.data.has_value()) {
            f.append(LogField("order_id", smart.data->order_id));
            if (smart.data->action_taken) {
                f.append(LogField("side", order_side_str(smart.data->executed_action)));
                f.append(LogField("order_qty", smart.data->executed_quantity));
            }
            f.append(LogField("result", smart.data->message));
            LOG_AUDIT(kUtAudit, logf(smart.data->action_taken ? QStringLiteral("Smart order accepted")
                                                              : QStringLiteral("Smart order finished"),
                                     f));
        } else {
            f.append(LogField("reason", smart.error));
            LOG_AUDIT(kUtAudit, logf(ut_outcome_unknown(smart.error) ? QStringLiteral("Smart order outcome unknown")
                                                                     : QStringLiteral("Smart order rejected"),
                                     f));
        }
    }
    return smart;
}

ApiResponse<QVector<BrokerQuote>> UnifiedTrading::get_multi_quotes(const QString& account_id,
                                                                   const QVector<QPair<QString, QString>>& symbols) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    auto* broker = ut_broker_for(account);
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

    auto* broker = ut_broker_for(account);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    return broker->get_market_depth(creds, symbol, exchange);
}

// ============================================================================
// Basket & Split Orders
// ============================================================================

void UnifiedTrading::place_basket_orders(const QString& account_id, const BasketOrderRequest& basket_in,
                                         std::function<void(const BasketOrderResult&)> callback) {
    // Each leg keeps its own provenance (an agent step in progress overrides a screen-set one).
    BasketOrderRequest basket = basket_in;
    for (auto& leg : basket.orders)
        leg.source = ut_effective_source(leg.source);

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

    // Approval gate (see place_order): a basket with an automated leg is queued whole,
    // as the "basketorder" entry the basket dialogs queue.
    QString gate_source;
    for (const auto& leg : basket.orders)
        if (approval_rule::needs_human_approval(leg.source)) {
            gate_source = leg.source;
            break;
        }
    if (!gate_source.isEmpty()) {
        if (auto gated = ut_approval_gate(account_id, QStringLiteral("basketorder"), gate_source, account.trading_mode,
                                          [&basket]() { return ActionCenter::serialize_basket_order(basket); })) {
            BasketOrderResult result;
            result.total = basket.orders.size();
            for (const auto& order : basket.orders) {
                result.results.append({order.symbol, order.exchange, false, {}, gated->message});
                result.failed++;
            }
            if (callback)
                callback(result);
            return;
        }
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
        LOG_AUDIT(kUtAudit, "Basket rejected",
                  {{"account", ut_audit_account(account_id)},
                   {"mode", account.trading_mode},
                   {"strategy", basket.strategy_name},
                   {"legs", static_cast<int>(basket.orders.size())},
                   {"reason", "Validation failed: " + err}});
        if (callback)
            callback(result);
        return;
    }
    const auto audit_basket_refused = [&](const QString& reason) {
        LOG_AUDIT(kUtAudit, "Basket rejected",
                  {{"venue", account.broker_id},
                   {"account", ut_audit_account(account_id)},
                   {"mode", account.trading_mode},
                   {"strategy", basket.strategy_name},
                   {"legs", static_cast<int>(basket.orders.size())},
                   {"reason", reason}});
    };

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
        broker = ut_broker_for(account);
        if (!broker) {
            audit_basket_refused(QStringLiteral("broker not found"));
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
            audit_basket_refused(QStringLiteral("no access token; the account is not signed in"));
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
            audit_basket_refused(refusal);
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
        for (auto& o : orders) {
            if (o.order_type == OrderType::Market && o.price <= 0.0) {
                const double ltp = ut_paper_market_ltp(account_id, account.broker_id, o.symbol);
                if (ltp > 0.0)
                    o.price = ltp;
            }
        }
    }

    QPointer<UnifiedTrading> self = this;
    const QString basket_strategy = basket.strategy_name;
    const QString broker_id = account.broker_id;
    // Audit: the basket as a whole, then each leg (paper legs by the engine, broker
    // legs submitted / outcome below), then the summary.
    const QString audit_venue = is_paper ? QStringLiteral("paper") : broker_id;
    const QString audit_mode = paper_mode ? QStringLiteral("paper") : QStringLiteral("live");
    LOG_AUDIT(kUtAudit, "Basket submitted",
              {{"venue", audit_venue},
               {"account", ut_audit_account(account_id)},
               {"mode", audit_mode},
               {"strategy", basket_strategy},
               {"legs", static_cast<int>(orders.size())}});
    // The batch sleeps between legs (rate-limit spacing), so it runs on the
    // blocking-jobs pool: never queued behind the global pool, and never holding
    // an order-pool thread a single BUY/CANCEL needs.
    (void)QtConcurrent::run(&blocking_thread_pool(), [self, account_id, basket_strategy, orders, is_paper, paper_mode,
                                                      broker, broker_id, creds, paper_portfolio_id, callback,
                                                      audit_venue, audit_mode]() {
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
            QString leg_risk = risk_block_reason(order.symbol, order.side, order.quantity, order.price, paper_mode,
                                                 order.exchange);
            const bool risk_manager_refused = !leg_risk.isEmpty(); // for the audit line only
            if (leg_risk.isEmpty()) {
                const auto gate = OrderValidator::pre_trade_risk(account_id, order, /*broker_routed=*/!is_paper);
                if (!gate.valid)
                    leg_risk = gate.errors.join("; ");
            }
            if (!leg_risk.isEmpty()) {
                // A settings-gate refusal was audited by pre_trade_risk; audit RiskManager's here.
                if (risk_manager_refused)
                    ut_audit_risk_rejected(account_id, audit_mode, order.symbol, order.side, order.quantity, leg_risk);
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
                QString fx_reason;
                if (is_market && !price_opt) {
                    r.success = false;
                    r.error = "No live price for market fill";
                    ut_audit_rejected(audit_venue, account_id, audit_mode, order, r.error);
                } else if (!PaperMarkService::prepare_symbol_fx(paper_portfolio_id, order.symbol, order.exchange,
                                                                &fx_reason)) {
                    // A leg priced in another currency than the paper book needs a real
                    // rate before cash is booked (never 1:1); the other legs go on.
                    r.success = false;
                    r.error = fx_reason; // audited by PaperMarkService (missing rate)
                } else {
                    try {
                        auto paper_order =
                            pt_place_order(paper_portfolio_id, order.symbol, side_str, type_str, order.quantity,
                                           price_opt, stop_opt, false, order.exchange, product, order.source);
                        if (is_market)
                            pt_fill_order(paper_order.id, *price_opt);
                        else
                            OrderMatcher::instance().add_order(paper_order);
                        r.success = true;
                        r.order_id = paper_order.id;
                    } catch (const std::exception& e) {
                        r.success = false;
                        r.error = QString("Paper order failed: %1").arg(e.what());
                        LOG_WARN(kUtLog, logf("Basket leg failed (paper)", {{"account", ut_audit_account(account_id)},
                                                                           {"symbol", order.symbol},
                                                                           {"side", side_str},
                                                                           {"qty", order.quantity},
                                                                           {"error", e.what()}}));
                    }
                }
            } else {
                // Worker thread — safe to block on the broker's per-second cap.
                rate_limit_broker(broker_id);
                ut_audit_submitted(broker_id, account_id, audit_mode, order);
                QElapsedTimer sent;
                sent.start();
                OrderPlaceResponse resp = broker->place_order(creds, order);
                ut_audit_outcome(broker_id, account_id, audit_mode, order, resp.success, resp.order_id, resp.error,
                                 sent.elapsed());
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

        LOG_AUDIT(kUtAudit, "Basket finished",
                  {{"venue", audit_venue},
                   {"account", ut_audit_account(account_id)},
                   {"mode", audit_mode},
                   {"strategy", basket_strategy},
                   {"ok", result.successful},
                   {"failed", result.failed},
                   {"legs", result.total}});

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

void UnifiedTrading::place_split_orders(const QString& account_id, const SplitOrderRequest& request_in,
                                        std::function<void(const SplitOrderResult&)> callback) {
    SplitOrderRequest request = request_in;
    request.base_order.source = ut_effective_source(request_in.base_order.source); // every chunk carries it
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

    // Approval gate (see place_order): an automated split is queued whole, as the
    // "splitorder" entry ActionCenter::execute_pending runs on approval.
    if (auto gated = ut_approval_gate(account_id, QStringLiteral("splitorder"), request.base_order.source,
                                      account.trading_mode, [&request]() {
                                          QJsonObject j = ActionCenter::serialize_unified_order(request.base_order);
                                          j[QStringLiteral("splitsize")] = request.split_size;
                                          j[QStringLiteral("delay_between_ms")] = request.delay_between_ms;
                                          return j;
                                      })) {
        SplitOrderResult result;
        result.results.append({request.base_order.symbol, request.base_order.exchange, false, {}, gated->message});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    // A split refused before any chunk is sent (audited with its reason).
    const auto audit_split_refused = [&](const QString& reason) {
        auto f = ut_order_fields(account.broker_id, account_id, account.trading_mode, request.base_order);
        f.append(LogField("reason", reason));
        LOG_AUDIT(kUtAudit, logf("Split order rejected", f));
    };

    if (request.split_size <= 0) {
        audit_split_refused(QStringLiteral("invalid split size"));
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
        audit_split_refused(QStringLiteral("quantity is not a whole number of at least 1"));
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
                                           request.base_order.price, account.trading_mode == "paper",
                                           request.base_order.exchange);
    const bool risk_manager_refused = !split_risk.isEmpty(); // for the audit line only
    if (split_risk.isEmpty()) {
        // Settings-driven gate on the whole split (one intent). Sanity / value
        // checks see the full quantity, not one chunk.
        const auto gate = OrderValidator::pre_trade_risk(account_id, request.base_order, !local_paper(account));
        if (!gate.valid)
            split_risk = gate.errors.join("; ");
    }
    if (!split_risk.isEmpty()) {
        // A settings-gate refusal was audited by pre_trade_risk; audit RiskManager's here.
        if (risk_manager_refused)
            ut_audit_risk_rejected(account_id, account.trading_mode, request.base_order.symbol,
                                   request.base_order.side, raw_qty, split_risk);
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
        audit_split_refused(QStringLiteral("too many chunks (%1, maximum 100)").arg(total_orders));
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
        broker = ut_broker_for(account);
        if (!broker) {
            audit_split_refused(QStringLiteral("broker not found"));
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
            audit_split_refused(QStringLiteral("no access token; the account is not signed in"));
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
            audit_split_refused(refusal);
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

    // Audit: the split as a whole, each chunk (paper chunks by the engine, broker chunks
    // submitted / outcome below), then the summary.
    const QString audit_venue = is_paper ? QStringLiteral("paper") : broker_id;
    const QString audit_mode = account.trading_mode == "paper" ? QStringLiteral("paper") : QStringLiteral("live");
    {
        auto f = ut_order_fields(audit_venue, account_id, audit_mode, base);
        f.append(LogField("chunks", static_cast<int>(chunk_qtys.size())));
        f.append(LogField("split_size", split_size));
        LOG_AUDIT(kUtAudit, logf("Split order submitted", f));
    }

    QPointer<UnifiedTrading> self = this;
    // Sleeps delay_ms between chunks — blocking-jobs pool (see place_basket_orders).
    (void)QtConcurrent::run(
        &blocking_thread_pool(),
        [self, base, chunk_qtys, delay_ms, is_paper, broker, broker_id, creds, paper_portfolio_id, callback,
         account_id, audit_venue, audit_mode]() {
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
                    QString fx_reason;
                    if (is_market && !price_opt) {
                        r.success = false;
                        r.error = "No live price for market fill";
                        ut_audit_rejected(audit_venue, account_id, audit_mode, chunk, r.error);
                    } else if (!PaperMarkService::prepare_symbol_fx(paper_portfolio_id, chunk.symbol, chunk.exchange,
                                                                    &fx_reason)) {
                        // Refreshed per chunk (the delay between chunks can be long);
                        // no rate -> this chunk is not booked at 1:1.
                        r.success = false;
                        r.error = fx_reason; // audited by PaperMarkService (missing rate)
                    } else {
                        try {
                            auto paper_order =
                                pt_place_order(paper_portfolio_id, chunk.symbol, side_str, type_str, chunk.quantity,
                                               price_opt, stop_opt, false, chunk.exchange, product, chunk.source);
                            if (is_market)
                                pt_fill_order(paper_order.id, *price_opt);
                            else
                                OrderMatcher::instance().add_order(paper_order);
                            r.success = true;
                            r.order_id = paper_order.id;
                        } catch (const std::exception& e) {
                            r.success = false;
                            r.error = QString("Paper order failed: %1").arg(e.what());
                            LOG_WARN(kUtLog,
                                     logf("Split order chunk failed (paper)", {{"account", ut_audit_account(account_id)},
                                                                               {"symbol", chunk.symbol},
                                                                               {"qty", chunk.quantity},
                                                                               {"error", e.what()}}));
                        }
                    }
                } else {
                    // Worker thread — safe to block on the broker's per-second cap.
                    rate_limit_broker(broker_id);
                    ut_audit_submitted(broker_id, account_id, audit_mode, chunk);
                    QElapsedTimer sent;
                    sent.start();
                    OrderPlaceResponse resp = broker->place_order(creds, chunk);
                    ut_audit_outcome(broker_id, account_id, audit_mode, chunk, resp.success, resp.order_id, resp.error,
                                     sent.elapsed());
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

            LOG_AUDIT(kUtAudit, "Split order finished",
                      {{"venue", audit_venue},
                       {"account", ut_audit_account(account_id)},
                       {"mode", audit_mode},
                       {"symbol", base.symbol},
                       {"ok", result.chunks_successful},
                       {"failed", result.chunks_failed},
                       {"qty_placed", result.total_quantity_placed}});

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
UnifiedTrading::unwind_basket_legs(const QString& account_id, const QVector<BasketOrderResult::OrderResult>& legs,
                                   const QString& source) {
    QVector<UnwindLegResult> out;
    QVector<BasketOrderResult::OrderResult> placed;
    for (const auto& l : legs)
        if (l.success && !l.order_id.isEmpty())
            placed.append(l);
    auto fail_all = [&](const QString& why) {
        LOG_AUDIT(kUtAudit, "Basket unwind refused",
                  {{"account", ut_audit_account(account_id)}, {"legs", static_cast<int>(placed.size())}, {"reason", why}});
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
        const UtCloseFxBudget fx_budget;
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
                    // The reversal is an exit: same FX rule as any close (never 1:1).
                    QString fx_reason;
                    if (!PaperMarkService::prepare_close_fx(account.paper_portfolio_id, o->symbol, l.exchange,
                                                            fx_budget.remaining_ms(), &fx_reason))
                        throw std::runtime_error(fx_reason.toStdString());
                    const QString opp = (o->side == QLatin1String("buy")) ? QStringLiteral("sell") : QStringLiteral("buy");
                    auto rev = pt_place_order(account.paper_portfolio_id, o->symbol, opp, "market", o->filled_qty, px,
                                              std::nullopt, /*reduce_only=*/true, l.exchange, QString(), source);
                    pt_fill_order(rev.id, px);
                    did << QString("reversed %1 @ %2 (paper)").arg(o->filled_qty).arg(px);
                }
                r.ok = true;
                r.action = did.isEmpty() ? QStringLiteral("nothing filled — nothing to unwind") : did.join("; ");
            } catch (const std::exception& e) {
                r.error = QString::fromUtf8(e.what());
                r.action = did.join("; ");
                LOG_WARN(kUtLog, logf("Basket unwind failed (paper)", {{"account", ut_audit_account(account_id)},
                                                                       {"order_id", o->id},
                                                                       {"symbol", o->symbol},
                                                                       {"error", e.what()}}));
            }
            out.append(r);
        }
        return out;
    }

    auto* broker = ut_broker_for(account);
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
            const QString unwind_mode = broker_mode_tag(account);
            ut_audit_action("Order cancel requested", account.broker_id, account_id, unwind_mode,
                            {{"order_id", info->order_id}, {"source", "basket_unwind"}});
            const auto c = broker->cancel_order(creds, info->order_id);
            if (c.success)
                ut_audit_action("Order cancelled", account.broker_id, account_id, unwind_mode,
                                {{"order_id", info->order_id}, {"source", "basket_unwind"}});
            else
                ut_audit_action(ut_outcome_unknown(c.error) ? "Order cancel outcome unknown" : "Order cancel rejected",
                                account.broker_id, account_id, unwind_mode,
                                {{"order_id", info->order_id}, {"source", "basket_unwind"}, {"reason", c.error}});
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
        rev.source = source; // an exit kind is verified reduce-only by place_order's gate
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
