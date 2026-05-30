#include "trading/instruments/parsers/SamcoInstrumentParser.h"

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

QByteArray download_samco(const BrokerCredentials&) {
    // Public static CSV — no auth.
    return http_get_blocking("https://developers.stocknote.com/doc/ScripMaster.csv");
}

// Samco MFO is the MCX F&O exchange code → canonical MCX. Others pass through.
QString canon_exchange(const QString& raw) {
    const QString e = raw.trimmed().toUpper();
    if (e == "MFO")
        return "MCX";
    return e;
}

QVector<Instrument> parse_samco(const QByteArray& payload) {
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

        const QString token = csv::field(row, idx, "symbolCode"); // "758960_NSE"
        if (token.isEmpty())
            continue;
        const QString raw_exch = csv::field(row, idx, "exchange");
        const QString exch = canon_exchange(raw_exch);
        const QString brsym = csv::field(row, idx, "tradingSymbol");
        const QString name = csv::field(row, idx, "name");

        const bool is_deriv = (exch == "NFO" || exch == "BFO" || exch == "CDS" || exch == "MCX");
        const QString bsym_up = brsym.toUpper();

        InstrumentType t;
        if (!is_deriv)
            t = InstrumentType::EQ;
        else if (bsym_up.endsWith("CE"))
            t = InstrumentType::CE;
        else if (bsym_up.endsWith("PE"))
            t = InstrumentType::PE;
        else
            t = InstrumentType::FUT;

        Instrument inst;
        inst.broker_id = "samco";
        inst.broker_token = token;
        inst.instrument_token = norm::stable_token(token);
        inst.exchange_token = inst.instrument_token;
        inst.brsymbol = brsym;
        inst.name = name.toUpper();
        inst.brexchange = raw_exch.trimmed().toUpper();
        inst.instrument_type = t;
        inst.exchange = norm::normalise_exchange(exch, t);
        inst.strike = csv::field(row, idx, "strikePrice").toDouble();
        inst.lot_size = qMax(1, csv::field(row, idx, "lotSize").toInt());
        const double tick = csv::field(row, idx, "tickSize").toDouble();
        inst.tick_size = tick > 0.0 ? tick : 0.05;

        const QString exp_nd = norm::expiry_to_nodash(csv::field(row, idx, "expiryDate")); // DD/MM/YY
        inst.expiry = norm::nodash_to_display(exp_nd);

        const QString fallback = (t == InstrumentType::EQ) ? brsym : name;
        inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, inst.strike, fallback);
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    // Indices are not in the CSV — inject the tradeable ones. brsymbol is the
    // exact name Samco's indexQuote API expects; symbol is canonical.
    struct Idx {
        const char* sym;
        const char* name;
        const char* exch;
    };
    static const Idx kIdx[] = {
        {"NIFTY", "NIFTY 50", "NSE"},      {"BANKNIFTY", "NIFTY BANK", "NSE"},
        {"FINNIFTY", "NIFTY FIN SERVICE", "NSE"}, {"MIDCPNIFTY", "NIFTY MID SELECT", "NSE"},
        {"SENSEX", "SENSEX", "BSE"},       {"BANKEX", "BANKEX", "BSE"},
    };
    for (const auto& ix : kIdx) {
        Instrument inst;
        inst.broker_id = "samco";
        inst.broker_token = QString("%1_INDEX").arg(ix.sym);
        inst.instrument_token = norm::stable_token(inst.broker_token);
        inst.exchange_token = inst.instrument_token;
        inst.symbol = ix.sym;
        inst.brsymbol = ix.name;
        inst.name = ix.sym;
        inst.brexchange = ix.exch;
        inst.exchange = QString("%1_INDEX").arg(ix.exch);
        inst.instrument_type = InstrumentType::INDEX;
        inst.lot_size = 1;
        inst.tick_size = 0.05;
        out.append(inst);
    }

    LOG_INFO("SamcoSource", QString("Parsed %1 Samco instruments").arg(out.size()));
    return out;
}

} // namespace

void register_samco_source() {
    SymbolResolver::instance().register_source({QStringLiteral("samco"),
                                                [](const BrokerCredentials& c) { return download_samco(c); },
                                                [](const QByteArray& p) { return parse_samco(p); }});
}

} // namespace fincept::trading
