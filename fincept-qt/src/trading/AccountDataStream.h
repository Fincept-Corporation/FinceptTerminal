#pragma once
// AccountDataStream — per-account data streaming service.
// Each active broker account gets one instance. Owns:
//   - WebSocket connection (if broker supports it: AngelOne, Zerodha)
//   - Polling timers (quote, portfolio, watchlist)
//   - Cached data snapshots (positions, holdings, orders, funds, quotes)
// Signals are emitted on the main thread for UI consumption.

#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QHash>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVector>

#include <atomic>

namespace fincept::trading {

class AccountDataStream : public QObject {
    Q_OBJECT
  public:
    explicit AccountDataStream(const QString& account_id, QObject* parent = nullptr);
    ~AccountDataStream() override;

    QString account_id() const { return account_id_; }
    QString broker_id() const { return broker_id_; }

    // --- Lifecycle ---
    void start();  // Load credentials, open WS / start timers
    void stop();   // Close WS, stop timers
    void pause();  // Stop timers (screen hidden), keep WS alive
    void resume(); // Restart timers (screen shown)
    bool is_running() const { return running_; }

    // Force an immediate one-shot refresh of positions / holdings / orders /
    // funds, bypassing the ADS_PORTFOLIO_POLL_MS (5-min) poll cadence. Called on
    // UI transitions where the stream is already running so start()/resume()
    // won't re-fetch on their own — screen reopen, account switch, paper↔live
    // toggle. Without it the blotter shows stale (or, after a context clear,
    // blank) data until the next poll. Each underlying fetch self-guards on
    // credentials, so this is a no-op for an unconfigured account.
    void refresh_portfolio_now();

    // Pull ONE channel now: "positions" | "holdings" | "orders" | "balance" |
    // "quote". Serves a forced DataHub refresh (DataStreamManager::refresh) without
    // refetching the other three portfolio channels. No-op when the stream is
    // stopped or the channel name is unknown; "quote" is skipped while the live
    // WebSocket is delivering ticks.
    void refresh_channel(const QString& channel);

    // Carry a previous incarnation's subscriptions — per-consumer symbol sets,
    // active-feed sets and the selected symbol — onto this (freshly started)
    // stream. DataStreamManager::restart_stream() destroys and recreates a stream
    // on every credential change, and the subscriptions live ONLY on the stream
    // object, so without this every consumer's symbols silently vanished and the
    // WebSocket re-subscribed to nothing.
    void adopt_subscriptions(const AccountDataStream& previous);

    // --- Symbol management (multi-consumer) ---
    // Each consumer ("equity:watchlist", "algo:<deploymentId>", …) owns an
    // independent symbol set. The WS/poll universe is the UNION across all
    // consumers plus selected_symbol_; a symbol is unsubscribed only when its
    // LAST consumer drops it. Replaces this consumer's set (empty == release).
    void subscribe_symbols(const QString& consumer_id, const QStringList& symbols);
    void unsubscribe_consumer(const QString& consumer_id);
    // Mark `symbols` as "active feed" for `consumer_id`: on brokers WITHOUT a
    // live WebSocket these get a fast (3s) REST poll instead of the 5-min
    // watchlist cadence, so algo deployments get timely ticks. No effect on WS
    // brokers (ticks already arrive live). Empty == release.
    void set_active_feed(const QString& consumer_id, const QStringList& symbols);
    void set_selected_symbol(const QString& symbol, const QString& exchange);
    QString selected_symbol() const { return selected_symbol_; }

    // --- On-demand fetches (for chart, orderbook, time & sales) ---
    void fetch_candles(const QString& symbol, const QString& timeframe);
    void fetch_orderbook(const QString& symbol);
    void fetch_time_sales(const QString& symbol);
    void fetch_latest_trade(const QString& symbol);
    void fetch_calendar();
    void fetch_clock();
    // Opening/closing auction prints for `symbol` (brokers that implement
    // get_historical_auctions_single, e.g. Alpaca; silent no-op elsewhere) →
    // auctions_fetched. And the trade-condition code table → condition_codes_fetched.
    void fetch_auctions(const QString& symbol);
    void fetch_condition_codes();

    // --- Cached data access (main thread only) ---
    QVector<BrokerPosition> cached_positions() const;
    QVector<BrokerHolding> cached_holdings() const;
    QVector<BrokerOrderInfo> cached_orders() const;
    BrokerFunds cached_funds() const;
    BrokerQuote cached_quote(const QString& symbol) const;

  signals:
    void quote_updated(const QString& account_id, const QString& symbol, const BrokerQuote& quote);
    void watchlist_updated(const QString& account_id, const QVector<BrokerQuote>& quotes);
    void positions_updated(const QString& account_id, const QVector<BrokerPosition>& positions);
    void holdings_updated(const QString& account_id, const QVector<BrokerHolding>& holdings);
    void orders_updated(const QString& account_id, const QVector<BrokerOrderInfo>& orders);
    void funds_updated(const QString& account_id, const BrokerFunds& funds);
    void candles_fetched(const QString& account_id, const QVector<BrokerCandle>& candles);
    void orderbook_fetched(const QString& account_id, const QVector<QPair<double, double>>& bids,
                           const QVector<QPair<double, double>>& asks, double spread, double spread_pct,
                           const QVector<int>& bid_orders, const QVector<int>& ask_orders);
    void time_sales_fetched(const QString& account_id, const QVector<BrokerTrade>& trades);
    void latest_trade_fetched(const QString& account_id, const BrokerTrade& trade);
    void calendar_fetched(const QString& account_id, const QVector<MarketCalendarDay>& days);
    void clock_fetched(const QString& account_id, const MarketClock& clock);
    void connection_state_changed(const QString& account_id, ConnectionState state);
    void token_expired(const QString& account_id);

    // Symbol-tagged twins of orderbook_fetched / latest_trade_fetched /
    // time_sales_fetched. The legacy signals carry no symbol, so a slow response (or
    // a late depth push) for the PREVIOUS symbol was indistinguishable from the
    // current one and got painted under it. Each of these is emitted immediately
    // after its legacy counterpart, with the symbol the data is for. New consumers
    // should connect here and drop payloads whose symbol is no longer selected.
    void orderbook_for_symbol(const QString& account_id, const QString& symbol,
                              const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                              double spread, double spread_pct, const QVector<int>& bid_orders,
                              const QVector<int>& ask_orders);
    void time_sales_for_symbol(const QString& account_id, const QString& symbol, const QVector<BrokerTrade>& trades);
    void latest_trade_for_symbol(const QString& account_id, const QString& symbol, const BrokerTrade& trade);
    // Results of fetch_auctions() / fetch_condition_codes(). `codes` maps
    // condition code → description (EquityBottomPanel::set_condition_codes shape).
    void auctions_fetched(const QString& account_id, const QString& symbol, const QVector<BrokerAuction>& auctions);
    void condition_codes_fetched(const QString& account_id, const QMap<QString, QString>& codes);

  private:
    // --- Token expiry check ---
    bool is_token_expired() const;

    // Inspect a broker error string for the "[TOKEN_EXPIRED]" prefix that all
    // brokers use to signal an expired/invalid session. When detected, marks
    // the account ConnectionState::TokenExpired via AccountManager and emits
    // token_expired(). Safe to call from any thread — marshals to the main
    // thread before touching AccountManager / emitting signals. No-op if the
    // error does not indicate token expiry. Returns true if expiry was handled.
    bool check_token_expiry(const QString& error);

    // --- Async fetch methods (all follow P8: QPointer + QtConcurrent + QueuedConnection) ---
    void async_fetch_quote();
    void async_fetch_positions();
    void async_fetch_holdings();
    void async_fetch_orders();
    void async_fetch_funds();
    void async_fetch_watchlist_quotes();

    // Union (deduped) of every consumer's symbols. The WS/poll universe.
    QStringList subscribed_symbols() const;
    // Union (deduped) of active-feed symbols across consumers.
    QStringList active_feed_symbol_union() const;
    void async_fetch_active_feed_quotes();
    void on_active_feed_timer();

    // --- Timer callbacks ---
    void on_quote_timer();
    void on_portfolio_timer();
    void on_watchlist_timer();

    // --- WebSocket ---
    void ws_init();
    void ws_teardown();
    bool ws_active() const;
    bool ws_connected() const; // connected+authenticated, regardless of tick count
    void ws_resubscribe();
    // Wire the common BrokerWebSocketBase signals (tick/depth/connect/error) into
    // this stream. Used by all Phase 2 adapters that share BrokerWebSocketBase.
    void wire_base_ws(class BrokerWebSocketBase* ws);

    // --- State ---
    QString account_id_;
    QString broker_id_;
    bool running_ = false;

    // WebSocket — polymorphic, null for brokers without WS
    QObject* ws_ = nullptr;
    // Latches once the streaming socket is refused with a permission verdict
    // (e.g. Kite 403 — API key has no active Connect/market-data subscription)
    // so the failure is surfaced to the user exactly once, not on every retry.
    // Reset when the socket reconnects.
    bool ws_permission_denied_ = false;

    // Polling timers
    QTimer* quote_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;
    QTimer* active_feed_timer_ = nullptr; // 3s fast poll for algo/active-feed symbols

    // Subscriptions
    QString selected_symbol_;
    QString selected_exchange_;
    // Consumer-keyed subscriptions (replaces the old single watchlist set).
    QHash<QString /*consumer_id*/, QStringList /*symbols*/> consumer_symbols_;
    // Subset of consumer symbols that need fast polling on non-WS brokers.
    QHash<QString /*consumer_id*/, QStringList /*symbols*/> active_feed_symbols_;

    // Cached data (main thread only — no mutex needed since all updates are via QueuedConnection)
    QVector<BrokerPosition> positions_;
    QVector<BrokerHolding> holdings_;
    QVector<BrokerOrderInfo> orders_;
    BrokerFunds funds_;
    QHash<QString, BrokerQuote> quote_cache_;

    // Fetch guards
    std::atomic<bool> quote_fetching_{false};
    std::atomic<bool> candles_fetching_{false};
    std::atomic<bool> portfolio_fetching_{false};
    std::atomic<int> ws_tick_count_{0};
};

} // namespace fincept::trading
