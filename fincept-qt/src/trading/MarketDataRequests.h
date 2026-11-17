#pragma once
// MarketDataRequests — pure helpers behind AccountDataStream's on-demand fetches
// (chart history, depth, time & sales). Header-only and Qt Core only, so
// tests/tst_market_data_requests.cpp covers it without the app.
//
//   LatestRequestGate            "latest request wins" for chart history
//   market_data_failure_reason   broker error text -> the reason a user is shown
//   market_symbols_match         one instrument spelled two ways ("NSE:SBIN-EQ" / "SBIN")

#include <QCoreApplication>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace fincept::trading {

// What one request asks for. `timeframe` is empty for requests without one.
struct MarketDataKey {
    QString symbol;
    QString timeframe;

    bool operator==(const MarketDataKey& o) const { return symbol == o.symbol && timeframe == o.timeframe; }
    bool operator!=(const MarketDataKey& o) const { return !(*this == o); }
};

// LatestRequestGate — one-shot requests whose answers arrive asynchronously and
// where only the NEWEST request matters (a chart showing one symbol/timeframe).
//
//   * At most one request is in flight. A request made meanwhile is remembered
//     (the newest replaces any older waiting one — never dropped) and handed back
//     by complete() when the in-flight one answers.
//   * An answer is "current" only when its key equals the newest request. Anything
//     else is stale and must not be shown as the current data.
//   * An in-flight request that stays silent longer than kAbandonAfterMs stops
//     blocking: the next request (or expire()) issues the waiting one anyway, and
//     the silent one, should it ever answer, is delivered only if still current.
//
// Not thread-safe: the owner calls everything from one thread. Time is passed in
// (monotonic ms) so the logic stays pure.
class LatestRequestGate {
  public:
    // Longer than a whole history request may legitimately take (two broker HTTP
    // attempts of 8 s each plus instrument resolution).
    static constexpr qint64 kAbandonAfterMs = 30000;

    struct Ticket {
        quint64 id = 0;
        MarketDataKey key;
    };
    struct Completion {
        bool current = false;       // the answer is for the newest request: deliver it
        std::optional<Ticket> next; // issue this now (the newest request was waiting)
    };

    // The caller wants `key`. Returns the ticket to issue now, or nullopt when it
    // has been queued behind the request in flight.
    std::optional<Ticket> request(const MarketDataKey& key, qint64 now_ms) {
        wanted_ = key;
        has_wanted_ = true;
        if (in_flight_id_ != 0 && now_ms - in_flight_since_ms_ < kAbandonAfterMs) {
            pending_ = true;
            return std::nullopt;
        }
        pending_ = false;
        return issue(key, now_ms);
    }

    // Ticket `id` (asking for `answered`) produced an answer — data or failure.
    Completion complete(quint64 id, const MarketDataKey& answered, qint64 now_ms) {
        Completion c;
        c.current = has_wanted_ && answered == wanted_;
        if (id == 0 || id != in_flight_id_)
            return c; // abandoned earlier: deliverable if still current, changes nothing else
        in_flight_id_ = 0;
        if (c.current) {
            pending_ = false; // the newest request has its answer
            return c;
        }
        if (pending_) {
            pending_ = false;
            c.next = issue(wanted_, now_ms);
        }
        return c;
    }

    // Call when the waiting request may be stuck behind a silent one. Issues the
    // waiting request once the in-flight one has been silent for kAbandonAfterMs.
    std::optional<Ticket> expire(qint64 now_ms) {
        if (!pending_ || in_flight_id_ == 0 || now_ms - in_flight_since_ms_ < kAbandonAfterMs)
            return std::nullopt;
        pending_ = false;
        return issue(wanted_, now_ms);
    }

    // ms until expire() could act (0 = now), or -1 when nothing is waiting.
    qint64 ms_until_expiry(qint64 now_ms) const {
        if (!pending_ || in_flight_id_ == 0)
            return -1;
        return qMax<qint64>(0, kAbandonAfterMs - (now_ms - in_flight_since_ms_));
    }

    // The newest request has not been answered yet (in flight or waiting).
    bool outstanding() const { return has_wanted_ && (in_flight_id_ != 0 || pending_); }
    bool has_wanted() const { return has_wanted_; }
    const MarketDataKey& wanted() const { return wanted_; }

  private:
    Ticket issue(const MarketDataKey& key, qint64 now_ms) {
        in_flight_id_ = ++last_id_;
        in_flight_since_ms_ = now_ms;
        return Ticket{in_flight_id_, key};
    }

    MarketDataKey wanted_;
    bool has_wanted_ = false;
    bool pending_ = false;       // wanted_ is waiting behind the request in flight
    quint64 in_flight_id_ = 0;   // 0 = nothing in flight
    qint64 in_flight_since_ms_ = 0;
    quint64 last_id_ = 0;
};

// The reason shown to a user for a failed market-data request. Reuses the
// broker's own error text; an expired session ("[TOKEN_EXPIRED] ..." — the prefix
// every broker uses, see AccountDataStream::check_token_expiry) is spelled out
// with what to do; an empty answer or the history service's bare "No data" reads
// as `no_data_text`.
inline QString market_data_failure_reason(const QString& broker_error, const QString& no_data_text) {
    const QString e = broker_error.trimmed();
    const QLatin1String expired("[TOKEN_EXPIRED]");
    if (e.startsWith(expired)) {
        QString detail = e.mid(expired.size()).trimmed();
        while (!detail.isEmpty() && (detail.front() == QLatin1Char(':') || detail.front() == QLatin1Char('-')))
            detail = detail.mid(1).trimmed();
        QString r = QCoreApplication::translate("MarketData",
                                                "The broker session has expired - reconnect the account in ACCOUNTS.");
        if (!detail.isEmpty())
            r += QStringLiteral(" (") + detail + QLatin1Char(')');
        return r;
    }
    if (e.isEmpty() || e.compare(QLatin1String("No data"), Qt::CaseInsensitive) == 0)
        return no_data_text;
    if (e == QLatin1String("No credentials"))
        return QCoreApplication::translate("MarketData",
                                           "No broker credentials are saved for this account - connect it in ACCOUNTS.");
    return e;
}

// The bare instrument name: exchange prefix ("NSE:") and the equity series
// suffix ("-EQ") removed — how WebSocket feeds and REST spell the same symbol.
inline QString market_symbol_core(QString s) {
    const int colon = s.lastIndexOf(QLatin1Char(':'));
    if (colon >= 0)
        s = s.mid(colon + 1);
    if (s.endsWith(QLatin1String("-EQ")))
        s.chop(3);
    return s;
}

inline bool market_symbols_match(const QString& a, const QString& b) {
    return market_symbol_core(a).compare(market_symbol_core(b), Qt::CaseInsensitive) == 0;
}

} // namespace fincept::trading
