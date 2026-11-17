#pragma once
// PaperMarkService — centralized, screen-independent mark-to-market + order
// matching for paper-trading positions.
//
// The Equity screen used to be the ONLY place that marked paper positions to the
// live feed and drove the OrderMatcher (limit/stop/SL-TP fills). So an F&O (or
// any) position only updated its P&L — and its resting orders only filled —
// while the Equity tab was open and focused. Open positions on any other tab
// showed a frozen entry price (and, before the zero-price guard, a phantom
// profit), and square-off acted on stale numbers.
//
// This service fixes that at the source: for every ACTIVE paper account that
// holds open positions, it keeps the account's data stream subscribed to those
// symbols, marks each position to the live quote (coalesced to SQLite, with the
// repo's zero/garbage-price guard), and drives the OrderMatcher on every tick —
// regardless of which screen, if any, is visible. It reconciles a broker's quote
// symbol to the stored position symbol by exact match, falling back to an option
// (underlying, side, strike) match for brokers whose live spelling differs from
// the REST/position spelling (the Fyers "…C<strike>" vs "…<strike>CE" case).
//
// Screens observe prices_marked()/portfolio_changed() to refresh their blotter;
// they no longer need to own the marking loop themselves.

#include "trading/PaperTrading.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

class QTimer;

namespace fincept::services {
struct QuoteData;
}

namespace fincept::trading {

struct BrokerQuote;

class PaperMarkService : public QObject {
    Q_OBJECT
  public:
    static PaperMarkService& instance();

    // Start the background loop. Idempotent. Call once after Database::open(),
    // AccountManager::reload_from_db() and DataStreamManager are ready.
    void start();

    // Re-evaluate which paper accounts/positions to track and (re)bind their
    // streams. Runs automatically on a timer and after every fill; call directly
    // right after placing/closing an order for an immediate refresh.
    void resync();

    // ── Delayed public prices (no broker feed) ─────────────────────────────────
    // A portfolio tracked here is marked and matched from the public quote path
    // (DataHub market:quote:<YAHOO>, delayed ~15 min) instead of a broker stream —
    // the built-in paper account always, and the screen may add a broker paper
    // account while its broker feed is unavailable. For every symbol with an open
    // position or resting / queued order (watchlist or not) it subscribes the
    // quote, marks the position, keeps the per-symbol FX factor current
    // (pt_set_symbol_fx, FxRateService), matches resting orders while the venue's
    // regular session is open (pt_match_delayed_quote) and settles intraday
    // positions / expires DAY orders at each venue's own close
    // (pt_settle_delayed_sessions; venue-settled portfolios only). Independent of
    // any screen being visible. A broker binding for the same portfolio is
    // suspended while it is tracked here, so the two sources never both mark it.
    //   `to_yahoo` maps a stored position / order symbol to its Yahoo ticker
    //   (empty = identity: the built-in account stores canonical symbols).
    void track_public_portfolio(const QString& portfolio_id, const QString& account_currency,
                                std::function<QString(const QString& stored_symbol)> to_yahoo = {});
    void untrack_public_portfolio(const QString& portfolio_id);
    bool is_public_tracked(const QString& portfolio_id) const;

    // The newest delayed quote held for a Yahoo ticker (invalid when none).
    PtDelayedQuote delayed_quote(const QString& yahoo) const;
    // A hub quote as the simulator sees it: price, receive time, exchange print time
    // and whether the instrument's venue (core/market/MarketSessions; Yahoo's own
    // market state when the venue is unknown) is in its regular session now.
    static PtDelayedQuote make_delayed_quote(const QString& yahoo, const services::QuoteData& q, qint64 received_ms);
    // Units of `account_ccy` per one price unit of `quote_ccy` (minor units such as
    // GBp handled); nullopt while FxRateService has no rate (one is then requested).
    // Never 1.0 for two different currencies without a rate: a stablecoin's
    // provisional 1:1 peg is refused too (cash is booked at this factor). A stale
    // rate is used.
    static std::optional<double> fx_factor(const QString& quote_ccy, const QString& account_ccy);
    // Before a local paper order books cash for `symbol`: the instrument's currency
    // (the venue of `exchange` — NSE INR, LSE GBp, NYSE USD; a crypto pair's quote
    // asset) against the paper portfolio's. Same currency, or an instrument
    // currency the venue table does not know: true, nothing set (the engine's
    // single-currency factor applies). Different: the per-symbol factor is set
    // from fx_factor() and true is returned — or false with a plain-language `why`
    // when there is no rate yet (never booked at 1.0). Any thread (DB read).
    static bool prepare_symbol_fx(const QString& portfolio_id, const QString& symbol, const QString& exchange,
                                  QString* why = nullptr);
    // The same guarantee for an order that CLOSES or reduces a local paper position
    // (an exit, square-off / close-all, a basket unwind, a smart order's flatten or
    // reduction) — its released margin, fee and realized P&L are booked at the
    // per-symbol factor, which lives in memory and is unset after a restart:
    //   1. The instrument's currency: `exchange`'s venue (a crypto pair's quote asset);
    //      when `exchange` is empty (positions store no venue), the venue of the newest
    //      order of `symbol` in this portfolio that names one; in a venue-settled book
    //      (the built-in account: canonical Yahoo symbols) the symbol's own currency.
    //      Still unknown: the account's own currency — the open was booked that way.
    //   2. Same currency as the account (pence in a GBP book included): true.
    //   3. A rate is known: the factor is (re)set from it; true.
    //   4. No rate, but a factor was set earlier in this run (from a real rate): kept; true.
    //   5. Otherwise the missing leg is requested and, off the GUI thread only, the call
    //      waits up to `wait_ms` for it. Still none: false, nothing booked, and `why`
    //      says the exit was NOT placed and to retry in a few seconds. An exit can be
    //      delayed by a missing rate; it is never booked at 1:1. Any thread.
    static constexpr int kCloseFxWaitMs = 3000;
    static bool prepare_close_fx(const QString& portfolio_id, const QString& symbol, const QString& exchange = {},
                                 int wait_ms = kCloseFxWaitMs, QString* why = nullptr);
    // Gate for a matcher pass (resting limit / stop orders, SL / TP brackets) of a
    // local paper symbol on a price tick: true when its fills can book cash — same
    // currency as the book, or a factor taken from a real rate (prepare_close_fx()
    // rules; refreshed at most once a minute). False while a cross-currency symbol has
    // no rate yet: mark the position, do not match it on this tick — it is never
    // filled at 1:1, and its orders stay armed. Cheap enough for every tick: decided
    // once per symbol, a missing rate re-checked at most every 2 s. Any thread.
    static bool fill_fx_ready(const QString& portfolio_id, const QString& symbol, const QString& exchange = {});
    // Factor price currency -> account currency for SHOWING a paper position's
    // money: the engine's per-symbol factor when set (kept current by the
    // delayed-price tracking), else fx_factor() (same currency 1, pence 1/100,
    // otherwise the FX service's rate), else NaN: no rate yet — never 1.0. An empty
    // price currency is taken to be the account's (1.0).
    static double display_fx(const QString& portfolio_id, const QString& symbol, const QString& price_ccy,
                             const QString& account_ccy);
    // Close (UTC) of the first regular session of `yahoo`'s venue ending after
    // `after_utc`; invalid for venues that never close (crypto) or are unknown.
    static QDateTime session_close_after(const QString& yahoo, const QDateTime& after_utc);

  signals:
    // The given paper portfolio's position prices were just marked to market
    // (throttled to the flush cadence) — a cue to repaint P&L.
    void prices_marked(const QString& portfolio_id);
    // A fill changed the portfolio's positions/orders — a cue for a full reload.
    void portfolio_changed(const QString& portfolio_id);

  private:
    PaperMarkService();

    void on_quote(const QString& account_id, const QString& portfolio_id, const QString& symbol,
                  const BrokerQuote& quote);
    void flush_prices();

    // Parsed identity of an option position, for matching a live tick whose
    // symbol spelling differs from the stored position symbol.
    struct PosLeg {
        QString underlying;
        bool is_call = false;
        QString symbol; // the stored position symbol (match target)
    };
    struct Bound {
        QString portfolio_id;
        QObject* stream = nullptr;    // bound AccountDataStream (detect recreation on re-auth)
        QMetaObject::Connection conn; // quote_updated → on_quote
        QSet<QString> symbols;        // currently subscribed on the stream
        QSet<QString> exact;          // position symbols for O(1) exact match
        QVector<PosLeg> legs;         // option positions for strike/side match
    };

    QHash<QString, Bound> bound_;                    // account_id → binding
    QHash<QString, QHash<QString, double>> pending_; // portfolio_id → {symbol: ltp} (coalesced)

    // Public (delayed) tracking — see track_public_portfolio().
    struct PublicBinding {
        QString account_currency;
        std::function<QString(const QString&)> to_yahoo;
        QHash<QString, QStringList> by_yahoo; // Yahoo ticker -> stored symbols of this portfolio
    };
    void resync_public();
    void on_public_quote(const QString& yahoo, const services::QuoteData& q);
    QString yahoo_for(const PublicBinding& b, const QString& stored) const;
    QHash<QString, PublicBinding> public_;        // portfolio_id → binding
    QSet<QString> public_subs_;                   // Yahoo tickers subscribed for public tracking
    QHash<QString, PtDelayedQuote> public_quotes_; // Yahoo ticker → newest delayed quote
    QHash<QString, QString> public_ccy_;          // Yahoo ticker → quote currency as reported
    bool started_ = false;
    bool flush_armed_ = false;
    int fill_cb_id_ = -1;
    QTimer* resync_timer_ = nullptr;
};

} // namespace fincept::trading
