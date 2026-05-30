#include "trading/instruments/parsers/AliceBlueInstrumentParser.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/parsers/CsvUtil.h"

#include <QHash>
#include <QStringList>
#include <QStringView>

namespace fincept::trading {
namespace {

const QStringList kKeys = {"NSE", "BSE", "NFO", "BFO", "CDS", "BCD", "MCX", "INDICES"};
const QString kMarker = QStringLiteral("#EX:");

QByteArray download_aliceblue(const BrokerCredentials&) {
    QByteArray combined;
    for (const QString& key : kKeys) {
        const QString url =
            QString("https://v2api.aliceblueonline.com/restpy/static/contract_master/V2/%1.csv").arg(key);
        const QByteArray csv = http_get_blocking(url);
        if (csv.isEmpty())
            continue;
        combined += (kMarker + key + "\n").toUtf8();
        combined += csv;
        if (!csv.endsWith('\n'))
            combined += '\n';
    }
    return combined;
}

QVector<Instrument> parse_aliceblue(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');

    QString key;
    QHash<QString, int> idx;
    bool want_header = false;
    for (const QStringView lv : lines) {
        const QString line = lv.trimmed().toString();
        if (line.isEmpty())
            continue;
        if (line.startsWith(kMarker)) {
            key = line.mid(kMarker.size());
            want_header = true;
            continue;
        }
        if (want_header) { // first line after a marker is that file's header
            idx = csv::header_index(line);
            want_header = false;
            continue;
        }
        const QStringList row = csv::split_csv_line(line);

        if (key == "INDICES") {
            // lowercase 3-col: exch, symbol, token
            const QString raw_exch = csv::field(row, idx, "exch");
            const QString raw_sym = csv::field(row, idx, "symbol");
            const QString token = csv::field(row, idx, "token");
            if (raw_sym.isEmpty())
                continue;
            Instrument inst;
            inst.broker_id = "aliceblue";
            inst.instrument_token = token.toLongLong();
            if (inst.instrument_token == 0)
                inst.instrument_token = norm::stable_token(raw_exch + raw_sym);
            inst.exchange_token = inst.instrument_token;
            inst.instrument_type = InstrumentType::INDEX;
            inst.name = norm::normalise_index_symbol(raw_sym);
            inst.symbol = inst.name;
            inst.brsymbol = raw_sym;
            inst.brexchange = raw_exch.trimmed().toUpper();
            inst.exchange = norm::normalise_exchange(raw_exch, InstrumentType::INDEX);
            inst.lot_size = 1;
            inst.tick_size = 0.05;
            out.append(inst);
            continue;
        }

        const QString exch = csv::field(row, idx, "Exch").trimmed().toUpper(); // already canonical
        const QString token = csv::field(row, idx, "Token");
        if (exch.isEmpty() || token.isEmpty())
            continue;
        const QString symbol_col = csv::field(row, idx, "Symbol");
        const QString br = csv::field(row, idx, "Trading Symbol");
        const QString ot = csv::field(row, idx, "Option Type").trimmed().toUpper();

        const bool is_deriv = (exch == "NFO" || exch == "BFO" || exch == "CDS" || exch == "BCD" || exch == "MCX");
        InstrumentType t;
        if (!is_deriv)
            t = InstrumentType::EQ;
        else if (ot == "CE")
            t = InstrumentType::CE;
        else if (ot == "PE")
            t = InstrumentType::PE;
        else
            t = InstrumentType::FUT; // XX / SF / IF / FUTCUR / FUTCOM / FUTIDX …

        Instrument inst;
        inst.broker_id = "aliceblue";
        inst.instrument_token = token.toLongLong();
        inst.exchange_token = inst.instrument_token;
        inst.brsymbol = br.isEmpty() ? symbol_col : br;
        inst.name = symbol_col.toUpper(); // underlying
        inst.brexchange = exch;
        inst.instrument_type = t;
        inst.exchange = norm::normalise_exchange(exch, t);
        inst.strike = csv::field(row, idx, "Strike Price").toDouble();
        inst.lot_size = qMax(1, csv::field(row, idx, "Lot Size").toInt());
        const double tick = csv::field(row, idx, "Tick Size").toDouble();
        inst.tick_size = tick > 0.0 ? tick : 0.05;

        const QString exp_nd = norm::expiry_to_nodash(csv::field(row, idx, "Expiry Date"));
        inst.expiry = norm::nodash_to_display(exp_nd);

        const QString fallback = (t == InstrumentType::EQ) ? inst.brsymbol : inst.name;
        inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, inst.strike, fallback);
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    LOG_INFO("AliceBlueSource", QString("Parsed %1 AliceBlue instruments").arg(out.size()));
    return out;
}

} // namespace

void register_aliceblue_source() {
    SymbolResolver::instance().register_source({QStringLiteral("aliceblue"),
                                                [](const BrokerCredentials& c) { return download_aliceblue(c); },
                                                [](const QByteArray& p) { return parse_aliceblue(p); }});
}

} // namespace fincept::trading
