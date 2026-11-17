#pragma once
// MarketHours — NSE session clock, kept for existing callers.
//
// Delegates to MarketSessions (core/market/MarketSessions.h), which knows the
// NSE time zone, the pre-open / post-close phases and the NSE holiday list, so
// a trading holiday now reads as closed and callers fall through to their
// REST / fallback path. New code should use MarketSessions directly.

#include "core/market/MarketSessions.h"

#include <QDateTime>
#include <QString>

namespace fincept::core::market {

// True during the NSE regular equity/F&O session (09:15-15:30 IST on NSE
// trading days). False when the venue table is unavailable.
inline bool nse_fo_market_open() {
    const Venue* nse = venue_by_code(QStringLiteral("NSE"));
    if (!nse)
        return false;
    return session_status(*nse, QDateTime::currentDateTimeUtc()).state == SessionState::Open;
}

} // namespace fincept::core::market
