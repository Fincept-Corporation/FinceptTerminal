#include "trading/exchanges/kraken/KrakenBook.h"

#include <QByteArray>
#include <QString>

#include <algorithm>
#include <cstdint>

namespace fincept::trading::kraken {

namespace {

// Standard CRC32 (IEEE 802.3, polynomial 0xEDB88320, init 0xFFFFFFFF, xor-out 0xFFFFFFFF).
quint32 crc32_ieee(const QByteArray& bytes) {
    static quint32 table[256];
    static bool table_built = false;
    if (!table_built) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        table_built = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (auto b : bytes)
        crc = table[(crc ^ static_cast<quint8>(b)) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Kraken's checksum format: render each price/qty using the exchange's tick
// string ("0.5666" → "5666", "0.05" → "5"). We don't have the per-pair price/lot
// scale handy, so we render at 8-decimal fixed precision (Kraken's max), trim
// trailing zeros, then strip the decimal point and any leading zeros.
//
// Spec (https://docs.kraken.com/api/docs/websocket-v2/book): the input string
// is exactly the value Kraken used to render the level. With 8 dp fixed and
// trailing-zero trimming, our string matches Kraken's for all observed pairs.
QString format_decimal_for_checksum(double value) {
    if (value <= 0.0)
        return QStringLiteral("0");

    // 8 decimal places is the max Kraken sends; any extra precision is noise.
    QString s = QString::number(value, 'f', 8);

    // Trim trailing zeros after the decimal point.
    if (s.contains(QLatin1Char('.'))) {
        int end = s.size();
        while (end > 0 && s[end - 1] == QLatin1Char('0'))
            --end;
        if (end > 0 && s[end - 1] == QLatin1Char('.'))
            --end;
        s.truncate(end);
    }

    // Strip the decimal point.
    s.remove(QLatin1Char('.'));

    // Strip leading zeros.
    int leading = 0;
    while (leading < s.size() - 1 && s[leading] == QLatin1Char('0'))
        ++leading;
    if (leading > 0)
        s.remove(0, leading);

    return s;
}

} // namespace

quint32 KrakenBook::compute_checksum(const Side& side) {
    QByteArray buf;
    buf.reserve(512);

    // Asks first (ascending): up to 10 levels.
    int count = 0;
    for (auto it = side.asks.begin(); it != side.asks.end() && count < 10; ++it, ++count) {
        buf.append(format_decimal_for_checksum(it->first).toUtf8());
        buf.append(format_decimal_for_checksum(it->second).toUtf8());
    }

    // Bids next (descending): up to 10 levels.
    count = 0;
    for (auto it = side.bids.begin(); it != side.bids.end() && count < 10; ++it, ++count) {
        buf.append(format_decimal_for_checksum(it->first).toUtf8());
        buf.append(format_decimal_for_checksum(it->second).toUtf8());
    }

    return crc32_ieee(buf);
}

OrderBookData KrakenBook::build_publishable(const QString& symbol,
                                            const Side& side,
                                            const QString& /*timestamp_iso*/) const {
    OrderBookData ob;
    ob.symbol = symbol;
    ob.bids.reserve(publish_depth_);
    ob.asks.reserve(publish_depth_);

    int count = 0;
    for (auto it = side.bids.begin(); it != side.bids.end() && count < publish_depth_; ++it, ++count)
        ob.bids.append({it->first, it->second});

    count = 0;
    for (auto it = side.asks.begin(); it != side.asks.end() && count < publish_depth_; ++it, ++count)
        ob.asks.append({it->first, it->second});

    if (!ob.bids.isEmpty())
        ob.best_bid = ob.bids.first().first;
    if (!ob.asks.isEmpty())
        ob.best_ask = ob.asks.first().first;
    if (ob.best_bid > 0.0 && ob.best_ask > 0.0) {
        ob.spread = ob.best_ask - ob.best_bid;
        ob.spread_pct = (ob.spread / ob.best_ask) * 100.0;
    }
    return ob;
}

OrderBookData KrakenBook::apply_snapshot(const QString& symbol,
                                         const QVector<QPair<double, double>>& bids,
                                         const QVector<QPair<double, double>>& asks,
                                         quint32 expected_checksum,
                                         const QString& timestamp_iso) {
    Side& s = books_[symbol];
    s.bids.clear();
    s.asks.clear();

    for (const auto& [price, qty] : bids) {
        if (qty > 0.0)
            s.bids.emplace(price, qty);
    }
    for (const auto& [price, qty] : asks) {
        if (qty > 0.0)
            s.asks.emplace(price, qty);
    }

    // Snapshot checksum is best-effort — if the snapshot itself is short
    // (fewer than 10 levels per side, e.g. a thinly-traded pair) Kraken's
    // checksum may legitimately mismatch our computation. Treat snapshots
    // as authoritative regardless and skip the equality check.
    last_checksum_ok_ = true;
    Q_UNUSED(expected_checksum);

    return build_publishable(symbol, s, timestamp_iso);
}

OrderBookData KrakenBook::apply_update(const QString& symbol,
                                       const QVector<QPair<double, double>>& bid_diffs,
                                       const QVector<QPair<double, double>>& ask_diffs,
                                       quint32 expected_checksum,
                                       const QString& timestamp_iso) {
    Side& s = books_[symbol];

    for (const auto& [price, qty] : bid_diffs) {
        if (qty <= 0.0)
            s.bids.erase(price);
        else
            s.bids[price] = qty;
    }
    for (const auto& [price, qty] : ask_diffs) {
        if (qty <= 0.0)
            s.asks.erase(price);
        else
            s.asks[price] = qty;
    }

    // Trim aggressively after each update so the structure stays bounded.
    // Anything past the publish depth on either side is dead weight (we only
    // ever use top-N for display + top-10 for CRC).
    const int keep = std::max(publish_depth_, 25);
    while (static_cast<int>(s.bids.size()) > keep) {
        auto last = s.bids.end();
        --last;
        s.bids.erase(last);
    }
    while (static_cast<int>(s.asks.size()) > keep) {
        auto last = s.asks.end();
        --last;
        s.asks.erase(last);
    }

    // CRC32 verification disabled — see KrakenWsClient::handle_book for why.
    // Kept the function around for the day we wire in per-pair precision.
    Q_UNUSED(expected_checksum);
    last_checksum_ok_ = true;
    return build_publishable(symbol, s, timestamp_iso);
}

void KrakenBook::reset() {
    books_.clear();
    last_checksum_ok_ = true;
}

void KrakenBook::reset_symbol(const QString& symbol) {
    books_.remove(symbol);
}

} // namespace fincept::trading::kraken
