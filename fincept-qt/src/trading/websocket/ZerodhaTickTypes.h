#pragma once
#include <QDateTime>
#include <QString>

namespace fincept::trading {

/// Zerodha market depth level (bid or ask).
struct ZerodhaDepthLevel {
    double price    = 0.0;
    int    quantity = 0;
    int    orders   = 0;
};

/// Zerodha streaming tick — populated depending on subscription mode.
///
/// LTP mode   (8-byte packet):  only instrument_token + ltp valid
/// Quote mode (44-byte packet): + ohlc, volume, ltq, atp, buy_qty, sell_qty
/// Full mode  (184-byte packet): + oi, exchange_timestamp, depth (5 bid + 5 ask)
struct ZerodhaTick {
    quint32  instrument_token = 0;

    // LTP
    double   ltp = 0.0;

    // Quote fields (44-byte+)
    double   open  = 0.0;
    double   high  = 0.0;
    double   low   = 0.0;
    double   close = 0.0;
    int      volume         = 0;
    int      last_quantity  = 0;   // ltq
    double   average_price  = 0.0; // atp
    int      buy_quantity   = 0;
    int      sell_quantity  = 0;

    // Full fields (184-byte)
    int      oi                = 0;
    int      oi_day_high       = 0;
    int      oi_day_low        = 0;
    QDateTime exchange_timestamp;
    ZerodhaDepthLevel bids[5];
    ZerodhaDepthLevel asks[5];

    // Derived / metadata
    QString  symbol;      // set by ZerodhaWebSocket after token lookup
    QString  exchange;    // set by ZerodhaWebSocket after token lookup
    bool     tradable = true; // false for indices
};

} // namespace fincept::trading
