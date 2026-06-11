#pragma once
// IExchangeVenue — abstract venue contract (legacy alpha_arena namespace).
//
// Sole remaining implementation: trading/exchanges/hyperliquid/
// HyperliquidVenue, which signs with an agent wallet (separate from the
// user's master wallet) and routes via Hyperliquid REST + WS. The new
// fincept::arena engine drives it through HyperliquidLiveVenue.
//
// All callbacks marshal back onto the calling QObject's thread via
// QPointer + invokeMethod (see HyperliquidVenue.cpp).

#include "core/result/Result.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services::alpha_arena {

/// Submitted order request — the venue is responsible for assigning a
/// venue-side order id and signalling fills through fill_callback.
struct OrderRequest {
    QString agent_id;          // logical agent identity (DB FK)
    QString coin;              // base symbol, e.g. "BTC"
    QString side;              // "buy" | "sell"
    double qty = 0.0;          // base-asset units, > 0
    int leverage = 1;          // [1, 20]
    /// Always market+IOC for entries in v1. Held here for forward compat.
    QString type = QStringLiteral("market");
    QString tif = QStringLiteral("ioc");
    /// Native TP/SL legs to attach — venue implementations decide how.
    /// Zero means "do not attach this leg".
    double profit_target = 0.0;
    double stop_loss = 0.0;
    /// reduce_only is set when closing or sizing-down a position.
    bool reduce_only = false;
};

/// Outcome of a place_order() call. Does not carry fills — those arrive
/// asynchronously via fill_callback.
struct OrderAck {
    QString venue_order_id;
    QString status;            // "accepted" | "rejected"
    QString error;             // populated when status="rejected"
};

/// One execution against an order. Venue may emit multiple per order
/// (partial fills) — but in v1 we use IOC so caller will see at most one
/// fill plus possibly a "rejected_unfilled" status update.
struct FillEvent {
    QString agent_id;
    QString venue_order_id;
    QString coin;
    QString side;
    double qty = 0.0;
    double price = 0.0;
    double fee = 0.0;
    qint64 utc_ms = 0;
};

/// Funding payment debited/credited at the 8h boundary.
struct FundingEvent {
    QString agent_id;
    QString coin;
    double amount_usd = 0.0;   // signed: negative = paid out
    qint64 utc_ms = 0;
};

/// Liquidation event — paper-mode emits when mark crosses liq_price; live
/// mode emits on receipt of the venue's userEvents/liquidation message.
struct LiquidationEvent {
    QString agent_id;
    QString coin;
    double exit_price = 0.0;
    double realized_pnl_usd = 0.0;
    qint64 utc_ms = 0;
};

/// Abstract venue. Implementations ARE QObjects so they can emit signals;
/// the interface itself is non-virtual-QObject so it can be inherited
/// alongside QObject without MOC tangling.
class IExchangeVenue {
  public:
    virtual ~IExchangeVenue() = default;

    /// Identity for logs / persistence / leaderboard badge ("paper" / "hyperliquid").
    virtual QString venue_kind() const = 0;

    /// Connection state.
    enum class ConnectionState { Disconnected, Connecting, Connected, Degraded };
    virtual ConnectionState connection_state() const = 0;

    /// Async order placement. Ack arrives via callback; later fills arrive
    /// through the channel installed via on_fill().
    virtual void place_order(const OrderRequest& req,
                             std::function<void(OrderAck)> ack_cb) = 0;

    /// Best-effort cancellation. May complete after the order has already
    /// filled — callers must handle the race.
    virtual void cancel_order(const QString& venue_order_id) = 0;

    /// Latest known mark price for `coin`. Returns NaN if unknown.
    virtual double last_mark(const QString& coin) const = 0;

    /// Install/replace event sinks. Sinks are called on the venue's thread;
    /// implementations are responsible for marshalling into the callee's
    /// thread.
    virtual void on_fill(std::function<void(FillEvent)> cb) = 0;
    virtual void on_funding(std::function<void(FundingEvent)> cb) = 0;
    virtual void on_liquidation(std::function<void(LiquidationEvent)> cb) = 0;

    /// Mark-price tick callback — called whenever a coin's mark moves. The
    /// engine uses this to drive paper-mode liq-price checks.
    virtual void on_mark_update(std::function<void(QString /*coin*/, double /*mark*/, qint64 /*utc_ms*/)> cb) = 0;
};

} // namespace fincept::services::alpha_arena
