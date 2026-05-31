#include "trading/instruments/parsers/PaytmInstrumentParser.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/parsers/CsvUtil.h"

#include <QStringList>
#include <QStringView>

namespace fincept::trading {
namespace {

QByteArray download_paytm(const BrokerCredentials&) {
    // Public static CSV — no auth.
    return http_get_blocking("https://developer.paytmmoney.com/data/v1/scrips/security_master.csv", {}, 120000);
}

QVector<Instrument> parse_paytm(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');
    if (lines.isEmpty())
        return out;

    const QHash<QString, int> idx = csv::header_index(lines.first());
    for (int i = 1; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed().toString();
        if (line.isEmpty())
            continue;
        const QStringList row = csv::split_csv_line(line);

        const QString token = csv::field(row, idx, "security_id");
        if (token.isEmpty())
            continue;
        const QString brsym = csv::field(row, idx, "symbol");
        const QString name = csv::field(row, idx, "name");
        const QString raw_exch = csv::field(row, idx, "exchange").toUpper(); // "NSE"/"BSE"
        const QString itf = csv::field(row, idx, "instrument_type").toUpper();

        InstrumentType t;
        QString canon_exch;
        if (itf == "ES" || itf == "ETF") {
            t = InstrumentType::EQ;
            canon_exch = raw_exch;
        } else if (itf == "I") {
            t = InstrumentType::INDEX;
            canon_exch = (raw_exch == "BSE") ? "BSE_INDEX" : "NSE_INDEX";
        } else if (itf == "FUTIDX" || itf == "FUTSTK") {
            t = InstrumentType::FUT;
            canon_exch = (raw_exch == "BSE") ? "BFO" : "NFO";
        } else if (itf == "OPTIDX" || itf == "OPTSTK") {
            canon_exch = (raw_exch == "BSE") ? "BFO" : "NFO";
            const QString name_up = name.toUpper();
            if (name_up.contains("PUT"))
                t = InstrumentType::PE;
            else
                t = InstrumentType::CE; // CALL or default
        } else {
            continue;
        }

        const QString base = name.split(' ').value(0).toUpper();

        Instrument inst;
        inst.broker_id = "paytm";
        inst.broker_token = token;
        inst.instrument_token = token.toLongLong();
        inst.exchange_token = inst.instrument_token;
        inst.brsymbol = brsym;
        inst.name = base;
        inst.brexchange = raw_exch;
        inst.instrument_type = t;
        inst.exchange = norm::normalise_exchange(canon_exch, t);
        inst.strike = csv::field(row, idx, "strike_price").toDouble();
        inst.lot_size = qMax(1, csv::field(row, idx, "lot_size").toInt());
        const double tick = csv::field(row, idx, "tick_size").toDouble();
        inst.tick_size = tick > 0.0 ? tick : 0.05;

        const QString exp_nd = norm::expiry_to_nodash(csv::field(row, idx, "expiry_date")); // "YYYY-MM-DD HH:MM:SS"
        inst.expiry = norm::nodash_to_display(exp_nd);

        QString fallback;
        if (t == InstrumentType::EQ)
            fallback = brsym;
        else if (t == InstrumentType::INDEX)
            fallback = name;
        else
            fallback = base;
        inst.symbol = norm::synthesize_symbol(base, t, exp_nd, inst.strike, fallback);
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    LOG_INFO("PaytmSource", QString("Parsed %1 Paytm instruments").arg(out.size()));
    return out;
}

} // namespace

void register_paytm_source() {
    SymbolResolver::instance().register_source({QStringLiteral("paytm"),
                                                [](const BrokerCredentials& c) { return download_paytm(c); },
                                                [](const QByteArray& p) { return parse_paytm(p); }});
}

} // namespace fincept::trading
