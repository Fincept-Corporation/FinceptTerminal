#pragma once
// NorenSymbolParser — shared canonical builder for NorenAPI-family masters
// (Shoonya, Flattrade). Each broker has its own download + column front-end
// (their file formats differ), but the symbol/expiry/exchange synthesis is
// identical, so it lives here.

#include "trading/instruments/InstrumentTypes.h"

#include <QString>

namespace fincept::trading {

/// Normalised native fields extracted from one Noren-family master row.
struct NorenRow {
    QString brexchange;     ///< Already-canonical exch: NSE/BSE/NFO/BFO/CDS/MCX.
    QString token;          ///< Numeric token string.
    QString name;           ///< Underlying (the Symbol column).
    QString brsymbol;       ///< Native trading symbol (TradingSymbol column).
    QString raw_instrument; ///< Instrument column: EQ/BE/INDEX/UNDIND/OPTSTK/…
    QString option_type;    ///< XX (future) / CE / PE / "" (cash).
    double strike = 0.0;
    QString expiry_raw;     ///< e.g. "30-JUN-2026".
    int lot_size = 1;
    double tick_size = 0.05;
    bool is_bse_index = false; ///< Manually-injected BSE index (SENSEX/BANKEX).
};

/// Build a canonical Instrument from a NorenRow. Decides instrument type from
/// option_type + raw_instrument, synthesises the canonical symbol, and maps the
/// exchange (adding _INDEX for indices).
Instrument noren_build(const QString& broker_id, const NorenRow& r);

} // namespace fincept::trading
