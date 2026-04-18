#pragma once
// DataStreamManager — manages all active AccountDataStream instances.
// Singleton that creates/destroys per-account data streams and forwards
// their signals as aggregated signals for the UI to consume.

#include "trading/AccountDataStream.h"
#include "trading/BrokerAccount.h"

#include <QHash>
#include <QObject>
#include <QString>

#    include "datahub/Producer.h"

namespace fincept::trading {

class DataStreamManager : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static DataStreamManager& instance();

    // --- Stream lifecycle ---
    AccountDataStream* stream_for(const QString& account_id);
    void start_stream(const QString& account_id);
    void stop_stream(const QString& account_id);
    void start_all_active();
    void stop_all();

    // --- Visibility control (P3: timers respect visibility) ---
    void pause_all();   // Screen hidden — pause all timers
    void resume_all();  // Screen shown — resume all timers

    // --- Query ---
    bool has_stream(const QString& account_id) const;
    QStringList active_stream_ids() const;

    // --- DataHub producer wiring (Phase 7) ---
    // Registers with the hub + installs broker:* policies. Idempotent.
    void ensure_registered_with_hub();

    // fincept::datahub::Producer
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

  signals:
    // Aggregated signals — the UI connects to these instead of individual streams
    void quote_updated(const QString& account_id, const QString& symbol, const BrokerQuote& quote);
    void watchlist_updated(const QString& account_id, const QVector<BrokerQuote>& quotes);
    void positions_updated(const QString& account_id, const QVector<BrokerPosition>& positions);
    void holdings_updated(const QString& account_id, const QVector<BrokerHolding>& holdings);
    void orders_updated(const QString& account_id, const QVector<BrokerOrderInfo>& orders);
    void funds_updated(const QString& account_id, const BrokerFunds& funds);
    void candles_fetched(const QString& account_id, const QVector<BrokerCandle>& candles);
    void orderbook_fetched(const QString& account_id,
                           const QVector<QPair<double, double>>& bids,
                           const QVector<QPair<double, double>>& asks,
                           double spread, double spread_pct);
    void time_sales_fetched(const QString& account_id, const QVector<BrokerTrade>& trades);
    void latest_trade_fetched(const QString& account_id, const BrokerTrade& trade);
    void calendar_fetched(const QString& account_id, const QVector<MarketCalendarDay>& days);
    void clock_fetched(const QString& account_id, const MarketClock& clock);
    void connection_state_changed(const QString& account_id, ConnectionState state);
    void token_expired(const QString& account_id);

  private:
    DataStreamManager();
    void wire_stream_signals(AccountDataStream* stream);

    // Slot-style handlers that fan the per-account signals to hub topics.
    // Connected from wire_stream_signals() so every stream publishes.
    void on_positions_for_hub(const QString& account_id, const QVector<BrokerPosition>& positions);
    void on_holdings_for_hub(const QString& account_id, const QVector<BrokerHolding>& holdings);
    void on_orders_for_hub(const QString& account_id, const QVector<BrokerOrderInfo>& orders);
    void on_funds_for_hub(const QString& account_id, const BrokerFunds& funds);
    void on_quote_for_hub(const QString& account_id, const QString& symbol, const BrokerQuote& quote);

    bool hub_registered_ = false;

    QHash<QString, AccountDataStream*> streams_; // account_id -> stream (parent = this)
};

} // namespace fincept::trading
