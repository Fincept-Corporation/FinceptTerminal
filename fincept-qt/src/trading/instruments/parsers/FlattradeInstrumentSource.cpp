#include "trading/instruments/parsers/FlattradeInstrumentSource.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/parsers/NorenSymbolParser.h"

#include <QStringList>

namespace fincept::trading {
namespace {

const QStringList kFiles = {
    "NSE_Equity.csv",          "Nfo_Equity_Derivatives.csv", "Nfo_Index_Derivatives.csv",
    "Currency_Derivatives.csv", "Commodity.csv",             "BSE_Equity.csv",
    "Bfo_Index_Derivatives.csv", "Bfo_Equity_Derivatives.csv",
};

QByteArray download_flattrade(const BrokerCredentials&) {
    QByteArray combined;
    for (const QString& f : kFiles) {
        const QString url = QString("https://flattrade.s3.ap-south-1.amazonaws.com/scripmaster/%1").arg(f);
        const QByteArray csv = http_get_blocking(url);
        if (csv.isEmpty())
            continue;
        combined += csv;
        if (!csv.endsWith('\n'))
            combined += '\n';
    }
    return combined;
}

QVector<Instrument> parse_flattrade(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    // Uniform 9-col layout across all files:
    // Exchange, Token, Lotsize, Symbol, Tradingsymbol, Instrument, Expiry, Strike, Optiontype
    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');
    for (const QStringView lv : lines) {
        const QString line = lv.trimmed().toString();
        if (line.isEmpty())
            continue;
        const QStringList c = line.split(',');
        if (c.size() < 9)
            continue;
        if (c[0].compare("Exchange", Qt::CaseInsensitive) == 0) // per-file header row
            continue;
        if (c[1].toLongLong() == 0)
            continue;

        NorenRow r;
        r.brexchange = c[0].trimmed().toUpper();
        r.token = c[1];
        r.lot_size = c[2].toInt();
        r.name = c[3];
        r.brsymbol = c[4];
        r.raw_instrument = c[5];
        r.expiry_raw = c[6];
        r.strike = c[7].toDouble();
        r.option_type = c[8];
        r.tick_size = (r.brexchange == "CDS") ? 0.0025 : 0.05; // no tick column in master
        out.append(noren_build(QStringLiteral("flattrade"), r));
    }

    LOG_INFO("FlattradeSource", QString("Parsed %1 Flattrade instruments").arg(out.size()));
    return out;
}

} // namespace

void register_flattrade_source() {
    SymbolResolver::instance().register_source({QStringLiteral("flattrade"),
                                                [](const BrokerCredentials& c) { return download_flattrade(c); },
                                                [](const QByteArray& p) { return parse_flattrade(p); }});
}

} // namespace fincept::trading
