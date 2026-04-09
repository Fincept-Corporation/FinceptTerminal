#pragma once
// Unified Trading — routes orders to live broker or paper trading engine

#include "trading/BrokerRegistry.h"
#include "trading/TradingTypes.h"

#include <QMutex>
#include <QThread>

#include <atomic>
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

    // Order bridge for algo trading
    void start_order_bridge();
    void stop_order_bridge();
    bool is_bridge_running() const;

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

    // Order bridge
    std::atomic<bool> bridge_running_{false};
};

} // namespace fincept::trading
