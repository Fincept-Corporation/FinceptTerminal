#pragma once
// MarketHours — lightweight NSE session clock.
//
// Header-only so it can be used from any layer without a CMake/unity entry.
// v1 covers only the regular cash + F&O session window by weekday + clock;
// there is NO holiday calendar yet, so NSE trading holidays read as "open"
// and callers gracefully fall through to their REST/fallback path (a stale
// snapshot, not a crash). Add a holiday source here when one is maintained.

#include <QDateTime>
#include <QTime>

namespace fincept::core::market {

// True during the NSE regular equity/F&O session: Mon–Fri, 09:15–15:30 IST.
// IST is derived by shifting the UTC instant by +5:30 (no tz database needed),
// matching the existing DataStreamManager token-expiry convention.
inline bool nse_fo_market_open() {
    const QDateTime ist = QDateTime::currentDateTimeUtc().addSecs((5 * 3600) + (30 * 60));
    const int dow = ist.date().dayOfWeek();  // 1=Mon … 7=Sun
    if (dow == 6 || dow == 7)
        return false;
    const QTime t = ist.time();
    static const QTime kOpen(9, 15, 0);
    static const QTime kClose(15, 30, 0);
    return t >= kOpen && t <= kClose;
}

} // namespace fincept::core::market
