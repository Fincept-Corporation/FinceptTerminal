#pragma once
#include <QDateTime>
#include <QString>

namespace fincept::trading {

// ── Exchange type codes (Angel One SmartStream) ────────────────────────────
enum class AoExchangeType : quint8 {
    NSE_CM  = 1,   // NSE Cash / Equity
    NSE_FO  = 2,   // NSE Futures & Options
    BSE_CM  = 3,   // BSE Cash / Equity
    BSE_FO  = 4,   // BSE Futures & Options
    MCX_FO  = 5,   // MCX Futures & Options
    NCX_FO  = 7,   // NCDEX
    CDE_FO  = 13,  // Currency Derivatives
};

// ── Subscription modes ─────────────────────────────────────────────────────
enum class AoSubMode : quint8 {
    LTP       = 1,  // 51-byte  — LTP only
    Quote     = 2,  // 123-byte — OHLCV + buy/sell qty
    SnapQuote = 3,  // 379-byte — Quote + OI + best-5 depth + circuit limits
    Depth20   = 4,  // Depth-20 order book (separate structure)
};

// ── Best-5 depth level ─────────────────────────────────────────────────────
struct AoDepthLevel {
    qint64  price     = 0;  // in paise (divide by 100 → rupees)
    qint64  quantity  = 0;
    quint16 orders    = 0;
};

// ── Streaming tick ─────────────────────────────────────────────────────────
/// Populated fields depend on the subscription mode.
///
/// LTP mode (mode=1):
///   ltp valid; all other price/volume fields = 0.
///
/// Quote mode (mode=2):
///   ltp, open, high, low, close, volume, ltq, atp, buy_qty, sell_qty valid.
///
/// SnapQuote mode (mode=3):
///   All Quote fields plus oi, circuit limits, 52w high/low, best_5 depth.
struct AoTick {
    AoSubMode    mode          = AoSubMode::LTP;
    AoExchangeType exchange_type = AoExchangeType::NSE_CM;
    QString      token;               // instrument token string (e.g. "2885")

    // LTP (all modes)
    double       ltp           = 0.0; // rupees

    // Quote fields (mode >= Quote)
    qint64       ltq           = 0;   // last traded quantity
    double       atp           = 0.0; // average traded price (rupees)
    qint64       volume        = 0;
    double       buy_qty       = 0.0; // total buy quantity (double in protocol)
    double       sell_qty      = 0.0; // total sell quantity
    double       open          = 0.0;
    double       high          = 0.0;
    double       low           = 0.0;
    double       close         = 0.0; // previous close

    // SnapQuote extra fields (mode == SnapQuote)
    qint64       oi            = 0;
    qint64       oi_change_pct = 0;   // × 100 in protocol
    double       upper_circuit = 0.0;
    double       lower_circuit = 0.0;
    double       week52_high   = 0.0;
    double       week52_low    = 0.0;

    // Best-5 depth (SnapQuote only)
    AoDepthLevel bids[5];
    AoDepthLevel asks[5];

    // Metadata
    QDateTime    exchange_timestamp;
    qint64       sequence_number = 0;

    // Derived (set by AngelOneWebSocket after lookup)
    QString      symbol;    // e.g. "RELIANCE"
    QString      exchange;  // e.g. "NSE"
};

} // namespace fincept::trading
