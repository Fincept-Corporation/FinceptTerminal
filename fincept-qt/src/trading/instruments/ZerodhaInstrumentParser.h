#pragma once
#include "trading/instruments/InstrumentTypes.h"

#include <QByteArray>
#include <QVector>

namespace fincept::trading {

/// Parses Zerodha's instruments CSV (from GET https://api.kite.trade/instruments)
/// and normalises each row into an Instrument struct.
///
/// Zerodha CSV columns (in order):
///   instrument_token, exchange_token, tradingsymbol, name, expiry,
///   strike, lot_size, instrument_type, exchange, tick_size, segment
///
/// Normalisation rules (mirrored from OpenAlgo master_contract_db.py):
///   EQ    : symbol = tradingsymbol  (unchanged)
///   FUT   : symbol = name + expiry_nodashes + "FUT"   e.g. "NIFTY28MAR24FUT"
///   CE/PE : symbol = name + expiry_nodashes + strike_int + type
///           e.g. "NIFTY28MAR2424000CE"
///   INDEX : symbol = mapped name   (NIFTY 50 → NIFTY, NIFTY BANK → BANKNIFTY …)
///   exchange normalisation:
///     segment == "INDICES" → exchange = brexchange + "_INDEX"
///     otherwise exchange = brexchange (NSE, BSE, NFO, CDS, MCX …)
class ZerodhaInstrumentParser {
  public:
    /// Parse raw CSV bytes downloaded from /instruments.
    /// Returns empty vector on parse failure (logged internally).
    static QVector<Instrument> parse(const QByteArray& csv_data);

  private:
    static Instrument parse_row(const QStringList& cols);
    static QString normalise_symbol(const QString& tradingsymbol, const QString& name, const QString& instrument_type,
                                    const QString& expiry_nodashes, double strike);
    static QString normalise_exchange(const QString& exchange, const QString& segment);
    static QString expiry_nodashes(const QString& expiry_iso); // "2024-03-28" → "28MAR24"
    static QString map_index_name(const QString& tradingsymbol);
};

} // namespace fincept::trading
