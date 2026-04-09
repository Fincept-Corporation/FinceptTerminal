#include "trading/instruments/ZerodhaInstrumentParser.h"

#include "core/logging/Logger.h"

#include <QDate>
#include <QMap>
#include <QStringList>
#include <QTextStream>

namespace fincept::trading {

// ── Index name mapping (Zerodha tradingsymbol → normalised) ──────────────────
// Mirrors OpenAlgo's master_contract_db.py index_mapping dict.
static const QMap<QString, QString>& index_map() {
    static const QMap<QString, QString> m = {
        {"NIFTY 50", "NIFTY"},
        {"NIFTY BANK", "BANKNIFTY"},
        {"NIFTY FIN SERVICE", "FINNIFTY"},
        {"NIFTY MID SELECT", "MIDCPNIFTY"},
        {"NIFTY NEXT 50", "NIFTYNXT50"},
        {"NIFTY100 LIQ 15", "NIFTY100LIQ15"},
        {"NIFTY MIDCAP 50", "NIFTYMIDCAP50"},
        {"NIFTY MIDCAP 100", "NIFTYMIDCAP100"},
        {"NIFTY SMALLCAP 50", "NIFTYSMALLCAP50"},
        {"NIFTY SMALLCAP 100", "NIFTYSMALLCAP100"},
        {"NIFTY 100", "NIFTY100"},
        {"NIFTY 200", "NIFTY200"},
        {"NIFTY 500", "NIFTY500"},
        {"NIFTY AUTO", "NIFTYAUTO"},
        {"NIFTY ENERGY", "NIFTYENERGY"},
        {"NIFTY FMCG", "NIFTYFMCG"},
        {"NIFTY IT", "NIFTYIT"},
        {"NIFTY MEDIA", "NIFTYMEDIA"},
        {"NIFTY METAL", "NIFTYMETAL"},
        {"NIFTY PHARMA", "NIFTYPHARMA"},
        {"NIFTY PSU BANK", "NIFTYPSUBANK"},
        {"NIFTY REALTY", "NIFTYREALTY"},
        {"NIFTY INFRA", "NIFTYINFRA"},
        {"NIFTY SERV SECTOR", "NIFTYSERVSECTOR"},
        {"NIFTY COMMODITIES", "NIFTYCOMMODITIES"},
        {"NIFTY CONSUMPTION", "NIFTYCONSUMPTION"},
        {"NIFTY CPSE", "NIFTYCPSE"},
        {"NIFTY GROWSECT 15", "NIFTYGROWSECT15"},
        {"NIFTY100 QUALTY30", "NIFTY100QUALTY30"},
        {"NIFTY50 VALUE 20", "NIFTY50VALUE20"},
        {"NIFTY ALPHA 50", "NIFTYALPHA50"},
        {"NIFTY50 TR 2X LEV", "NIFTY50TR2XLEV"},
        {"NIFTY50 PR 2X LEV", "NIFTY50PR2XLEV"},
        {"NIFTY50 TR 1X INV", "NIFTY50TR1XINV"},
        {"NIFTY50 PR 1X INV", "NIFTY50PR1XINV"},
        {"NIFTY ALPHA LOW-VOL", "NIFTYALPHALOWVOL"},
        {"NIFTY200 QUALTY30", "NIFTY200QUALTY30"},
        // BSE indices
        {"SENSEX", "SENSEX"},
        {"SENSEX 50", "SENSEX50"},
        {"BSE100", "BSE100"},
        {"BSE200", "BSE200"},
        {"BSE500", "BSE500"},
        {"BSEAUTO", "BSEAUTO"},
        {"BSEBANKEX", "BSEBANKEX"},
        {"BSEBASICMATERIALS", "BSEBASICMATERIALS"},
        {"BSECDN", "BSECDN"},
        {"BSECG", "BSECG"},
        {"BSEFINANCE", "BSEFINANCE"},
        {"BSEFMCG", "BSEFMCG"},
        {"BSEHEALTHCARE", "BSEHEALTHCARE"},
        {"BSEIT", "BSEIT"},
        {"BSEMETAL", "BSEMETAL"},
        {"BSEOIL&GAS", "BSEOILGAS"},
        {"BSEPOWER", "BSEPOWER"},
        {"BSEREALTY", "BSEREALTY"},
        {"BSETECK", "BSETECK"},
        {"BSETELCOM", "BSETELCOM"},
        {"BSEUTILITY", "BSEUTILITY"},
    };
    return m;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// "2024-03-28" → "28MAR24"
QString ZerodhaInstrumentParser::expiry_nodashes(const QString& expiry_iso) {
    if (expiry_iso.isEmpty())
        return {};
    QDate d = QDate::fromString(expiry_iso, "yyyy-MM-dd");
    if (!d.isValid())
        return {};
    return d.toString("ddMMMyy").toUpper(); // "28MAR24"
}

QString ZerodhaInstrumentParser::map_index_name(const QString& tradingsymbol) {
    return index_map().value(tradingsymbol.toUpper(), tradingsymbol);
}

QString ZerodhaInstrumentParser::normalise_exchange(const QString& exchange, const QString& segment) {
    // Zerodha segment field contains e.g. "NSE", "NFO-FUT", "NFO-OPT", "INDICES"
    if (segment.contains("INDICES", Qt::CaseInsensitive))
        return exchange + "_INDEX";
    return exchange;
}

QString ZerodhaInstrumentParser::normalise_symbol(const QString& tradingsymbol, const QString& name,
                                                  const QString& instrument_type, const QString& expiry_nd,
                                                  double strike) {
    if (instrument_type == "FUT") {
        // e.g. NIFTY + 28MAR24 + FUT = NIFTY28MAR24FUT
        return name.toUpper() + expiry_nd + "FUT";
    }
    if (instrument_type == "CE" || instrument_type == "PE") {
        // strike as integer (drop .0)
        QString strike_str = QString::number(static_cast<int>(strike));
        return name.toUpper() + expiry_nd + strike_str + instrument_type.toUpper();
    }
    if (instrument_type == "INDEX") {
        return map_index_name(tradingsymbol);
    }
    // EQ and everything else: use tradingsymbol as-is
    return tradingsymbol;
}

// ── Row parser ────────────────────────────────────────────────────────────────
// CSV column order from Zerodha:
// 0:instrument_token  1:exchange_token  2:tradingsymbol  3:name
// 4:expiry  5:strike  6:lot_size  7:instrument_type  8:exchange
// 9:tick_size  10:segment

Instrument ZerodhaInstrumentParser::parse_row(const QStringList& cols) {
    if (cols.size() < 11)
        return {};

    Instrument inst;
    inst.broker_id = "zerodha";
    inst.instrument_token = cols[0].toLongLong();
    inst.exchange_token = cols[1].toLongLong();
    inst.brsymbol = cols[2].trimmed();
    inst.name = cols[3].trimmed().toUpper();
    QString expiry_iso = cols[4].trimmed(); // "2024-03-28" or ""
    inst.strike = cols[5].toDouble();
    inst.lot_size = cols[6].toInt();
    QString itype = cols[7].trimmed().toUpper();
    inst.brexchange = cols[8].trimmed().toUpper();
    inst.tick_size = cols[9].toDouble();
    QString segment = cols[10].trimmed().toUpper();

    // Normalise expiry to DD-MMM-YY display format and nodashes for symbol
    QString exp_nd = expiry_nodashes(expiry_iso);
    if (!exp_nd.isEmpty()) {
        // Store as "28-MAR-24" for display
        inst.expiry = exp_nd.left(2) + "-" + exp_nd.mid(2, 3) + "-" + exp_nd.right(2);
    }

    inst.exchange = normalise_exchange(inst.brexchange, segment);
    inst.instrument_type = parse_instrument_type(itype);
    inst.symbol = normalise_symbol(inst.brsymbol, inst.name, itype, exp_nd, inst.strike);

    return inst;
}

// ── Public entry point ────────────────────────────────────────────────────────

QVector<Instrument> ZerodhaInstrumentParser::parse(const QByteArray& csv_data) {
    QVector<Instrument> results;
    if (csv_data.isEmpty()) {
        LOG_WARN("ZerodhaParser", "Empty CSV data received");
        return results;
    }

    QTextStream stream(csv_data);
    QString header = stream.readLine(); // skip header row
    Q_UNUSED(header)

    int row = 1;
    results.reserve(150000); // Zerodha has ~120k instruments
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;

        // Simple CSV split — Zerodha's instruments CSV has no quoted fields with commas
        QStringList cols = line.split(',');
        if (cols.size() < 11) {
            LOG_WARN("ZerodhaParser", QString("Row %1: only %2 columns, skipping").arg(row).arg(cols.size()));
            ++row;
            continue;
        }

        Instrument inst = parse_row(cols);
        // Skip rows with no token (malformed)
        if (inst.instrument_token == 0) {
            ++row;
            continue;
        }

        results.append(inst);
        ++row;
    }

    LOG_INFO("ZerodhaParser", QString("Parsed %1 instruments").arg(results.size()));
    return results;
}

} // namespace fincept::trading
