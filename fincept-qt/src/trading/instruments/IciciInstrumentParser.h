#pragma once
#include "trading/instruments/InstrumentTypes.h"

#include <QByteArray>
#include <QStringList>
#include <QVector>

namespace fincept::trading {

/// Parses ICICI Direct's StockScriptNew.csv (from
/// https://traderweb.icicidirect.com/Content/File/txtFile/ScripFile/StockScriptNew.csv)
/// into normalised Instrument rows. This single, unzipped CSV is the full ICICI
/// security master — equity, F&O, and commodity across NSE/BSE/NFO/BFO/MCX —
/// which lets us avoid the SecurityMaster.zip (and a Qt private-header unzip dep).
///
/// CSV columns (13 fields, header "SC,SN,EC,SM,SG,TK,LS,CD,NS,TS,ISIN,SR,SI"):
///   0:  SC  — ICICI stock code               (e.g. "ICIBAN", "NIFTY")  → brsymbol
///   1:  SN  — security / company name         (free text; may rarely contain commas)
///   2:  EC  — exchange                        (NSE/BSE/NFO/BFO/MCX)
///   3:  SM  — short mnemonic. EQ: == SC. Derivative encodes the contract:
///               "<UNDER>~F:DD-Mon-YYYY"                       (future)
///               "<UNDER>~O:DD-Mon-YYYY:CE|PE:<strike×100>"    (option)
///   4:  SG  — segment                         (EQUITY/DERIVATIVE/COMMODITY)
///   5:  TK  — ICICI numeric token             → instrument_token / exchange_token
///   6:  LS  — lot size
///   7:  CD  — descriptive code (FUT-… / OPT-…; unused, we decode SM instead)
///   8:  NS  — exchange ticker symbol          (e.g. "ICICIBANK", "NIFTY")  → symbol
///   9:  TS  — tick size
///   10: ISIN   11: SR (series)   12: SI (alt scrip id)
///
/// Normalisation mirrors the other parsers so cross-broker lookups behave
/// identically:
///   EQ    : symbol = NS (exchange ticker), brsymbol = SC (ICICI code)
///   FUT   : symbol = underlying + DDMMMYY + "FUT"
///   CE/PE : symbol = underlying + DDMMMYY + strike + CE/PE
/// For derivatives brsymbol carries the *underlying* ICICI code (SC) — that is
/// the `stock_code` Breeze wants on F&O orders, with expiry/strike/right passed
/// separately (the broker reads them from the Instrument's fields).
class IciciInstrumentParser {
  public:
    /// Parse raw CSV bytes downloaded from the StockScriptNew.csv URL.
    /// Returns empty vector on parse failure (logged internally).
    static QVector<Instrument> parse(const QByteArray& csv_data);

  private:
    static Instrument parse_row(const QStringList& cols);
};

} // namespace fincept::trading
