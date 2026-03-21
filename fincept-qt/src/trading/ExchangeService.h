#pragma once
// ExchangeService — Bridge between Python ccxt scripts and C++ paper trading
// Qt-adapted port using QProcess for subprocess management.

#include "trading/TradingTypes.h"
#include <QObject>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QTimer>
#include <QProcess>
#include <functional>
#include <atomic>

namespace fincept::trading {

using PriceUpdateCallback = std::function<void(const QString& symbol, const TickerData& ticker)>;
using OrderBookCallback   = std::function<void(const QString& symbol, const OrderBookData& ob)>;
using CandleCallback      = std::function<void(const QString& symbol, const Candle& candle)>;
using TradeCallback       = std::function<void(const QString& symbol, const TradeData& trade)>;

class ExchangeService : public QObject {
    Q_OBJECT
public:
    static ExchangeService& instance();

    // Configuration
    void set_exchange(const QString& exchange_id);
    QString get_exchange() const;
    void set_credentials(const ExchangeCredentials& creds);

    // Price feed
    void watch_symbol(const QString& symbol, const QString& portfolio_id);
    void unwatch_symbol(const QString& symbol, const QString& portfolio_id);
    void start_price_feed(int interval_seconds = 5);
    void stop_price_feed();
    bool is_feed_running() const;

    // Callbacks
    int on_price_update(PriceUpdateCallback callback);
    void remove_price_callback(int id);
    int on_orderbook_update(OrderBookCallback callback);
    void remove_orderbook_callback(int id);
    int on_candle_update(CandleCallback callback);
    void remove_candle_callback(int id);
    int on_trade_update(TradeCallback callback);
    void remove_trade_callback(int id);

    // WebSocket streaming
    void start_ws_stream(const QString& primary_symbol,
                         const QStringList& all_symbols);
    void stop_ws_stream();
    bool is_ws_connected() const;
    void set_ws_primary_symbol(const QString& symbol);
    QString get_ws_primary_symbol() const;

    // One-shot data fetches
    TickerData fetch_ticker(const QString& symbol);
    QVector<TickerData> fetch_tickers(const QStringList& symbols);
    OrderBookData fetch_orderbook(const QString& symbol, int limit = 20);
    QVector<Candle> fetch_ohlcv(const QString& symbol,
                                 const QString& timeframe = "1h",
                                 int limit = 100);
    QVector<MarketInfo> fetch_markets(const QString& type = "");
    QStringList list_exchange_ids();
    QVector<TradeData> fetch_trades(const QString& symbol, int limit = 50);
    FundingRateData fetch_funding_rate(const QString& symbol);
    OpenInterestData fetch_open_interest(const QString& symbol);

    // Authenticated operations
    QJsonObject fetch_balance();
    QJsonObject place_exchange_order(const QString& symbol, const QString& side,
                                      const QString& type, double amount, double price = 0.0);
    QJsonObject cancel_exchange_order(const QString& order_id, const QString& symbol);
    QJsonObject fetch_positions_live(const QString& symbol = "");
    QJsonObject fetch_open_orders_live(const QString& symbol = "");

    // Cache
    const QHash<QString, TickerData>& get_price_cache() const;
    TickerData get_cached_price(const QString& symbol) const;

    ExchangeService(const ExchangeService&) = delete;
    ExchangeService& operator=(const ExchangeService&) = delete;

private:
    ExchangeService();
    ~ExchangeService();

    void poll_prices();
    QJsonObject call_script(const QString& script, const QStringList& args);
    QJsonObject call_script_with_credentials(const QString& script, const QStringList& args);

    static TickerData parse_ticker(const QJsonObject& j);
    static OrderBookData parse_orderbook(const QJsonObject& j);
    static Candle parse_candle(const QJsonObject& j);
    static MarketInfo parse_market(const QJsonObject& j);

    QString exchange_id_ = "binance";
    ExchangeCredentials credentials_;

    QHash<QString, QSet<QString>> watched_;
    QHash<QString, TickerData> price_cache_;

    QHash<int, PriceUpdateCallback> price_callbacks_;
    QHash<int, OrderBookCallback> orderbook_callbacks_;
    QHash<int, CandleCallback> candle_callbacks_;
    QHash<int, TradeCallback> trade_callbacks_;
    int next_callback_id_ = 1;

    QTimer* feed_timer_ = nullptr;
    std::atomic<bool> feed_running_{false};
    int feed_interval_ = 5;

    // WebSocket subprocess
    QProcess* ws_process_ = nullptr;
    std::atomic<bool> ws_connected_{false};
    QString ws_primary_symbol_;
    QStringList ws_all_symbols_;
    void handle_ws_line(const QString& line);

    mutable QMutex mutex_;
};

} // namespace fincept::trading
