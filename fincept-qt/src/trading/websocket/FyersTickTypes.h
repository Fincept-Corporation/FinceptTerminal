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
    double oi = 0.0;        // open interest (sf field 12); 0 for cash/equity
    quint32 timestamp = 0;
    quint64 fytoken = 0;
    quint16 fy_code = 0;

    // Derived (set by FyersWebSocket after token lookup)
    QString symbol;   // plain name, e.g. "RELIANCE"
    QString exchange; // e.g. "NSE"
};

// ── Fyers F&O option symbol reconciliation ──────────────────────────────────
//
// Fyers spells the SAME option two ways, and the two never string-match:
//   • HSM / WebSocket tick feed :  "NIFTY09JUN26C23200"  (… C|P + strike)
//   • REST / chain-v3 / position:  "NIFTY2660923200CE"   (… strike + CE|PE)
// So an option position cannot be marked-to-market by comparing the live tick's
// symbol against the position's symbol the way equities are. Callers reconcile
// the two by a format-independent leg identity — (underlying, strike, side).

struct FyersOptionLeg {
    QString underlying;   // leading alphabetic root, e.g. "NIFTY", "BANKNIFTY"
    double strike = 0.0;  // 0 for the CE/PE spelling (its strike is positional —
                          // the caller matches it against the live tick's strike)
    bool is_call = false; // true = CE/Call, false = PE/Put
    bool valid = false;   // false for non-options (equities, futures, indices)
};

// Parse a Fyers option symbol in EITHER spelling (an "NSE:"/"NFO:" prefix is
// stripped first). The HSM/brsymbol form carries a parseable strike; the CE/PE
// form yields underlying + side only. Returns {valid=false} for anything that
// isn't an option — the digit-before-"CE"/"PE" guard keeps equities that merely
// end in those letters (e.g. "RELIANCE", "ACE") from being misread as options.
inline FyersOptionLeg fyers_parse_option(const QString& raw) {
    FyersOptionLeg leg;
    QString s = raw;
    const qsizetype colon = s.lastIndexOf(QLatin1Char(':'));
    if (colon >= 0)
        s = s.mid(colon + 1);
    const int n = int(s.size());
    if (n < 4)
        return leg;
    int u = 0;
    while (u < n && s.at(u).isLetter())
        ++u;                                   // underlying root (leading letters)
    if (u == 0)
        return leg;

    // Form 1 — HSM/brsymbol: trailing strike digits, a 'C'/'P' before them, and a
    // (date) digit before that:  "NIFTY09JUN26C23200".
    int i = n - 1;
    while (i >= 0 && s.at(i).isDigit())
        --i;
    if (i > 0 && i < n - 1) {
        const QChar cp = s.at(i);
        if ((cp == QLatin1Char('C') || cp == QLatin1Char('P')) && s.at(i - 1).isDigit()) {
            bool ok = false;
            const double k = QStringView(s).mid(i + 1).toDouble(&ok);
            if (ok && k > 0) {
                leg.underlying = s.left(u);
                leg.strike = k;
                leg.is_call = (cp == QLatin1Char('C'));
                leg.valid = true;
                return leg;
            }
        }
    }

    // Form 2 — REST/chain/position: "…<strike>CE" / "…<strike>PE". Require a digit
    // immediately before the CE/PE so equities ending in those letters don't match.
    const QStringView suf = QStringView(s).right(2);
    if ((suf == QLatin1String("CE") || suf == QLatin1String("PE")) && s.at(n - 3).isDigit()) {
        leg.underlying = s.left(u);
        leg.is_call = (suf == QLatin1String("CE"));
        leg.valid = true;                      // strike stays 0 — see FyersOptionLeg
        return leg;
    }
    return leg;
}

// True when `sym` is a Fyers F&O option symbol (either spelling). Used so the WS
// subscribe path maps these to "NSE:<sym>" instead of the equity "NSE:<sym>-EQ"
// form — otherwise option positions never resolve a live feed.
inline bool is_fyers_fno_symbol(const QString& sym) {
    return fyers_parse_option(sym).valid;
}

} // namespace fincept::trading
