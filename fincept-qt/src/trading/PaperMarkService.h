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

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class QTimer;

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
    bool started_ = false;
    bool flush_armed_ = false;
    int fill_cb_id_ = -1;
    QTimer* resync_timer_ = nullptr;
};

} // namespace fincept::trading
