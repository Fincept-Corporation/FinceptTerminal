#pragma once
// EquityPublicFeed — broker-less market data for the Equity Trading screen.
//
// Every price on the trading screen normally comes from the focused broker
// account's AccountDataStream. When no usable broker stream exists (no account,
// no credentials, an expired token, a broker that refuses market data, a session
// that dropped) this feed serves the same panels from the app's public data path,
// MarketDataService's Yahoo Finance topics on the DataHub:
//
//   market:quote:<YAHOO>                        -> watchlist rows, ticker bar
//   market:history:<YAHOO>:<period>:<interval>  -> chart candles
//
// Values are converted into the broker shapes (BrokerQuote / BrokerCandle) and
// handed to the screen through Sinks, so the screen renders them with the same
// entry points the broker path uses. Refresh cadence, batching, per-topic backoff
// and the error text all come from the hub and MarketDataService; this class runs
// no polling timer of its own.
//
// It also owns the two decisions the screen needs to pick a source: whether the
// focused account can serve market data (assess_broker_feed) and how a broker
// symbol maps to a Yahoo ticker (yahoo_symbol).
//
// Deliberately not a Q_OBJECT: it is only a DataHub subscription owner (lifetime
// guard + connection context), so it needs no moc.

#include "screens/equity_trading/EquitySymbolMap.h"
#include "services/markets/MarketDataTypes.h"
#include "trading/TradingTypes.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace fincept::screens::equity {

class EquityPublicFeed : public QObject {
  public:
    // ── Source decision ─────────────────────────────────────────────────────
    enum class BrokerStatus {
        Usable,        // the account's stream serves this screen's market data
        NoAccount,     // nothing focused / account gone
        NoCredentials, // account exists but was never connected (no api key)
        TokenExpired,  // session token dead — broker rejects every request
        NoMarketData,  // authenticated, but the broker refuses market data (state Error)
        Disconnected,  // a token-based session that dropped (WebSocket closed)
        BuiltinPaper,  // the built-in paper account: no broker by design, public data only
    };
    struct BrokerAssessment {
        BrokerStatus status = BrokerStatus::NoAccount;
        QString account_name; // focused account's display name (empty when none)
        QString broker_name;  // broker display name from its profile
        QString detail;       // the broker's own error text (NoMarketData), else empty
        bool usable() const { return status == BrokerStatus::Usable; }
    };
    // The single predicate for "can the focused account feed this screen?".
    // Mirrors how the screen already gates broker fetches (api_key present) and
    // AccountManager's connection model. A Disconnected account that authenticates
    // with an API key only (no session token) counts as usable: nothing ever marks
    // such an account Connected after a restart, yet its stream polls fine.
    static BrokerAssessment assess_broker_feed(const QString& account_id);

    // ── Symbol mapping ──────────────────────────────────────────────────────
    // Pure, Qt-Core-only and unit-tested: the logic lives in EquitySymbolMap.h.
    using YahooTicker = PublicYahooTicker;
    // Broker symbol + exchange -> Yahoo ticker. Builds on research_ticker_for()
    // (the screen's existing NSE->.NS / BSE->.BO mapping) and adds indices, broker
    // series suffixes (-EQ), US class shares and FX pairs.
    static YahooTicker yahoo_symbol(const QString& symbol, const QString& exchange) {
        return public_yahoo_ticker(symbol, exchange);
    }

    // Chart timeframe ("1m".."1w") -> the Yahoo history request that serves it.
    using HistorySpec = PublicHistorySpec;
    static HistorySpec history_spec(const QString& timeframe) { return public_history_spec(timeframe); }

    // ── Delivery ────────────────────────────────────────────────────────────
    // `symbol` is always the screen's own symbol (a broker symbol, or the canonical
    // Yahoo-form symbol on the built-in paper account), never assumed to be the ticker.
    struct Sinks {
        std::function<void(const QString& symbol, const trading::BrokerQuote& quote)> quote;
        // The same quote with everything Yahoo reported (currency, price decimals,
        // session state, delay, previous close, ranges, average volume) and when it
        // was taken: Yahoo's answer time, never later than the hub received it
        // (MarketDataService::quote_fetched_ms). Delivered right before `quote`.
        std::function<void(const QString& symbol, const services::QuoteData& q, qint64 received_ms)> quote_detail;
        // Quote unavailable: the hub's error text as delivered, or why the symbol
        // has no public equivalent.
        std::function<void(const QString& symbol, const QString& reason)> quote_error;
        // The charted symbol's price moved — roll it into the forming candle. Not
        // sent while the price is unchanged (closed market), so no phantom bars.
        std::function<void(const trading::BrokerQuote& quote)> chart_tick;
        std::function<void(const QString& symbol, const QString& timeframe)> chart_loading;
        std::function<void(const QString& symbol, const QString& timeframe,
                           const QVector<trading::BrokerCandle>& candles)>
            chart_candles;
        std::function<void(const QString& symbol, const QString& timeframe, const QString& reason)> chart_error;
    };

    explicit EquityPublicFeed(QObject* parent = nullptr);
    ~EquityPublicFeed() override;

    void set_sinks(Sinks sinks) { sinks_ = std::move(sinks); }

    // start() subscribes the current targets; stop() drops every subscription and
    // invalidates anything already queued, so nothing from this feed is painted
    // after the screen switches back to a broker stream. Both are idempotent.
    void start();
    void stop();
    bool is_active() const { return active_; }

    // Targets. Diff-based and cheap to call repeatedly; stored while inactive. The
    // screen decides each symbol's Yahoo ticker (core::instrument: the canonical
    // symbol itself, or a broker symbol + exchange canonicalised); an empty ticker
    // makes the row report `unsupported_reason` instead of a price.
    struct Target {
        QString symbol;             // screen symbol
        QString yahoo;              // Yahoo ticker; empty = no public equivalent
        QString unsupported_reason; // shown when yahoo is empty
    };
    void set_watch(const QVector<Target>& targets);
    void set_chart(const Target& target, const QString& timeframe);

    // Re-deliver what is known for one watched symbol right now — its fresh cached
    // quote, else its current error / unsupported reason. For a newly selected
    // symbol whose topic is already subscribed (nothing new would arrive until
    // the next refresh). No-op while inactive.
    void replay_quote(const QString& symbol);

  private:
    void subscribe_quote(const QString& yahoo);
    void unsubscribe_quote(const QString& yahoo);
    void on_quote(const QString& yahoo, const services::QuoteData& q);
    void deliver_quote(const QString& yahoo, const services::QuoteData& q, const QStringList& targets);
    void on_quote_error(const QString& yahoo, const QString& error);
    void report_unsupported();

    void bind_chart(const QString& previous_topic);
    void on_history(const QVariant& value);
    void on_chart_error(const QString& error);

    void on_topic_timeout(const QString& topic);

    Sinks sinks_;
    bool active_ = false;
    // Bumped on every start/stop. Each hub callback captures the value it was
    // registered under, so a delivery queued before a stop() is dropped.
    quint64 generation_ = 0;

    QHash<QString, QStringList> watch_;   // Yahoo ticker -> screen symbols shown for it
    QHash<QString, QString> unsupported_; // screen symbol -> reason it has no Yahoo ticker
    QSet<QString> quote_subs_;            // Yahoo tickers currently subscribed

    QString chart_symbol_;
    QString chart_tf_;
    QString chart_yahoo_;
    QString chart_unsupported_;
    QString chart_topic_; // empty when the charted symbol has no public equivalent
    HistorySpec chart_spec_;
    bool chart_bound_ = false;
    double chart_last_px_ = 0.0;
};

} // namespace fincept::screens::equity
