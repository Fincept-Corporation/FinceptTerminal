#pragma once
// Broker Interface — abstract base class for all broker integrations

#include "trading/TradingTypes.h"

#include <QStringList>

#include <memory>

namespace fincept::trading {

// ============================================================================
// BrokerProfile — single source of truth for per-broker UI/config
// Each broker returns one of these from profile(). The UI reads it on switch.
// ============================================================================

enum class CredentialField {
    ApiKey,      // "API Key" / "Client ID"
    ApiSecret,   // "API Secret" / "Secret Key" / "MPIN"
    AuthCode,    // "Auth Code" / "TOTP secret" (base32)
    Environment, // "Live / Paper" toggle — Alpaca only
    ClientCode,  // Secondary login ID (AngelOne: client code separate from API key)
    Password,    // Zerodha login password (auto-login flow)
    TotpSecret,  // Zerodha Base32 TOTP secret (auto-login flow)
    UserId,      // Zerodha Kite user id (e.g., AB1234)
};

struct CredentialFieldDef {
    CredentialField field;
    QString label;       // display label, e.g. "API Key ID"
    QString placeholder; // input placeholder
    bool secret = false; // echo as password
};

struct ProductTypeDef {
    QString label; // display label, e.g. "Intraday (MIS)"
    trading::ProductType value;
};

// ============================================================================
// estimate_order_margin — shared fallback margin estimator
// Used by brokers that have no native pre-trade margin API (OpenAlgo's Python
// raises NotImplementedError for these). Mirrors Phase 3 §5's heuristic:
//   Intraday   → 20% (≈5x)
//   Delivery   → 100% (full payment)
//   Futures    → 10% (≈10x)
//   Option buy → premium (full)
//   Option sell→ 15% (SPAN estimate)
//   default    → 20%
// ============================================================================
inline OrderMargin estimate_order_margin(const UnifiedOrder& order) {
    const double trade_value = order.quantity * order.price;
    OrderMargin m;
    m.symbol = order.symbol;
    m.exchange = order.exchange;
    m.side = (order.side == OrderSide::Buy) ? "BUY" : "SELL";
    m.quantity = order.quantity;
    m.price = order.price;

    const bool is_opt = order.symbol.endsWith("CE") || order.symbol.endsWith("PE");
    const bool is_fut = order.symbol.contains("FUT");

    if (order.product_type == ProductType::Intraday)
        m.total = trade_value * 0.20; // ~5x
    else if (order.product_type == ProductType::Delivery)
        m.total = trade_value; // full payment
    else if (is_fut)
        m.total = trade_value * 0.10; // ~10x
    else if (is_opt && order.side == OrderSide::Buy)
        m.total = trade_value; // premium only
    else if (is_opt)
        m.total = trade_value * 0.15; // sell SPAN estimate
    else
        m.total = trade_value * 0.20;

    m.cash = m.total;
    m.leverage = (m.total > 0.0) ? trade_value / m.total : 1.0;
    return m;
}

struct BrokerProfile {
    // Identity
    QString id;           // e.g. "alpaca"
    QString display_name; // e.g. "Alpaca"
    QString region;       // "IN", "US", "EU"
    QString currency;     // "INR", "USD", "GBP", "EUR"

    // Credentials dialog — only listed fields are shown
    QVector<CredentialFieldDef> credential_fields;

    // Order form
    QStringList exchanges;                 // available exchanges in order form
    QVector<ProductTypeDef> product_types; // available product types
    bool supports_intraday = true;
    bool supports_bracket_order = false;
    bool supports_cover_order = false;

    // Paper trading
    bool has_native_paper = false; // broker provides its own paper trading (Alpaca)
    double default_paper_balance = 1000000.0;

    // Market data
    QStringList default_watchlist; // symbols to show on first switch
    QString default_symbol;        // first selected symbol
    QString default_exchange;      // first selected exchange

    // Brokerage info (for display only)
    QString brokerage_info; // e.g. "₹20/order or 0.03%"
};

// ============================================================================
// SessionCheck — result of a live token health-check (IBroker::validate_session)
// ============================================================================
// status:
//   Valid        → token works right now
//   Expired      → token is definitively dead/invalid (safe to disconnect/purge)
//   Inconclusive → network/rate-limit/other; caller MUST leave state unchanged
// expires_at_epoch: the broker's *real* reported expiry when it returns one
//   (e.g. Dhan /v2/profile tokenValidity). 0 means unknown. On a Valid result
//   AccountManager writes this back to storage so the persisted hint stops being
//   stale.
struct SessionCheck {
    enum class Status { Valid, Expired, Inconclusive };
    Status status = Status::Inconclusive;
    qint64 expires_at_epoch = 0;
    QString detail;
};

// ============================================================================
// IBroker
// ============================================================================

class IBroker {
  public:
    virtual ~IBroker() = default;

    virtual BrokerId id() const = 0;
    virtual const char* name() const = 0;
    virtual const char* base_url() const = 0;
    virtual BrokerProfile profile() const = 0;

    // --- Authentication ---
    virtual TokenExchangeResponse exchange_token(const QString& api_key, const QString& api_secret,
                                                 const QString& auth_code) = 0;

    // --- Session validation (live token health-check) ---
    // Issues a cheap authenticated call (default: get_funds) and classifies it
    // into a SessionCheck. Brokers tag auth failures with the "[TOKEN_EXPIRED]"
    // marker inside their checked_error()/*_check_auth() helpers; this default
    // relies on that marker, so it works uniformly across every broker without a
    // per-broker override. Override when the broker exposes a real expiry (so the
    // persisted hint can be refreshed) or a cheaper dedicated endpoint.
    virtual SessionCheck validate_session(const BrokerCredentials& creds) {
        auto r = get_funds(creds);
        if (r.success)
            return {SessionCheck::Status::Valid, 0, QString()};
        if (r.error.contains(QStringLiteral("[TOKEN_EXPIRED]")))
            return {SessionCheck::Status::Expired, 0, r.error};
        return {SessionCheck::Status::Inconclusive, 0, r.error};
    }

    // Whether this broker can obtain a fresh access token with no user
    // interaction — either via a refresh token or by replaying a stored TOTP
    // login. AccountManager only attempts refresh_session() when this is true.
    virtual bool supports_silent_refresh() const { return false; }

    // Attempt a silent token refresh from stored credentials. On success returns
    // a populated TokenExchangeResponse (access_token [+ refresh_token] +
    // additional_data carrying an updated "token_expires_at"). Default: not
    // supported. Uses blocking HTTP internally — call from a worker thread only.
    virtual TokenExchangeResponse refresh_session(const BrokerCredentials& creds) {
        Q_UNUSED(creds);
        return {false, QString(), QString(), QString(), QString(), QStringLiteral("Silent refresh not supported")};
    }

    // --- Orders ---
    virtual OrderPlaceResponse place_order(const BrokerCredentials& creds, const UnifiedOrder& order) = 0;
    virtual ApiResponse<QJsonObject> modify_order(const BrokerCredentials& creds, const QString& order_id,
                                                  const QJsonObject& modifications) = 0;
    virtual ApiResponse<QJsonObject> cancel_order(const BrokerCredentials& creds, const QString& order_id) = 0;
    virtual ApiResponse<QVector<BrokerOrderInfo>> get_orders(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<QJsonObject> get_trade_book(const BrokerCredentials& creds) = 0;

    // --- Portfolio ---
    virtual ApiResponse<QVector<BrokerPosition>> get_positions(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<QVector<BrokerHolding>> get_holdings(const BrokerCredentials& creds) = 0;
    virtual ApiResponse<BrokerFunds> get_funds(const BrokerCredentials& creds) = 0;

    // --- Market Data ---
    virtual ApiResponse<QVector<BrokerQuote>> get_quotes(const BrokerCredentials& creds,
                                                         const QVector<QString>& symbols) = 0;
    virtual ApiResponse<QVector<BrokerCandle>> get_history(const BrokerCredentials& creds, const QString& symbol,
                                                           const QString& resolution, const QString& from_date,
                                                           const QString& to_date) = 0;

    // --- Calendar & Clock ---
    virtual ApiResponse<QVector<MarketCalendarDay>> get_calendar(const BrokerCredentials& creds, const QString& start,
                                                                 const QString& end) {
        Q_UNUSED(creds);
        Q_UNUSED(start);
        Q_UNUSED(end);
        return {false, std::nullopt, "Calendar not supported for this broker"};
    }
    virtual ApiResponse<MarketClock> get_clock(const BrokerCredentials& creds) {
        Q_UNUSED(creds);
        return {false, std::nullopt, "Clock not supported for this broker"};
    }

    // --- Stock market data (multi-symbol) ---
    virtual ApiResponse<QVector<BrokerCandle>> get_latest_bars(const BrokerCredentials& creds,
                                                               const QVector<QString>& symbols) {
        Q_UNUSED(creds);
        Q_UNUSED(symbols);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerCandle>> get_historical_bars(const BrokerCredentials& creds,
                                                                   const QVector<QString>& symbols,
                                                                   const QString& timeframe, const QString& start,
                                                                   const QString& end) {
        Q_UNUSED(creds);
        Q_UNUSED(symbols);
        Q_UNUSED(timeframe);
        Q_UNUSED(start);
        Q_UNUSED(end);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerQuote>> get_latest_quotes(const BrokerCredentials& creds,
                                                                const QVector<QString>& symbols) {
        Q_UNUSED(creds);
        Q_UNUSED(symbols);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerTrade>> get_latest_trades(const BrokerCredentials& creds,
                                                                const QVector<QString>& symbols) {
        Q_UNUSED(creds);
        Q_UNUSED(symbols);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerTrade>> get_historical_trades(const BrokerCredentials& creds,
                                                                    const QVector<QString>& symbols,
                                                                    const QString& start, const QString& end,
                                                                    int limit = 1000) {
        Q_UNUSED(creds);
        Q_UNUSED(symbols);
        Q_UNUSED(start);
        Q_UNUSED(end);
        Q_UNUSED(limit);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerAuction>> get_historical_auctions(const BrokerCredentials& creds,
                                                                        const QVector<QString>& symbols,
                                                                        const QString& start, const QString& end) {
        Q_UNUSED(creds);
        Q_UNUSED(symbols);
        Q_UNUSED(start);
        Q_UNUSED(end);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerMetaEntry>> get_condition_codes(const BrokerCredentials& creds,
                                                                      const QString& ticktype, const QString& tape) {
        Q_UNUSED(creds);
        Q_UNUSED(ticktype);
        Q_UNUSED(tape);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerMetaEntry>> get_exchange_codes(const BrokerCredentials& creds,
                                                                     const QString& asset_class) {
        Q_UNUSED(creds);
        Q_UNUSED(asset_class);
        return {false, std::nullopt, "Not supported"};
    }

    // --- Stock market data (single symbol) ---
    virtual ApiResponse<BrokerCandle> get_latest_bar(const BrokerCredentials& creds, const QString& symbol) {
        Q_UNUSED(creds);
        Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<BrokerQuote> get_latest_quote(const BrokerCredentials& creds, const QString& symbol) {
        Q_UNUSED(creds);
        Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<BrokerTrade> get_latest_trade(const BrokerCredentials& creds, const QString& symbol) {
        Q_UNUSED(creds);
        Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<BrokerQuote> get_snapshot(const BrokerCredentials& creds, const QString& symbol) {
        Q_UNUSED(creds);
        Q_UNUSED(symbol);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerTrade>> get_historical_trades_single(const BrokerCredentials& creds,
                                                                           const QString& symbol, const QString& start,
                                                                           const QString& end, int limit = 1000) {
        Q_UNUSED(creds);
        Q_UNUSED(symbol);
        Q_UNUSED(start);
        Q_UNUSED(end);
        Q_UNUSED(limit);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerQuote>> get_historical_quotes_single(const BrokerCredentials& creds,
                                                                           const QString& symbol, const QString& start,
                                                                           const QString& end, int limit = 1000) {
        Q_UNUSED(creds);
        Q_UNUSED(symbol);
        Q_UNUSED(start);
        Q_UNUSED(end);
        Q_UNUSED(limit);
        return {false, std::nullopt, "Not supported"};
    }
    virtual ApiResponse<QVector<BrokerAuction>> get_historical_auctions_single(const BrokerCredentials& creds,
                                                                               const QString& symbol,
                                                                               const QString& start,
                                                                               const QString& end) {
        Q_UNUSED(creds);
        Q_UNUSED(symbol);
        Q_UNUSED(start);
        Q_UNUSED(end);
        return {false, std::nullopt, "Not supported"};
    }

    // --- Margin Calculator ---
    /// Pre-trade margin check for a single order. Returns breakdown of margin required.
    virtual ApiResponse<OrderMargin> get_order_margins(const BrokerCredentials& creds, const UnifiedOrder& order) {
        Q_UNUSED(creds);
        Q_UNUSED(order);
        return {false, std::nullopt, "Margin calculator not supported for this broker"};
    }
    /// Pre-trade margin check for a basket of orders (with netting across legs).
    virtual ApiResponse<BasketMargin> get_basket_margins(const BrokerCredentials& creds,
                                                         const QVector<UnifiedOrder>& orders) {
        Q_UNUSED(creds);
        Q_UNUSED(orders);
        return {false, std::nullopt, "Basket margin not supported for this broker"};
    }

    // --- GTT (Good Till Triggered) Orders ---
    // Default implementations return "Not supported" — only override in brokers that have GTT.
    virtual GttPlaceResponse gtt_place(const BrokerCredentials& creds, const GttOrder& order) {
        Q_UNUSED(creds);
        Q_UNUSED(order);
        return {false, "", "GTT not supported for this broker"};
    }
    virtual ApiResponse<GttOrder> gtt_get(const BrokerCredentials& creds, const QString& gtt_id) {
        Q_UNUSED(creds);
        Q_UNUSED(gtt_id);
        return {false, std::nullopt, "GTT not supported for this broker"};
    }
    virtual ApiResponse<QVector<GttOrder>> gtt_list(const BrokerCredentials& creds) {
        Q_UNUSED(creds);
        return {false, std::nullopt, "GTT not supported for this broker"};
    }
    virtual ApiResponse<GttOrder> gtt_modify(const BrokerCredentials& creds, const QString& gtt_id,
                                             const GttOrder& updated) {
        Q_UNUSED(creds);
        Q_UNUSED(gtt_id);
        Q_UNUSED(updated);
        return {false, std::nullopt, "GTT not supported for this broker"};
    }
    virtual ApiResponse<QJsonObject> gtt_cancel(const BrokerCredentials& creds, const QString& gtt_id) {
        Q_UNUSED(creds);
        Q_UNUSED(gtt_id);
        return {false, std::nullopt, "GTT not supported for this broker"};
    }

    // --- Bulk Operations (Phase 1: OpenAlgo bridge) ---

    /// Cancel all open/pending orders. Default: fetch order book, cancel each open order.
    virtual ApiResponse<CancelAllResult> cancel_all_orders(const BrokerCredentials& creds) {
        CancelAllResult result;
        auto orders_resp = get_orders(creds);
        if (!orders_resp.success)
            return {false, std::nullopt, orders_resp.error};
        for (const auto& o : orders_resp.data.value_or(QVector<BrokerOrderInfo>{})) {
            auto s = o.status.toLower();
            if (s == "open" || s == "pending" || s == "new" || s == "trigger pending"
                || s == "trigger_pending" || s == "ordered" || s == "transit"
                || s == "accepted" || s == "partially_filled" || s == "working") {
                result.total_attempted++;
                auto r = cancel_order(creds, o.order_id);
                if (r.success)
                    result.canceled_order_ids.append(o.order_id);
                else
                    result.failed.append({o.order_id, r.error});
            }
        }
        return {true, result, {}};
    }

    /// Close all open positions by placing market counter-orders.
    virtual ApiResponse<CloseAllResult> close_all_positions(const BrokerCredentials& creds) {
        CloseAllResult result;
        auto pos_resp = get_positions(creds);
        if (!pos_resp.success)
            return {false, std::nullopt, pos_resp.error};
        for (const auto& p : pos_resp.data.value_or(QVector<BrokerPosition>{})) {
            if (p.quantity == 0) continue;
            result.total_positions++;
            UnifiedOrder counter;
            counter.symbol = p.symbol;
            counter.exchange = p.exchange;
            counter.side = (p.quantity > 0) ? OrderSide::Sell : OrderSide::Buy;
            counter.quantity = std::abs(p.quantity);
            counter.order_type = OrderType::Market;
            auto r = place_order(creds, counter);
            if (r.success)
                result.closed_symbols.append(p.symbol);
            else
                result.failed.append({p.symbol, r.error});
        }
        return {true, result, {}};
    }

    /// Close a single position by symbol/exchange/product.
    virtual ApiResponse<OrderPlaceResponse> close_position(
        const BrokerCredentials& creds,
        const QString& symbol, const QString& exchange, const QString& product_type)
    {
        auto pos_resp = get_positions(creds);
        if (!pos_resp.success)
            return {false, std::nullopt, pos_resp.error};
        for (const auto& p : pos_resp.data.value_or(QVector<BrokerPosition>{})) {
            if (p.symbol == symbol && p.exchange == exchange
                && (product_type.isEmpty() || p.product_type == product_type)
                && p.quantity != 0) {
                UnifiedOrder counter;
                counter.symbol = p.symbol;
                counter.exchange = p.exchange;
                counter.side = (p.quantity > 0) ? OrderSide::Sell : OrderSide::Buy;
                counter.quantity = std::abs(p.quantity);
                counter.order_type = OrderType::Market;
                auto opr = place_order(creds, counter);
                if (!opr.success)
                    return {false, std::nullopt, opr.error};
                return {true, opr, ""};
            }
        }
        return {false, std::nullopt, "Position not found"};
    }

    /// Batch quotes for multiple symbols. Default: sequential fallback.
    virtual ApiResponse<QVector<BrokerQuote>> get_multi_quotes(
        const BrokerCredentials& creds,
        const QVector<QPair<QString, QString>>& symbols)
    {
        QVector<BrokerQuote> results;
        for (const auto& [sym, exch] : symbols) {
            auto r = get_quotes(creds, {sym});
            if (r.success && r.data.has_value() && !r.data->isEmpty())
                results.append(r.data->first());
        }
        return {true, results, {}};
    }

    /// Market depth (Level 2 bid/ask). Default: not supported.
    virtual ApiResponse<MarketDepth> get_market_depth(
        const BrokerCredentials& creds,
        const QString& symbol, const QString& exchange)
    {
        Q_UNUSED(creds); Q_UNUSED(symbol); Q_UNUSED(exchange);
        return {false, std::nullopt, "Market depth not supported for this broker"};
    }

    /// Option chain for an underlying. Default: not supported.
    virtual ApiResponse<QVector<OptionChainEntry>> get_option_chain(
        const BrokerCredentials& creds,
        const QString& underlying, const QString& exchange,
        const QString& expiry, int strike_count = 0)
    {
        Q_UNUSED(creds); Q_UNUSED(underlying); Q_UNUSED(exchange);
        Q_UNUSED(expiry); Q_UNUSED(strike_count);
        return {false, std::nullopt, "Option chain not supported for this broker"};
    }

    // --- WebSocket streaming ---
    virtual const char* ws_adapter_name() const { return ""; }

    // --- Credential helpers ---
    BrokerCredentials load_credentials() const;
    BrokerCredentials load_credentials_for_env(const QString& env) const; // env="live"/"paper"/""
    void save_credentials(const BrokerCredentials& creds) const;

  protected:
    virtual QMap<QString, QString> auth_headers(const BrokerCredentials& creds) const = 0;
};

using BrokerPtr = std::unique_ptr<IBroker>;

} // namespace fincept::trading
