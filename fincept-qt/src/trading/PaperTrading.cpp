// Paper Trading Engine — Business logic layer
// Delegates all DB access to PaperTradingRepository.
// Contains: validation, margin checks, fill engine, position management.

#include "trading/PaperTrading.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/PaperTradingRepository.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/SqliteSupport.h"
#include "trading/BrokerAccount.h"
#include "trading/OrderMatcher.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QTime>
#include <QTimeZone>
#include <QUuid>

#include <cmath>
#include <stdexcept>

namespace fincept::trading {

static QString generate_uuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

static QString now_rfc3339() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

static auto& repo() {
    return PaperTradingRepository::instance();
}

// Mutex protecting fill engine — prevents concurrent fills on the same portfolio
// from corrupting balance or positions.
static QMutex s_fill_mutex;

// ============================================================================
// All-or-nothing writes
// ============================================================================
//
// A fill, an order placement, a cancel and a product conversion are each several
// repository writes. They run inside one BEGIN IMMEDIATE on this thread's
// connection (Database hands every thread its own). The first failing write — or
// a failed BEGIN / COMMIT — throws, and PtWriteTransaction rolls everything back,
// so the books never hold half an operation (balance moved but no trade row, a
// position without its margin, a filled order with no position). The repository
// returns errors instead of throwing, so every write result is checked with
// pt_require(); the old try/catch rolled back only on C++ exceptions, which the
// repository never raises. Callers already handle the std::runtime_error contract.

namespace {

// Logs the driver text and throws a plain-language message for the caller.
[[noreturn]] void pt_storage_failure(const char* step, const std::string& raw) {
    LOG_ERROR("PaperTrading", logf("Paper trade could not be saved", {{"step", step}, {"error", QString::fromStdString(raw)}}));
    const QString why = fincept::db::user_message(fincept::db::classify_text(QString::fromStdString(raw)),
                                                  QStringLiteral("The terminal database"));
    throw std::runtime_error(QString("The paper trade was not saved and nothing was changed. %1").arg(why).toStdString());
}

void pt_require(const Result<void>& r, const char* step) {
    if (r.is_err())
        pt_storage_failure(step, r.error());
}

// Rolls back in the destructor unless commit() succeeded.
class PtWriteTransaction {
  public:
    explicit PtWriteTransaction(const char* what) : what_(what) {
        auto r = Database::instance().begin_transaction();
        if (r.is_err())
            pt_storage_failure(what_, "BEGIN: " + r.error()); // nothing started, nothing to undo
    }
    ~PtWriteTransaction() {
        if (!open_)
            return;
        auto r = Database::instance().rollback();
        if (r.is_err())
            LOG_ERROR("PaperTrading", QString("%1: rollback failed: %2")
                                          .arg(QLatin1String(what_), QString::fromStdString(r.error())));
        else
            LOG_WARN("PaperTrading", QString("%1 rolled back").arg(QLatin1String(what_)));
    }
    void commit() {
        auto r = Database::instance().commit();
        if (r.is_err())
            pt_storage_failure(what_, "COMMIT: " + r.error()); // still open: the destructor rolls back
        open_ = false;
    }
    PtWriteTransaction(const PtWriteTransaction&) = delete;
    PtWriteTransaction& operator=(const PtWriteTransaction&) = delete;

  private:
    const char* what_;
    bool open_ = true;
};

// ── Audit trail (audit.log; policy B.7) ─────────────────────────────────────
// Every paper order event the engine decides is audited here, whoever called it
// (order ticket, built-in paper account, workflows, algos, matcher, square-off):
// Order accepted / rejected / filled / cancelled / modified, venue=paper mode=paper.
constexpr const char* kPtAudit = "Orders";

QList<LogField> pt_audit_fields(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                                const QString& side, const QString& type, const QString& product, double qty,
                                std::optional<double> price, std::optional<double> stop,
                                const QString& source = QString()) {
    QList<LogField> f{{"venue", "paper"},
                      {"mode", "paper"},
                      {"portfolio", log_mask(portfolio_id)},
                      {"symbol", symbol},
                      {"exchange", exchange},
                      {"side", side},
                      {"type", type},
                      {"product", product},
                      {"qty", qty}};
    if (price)
        f.append(LogField("price", *price));
    if (stop)
        f.append(LogField("trigger", *stop));
    if (!source.isEmpty()) // who created the order (UnifiedOrder::source); omitted when not stated
        f.append(LogField("source", source));
    return f;
}

void pt_audit_rejected(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                       const QString& side, const QString& type, const QString& product, double qty,
                       std::optional<double> price, std::optional<double> stop, const QString& reason,
                       const QString& source = QString()) {
    auto f = pt_audit_fields(portfolio_id, symbol, exchange, side, type, product, qty, price, stop, source);
    f.append(LogField("reason", reason));
    LOG_AUDIT(kPtAudit, logf("Order rejected", f));
}

} // namespace

// ============================================================================
// Leverage / Market-hours Config (Phase 3 §4)
// ============================================================================
//
// Per-portfolio leverage rules and the market-hours enforcement flag are held
// in memory rather than in the pt_portfolios table. The existing schema has no
// columns for them and the task forbids adding a migration / editing CMake, so
// an in-memory map (rebuilt at runtime by callers via the setters) is the
// source of truth. Defaults match OpenAlgo's sandbox.

namespace {

struct PtRuntimeConfig {
    PtLeverageConfig leverage;
    bool enforce_market_hours = false;
    // symbol -> units of the portfolio currency per price unit (pt_set_symbol_fx).
    QHash<QString, double> symbol_fx;
};

QMutex s_config_mutex;
QHash<QString, PtRuntimeConfig>& config_map() {
    static QHash<QString, PtRuntimeConfig> m;
    return m;
}

PtRuntimeConfig config_for(const QString& portfolio_id) {
    QMutexLocker lock(&s_config_mutex);
    return config_map().value(portfolio_id, PtRuntimeConfig{});
}

} // namespace

void pt_set_leverage_config(const QString& portfolio_id, const PtLeverageConfig& cfg) {
    QMutexLocker lock(&s_config_mutex);
    config_map()[portfolio_id].leverage = cfg;
}

PtLeverageConfig pt_get_leverage_config(const QString& portfolio_id) {
    return config_for(portfolio_id).leverage;
}

void pt_set_enforce_market_hours(const QString& portfolio_id, bool enforce) {
    QMutexLocker lock(&s_config_mutex);
    config_map()[portfolio_id].enforce_market_hours = enforce;
}

bool pt_get_enforce_market_hours(const QString& portfolio_id) {
    return config_for(portfolio_id).enforce_market_hours;
}

void pt_set_symbol_fx(const QString& portfolio_id, const QString& symbol, double fx_to_portfolio_ccy) {
    if (!std::isfinite(fx_to_portfolio_ccy) || fx_to_portfolio_ccy <= 0.0)
        return; // never store a factor that would zero or poison cash amounts
    QMutexLocker lock(&s_config_mutex);
    config_map()[portfolio_id].symbol_fx.insert(symbol, fx_to_portfolio_ccy);
}

double pt_get_symbol_fx(const QString& portfolio_id, const QString& symbol) {
    QMutexLocker lock(&s_config_mutex);
    const auto it = config_map().constFind(portfolio_id);
    if (it == config_map().constEnd())
        return 1.0;
    return it->symbol_fx.value(symbol, 1.0);
}

bool pt_has_symbol_fx(const QString& portfolio_id, const QString& symbol) {
    QMutexLocker lock(&s_config_mutex);
    const auto it = config_map().constFind(portfolio_id);
    return it != config_map().constEnd() && it->symbol_fx.contains(symbol);
}

bool pt_portfolio_settles_by_venue(const PtPortfolio& portfolio) {
    return portfolio.exchange == QLatin1String(kBuiltinPaperAccountId);
}

// ============================================================================
// Instrument-type helpers (Phase 3 §4)
// ============================================================================

bool pt_is_option(const QString& symbol) {
    // Canonical option symbols end with CE (call) or PE (put), e.g.
    // "NIFTY28FEB2520000CE". Match case-insensitively on the suffix.
    return symbol.endsWith("CE", Qt::CaseInsensitive) || symbol.endsWith("PE", Qt::CaseInsensitive);
}

bool pt_is_future(const QString& symbol) {
    // Canonical future symbols end with FUT, e.g. "NIFTY28FEB25FUT".
    return symbol.endsWith("FUT", Qt::CaseInsensitive);
}

// ============================================================================
// Margin calculation (Phase 3 §4)
// ============================================================================

namespace {

// Select leverage for a trade based on instrument type, product, exchange and
// side. Mirrors OpenAlgo fund_manager.py::_get_leverage.
double select_leverage(const PtLeverageConfig& cfg, const QString& symbol, const QString& exchange,
                       const QString& product, const QString& side) {
    // Futures take priority (FUT suffix), then options (CE/PE suffix).
    if (pt_is_future(symbol))
        return cfg.futures;
    if (pt_is_option(symbol))
        return (side.compare("buy", Qt::CaseInsensitive) == 0) ? cfg.options_buy : cfg.options_sell;

    // Equity: leverage depends on product. MIS (intraday) gets equity_mis,
    // everything else (CNC/NRML/delivery/blank) gets equity_cnc.
    const QString ex = exchange.toUpper();
    if (ex == "NSE" || ex == "BSE" || ex.isEmpty()) {
        if (product.compare("MIS", Qt::CaseInsensitive) == 0 || product.compare("intraday", Qt::CaseInsensitive) == 0)
            return cfg.equity_mis;
        return cfg.equity_cnc;
    }

    // Unknown exchange: fall back to 1x (full margin) — safest default.
    return 1.0;
}

} // namespace

double pt_calculate_required_margin(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                                    const QString& product, double quantity, double price, const QString& side) {
    if (!std::isfinite(quantity) || quantity <= 0.0 || !std::isfinite(price) || price <= 0.0)
        return 0.0;

    PtLeverageConfig cfg = pt_get_leverage_config(portfolio_id);
    double leverage = select_leverage(cfg, symbol, exchange, product, side);
    if (!std::isfinite(leverage) || leverage <= 0.0)
        leverage = 1.0;

    double trade_value = quantity * price;
    return trade_value / leverage;
}

// ============================================================================
// Exchange hours (Phase 3 §4)
// ============================================================================

bool pt_is_market_open(const QString& exchange) {
    // IST = UTC + 5:30.
    QDateTime ist = QDateTime::currentDateTimeUtc().addSecs(5 * 3600 + 30 * 60);
    QTime t = ist.time();
    const QString ex = exchange.toUpper();

    if (ex == "NSE" || ex == "BSE" || ex == "NFO" || ex == "BFO") {
        // Equity / equity derivatives: 09:15 – 15:30 IST.
        return t >= QTime(9, 15) && t <= QTime(15, 30);
    }
    if (ex == "MCX") {
        // Commodities: 09:00 – 23:30 IST.
        return t >= QTime(9, 0) && t <= QTime(23, 30);
    }
    if (ex == "CDS" || ex == "BCD") {
        // Currency derivatives: 09:00 – 17:00 IST.
        return t >= QTime(9, 0) && t <= QTime(17, 0);
    }

    // Unknown / crypto / international: always open (24/7).
    return true;
}

// ============================================================================
// Portfolio Operations
// ============================================================================

PtPortfolio pt_create_portfolio(const QString& name, double balance, const QString& currency, double leverage,
                                const QString& margin_mode, double fee_rate, const QString& exchange) {
    if (name.isEmpty())
        throw std::runtime_error("Portfolio name cannot be empty");
    if (!std::isfinite(balance) || balance <= 0.0)
        throw std::runtime_error("Invalid balance: must be positive");
    if (!std::isfinite(leverage) || leverage <= 0.0)
        throw std::runtime_error("Invalid leverage: must be positive");
    if (!std::isfinite(fee_rate) || fee_rate < 0.0 || fee_rate > 1.0)
        throw std::runtime_error("Invalid fee_rate: must be between 0 and 1");

    PtPortfolio p;
    p.id = generate_uuid();
    p.name = name;
    p.initial_balance = balance;
    p.balance = balance;
    p.currency = currency;
    p.leverage = leverage;
    p.margin_mode = margin_mode;
    p.fee_rate = fee_rate;
    p.exchange = exchange;
    p.created_at = now_rfc3339();

    auto r = repo().insert_portfolio(p);
    if (r.is_err())
        throw std::runtime_error(r.error());
    return p;
}

PtPortfolio pt_get_portfolio(const QString& id) {
    auto r = repo().get_portfolio(id);
    if (r.is_err())
        throw std::runtime_error(r.error());
    return r.value();
}

void pt_set_balance(const QString& portfolio_id, double new_balance) {
    if (!std::isfinite(new_balance) || new_balance < 0.0)
        throw std::runtime_error("Invalid balance: must be finite and non-negative");
    auto r = repo().update_balance(portfolio_id, new_balance);
    if (r.is_err())
        throw std::runtime_error(r.error());
    LOG_AUDIT(kPtAudit, "Paper balance set",
              {{"venue", "paper"}, {"mode", "paper"}, {"portfolio", log_mask(portfolio_id)}, {"balance", new_balance}});
}

std::optional<PtPortfolio> pt_find_portfolio(const QString& name, const QString& exchange) {
    return repo().find_portfolio(name, exchange);
}

QVector<PtPortfolio> pt_list_portfolios(const QString& exchange) {
    auto r = repo().list_portfolios(exchange);
    if (r.is_err()) {
        LOG_ERROR_RL("PaperTrading", QStringLiteral("list_portfolios"), 60000,
                     logf("Paper portfolios could not be listed", {{"error", QString::fromStdString(r.error())}}));
        return {};
    }
    return r.value();
}

void pt_delete_portfolio(const QString& id) {
    repo().delete_all_trades(id);
    repo().cancel_all_orders(id);
    repo().delete_all_positions(id);
    repo().delete_portfolio(id);
}

PtPortfolio pt_reset_portfolio(const QString& id) {
    pt_get_portfolio(id); // verify exists
    repo().delete_all_trades(id);
    repo().cancel_all_orders(id);
    repo().delete_all_positions(id);
    repo().delete_all_margin_blocks(id); // clear any blocked margin (Phase 3 §4)
    // Reset balance to initial
    auto p = pt_get_portfolio(id);
    repo().update_balance(id, p.initial_balance);
    return pt_get_portfolio(id);
}

// ============================================================================
// Order Operations
// ============================================================================

// Persist a rejected order record (best-effort) then throw, so the rejection is
// both visible in the order book and surfaced to callers via the existing
// exception-based contract.
[[noreturn]] static void reject_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                                      const QString& order_type, double quantity, std::optional<double> price,
                                      std::optional<double> stop_price, bool reduce_only, const QString& product,
                                      const QString& exchange, const std::string& reason,
                                      const QString& source = QString()) {
    PtOrder rej;
    rej.id = generate_uuid();
    rej.portfolio_id = portfolio_id;
    rej.symbol = symbol;
    rej.side = side;
    rej.order_type = order_type;
    rej.quantity = quantity;
    rej.price = price;
    rej.stop_price = stop_price;
    rej.filled_qty = 0.0;
    rej.status = "rejected";
    rej.reduce_only = reduce_only;
    rej.margin_blocked = 0.0;
    rej.product = product;
    rej.exchange = exchange;
    rej.created_at = now_rfc3339();
    repo().insert_order(rej); // best-effort; ignore failure
    pt_audit_rejected(portfolio_id, symbol, exchange, side, order_type, product, quantity, price, stop_price,
                      QString::fromStdString(reason), source);
    throw std::runtime_error(reason);
}

PtOrder pt_place_order(const QString& portfolio_id, const QString& symbol, const QString& side,
                       const QString& order_type, double quantity, std::optional<double> price,
                       std::optional<double> stop_price, bool reduce_only, const QString& exchange,
                       const QString& product, const QString& source) {
    // Normalize order-type vocabulary. UnifiedOrder serializes via order_type_str()
    // as "stop_loss"/"stop_loss_limit", but the paper engine's canonical spellings
    // are "stop"/"stop_limit" (what OrderMatcher matches on). Accept both and store
    // the canonical form so SL / SL-Limit paper orders are not rejected here.
    QString type = order_type;
    if (type == "stop_loss")
        type = "stop";
    else if (type == "stop_loss_limit")
        type = "stop_limit";

    // Validation
    // Every refusal below is audited before it is thrown (same text as the exception).
    const auto audit_refusal = [&](const char* reason) {
        pt_audit_rejected(portfolio_id, symbol, exchange, side, type, product, quantity, price, stop_price,
                          QString::fromLatin1(reason), source);
    };
    if (type != "market" && type != "limit" && type != "stop" && type != "stop_limit") {
        audit_refusal("Invalid order type");
        throw std::runtime_error("Invalid order type: " + type.toStdString());
    }
    if (side != "buy" && side != "sell") {
        audit_refusal("Invalid side");
        throw std::runtime_error("Invalid side: " + side.toStdString());
    }
    if (!std::isfinite(quantity) || quantity <= 0.0) {
        audit_refusal("Invalid quantity");
        throw std::runtime_error("Invalid quantity");
    }
    if (price && (!std::isfinite(*price) || *price <= 0.0)) {
        audit_refusal("Invalid price");
        throw std::runtime_error("Invalid price");
    }
    if (stop_price && (!std::isfinite(*stop_price) || *stop_price <= 0.0)) {
        audit_refusal("Invalid stop price");
        throw std::runtime_error("Invalid stop price");
    }
    if (type == "limit" && !price) {
        audit_refusal("Limit order requires price");
        throw std::runtime_error("Limit order requires price");
    }
    if ((type == "stop" || type == "stop_limit") && !stop_price) {
        audit_refusal("Stop order requires stop_price");
        throw std::runtime_error("Stop order requires stop_price");
    }

    // Serialize the balance read-modify-write against concurrent fills/cancels.
    // pt_fill_order / pt_cancel_order already hold s_fill_mutex while they read
    // and write portfolio.balance; without this lock a fill landing between our
    // balance read (here) and the margin-block write (below) has its balance
    // delta silently reverted. Safe: the helpers called under the lock
    // (pt_get_portfolio, pt_calculate_required_margin, pt_is_market_open) never
    // take s_fill_mutex — pt_convert_position_product already calls them under
    // it — and reject_order's throw releases the lock via RAII.
    QMutexLocker fill_lock(&s_fill_mutex);

    auto portfolio = pt_get_portfolio(portfolio_id);

    // Exchange-hours enforcement — opt-in per portfolio (default off preserves
    // existing behavior). Only enforced when an exchange is supplied.
    if (pt_get_enforce_market_hours(portfolio_id) && !exchange.isEmpty() && !pt_is_market_open(exchange)) {
        reject_order(portfolio_id, symbol, side, type, quantity, price, stop_price, reduce_only, product, exchange,
                     ("Market closed for exchange " + exchange).toStdString(), source);
    }

    // Margin blocking — skip for reduce_only orders and for the portion of an
    // order that closes/reduces an existing opposite position. Only the net new
    // exposure consumes margin.
    double margin_to_block = 0.0;
    if (!reduce_only) {
        QString opposite_side = (side == "buy") ? "short" : "long";
        auto opposite_pos = repo().find_position(portfolio_id, symbol, opposite_side);
        double net_new_qty = quantity;
        if (opposite_pos) {
            // Subtract the quantity that will close the existing position
            net_new_qty = std::max(0.0, quantity - opposite_pos->quantity);
        }

        if (net_new_qty > 0.0) {
            double ref = price.value_or(stop_price.value_or(0.0));
            if (ref <= 0.0) {
                audit_refusal("Market orders require a reference price for margin calculation");
                throw std::runtime_error("Market orders require a reference price for margin calculation");
            }

            // Per-product / per-instrument leverage. If exchange/product are not
            // supplied, select_leverage falls back to the equity rules /
            // portfolio default, and for blank exchange the equity CNC path.
            // Margin is cash: in the portfolio currency (pt_set_symbol_fx; 1.0 unless set).
            margin_to_block = pt_calculate_required_margin(portfolio_id, symbol, exchange, product, net_new_qty, ref,
                                                           side) *
                              pt_get_symbol_fx(portfolio_id, symbol);

            if (margin_to_block > portfolio.balance) {
                reject_order(portfolio_id, symbol, side, type, quantity, price, stop_price, reduce_only, product,
                             exchange, "Insufficient margin", source);
            }
        }
    }

    PtOrder order;
    order.id = generate_uuid();
    order.portfolio_id = portfolio_id;
    order.symbol = symbol;
    order.side = side;
    order.order_type = type;
    order.quantity = quantity;
    order.price = price;
    order.stop_price = stop_price;
    order.filled_qty = 0.0;
    order.status = "pending";
    order.reduce_only = reduce_only;
    order.margin_blocked = margin_to_block;
    order.product = product;
    order.exchange = exchange;
    order.created_at = now_rfc3339();

    // The order row, the balance debit and the margin-block row land together: an
    // order whose margin was never recorded could not release it on fill/cancel.
    PtWriteTransaction tx("Paper order placement");
    pt_require(repo().insert_order(order), "insert order");

    // Block the margin from available balance and record it against the order so
    // the exact amount can be released on fill/cancel.
    if (margin_to_block > 0.0) {
        pt_require(repo().update_balance(portfolio_id, portfolio.balance - margin_to_block), "block margin (balance)");
        pt_require(repo().insert_margin_block(generate_uuid(), portfolio_id, order.id, symbol, margin_to_block),
                   "block margin (record)");
    }
    tx.commit();

    {
        auto f =
            pt_audit_fields(portfolio_id, symbol, exchange, side, type, product, quantity, price, stop_price, source);
        f.append(LogField("order_id", order.id));
        if (reduce_only)
            f.append(LogField("reduce_only", true));
        if (margin_to_block > 0.0)
            f.append(LogField("margin_blocked", margin_to_block));
        LOG_AUDIT(kPtAudit, logf("Order accepted", f));
    }
    return order;
}

void pt_cancel_order(const QString& order_id) {
    // Release any margin blocked when the order was placed back to available
    // balance (no realized P&L — the order never filled). Serialized under the
    // fill mutex so a concurrent fill can't double-release the same block.
    QMutexLocker lock(&s_fill_mutex);

    // Only a resting order can be cancelled. repo().cancel_order() is an
    // unconditional "SET status='cancelled'", so without this gate cancelling an
    // already-FILLED order (stale blotter row, double click, MCP tool, workflow
    // node) silently rewrote its history to "cancelled" while the position and the
    // trade it produced stayed on the books. An unknown id stays a silent no-op,
    // exactly as before.
    auto ord = repo().get_order(order_id);
    if (ord.is_err()) {
        // Unknown id or unreadable row: nothing is cancelled (the caller still reports success).
        LOG_WARN("PaperTrading", logf("Paper cancel skipped, order could not be read",
                                      {{"order_id", order_id}, {"error", QString::fromStdString(ord.error())}}));
        return;
    }
    if (ord.value().status != "pending" && ord.value().status != "partial") {
        LOG_DEBUG("PaperTrading", QString("pt_cancel_order: order %1 is %2 — nothing to cancel")
                                      .arg(order_id, ord.value().status));
        return;
    }

    // Release + cancel are one unit: a released block on an order that stayed
    // pending (or a cancelled order whose block was never released) would leave
    // the cash wrong. A failure now throws instead of passing silently.
    double blocked = repo().get_margin_block(order_id);
    PtWriteTransaction tx("Paper order cancel");
    if (blocked > 0.0) {
        auto portfolio = pt_get_portfolio(ord.value().portfolio_id);
        pt_require(repo().update_balance(portfolio.id, portfolio.balance + blocked), "release margin (balance)");
        pt_require(repo().delete_margin_block(order_id), "release margin (record)");
    }

    pt_require(repo().cancel_order(order_id), "cancel order");
    tx.commit();
    LOG_AUDIT(kPtAudit, "Order cancelled",
              {{"venue", "paper"},
               {"mode", "paper"},
               {"portfolio", log_mask(ord.value().portfolio_id)},
               {"order_id", order_id},
               {"symbol", ord.value().symbol},
               {"side", ord.value().side},
               {"qty", ord.value().quantity - ord.value().filled_qty},
               {"margin_released", blocked}});
}

QVector<PtOrder> pt_get_orders(const QString& portfolio_id, const QString& status) {
    auto r = repo().get_orders(portfolio_id, status);
    if (r.is_err()) {
        LOG_ERROR_RL("PaperTrading", QStringLiteral("get_orders|") + portfolio_id, 60000,
                     logf("Paper orders could not be read", {{"portfolio", log_mask(portfolio_id)},
                                                             {"status", status},
                                                             {"error", QString::fromStdString(r.error())}}));
        return {};
    }
    return r.value();
}

// ============================================================================
// Fill Order — The Core Engine
// ============================================================================

PtTrade pt_fill_order(const QString& order_id, double fill_price, std::optional<double> fill_qty,
                      const QString& fill_time) {
    if (!std::isfinite(fill_price) || fill_price <= 0.0)
        throw std::runtime_error("Invalid fill price");
    if (fill_qty && (!std::isfinite(*fill_qty) || *fill_qty <= 0.0))
        throw std::runtime_error("Invalid fill quantity");

    // Serialize fills to prevent concurrent corruption of balance/positions
    QMutexLocker fill_lock(&s_fill_mutex);

    // 1. Get order
    auto or1 = repo().get_order(order_id);
    if (or1.is_err())
        throw std::runtime_error(or1.error());
    PtOrder order = or1.value();

    if (order.status != "pending" && order.status != "partial")
        throw std::runtime_error("Order not fillable");

    // 2. Get portfolio
    PtPortfolio portfolio = pt_get_portfolio(order.portfolio_id);

    // 3. Fill qty and fee
    double qty = fill_qty.value_or(order.quantity - order.filled_qty);
    if (qty <= 0.0)
        throw std::runtime_error("Nothing left to fill");

    QString position_side = (order.side == "buy") ? "long" : "short";
    QString opposite_side = (order.side == "buy") ? "short" : "long";

    // Reduce-only: the order may only shrink an existing opposite position — it
    // must never open or flip one. pt_place_order skips the margin check for
    // reduce-only orders, so letting the closing leg's surplus fall through to the
    // opening leg below would open an UNCHECKED opposite position (e.g. a
    // reduce-only sell of 10 against a long of 1 closed the 1 and opened a 9-lot
    // short, charging margin that was never validated). Cap the fill at the
    // position size and treat the order as complete (exchanges cancel the
    // surplus); with nothing to reduce, cancel the order and refuse the fill.
    bool reduce_capped = false;
    if (order.reduce_only) {
        const auto reducible_pos = repo().find_position(order.portfolio_id, order.symbol, opposite_side);
        const double reducible = reducible_pos ? reducible_pos->quantity : 0.0;
        if (reducible <= 0.0) {
            repo().cancel_order(order_id);
            LOG_AUDIT(kPtAudit, "Order cancelled",
                      {{"venue", "paper"},
                       {"mode", "paper"},
                       {"portfolio", log_mask(order.portfolio_id)},
                       {"order_id", order_id},
                       {"symbol", order.symbol},
                       {"side", order.side},
                       {"reason", "reduce-only order has no position to reduce"}});
            throw std::runtime_error("Reduce-only order has no position to reduce");
        }
        if (qty > reducible) {
            qty = reducible;
            reduce_capped = true;
        }
    }

    // Cash amounts (fee, margin, realized P&L) are booked in the portfolio currency;
    // prices stay in the instrument's. 1.0 unless pt_set_symbol_fx was used.
    const double fx = pt_get_symbol_fx(order.portfolio_id, order.symbol);
    double fee = qty * fill_price * portfolio.fee_rate * fx;
    QString now = fill_time.isEmpty() ? now_rfc3339() : fill_time;

    double pnl = 0.0;
    double new_filled = order.filled_qty + qty;
    bool fully_filled = (new_filled >= order.quantity) || reduce_capped;

    // ── One transaction: every write below lands, or none does ─────────
    // A failed BEGIN, any failed write (pt_require) or a failed COMMIT throws and
    // the guard rolls the whole fill back.
    PtWriteTransaction tx("Paper fill");
    {
        // Net change to available cash from this fill. Margin model (v040):
        //   • Opening exposure KEEPS its margin locked on the position (held_margin)
        //     — it is NOT released to cash on fill. This is what finally makes
        //     available balance drop when a position is opened (the old code released
        //     the block on fill, so an open position locked no cash — "stuck at 10L").
        //   • Closing/reducing a position RELEASES that position's held margin back
        //     to cash, proportionally, plus realized P&L.
        //   • Brokerage (fee) is charged on every fill.
        double balance_delta = -fee;
        const double order_blocked = repo().get_margin_block(order_id);
        double block_consumed = 0.0; // portion of the order's block turned into position margin

        // ── Closing leg: net against an opposite-side position ──
        double close_qty = 0.0;
        auto opp = repo().find_position(order.portfolio_id, order.symbol, opposite_side);
        if (opp) {
            PtPosition pos = *opp;
            close_qty = std::min(qty, pos.quantity);
            pnl = ((pos.side == "long") ? (fill_price - pos.entry_price) * close_qty
                                        : (pos.entry_price - fill_price) * close_qty) *
                  fx;
            const double frac_close = pos.quantity > 0.0 ? (close_qty / pos.quantity) : 1.0;
            const double margin_released = pos.held_margin * frac_close;
            if (close_qty >= pos.quantity) {
                pt_require(repo().delete_position(pos.id), "close position");
            } else {
                pt_require(repo().update_position(pos.id, pos.quantity - close_qty, pos.entry_price),
                           "reduce position");
                pt_require(repo().set_position_margin(pos.id, pos.held_margin - margin_released),
                           "release position margin");
                pt_require(repo().add_realized_pnl(pos.id, pnl), "record realized P&L");
            }
            balance_delta += margin_released + pnl;
        }

        // ── Opening leg: net-new exposure on the order's own side ──
        const double open_qty = qty - close_qty;
        if (open_qty > 0.0) {
            // Margin the new exposure must lock, per product / leverage at fill price.
            double open_margin = pt_calculate_required_margin(order.portfolio_id, order.symbol, order.exchange,
                                                              order.product, open_qty, fill_price, order.side) *
                                 fx;
            // That margin was pre-blocked on the order at placement: convert the
            // block into position margin (release the block, re-lock the actual).
            block_consumed = fully_filled ? order_blocked : std::min(order_blocked, open_margin);
            balance_delta += block_consumed - open_margin;

            const QString product = order.product.isEmpty() ? QStringLiteral("MIS") : order.product;
            auto same = repo().find_position(order.portfolio_id, order.symbol, position_side);
            if (same) {
                PtPosition pos = *same;
                double new_qty = pos.quantity + open_qty;
                if (new_qty <= 0.0)
                    throw std::runtime_error("Invalid position quantity after averaging");
                double new_entry = (pos.entry_price * pos.quantity + fill_price * open_qty) / new_qty;
                pt_require(repo().update_position(pos.id, new_qty, new_entry), "add to position");
                pt_require(repo().set_position_margin(pos.id, pos.held_margin + open_margin), "lock position margin");
            } else {
                PtPosition np;
                np.id = generate_uuid();
                np.portfolio_id = order.portfolio_id;
                np.symbol = order.symbol;
                np.side = position_side;
                np.quantity = open_qty;
                np.entry_price = fill_price;
                np.current_price = fill_price;
                np.leverage = open_margin > 0.0 ? (open_qty * fill_price) / open_margin : portfolio.leverage;
                np.opened_at = now;
                np.product = product;
                np.held_margin = open_margin;
                pt_require(repo().insert_position(np), "open position");
            }
        }

        // ── Settle the order's margin block ──
        if (order_blocked > 0.0) {
            if (fully_filled) {
                // A fill that only closed existing exposure (open_qty == 0) never
                // reached the opening leg, so none of this order's pre-blocked margin
                // was converted or handed back — dropping the row here without
                // crediting it leaked the whole block out of cash for good.
                if (open_qty <= 0.0)
                    balance_delta += order_blocked;
                pt_require(repo().delete_margin_block(order_id), "settle order margin");
            } else {
                double remaining = std::max(0.0, order_blocked - block_consumed);
                pt_require(
                    repo().insert_margin_block(generate_uuid(), order.portfolio_id, order_id, order.symbol, remaining),
                    "keep remaining order margin");
            }
        }

        pt_require(repo().update_balance(order.portfolio_id, portfolio.balance + balance_delta), "update balance");

        // 8. Update order
        QString new_status = fully_filled ? "filled" : "partial";
        double prev_avg = order.avg_price.value_or(0.0);
        double new_avg =
            (new_filled > 0.0) ? (prev_avg * order.filled_qty + fill_price * qty) / new_filled : fill_price;
        pt_require(repo().update_order_fill(order_id, new_filled, new_avg, new_status, now), "update order");

        // 9. Create trade record
        PtTrade trade;
        trade.id = generate_uuid();
        trade.portfolio_id = order.portfolio_id;
        trade.order_id = order_id;
        trade.symbol = order.symbol;
        trade.side = order.side;
        trade.price = fill_price;
        trade.quantity = qty;
        trade.fee = fee;
        trade.pnl = pnl;
        trade.timestamp = now;
        pt_require(repo().insert_trade(trade), "record trade");

        tx.commit();
        LOG_AUDIT(kPtAudit, "Order filled",
                  {{"venue", "paper"},
                   {"mode", "paper"},
                   {"portfolio", log_mask(order.portfolio_id)},
                   {"order_id", order_id},
                   {"symbol", order.symbol},
                   {"side", order.side},
                   {"qty", qty},
                   {"price", fill_price},
                   {"status", new_status},
                   {"fee", fee},
                   {"pnl", pnl}});

        // Emit event (outside transaction — non-critical)
        EventBus::instance().publish("paper_trading.order_filled", {{"trade_id", trade.id},
                                                                    {"portfolio_id", trade.portfolio_id},
                                                                    {"symbol", trade.symbol},
                                                                    {"side", trade.side},
                                                                    {"price", trade.price},
                                                                    {"quantity", trade.quantity},
                                                                    {"pnl", trade.pnl},
                                                                    {"fee", trade.fee}});

        return trade;
    }
}

// ============================================================================
// Position & Trade Queries
// ============================================================================

QVector<PtPosition> pt_get_positions(const QString& portfolio_id) {
    auto r = repo().get_positions(portfolio_id);
    if (r.is_err()) {
        LOG_ERROR_RL("PaperTrading", QStringLiteral("get_positions|") + portfolio_id, 60000,
                     logf("Paper positions could not be read", {{"portfolio", log_mask(portfolio_id)},
                                                                {"error", QString::fromStdString(r.error())}}));
        return {};
    }
    return r.value();
}

void pt_update_position_price(const QString& portfolio_id, const QString& symbol, double price) {
    repo().update_position_price(portfolio_id, symbol, price);
}

QVector<PtTrade> pt_get_trades(const QString& portfolio_id, int64_t limit) {
    auto r = repo().get_trades(portfolio_id, limit);
    if (r.is_err()) {
        LOG_WARN_RL("PaperTrading", QStringLiteral("get_trades|") + portfolio_id, 60000,
                    logf("Paper trades could not be read", {{"portfolio", log_mask(portfolio_id)},
                                                            {"error", QString::fromStdString(r.error())}}));
        return {};
    }
    return r.value();
}

PtStats pt_get_stats(const QString& portfolio_id) {
    auto r = repo().get_stats(portfolio_id);
    if (r.is_err()) {
        LOG_WARN_RL("PaperTrading", QStringLiteral("get_stats|") + portfolio_id, 60000,
                    logf("Paper stats could not be read", {{"portfolio", log_mask(portfolio_id)},
                                                           {"error", QString::fromStdString(r.error())}}));
        return {};
    }
    return r.value();
}

// ============================================================================
// Day-scoped queries / Settlement / Product conversion (v040)
// ============================================================================

namespace {

// IST = UTC + 5:30. The UTC instant at which the given IST calendar day begins.
QDateTime ist_day_start_utc(const QDate& ist_day) {
    QDateTime utc_midnight(ist_day, QTime(0, 0, 0), QTimeZone::UTC);
    return utc_midnight.addSecs(-330 * 60); // shift IST-midnight back to its UTC instant
}

// The IST calendar date of a stored (naive/Z UTC) ISO timestamp.
QDate ist_date_of(const QString& iso_utc, const QDate& fallback) {
    QDateTime dt = QDateTime::fromString(iso_utc, Qt::ISODate);
    if (!dt.isValid())
        return fallback;
    dt.setTimeZone(QTimeZone::UTC); // timestamps are UTC; reinterpret if no zone was parsed
    return dt.addSecs(330 * 60).date();
}

} // namespace

QVector<PtOrder> pt_get_orders_for_day(const QString& portfolio_id, const QDate& ist_day) {
    const QDateTime start = ist_day_start_utc(ist_day);
    auto r =
        repo().get_orders_between(portfolio_id, start.toString(Qt::ISODate), start.addDays(1).toString(Qt::ISODate));
    return r.is_ok() ? r.value() : QVector<PtOrder>{};
}

QVector<PtTrade> pt_get_trades_for_day(const QString& portfolio_id, const QDate& ist_day) {
    const QDateTime start = ist_day_start_utc(ist_day);
    auto r =
        repo().get_trades_between(portfolio_id, start.toString(Qt::ISODate), start.addDays(1).toString(Qt::ISODate));
    return r.is_ok() ? r.value() : QVector<PtTrade>{};
}

void pt_convert_position_product(const QString& position_id, const QString& new_product) {
    QMutexLocker lock(&s_fill_mutex);

    auto pr = repo().get_position(position_id);
    if (pr.is_err())
        throw std::runtime_error(pr.error());
    PtPosition pos = pr.value();
    if (pos.product.compare(new_product, Qt::CaseInsensitive) == 0)
        return; // already that product — no-op

    auto portfolio = pt_get_portfolio(pos.portfolio_id);

    // Margin the position would lock under the NEW product, at its entry basis.
    // (CNC/delivery = full notional at 1x; MIS = notional/leverage.)
    const QString side = (pos.side == "long") ? "buy" : "sell";
    const double new_margin = pt_calculate_required_margin(pos.portfolio_id, pos.symbol, QString(), new_product,
                                                           pos.quantity, pos.entry_price, side) *
                              pt_get_symbol_fx(pos.portfolio_id, pos.symbol);
    const double extra = new_margin - pos.held_margin; // additional cash to lock (CNC needs more than MIS)
    if (extra > portfolio.balance + 1e-6)
        throw std::runtime_error(("Insufficient balance to convert to " + new_product).toStdString());

    PtWriteTransaction tx("Paper product conversion");
    pt_require(repo().update_balance(pos.portfolio_id, portfolio.balance - extra), "update balance");
    pt_require(repo().set_position_margin(position_id, new_margin), "set position margin");
    pt_require(repo().set_position_product(position_id, new_product), "set position product");
    tx.commit();
}

int pt_settle_intraday(const QString& portfolio_id) {
    // Intraday auto-square applies ONLY to Indian (INR) equity portfolios. Crypto
    // and US (USD) paper portfolios trade on other schedules and must never be
    // squared at the IST 15:30 cutoff (this previously closed month-old crypto
    // positions because their product had defaulted to MIS).
    PtPortfolio portfolio;
    try {
        portfolio = pt_get_portfolio(portfolio_id);
    } catch (...) {
        LOG_WARN("PaperTrading", logf("Auto-square skipped, portfolio could not be read",
                                      {{"portfolio", log_mask(portfolio_id)}}));
        return 0;
    }
    if (portfolio.currency.compare("INR", Qt::CaseInsensitive) != 0)
        return 0;
    // The built-in paper account trades every venue: an INR account holding AAPL
    // must not be squared at 15:30 IST. pt_settle_delayed_sessions settles it per venue.
    if (pt_portfolio_settles_by_venue(portfolio))
        return 0;

    const QDateTime ist_now = QDateTime::currentDateTimeUtc().addSecs(330 * 60);
    const QDate ist_today = ist_now.date();
    const bool past_close = ist_now.time() >= QTime(15, 30);

    int squared = 0;
    const auto positions = pt_get_positions(portfolio_id);
    for (const auto& pos : positions) {
        if (!product_is_intraday(pos.product))
            continue; // CNC / NRML carry forward
        const QDate opened_ist = ist_date_of(pos.opened_at, ist_today);
        const bool from_prior_day = opened_ist < ist_today;
        if (!past_close && !from_prior_day)
            continue; // still within today's session and not yet 15:30

        const double price = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
        if (price <= 0.0)
            continue; // can't price the square-off

        // Stamp the square-off at the session close it actually belongs to: 15:30
        // IST on the day the position was opened. For a same-day position past the
        // cutoff that's today's 15:30; for a carried-over position it's that prior
        // day's close — so a catch-up auto-square lands in THAT day's book, never
        // polluting today's order list with trades the user didn't place.
        const QDateTime close_utc = QDateTime(opened_ist, QTime(15, 30, 0), QTimeZone::UTC).addSecs(-330 * 60);
        const QString close_iso = close_utc.toString(Qt::ISODate);

        // Square off = insert a reduce-only market order on the opposite side and
        // fill it at the last price. Reuses the fill engine's closing leg (margin
        // release + realized P&L + trade record), and leaves an auto-square order
        // in the book just like a real broker's 3:30 cutoff.
        try {
            PtOrder o;
            o.id = generate_uuid();
            o.portfolio_id = portfolio_id;
            o.symbol = pos.symbol;
            o.side = (pos.side == "long") ? QStringLiteral("sell") : QStringLiteral("buy");
            o.order_type = "market";
            o.quantity = pos.quantity;
            o.price = price;
            o.filled_qty = 0.0;
            o.status = "pending";
            o.reduce_only = true;
            o.margin_blocked = 0.0;
            o.product = pos.product;
            o.created_at = close_iso;
            auto ir = repo().insert_order(o);
            if (ir.is_err()) {
                LOG_ERROR("PaperTrading", logf("Auto-square order could not be saved",
                                               {{"portfolio", log_mask(portfolio_id)},
                                                {"symbol", pos.symbol},
                                                {"error", QString::fromStdString(ir.error())}}));
                continue;
            }
            {
                auto f = pt_audit_fields(portfolio_id, o.symbol, QString(), o.side, o.order_type, o.product,
                                         o.quantity, o.price, std::nullopt);
                f.append(LogField("order_id", o.id));
                f.append(LogField("source", "auto_square_off"));
                LOG_AUDIT(kPtAudit, logf("Order accepted", f));
            }
            pt_fill_order(o.id, price, std::nullopt, close_iso);
            ++squared;
        } catch (const std::exception& e) {
            LOG_WARN("PaperTrading", QString("Auto-square failed for %1: %2").arg(pos.symbol, e.what()));
        }
    }

    // Stale DAY orders from earlier IST sessions expire at the close — cancel them
    // so they don't linger as pending or fire on a later day.
    const auto pending = pt_get_orders(portfolio_id, "pending");
    for (const auto& o : pending) {
        if (ist_date_of(o.created_at, ist_today) < ist_today) {
            try {
                pt_cancel_order(o.id);
            } catch (...) {
                LOG_WARN("PaperTrading", logf("Stale pending order could not be cancelled", {{"order_id", o.id}}));
            }
        }
    }
    return squared;
}

int pt_settle_intraday_all() {
    int total = 0;
    for (const auto& p : pt_list_portfolios())
        total += pt_settle_intraday(p.id);
    return total;
}

// ============================================================================
// Delayed-price simulation (built-in paper account)
// ============================================================================

bool PtDelayedQuote::valid() const {
    return std::isfinite(price) && price > 0.0;
}

namespace {

QString pt_sim_tr(const char* text) {
    return QCoreApplication::translate("PaperTrading", text);
}

QString pt_sim_age(qint64 ms) {
    const qint64 s = std::max<qint64>(0, ms / 1000);
    if (s < 120)
        return pt_sim_tr("%1 s").arg(s);
    return pt_sim_tr("%1 min").arg(s / 60);
}

QString pt_sim_normalise_type(const QString& t) {
    if (t == QLatin1String("stop_loss"))
        return QStringLiteral("stop");
    if (t == QLatin1String("stop_loss_limit"))
        return QStringLiteral("stop_limit");
    return t;
}

QDateTime pt_sim_parse_utc(const QString& iso) {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (dt.isValid())
        dt.setTimeZone(QTimeZone::UTC); // the engine writes UTC; a naive stamp is UTC too
    return dt;
}

std::optional<PtTrade> pt_sim_trade_of(const QString& portfolio_id, const QString& order_id) {
    for (const auto& t : pt_get_trades(portfolio_id, 20))
        if (t.order_id == order_id)
            return t;
    return std::nullopt;
}

} // namespace

PtSimResult pt_submit_delayed_order(const PtSimRequest& r, const PtDelayedQuote& q, qint64 now_ms,
                                    qint64 max_quote_age_ms) {
    PtSimResult res;
    const QString type = pt_sim_normalise_type(r.order_type);
    const bool is_market = (type == QLatin1String("market"));
    const bool fresh = q.valid() && q.received_ms > 0 && (now_ms - q.received_ms) <= max_quote_age_ms;

    if (std::isfinite(r.fx_to_account) && r.fx_to_account > 0.0)
        pt_set_symbol_fx(r.portfolio_id, r.symbol, r.fx_to_account);

    // Refusals of the delayed-price simulation are audited like the engine's own.
    const auto audit_sim_refusal = [&](const char* reason) {
        pt_audit_rejected(r.portfolio_id, r.symbol, r.exchange, r.side, type, r.product, r.quantity, r.price,
                          r.stop_price, QString::fromLatin1(reason), r.source);
    };
    if (is_market) {
        if (!q.valid()) {
            audit_sim_refusal("no delayed price has arrived yet for a market order");
            res.message = pt_sim_tr("No delayed price for %1 has arrived yet, so a MARKET order cannot be filled. Wait "
                                    "for its quote, or place a LIMIT order.")
                              .arg(r.symbol);
            return res;
        }
        if (q.session_open && !fresh) {
            audit_sim_refusal("the delayed price feed is not updating (stale quote) for a market order");
            res.message = pt_sim_tr("The latest delayed price for %1 was received %2 ago (limit %3) — the price feed "
                                    "is not updating, so a MARKET order cannot be filled. Use a LIMIT order, or try "
                                    "again when prices update.")
                              .arg(r.symbol, pt_sim_age(now_ms - q.received_ms), pt_sim_age(max_quote_age_ms));
            return res;
        }
    }

    // A MARKET order carries the delayed price as its margin reference (and, while
    // the session is open, its fill price).
    std::optional<double> price = r.price;
    if (is_market)
        price = q.price;

    PtOrder order;
    try {
        order = pt_place_order(r.portfolio_id, r.symbol, r.side, type, r.quantity, price, r.stop_price, r.reduce_only,
                               r.exchange, r.product, r.source);
    } catch (const std::exception& e) {
        res.message = pt_sim_tr("Order rejected: %1").arg(QString::fromUtf8(e.what()));
        return res;
    }
    res.order = order;

    if (is_market && q.session_open) {
        try {
            res.trade = pt_fill_order(order.id, q.price);
            res.order.status = QStringLiteral("filled");
            res.order.filled_qty = order.quantity;
            res.order.avg_price = q.price;
            res.outcome = PtSimOutcome::Filled;
            res.message = pt_sim_tr("Simulated fill: %1 %2 %3 @ %4 — the latest delayed price, not a real execution.")
                              .arg(r.side.toUpper())
                              .arg(r.quantity)
                              .arg(r.symbol)
                              .arg(q.price, 0, 'f', 4);
        } catch (const std::exception& e) {
            LOG_WARN("PaperTrading", logf("Simulated fill failed, order cancelled",
                                          {{"order_id", order.id}, {"symbol", r.symbol}, {"error", e.what()}}));
            try {
                pt_cancel_order(order.id);
            } catch (...) {
                LOG_WARN("PaperTrading", logf("Rejected paper order could not be cancelled, margin may stay blocked",
                                              {{"order_id", order.id}}));
            }
            res.outcome = PtSimOutcome::Rejected;
            res.message = pt_sim_tr("The simulated fill failed and the order was cancelled: %1")
                              .arg(QString::fromUtf8(e.what()));
        }
        return res;
    }

    OrderMatcher::instance().add_order(order);
    if (is_market) {
        res.outcome = PtSimOutcome::QueuedForOpen;
        res.message = pt_sim_tr("Market closed — %1 order for %2 queued. It fills at the first delayed price printed "
                                "after the open.")
                          .arg(r.side.toUpper(), r.symbol);
        return res;
    }

    // Resting order: while the session is open on a fresh delayed price, give the
    // matcher one look at the new order alone — a marketable limit / triggered stop
    // fills now by OrderMatcher's rules.
    if (q.session_open && fresh) {
        PriceData pd;
        pd.last = q.price;
        pd.timestamp = q.printed_ms;
        OrderMatcher::instance().check_orders(r.symbol, pd, r.portfolio_id, /*printed_at_ms=*/0, order.id);
        auto stored = PaperTradingRepository::instance().get_order(order.id);
        if (stored.is_ok() && stored.value().status == QLatin1String("filled")) {
            res.order = stored.value();
            res.trade = pt_sim_trade_of(r.portfolio_id, order.id);
            res.outcome = PtSimOutcome::Filled;
            res.message = pt_sim_tr("Simulated fill: %1 %2 %3 @ %4 on the latest delayed price — not a real execution.")
                              .arg(r.side.toUpper())
                              .arg(r.quantity)
                              .arg(r.symbol)
                              .arg(res.order.avg_price.value_or(q.price), 0, 'f', 4);
            return res;
        }
    }
    res.outcome = PtSimOutcome::Resting;
    res.message = q.session_open
                      ? pt_sim_tr("%1 %2 order for %3 is resting; it is checked against every new delayed price.")
                            .arg(type.toUpper(), r.side.toUpper(), r.symbol)
                      : pt_sim_tr("Market closed — %1 %2 order for %3 is resting and is checked from the open.")
                            .arg(type.toUpper(), r.side.toUpper(), r.symbol);
    return res;
}

int pt_match_delayed_quote(const QString& portfolio_id, const QString& symbol, const PtDelayedQuote& q) {
    if (!q.valid() || !q.session_open)
        return 0; // nothing fills outside the regular session
    auto& matcher = OrderMatcher::instance();
    const int before = int(matcher.get_pending_orders(symbol, portfolio_id).size());
    PriceData pd;
    pd.last = q.price;
    pd.timestamp = q.printed_ms;
    matcher.check_orders(symbol, pd, portfolio_id, q.printed_ms);
    const int after = int(matcher.get_pending_orders(symbol, portfolio_id).size());
    matcher.check_sl_tp_triggers(portfolio_id, symbol, q.price);
    return std::max(0, before - after);
}

int pt_settle_delayed_sessions(const QString& portfolio_id,
                               const std::function<QDateTime(const QString& symbol, const QDateTime& after_utc)>& close_after,
                               const QDateTime& now_utc) {
    if (!close_after)
        return 0;
    int settled = 0;

    // Intraday positions: squared at the close of the session they were opened in.
    for (const auto& pos : pt_get_positions(portfolio_id)) {
        if (!product_is_intraday(pos.product) || pos.quantity <= 0.0)
            continue;
        const QDateTime opened = pt_sim_parse_utc(pos.opened_at);
        if (!opened.isValid())
            continue;
        const QDateTime close = close_after(pos.symbol, opened);
        if (!close.isValid() || now_utc < close)
            continue;
        const double price = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
        if (price <= 0.0)
            continue;
        const QString close_iso = close.toUTC().toString(Qt::ISODate);
        try {
            PtOrder o;
            o.id = generate_uuid();
            o.portfolio_id = portfolio_id;
            o.symbol = pos.symbol;
            o.side = (pos.side == QLatin1String("long")) ? QStringLiteral("sell") : QStringLiteral("buy");
            o.order_type = QStringLiteral("market");
            o.quantity = pos.quantity;
            o.price = price;
            o.status = QStringLiteral("pending");
            o.reduce_only = true;
            o.product = pos.product;
            o.created_at = close_iso;
            if (auto ir = repo().insert_order(o); ir.is_err()) {
                LOG_ERROR("PaperTrading", logf("Session-close settlement order could not be saved",
                                               {{"portfolio", log_mask(portfolio_id)},
                                                {"symbol", pos.symbol},
                                                {"error", QString::fromStdString(ir.error())}}));
                continue;
            }
            {
                auto f = pt_audit_fields(portfolio_id, o.symbol, QString(), o.side, o.order_type, o.product,
                                         o.quantity, o.price, std::nullopt);
                f.append(LogField("order_id", o.id));
                f.append(LogField("source", "session_close"));
                LOG_AUDIT(kPtAudit, logf("Order accepted", f));
            }
            pt_fill_order(o.id, price, std::nullopt, close_iso);
            ++settled;
        } catch (const std::exception& e) {
            LOG_WARN("PaperTrading", QString("Session-close square-off failed for %1: %2").arg(pos.symbol, e.what()));
        }
    }

    // DAY orders: expire at the close of the first session ending after placement.
    for (const auto& o : pt_get_orders(portfolio_id, QStringLiteral("pending"))) {
        const QDateTime created = pt_sim_parse_utc(o.created_at);
        if (!created.isValid())
            continue;
        const QDateTime close = close_after(o.symbol, created);
        if (!close.isValid() || now_utc < close)
            continue;
        try {
            OrderMatcher::instance().remove_order(o.id);
            pt_cancel_order(o.id);
            ++settled;
        } catch (const std::exception& e) {
            LOG_WARN("PaperTrading", QString("Expiring order %1 failed: %2").arg(o.id, e.what()));
        }
    }
    return settled;
}

PtOrder pt_modify_order(const QString& order_id, double new_quantity, double new_price, double new_stop_price) {
    const auto audit_modify_refused = [&](const QString& reason) {
        LOG_AUDIT(kPtAudit, "Order modify rejected",
                  {{"venue", "paper"}, {"mode", "paper"}, {"order_id", order_id}, {"qty", new_quantity},
                   {"price", new_price}, {"trigger", new_stop_price}, {"reason", reason}});
    };
    auto got = repo().get_order(order_id);
    if (got.is_err()) {
        audit_modify_refused(QStringLiteral("order not found"));
        throw std::runtime_error("Order not found");
    }
    const PtOrder old = got.value();
    if (old.status != QLatin1String("pending")) {
        audit_modify_refused(QStringLiteral("order is no longer resting"));
        throw std::runtime_error("This order is no longer resting (filled, cancelled or partly filled) — it cannot be "
                                 "modified");
    }
    if (!std::isfinite(new_quantity) || !std::isfinite(new_price) || !std::isfinite(new_stop_price)) {
        audit_modify_refused(QStringLiteral("invalid quantity or price"));
        throw std::runtime_error("Invalid quantity or price");
    }

    const double qty = new_quantity > 0.0 ? new_quantity : old.quantity;
    std::optional<double> price = old.price;
    std::optional<double> stop = old.stop_price;
    if (new_price > 0.0) {
        if (old.order_type == QLatin1String("limit") || old.order_type == QLatin1String("stop_limit"))
            price = new_price;
        else if (old.order_type == QLatin1String("stop"))
            stop = new_price; // an SL-M order's one price is its trigger
    }
    if (new_stop_price > 0.0 && (old.order_type == QLatin1String("stop") || old.order_type == QLatin1String("stop_limit")))
        stop = new_stop_price;

    OrderMatcher::instance().remove_order(order_id);
    pt_cancel_order(order_id); // releases the old order's margin
    try {
        PtOrder repl = pt_place_order(old.portfolio_id, old.symbol, old.side, old.order_type, qty, price, stop,
                                      old.reduce_only, old.exchange, old.product);
        OrderMatcher::instance().add_order(repl);
        {
            // A modify is a cancel + re-place (both audited above); this line links them.
            QList<LogField> f{{"venue", "paper"},
                              {"mode", "paper"},
                              {"portfolio", log_mask(old.portfolio_id)},
                              {"order_id", order_id},
                              {"new_order_id", repl.id},
                              {"symbol", old.symbol},
                              {"qty", qty}};
            if (price)
                f.append(LogField("price", *price));
            if (stop)
                f.append(LogField("trigger", *stop));
            LOG_AUDIT(kPtAudit, logf("Order modified", f));
        }
        return repl;
    } catch (const std::exception& e) {
        audit_modify_refused(QString::fromUtf8(e.what()));
        // Put the original back so a refused change never silently drops the order.
        try {
            PtOrder back = pt_place_order(old.portfolio_id, old.symbol, old.side, old.order_type, old.quantity,
                                          old.price, old.stop_price, old.reduce_only, old.exchange, old.product);
            OrderMatcher::instance().add_order(back);
        } catch (...) {
            throw std::runtime_error(std::string("Modify refused (") + e.what() +
                                     ") and the original order could not be restored — it is cancelled");
        }
        throw std::runtime_error(std::string("Modify refused: ") + e.what() + " — the original order was re-placed");
    }
}

} // namespace fincept::trading
