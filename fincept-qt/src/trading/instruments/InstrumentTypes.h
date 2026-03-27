#pragma once
#include <QString>
#include <QVector>

namespace fincept::trading {

enum class InstrumentType { EQ, FUT, CE, PE, INDEX, UNKNOWN };

inline InstrumentType parse_instrument_type(const QString& s) {
    if (s == "EQ")    return InstrumentType::EQ;
    if (s == "FUT")   return InstrumentType::FUT;
    if (s == "CE")    return InstrumentType::CE;
    if (s == "PE")    return InstrumentType::PE;
    if (s == "INDEX") return InstrumentType::INDEX;
    return InstrumentType::UNKNOWN;
}

inline const char* instrument_type_str(InstrumentType t) {
    switch (t) {
        case InstrumentType::EQ:    return "EQ";
        case InstrumentType::FUT:   return "FUT";
        case InstrumentType::CE:    return "CE";
        case InstrumentType::PE:    return "PE";
        case InstrumentType::INDEX: return "INDEX";
        default:                    return "UNKNOWN";
    }
}

/// One row in the instruments table.
struct Instrument {
    qint64  instrument_token = 0;   // Zerodha numeric token (used in historical API)
    qint64  exchange_token   = 0;   // Zerodha exchange-level token
    QString symbol;                 // Normalised symbol  e.g. "NIFTY28MAR24FUT"
    QString brsymbol;               // Broker native      e.g. "NIFTY 50 MAR24 FUT"
    QString name;                   // Underlying         e.g. "NIFTY"
    QString exchange;               // Normalised exchange e.g. "NSE", "NFO", "NSE_INDEX"
    QString brexchange;             // Broker exchange     e.g. "NSE", "NFO"
    QString expiry;                 // "28-MAR-24" or ""
    double  strike    = 0.0;        // Option strike, 0 for others
    int     lot_size  = 1;
    InstrumentType instrument_type = InstrumentType::UNKNOWN;
    double  tick_size = 0.05;
    QString broker_id;              // "zerodha", "angelone", etc.
};

} // namespace fincept::trading
