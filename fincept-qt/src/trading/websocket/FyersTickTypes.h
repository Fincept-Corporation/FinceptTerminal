#pragma once
#include <QString>

namespace fincept::trading {

struct FyersTick {
    double ltp = 0.0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double prev_close = 0.0;
    double volume = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double tot_buy_qty = 0.0;
    double tot_sell_qty = 0.0;
    double ltq = 0.0;
    double atp = 0.0;
    quint32 timestamp = 0;
    quint64 fytoken = 0;
    quint16 fy_code = 0;

    // Derived (set by FyersWebSocket after token lookup)
    QString symbol;   // plain name, e.g. "RELIANCE"
    QString exchange; // e.g. "NSE"
};

} // namespace fincept::trading
