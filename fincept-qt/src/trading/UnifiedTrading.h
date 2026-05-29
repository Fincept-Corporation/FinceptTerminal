#pragma once
// Unified Trading — routes orders to live broker or paper trading engine

#include "trading/BrokerRegistry.h"
#include "trading/TradingTypes.h"

#include <QMutex>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <functional>
#include <optional>

namespace fincept::trading {

class UnifiedTrading : public QObject {
    Q_OBJECT
  public:
    static UnifiedTrading& instance();

    // Session management (legacy single-session API — kept for backward compat)
    TradingSession init_session(const QString& broker, const QString& mode, const QString& paper_portfolio_id = "");
    std::optional<TradingSession> get_session() const;
    TradingSession switch_mode(const QString& mode);

    // Order routing (legacy — uses session_)
    UnifiedOrderResponse place_order(const UnifiedOrder& order);
    UnifiedOrderResponse cancel_order(const QString& order_id);

    // Account-aware order routing (new — uses AccountManager for credentials)
    UnifiedOrderResponse place_order(const QString& account_id, const UnifiedOrder& order);
    UnifiedOrderResponse cancel_order(const QString& account_id, const QString& order_id);
    UnifiedOrderResponse modify_order(const QString& account_id, const QString& order_id,
                                      const QJsonObject& modifications);

    // Broadcast order to multiple accounts simultaneously
    // Each account gets an independent order — different credentials, different fills.
    // Returns one result per account. Safe to call from background thread.
    struct BroadcastResult {
        QString account_id;
        QString display_name;
        UnifiedOrderResponse response;
    };
    QVector<BroadcastResult> broadcast_order(const QStringList& account_ids, const UnifiedOrder& order);

    // --- Phase 1: Bulk operations (OpenAlgo bridge) ---
    ApiResponse<CancelAllResult> cancel_all_orders(const QString& account_id);
    ApiResponse<CloseAllResult> close_all_positions(const QString& account_id);
    ApiResponse<OrderPlaceResponse> close_position(const QString& account_id,
        const QString& symbol, const QString& exchange, const QString& product_type = {});
    ApiResponse<SmartOrderResult> place_smart_order(const QString& account_id, const SmartOrder& order);
    ApiResponse<QVector<BrokerQuote>> get_multi_quotes(const QString& account_id,
        const QVector<QPair<QString, QString>>& symbols);
    ApiResponse<MarketDepth> get_market_depth(const QString& account_id,
        const QString& symbol, const QString& exchange);

    // --- Basket & Split orders ---
    // Both run asynchronously on a background thread (orders are placed
    // sequentially in batches/chunks with sleeps between) and deliver their
    // result back on the caller's thread via the supplied callback.
    void place_basket_orders(const QString& account_id, const BasketOrderRequest& basket,
        std::function<void(const BasketOrderResult&)> callback);
    void place_split_orders(const QString& account_id, const SplitOrderRequest& request,
        std::function<void(const SplitOrderResult&)> callback);

    // Quantity-freeze-aware placement (Phase 3 §17). If the order quantity is
    // within the freeze limit for (symbol, exchange) — or no limit is set — this
    // places a single normal order and delivers a one-element SplitOrderResult.
    // If it exceeds the limit, it auto-splits into chunks of `freeze_limit` via
    // place_split_orders. Asynchronous in BOTH cases (background thread +
    // callback) so callers get a uniform contract regardless of whether a split
    // actually happened. Use this instead of place_order when you want
    // transparent freeze handling; use place_order for the strict sync path.
    void place_order_auto_split(const QString& account_id, const UnifiedOrder& order,
        std::function<void(const SplitOrderResult&)> callback);

    UnifiedTrading(const UnifiedTrading&) = delete;
    UnifiedTrading& operator=(const UnifiedTrading&) = delete;

  private:
    UnifiedTrading() = default;

    UnifiedOrderResponse place_paper_order(const TradingSession& session, const UnifiedOrder& order);
    UnifiedOrderResponse place_live_order(const TradingSession& session, const UnifiedOrder& order);

    // Account-aware helpers
    UnifiedOrderResponse place_paper_order_for_account(const QString& account_id, const UnifiedOrder& order);
    UnifiedOrderResponse place_live_order_for_account(const QString& account_id, const UnifiedOrder& order);

    std::optional<TradingSession> session_;
    mutable QMutex mutex_;
};

} // namespace fincept::trading
