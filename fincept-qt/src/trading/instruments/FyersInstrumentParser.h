#pragma once
#include "trading/instruments/InstrumentTypes.h"

#include <QByteArray>
#include <QVector>

namespace fincept::trading {

/// Parses Fyers' public symbol master JSON files and normalises each entry
/// into an Instrument struct.
///
/// Fyers JSON is a flat object: { "NSE:SYMBOL-EQ": { ... }, ... }
/// Each value carries: fyToken, exToken, exSymbol, underSym, optType,
/// strikePrice, minLotSize, tickSize, expiryDate (unix ts), symTicker,
/// exchangeName, exInstType (0=EQ, 11=FUT, 14=CE, 15=PE), segment.
///
/// Downloads cover multiple exchanges (NSE_CM, NSE_FO, BSE_CM, MCX_COM, etc.)
/// and are concatenated before parsing.
class FyersInstrumentParser {
  public:
    static QVector<Instrument> parse(const QByteArray& json_data);
};

} // namespace fincept::trading
