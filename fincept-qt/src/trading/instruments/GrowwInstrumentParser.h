#pragma once
#include "trading/instruments/InstrumentTypes.h"

#include <QByteArray>
#include <QVector>

namespace fincept::trading {

/// Parses Groww's instruments CSV (from
/// https://growwapi-assets.groww.in/instruments/instrument.csv) and normalises
/// each row into an Instrument struct.
///
/// Groww CSV columns (21 fields, in order):
///   0: exchange                1: exchange_token         2: trading_symbol
///   3: groww_symbol            4: name                   5: instrument_type
///   6: segment                 7: series                 8: isin
///   9: underlying_symbol       10: underlying_exchange_token
///   11: expiry_date            12: strike_price          13: lot_size
///   14: tick_size              15: freeze_quantity       16: is_reserved
///   17: buy_allowed            18: sell_allowed          19: internal_trading_symbol
///   20: is_intraday
///
/// Normalisation rules (mirrored from ZerodhaInstrumentParser so that cross-broker
/// symbol lookups behave identically):
///   EQ    : symbol = trading_symbol
///   FUT   : symbol = underlying_symbol + expiry_nodashes + "FUT"
///   CE/PE : symbol = underlying_symbol + expiry_nodashes + strike_int + type
///   IDX   : symbol = map_index_name(trading_symbol); exchange = "NSE_INDEX" / "BSE_INDEX"
///   exchange normalisation (fincept convention):
///     CASH + NSE → NSE          FNO + NSE → NFO        COMMODITY + MCX → MCX
///     CASH + BSE → BSE          FNO + BSE → BFO        (IDX → <exchange>_INDEX)
class GrowwInstrumentParser {
  public:
    /// Parse raw CSV bytes downloaded from the Groww instruments URL.
    /// Returns empty vector on parse failure (logged internally).
    static QVector<Instrument> parse(const QByteArray& csv_data);

  private:
    static Instrument parse_row(const QStringList& cols);
    static QString normalise_symbol(const QString& trading_symbol, const QString& underlying, const QString& itype,
                                    const QString& expiry_nd, double strike);
    static QString normalise_exchange(const QString& raw_exchange, const QString& segment, const QString& itype);
    static QString expiry_nodashes(const QString& expiry_iso); // "2026-04-28" → "28APR26"
    static QString map_index_name(const QString& trading_symbol);
};

} // namespace fincept::trading
