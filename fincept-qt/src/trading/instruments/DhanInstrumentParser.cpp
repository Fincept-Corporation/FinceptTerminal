#include "trading/instruments/DhanInstrumentParser.h"

#include "trading/instruments/InstrumentNormalize.h"

#include "core/logging/Logger.h"

#include <QDate>
#include <QMap>
#include <QStringList>
#include <QTextStream>

namespace fincept::trading {
namespace {

// Dhan index trading symbols carry spaces ("NIFTY 50", "NIFTY BANK"); map the
// well-known tradeable ones to the fincept normalised form. Anonymous namespace
// isolates this from the other parsers' index_map() under unity builds.
const QMap<QString, QString>& dhan_index_map() {
    static const QMap<QString, QString> m = {
        {"NIFTY 50", "NIFTY"},
        {"NIFTY BANK", "BANKNIFTY"},
        {"NIFTY FIN SERVICE", "FINNIFTY"},
        {"NIFTY FINANCIAL SERVICES", "FINNIFTY"},
        {"NIFTY MID SELECT", "MIDCPNIFTY"},
        {"NIFTY MIDCAP SELECT", "MIDCPNIFTY"},
        {"NIFTY NEXT 50", "NIFTYNXT50"},
        {"INDIA VIX", "INDIAVIX"},
        {"SENSEX", "SENSEX"},
        {"BANKEX", "BANKEX"},
        {"SENSEX 50", "SENSEX50"},
    };
    return m;
}

InstrumentType map_dhan_type(const QString& instrument_name, const QString& option_type) {
    if (instrument_name == "EQUITY")
        return InstrumentType::EQ;
    if (instrument_name == "INDEX")
        return InstrumentType::INDEX;
    if (instrument_name.startsWith("OPT")) {
        if (option_type == "CE")
            return InstrumentType::CE;
        if (option_type == "PE")
            return InstrumentType::PE;
        return InstrumentType::UNKNOWN;
    }
    if (instrument_name.startsWith("FUT"))
        return InstrumentType::FUT;
    return InstrumentType::UNKNOWN;
}

} // namespace

QString DhanInstrumentParser::map_index_name(const QString& trading_symbol) {
    const QString up = trading_symbol.toUpper();
    // Broker map first (handles Dhan's spaced names), then the shared normalizer
    // as the final authority so the canonical index name matches every broker.
    const QString mapped = dhan_index_map().value(up, QString(up).remove(' '));
    return norm::normalise_index_symbol(mapped);
}

// "2024-08-28 14:30:00" (or "2024-08-28") → "28AUG24"
QString DhanInstrumentParser::expiry_nodashes(const QString& expiry_raw) {
    if (expiry_raw.isEmpty())
        return {};
    QDate d = QDate::fromString(expiry_raw.left(10), "yyyy-MM-dd");
    if (!d.isValid() || d.year() <= 1)
        return {}; // Dhan uses "0001-01-01" as a null-expiry sentinel
    return d.toString("ddMMMyy").toUpper();
}

QString DhanInstrumentParser::normalise_exchange(const QString& exch, const QString& segment, const QString& itype) {
    Q_UNUSED(itype)
    const QString ex = exch.toUpper();
    const QString seg = segment.toUpper();
    if (seg == "I")
        return ex + "_INDEX";
    if (seg == "D")
        return ex == "NSE" ? QStringLiteral("NFO") : ex == "BSE" ? QStringLiteral("BFO") : ex;
    if (seg == "C")
        return ex == "NSE" ? QStringLiteral("CDS") : ex == "BSE" ? QStringLiteral("BCD") : ex;
    if (seg == "M")
        return ex == "MCX" ? QStringLiteral("MCX") : ex;
    // E (equity/cash) and anything else → exchange unchanged.
    return ex;
}

QString DhanInstrumentParser::normalise_symbol(const QString& trading_symbol, const QString& underlying,
                                               const QString& itype, const QString& expiry_nd, double strike) {
    if (itype == "INDEX")
        return map_index_name(trading_symbol);
    if (itype == "FUT") {
        if (!underlying.isEmpty() && !expiry_nd.isEmpty())
            return map_index_name(underlying) + expiry_nd + "FUT";
        return trading_symbol.toUpper();
    }
    if (itype == "CE" || itype == "PE") {
        if (!underlying.isEmpty() && !expiry_nd.isEmpty()) {
            QString strike_str = norm::format_strike(strike);
            return map_index_name(underlying) + expiry_nd + strike_str + itype;
        }
        return trading_symbol.toUpper();
    }
    // EQ and everything else: trading_symbol as-is (already exchange-canonical, bare).
    return trading_symbol.toUpper();
}

Instrument DhanInstrumentParser::parse_row(const QStringList& cols) {
    if (cols.size() < 16)
        return {};

    const QString exch = cols[0].trimmed();           // NSE/BSE/MCX
    const QString segment = cols[1].trimmed();         // E/D/C/M/I
    const QString security_id = cols[2].trimmed();     // numeric
    const QString instrument_name = cols[3].trimmed(); // EQUITY/INDEX/OPTSTK...
    const QString trading_symbol = cols[5].trimmed();
    const double lot = cols[6].toDouble();
    const QString expiry_raw = cols[8].trimmed();
    const double strike = cols[9].toDouble();
    const QString option_type = cols[10].trimmed().toUpper();
    const double tick = cols[11].toDouble();
    const QString underlying = cols[15].trimmed().toUpper();

    Instrument inst;
    inst.broker_id = QStringLiteral("dhan");

    bool token_ok = false;
    const qint64 sid = security_id.toLongLong(&token_ok);
    inst.instrument_token = token_ok ? sid : 0;
    inst.exchange_token = inst.instrument_token;

    const InstrumentType type = map_dhan_type(instrument_name, option_type);
    QString itype_str;
    switch (type) {
    case InstrumentType::EQ:    itype_str = "EQ"; break;
    case InstrumentType::FUT:   itype_str = "FUT"; break;
    case InstrumentType::CE:    itype_str = "CE"; break;
    case InstrumentType::PE:    itype_str = "PE"; break;
    case InstrumentType::INDEX: itype_str = "INDEX"; break;
    default:                    itype_str = "UNKNOWN"; break;
    }

    inst.brsymbol = trading_symbol;
    inst.name = underlying.isEmpty() ? trading_symbol.toUpper() : underlying;
    inst.brexchange = exch.toUpper();
    inst.tick_size = tick > 0 ? tick : 0.05;
    inst.lot_size = lot > 0 ? static_cast<int>(lot) : 1;
    inst.strike = (type == InstrumentType::CE || type == InstrumentType::PE) ? strike : 0.0;

    const QString expiry_nd = expiry_nodashes(expiry_raw);
    if (!expiry_nd.isEmpty())
        inst.expiry = expiry_nd.left(2) + "-" + expiry_nd.mid(2, 3) + "-" + expiry_nd.right(2); // "28-AUG-24"

    inst.exchange = normalise_exchange(exch, segment, itype_str);
    inst.instrument_type = type;
    inst.symbol = normalise_symbol(trading_symbol, underlying, itype_str, expiry_nd, strike);

    return inst;
}

QVector<Instrument> DhanInstrumentParser::parse(const QByteArray& csv_data) {
    QVector<Instrument> results;
    if (csv_data.isEmpty()) {
        LOG_WARN("DhanParser", "Empty CSV data received");
        return results;
    }

    QTextStream stream(csv_data);
    QString header = stream.readLine();
    Q_UNUSED(header)

    int skipped = 0;
    results.reserve(230000); // Dhan ships ~220k rows (full equity + F&O + commodity master)
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.isEmpty())
            continue;

        // Dhan's CSV has exactly 16 fields and no quoted/comma values — simple split is safe.
        const QStringList cols = line.split(',');
        if (cols.size() < 16) {
            ++skipped;
            continue;
        }

        Instrument inst = parse_row(cols);
        // Require a usable securityId + symbol + exchange.
        if (inst.instrument_token <= 0 || inst.symbol.isEmpty() || inst.exchange.isEmpty()) {
            ++skipped;
            continue;
        }
        results.append(inst);
    }

    LOG_INFO("DhanParser",
             QString("Parsed %1 instruments (skipped %2 malformed rows)").arg(results.size()).arg(skipped));
    return results;
}

} // namespace fincept::trading
