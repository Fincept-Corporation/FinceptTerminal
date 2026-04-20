#include "trading/instruments/GrowwInstrumentParser.h"

#include "core/logging/Logger.h"

#include <QDate>
#include <QHash>
#include <QMap>
#include <QStringList>
#include <QTextStream>

namespace fincept::trading {
namespace {

// Groww uses short trading_symbols for indices (NIFTY, BANKNIFTY, INDIAVIX, MIDCAP50 …).
// Most already match the fincept normalised form — map the handful that differ.
// Anonymous namespace isolates this from ZerodhaInstrumentParser's index_map()
// under Ninja's unity-build (same symbol name otherwise collides).
const QMap<QString, QString>& groww_index_map() {
    static const QMap<QString, QString> m = {
        // Groww short form → fincept normalised
        {"NIFTY", "NIFTY"},
        {"BANKNIFTY", "BANKNIFTY"},
        {"NIFTYJR", "NIFTYNXT50"},
        {"MIDCAP50", "NIFTYMIDCAP50"},
        {"NIFTYMIDCAP", "NIFTYMIDCAP100"},
        {"NIFTYMIDCAP150", "NIFTYMIDCAP150"},
        {"NIFTYSMALLCAP250", "NIFTYSMALLCAP250"},
        {"NIFTYMIDSELECT", "MIDCPNIFTY"},
        {"NIFTYPVTBANK", "NIFTYPVTBANK"},
        {"NIFTYPHARMA", "NIFTYPHARMA"},
        {"NIFTYIT", "NIFTYIT"},
        {"NIFTYMETAL", "NIFTYMETAL"},
        {"NIFTYMEDIA", "NIFTYMEDIA"},
        {"NIFTYAUTO", "NIFTYAUTO"},
        {"NIFTYENERGY", "NIFTYENERGY"},
        {"NIFTYFMCG", "NIFTYFMCG"},
        {"NIFTYREALTY", "NIFTYREALTY"},
        {"NIFTYPSUBANK", "NIFTYPSUBANK"},
        {"NIFTYINFRA", "NIFTYINFRA"},
        {"NIFTY100", "NIFTY100"},
        {"NIFTY200", "NIFTY200"},
        {"NIFTY500", "NIFTY500"},
        {"NIFTYTOTALMCAP", "NIFTYTOTALMCAP"},
        {"INDIAVIX", "INDIAVIX"},
        // BSE
        {"SENSEX", "SENSEX"},
        {"BANKEX", "BANKEX"},
        {"SENSEX50", "SENSEX50"},
    };
    return m;
}

} // namespace

// "2026-04-28" → "28APR26"
QString GrowwInstrumentParser::expiry_nodashes(const QString& expiry_iso) {
    if (expiry_iso.isEmpty())
        return {};
    QDate d = QDate::fromString(expiry_iso, "yyyy-MM-dd");
    if (!d.isValid())
        return {};
    return d.toString("ddMMMyy").toUpper();
}

QString GrowwInstrumentParser::map_index_name(const QString& trading_symbol) {
    return groww_index_map().value(trading_symbol.toUpper(), trading_symbol.toUpper());
}

// Groww exchange column is NSE/BSE/MCX even for derivatives; segment distinguishes
// CASH vs FNO vs COMMODITY. Fincept uses NFO/BFO for Indian derivatives.
QString GrowwInstrumentParser::normalise_exchange(const QString& raw_exchange, const QString& segment,
                                                  const QString& itype) {
    QString ex = raw_exchange.toUpper();
    QString seg = segment.toUpper();

    if (itype == "IDX") {
        // Index: append _INDEX suffix
        return ex + "_INDEX";
    }
    if (seg == "FNO") {
        if (ex == "NSE")
            return QStringLiteral("NFO");
        if (ex == "BSE")
            return QStringLiteral("BFO");
        return ex; // unexpected, leave as-is
    }
    if (seg == "COMMODITY") {
        return QStringLiteral("MCX");
    }
    // CASH → NSE/BSE unchanged
    return ex;
}

QString GrowwInstrumentParser::normalise_symbol(const QString& trading_symbol, const QString& underlying,
                                                const QString& itype, const QString& expiry_nd, double strike) {
    if (itype == "FUT") {
        // underlying + DDMMMYY + FUT, e.g. "NIFTY28APR26FUT"
        if (!underlying.isEmpty() && !expiry_nd.isEmpty())
            return underlying.toUpper() + expiry_nd + "FUT";
        return trading_symbol; // fallback
    }
    if (itype == "CE" || itype == "PE") {
        if (!underlying.isEmpty() && !expiry_nd.isEmpty()) {
            // Drop decimals for whole-number strikes; keep decimals otherwise (e.g. silver)
            QString strike_str;
            if (strike == static_cast<qint64>(strike))
                strike_str = QString::number(static_cast<qint64>(strike));
            else
                strike_str = QString::number(strike, 'f', 2);
            return underlying.toUpper() + expiry_nd + strike_str + itype;
        }
        return trading_symbol;
    }
    if (itype == "IDX") {
        return map_index_name(trading_symbol);
    }
    // EQ and everything else: use trading_symbol as-is (already exchange-canonical).
    return trading_symbol;
}

static InstrumentType map_groww_type(const QString& itype) {
    if (itype == "EQ")
        return InstrumentType::EQ;
    if (itype == "FUT")
        return InstrumentType::FUT;
    if (itype == "CE")
        return InstrumentType::CE;
    if (itype == "PE")
        return InstrumentType::PE;
    if (itype == "IDX")
        return InstrumentType::INDEX;
    return InstrumentType::UNKNOWN;
}

Instrument GrowwInstrumentParser::parse_row(const QStringList& cols) {
    if (cols.size() < 21)
        return {};

    Instrument inst;
    inst.broker_id = QStringLiteral("groww");

    QString raw_exchange = cols[0].trimmed();
    QString exchange_token_raw = cols[1].trimmed();
    QString trading_symbol = cols[2].trimmed();
    // cols[3]  groww_symbol (human-readable, unused)
    QString name_field = cols[4].trimmed();
    QString itype = cols[5].trimmed().toUpper();
    QString segment = cols[6].trimmed().toUpper();
    // cols[7]  series (unused)
    // cols[8]  isin (unused)
    QString underlying = cols[9].trimmed().toUpper();
    // cols[10] underlying_exchange_token (unused for now)
    QString expiry_iso = cols[11].trimmed();
    double strike = cols[12].toDouble();
    int lot_size = cols[13].toInt();
    double tick_size = cols[14].toDouble();

    // Groww indices carry a string exchange_token (e.g. "NIFTY"); other types are numeric.
    // Parse as long; non-numeric tokens get 0 and we fall back to symbol lookup.
    bool token_ok = false;
    qint64 etoken = exchange_token_raw.toLongLong(&token_ok);
    inst.exchange_token = token_ok ? etoken : 0;
    inst.instrument_token = inst.exchange_token; // Groww has only one token per row

    inst.brsymbol = trading_symbol;
    // For F&O the "name" column is empty, so prefer underlying_symbol; for EQ use the
    // descriptive security name when present.
    inst.name = underlying.isEmpty() ? name_field.toUpper() : underlying;
    inst.brexchange = raw_exchange.toUpper();
    inst.tick_size = tick_size > 0 ? tick_size : 0.05;
    inst.lot_size = lot_size > 0 ? lot_size : 1;

    // Strike of -0.01 is a sentinel Groww uses for FUT rows (no strike).
    inst.strike = (itype == "CE" || itype == "PE") ? strike : 0.0;

    if (!expiry_iso.isEmpty()) {
        QString nd = expiry_nodashes(expiry_iso);
        if (!nd.isEmpty())
            inst.expiry = nd.left(2) + "-" + nd.mid(2, 3) + "-" + nd.right(2); // "28-APR-26"
    }

    inst.exchange = normalise_exchange(raw_exchange, segment, itype);
    inst.instrument_type = map_groww_type(itype);
    inst.symbol = normalise_symbol(trading_symbol, underlying, itype, expiry_nodashes(expiry_iso), strike);

    return inst;
}

QVector<Instrument> GrowwInstrumentParser::parse(const QByteArray& csv_data) {
    QVector<Instrument> results;
    if (csv_data.isEmpty()) {
        LOG_WARN("GrowwParser", "Empty CSV data received");
        return results;
    }

    QTextStream stream(csv_data);
    QString header = stream.readLine();
    Q_UNUSED(header)

    int skipped = 0;
    results.reserve(250000); // Groww has ~200k rows (equity + full F&O chain)
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;

        // Groww CSV has no quoted fields with commas — simple split is safe.
        QStringList cols = line.split(',');
        if (cols.size() < 21) {
            ++skipped;
            continue;
        }

        Instrument inst = parse_row(cols);
        // Require at least trading_symbol + brexchange.
        if (inst.brsymbol.isEmpty() || inst.brexchange.isEmpty()) {
            ++skipped;
            continue;
        }

        results.append(inst);
    }

    LOG_INFO("GrowwParser",
             QString("Parsed %1 instruments (skipped %2 malformed rows)").arg(results.size()).arg(skipped));
    return results;
}

} // namespace fincept::trading
