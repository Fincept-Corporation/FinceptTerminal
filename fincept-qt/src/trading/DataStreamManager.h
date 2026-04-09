#pragma once
// DataStreamManager — manages all active AccountDataStream instances.
// Singleton that creates/destroys per-account data streams and forwards
// their signals as aggregated signals for the UI to consume.

#include "trading/AccountDataStream.h"
#include "trading/BrokerAccount.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace fincept::trading {

class DataStreamManager : public QObject {
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

    QHash<QString, AccountDataStream*> streams_; // account_id -> stream (parent = this)
};

} // namespace fincept::trading
