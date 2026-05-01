#pragma once
// Kraken local order book.
//
// Kraken WS v2 sends one `book` snapshot then a stream of diff updates per
// symbol. To produce the snapshot-style OrderBookData our publisher / hub
// expects, we maintain a local book per symbol, merge each diff in
// (qty=0 means erase that price level), trim to depth-N, and re-emit.
//
// Each `book` message carries a CRC32 checksum of the top-10 (price, qty)
// levels on each side. We verify it after every merge — on mismatch the
// book is desynced (Kraken's recommended remedy is unsubscribe + resubscribe
// to that symbol). We expose `last_checksum_ok()` so the WS client can act.
//
// Threading: instances are owned by KrakenWsClient and only touched on its
// thread. No locks.

#include "trading/TradingTypes.h"

#include <QHash>
#include <QString>

#include <map>

namespace fincept::trading::kraken {

class KrakenBook {
  public:
    /// Replace local state with a fresh snapshot. Returns the assembled
    /// OrderBookData ready to publish.
    OrderBookData apply_snapshot(const QString& symbol,
                                 const QVector<QPair<double, double>>& bids,
                                 const QVector<QPair<double, double>>& asks,
                                 quint32 expected_checksum,
                                 const QString& timestamp_iso);

    /// Merge an incremental update. Returns the assembled OrderBookData.
    /// `last_checksum_ok()` reflects whether the post-merge top-10 CRC32
    /// matched `expected_checksum`. Caller decides whether to resubscribe.
    OrderBookData apply_update(const QString& symbol,
                               const QVector<QPair<double, double>>& bid_diffs,
                               const QVector<QPair<double, double>>& ask_diffs,
                               quint32 expected_checksum,
                               const QString& timestamp_iso);

    /// Drop all per-symbol state (e.g. on disconnect / resubscribe).
    void reset();
    void reset_symbol(const QString& symbol);

    /// True iff the most recent apply_* call passed CRC32 verification.
    bool last_checksum_ok() const { return last_checksum_ok_; }

    /// Trim the published book to this many levels per side. Default 25 —
    /// the screen renders at most that many. CRC32 always uses Kraken's
    /// fixed top-10 regardless.
    void set_publish_depth(int levels) { publish_depth_ = levels; }

  private:
    /// Per-symbol book state. std::map gives us sorted iteration cheaply.
    /// Bids: descending price (greater first). Asks: ascending price.
    struct Side {
        std::map<double, double, std::greater<double>> bids;
        std::map<double, double> asks;
    };

    /// Compute CRC32 over Kraken's canonical top-10 representation.
    /// Per the v2 docs the input string is `price|qty` for each of the top
    /// 10 ask levels (ascending), then the top 10 bid levels (descending),
    /// concatenated, with the decimal point stripped and any leading zeros
    /// after the strip removed. Returns the CRC32 of the resulting ASCII.
    static quint32 compute_checksum(const Side& side);

    /// Build the publishable OrderBookData (top-N each side + spread).
    OrderBookData build_publishable(const QString& symbol,
                                    const Side& side,
                                    const QString& timestamp_iso) const;

    QHash<QString, Side> books_;
    int publish_depth_ = 25;
    bool last_checksum_ok_ = true;
};

} // namespace fincept::trading::kraken
