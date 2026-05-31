#pragma once
#include "trading/instruments/InstrumentTypes.h"

#include <QByteArray>
#include <QVector>

namespace fincept::trading {

/// Parses Dhan's scrip-master CSV (from
/// https://images.dhan.co/api-data/api-scrip-master.csv) and normalises each row
/// into an Instrument struct whose `instrument_token` carries Dhan's numeric
/// securityId — which DhanBroker::lookup_security_id() reads back for orders,
/// quotes, and historical data.
///
/// Dhan CSV columns (16 fields, in order):
///   0:  SEM_EXM_EXCH_ID        (NSE/BSE/MCX)
///   1:  SEM_SEGMENT            (E=equity, D=derivative, C=currency, M=commodity, I=index)
///   2:  SEM_SMST_SECURITY_ID   (numeric — used as instrument_token)
///   3:  SEM_INSTRUMENT_NAME    (EQUITY/INDEX/OPTSTK/OPTIDX/FUTSTK/FUTIDX/OPTFUT/FUTCOM/OPTCUR/FUTCUR)
///   4:  SEM_EXPIRY_CODE        5:  SEM_TRADING_SYMBOL   6:  SEM_LOT_UNITS
///   7:  SEM_CUSTOM_SYMBOL      8:  SEM_EXPIRY_DATE ("YYYY-MM-DD HH:MM:SS" or blank)
///   9:  SEM_STRIKE_PRICE (-0.01 sentinel for non-options)   10: SEM_OPTION_TYPE (CE/PE/XX)
///   11: SEM_TICK_SIZE          12: SEM_EXPIRY_FLAG    13: SEM_EXCH_INSTRUMENT_TYPE
///   14: SEM_SERIES             15: SM_SYMBOL_NAME (underlying)
///
/// Normalisation mirrors the other parsers so cross-broker lookups behave
/// identically:
///   EQ    : symbol = trading_symbol            (e.g. "RELIANCE")
///   INDEX : symbol = map_index_name(trading_symbol); exchange = "<EX>_INDEX"
///   FUT   : symbol = underlying + DDMMMYY + "FUT"
///   CE/PE : symbol = underlying + DDMMMYY + strike + CE/PE
///   exchange: E→NSE/BSE, D→NFO/BFO, C→CDS/BCD, M→MCX, I→<EX>_INDEX
class DhanInstrumentParser {
  public:
    /// Parse raw CSV bytes downloaded from the Dhan scrip-master URL.
    /// Returns empty vector on parse failure (logged internally).
    static QVector<Instrument> parse(const QByteArray& csv_data);

  private:
    static Instrument parse_row(const QStringList& cols);
    static QString normalise_exchange(const QString& exch, const QString& segment, const QString& itype);
    static QString normalise_symbol(const QString& trading_symbol, const QString& underlying, const QString& itype,
                                    const QString& expiry_nd, double strike);
    static QString expiry_nodashes(const QString& expiry_raw); // "2024-08-28 14:30:00" → "28AUG24"
    static QString map_index_name(const QString& trading_symbol);
};

} // namespace fincept::trading
