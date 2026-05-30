#include "trading/instruments/parsers/FivePaisaInstrumentParser.h"

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

QByteArray download_fivepaisa(const BrokerCredentials&) {
    // Public ScripMaster CSV (all segments) — no auth.
    return http_get_blocking("https://openapi.5paisa.com/VendorsAPI/Service1.svc/ScripMaster/segment/all", {},
                             120000);
}

// Map the 5paisa (Exch,ExchType) tuple to a canonical exchange. Returns "" if
// the tuple is unmapped (caller skips the row).
QString canon_exchange(const QString& exch, const QString& exch_type) {
    const QString e = exch.trimmed().toUpper();
    const QString t = exch_type.trimmed().toUpper();
    if (e == "N" && t == "C")
        return "NSE";
    if (e == "B" && t == "C")
        return "BSE";
    if (e == "N" && t == "D")
        return "NFO";
    if (e == "B" && t == "D")
        return "BFO";
    if (e == "N" && t == "U")
        return "CDS";
    if (e == "B" && t == "U")
        return "BCD";
    if (e == "M" && t == "D")
        return "MCX";
    return {};
}

QVector<Instrument> parse_fivepaisa(const QByteArray& payload) {
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
        // FullName may contain commas — always use the quoting-aware splitter.
        const QStringList row = csv::split_csv_line(line);

        const QString token = csv::field(row, idx, "ScripCode");
        if (token.isEmpty() || token.toLongLong() == 0)
            continue;

        const QString rawExch = csv::field(row, idx, "Exch");
        const QString rawExchType = csv::field(row, idx, "ExchType");
        QString canonExch = canon_exchange(rawExch, rawExchType);
        if (canonExch.isEmpty())
            continue;

        const QString brsym = csv::field(row, idx, "Name").toUpper();
        const QString fullName = csv::field(row, idx, "FullName");
        QString symbolRoot = csv::field(row, idx, "SymbolRoot");
        QString underlyingName = symbolRoot.isEmpty() ? (fullName.isEmpty() ? brsym : fullName) : symbolRoot;

        // INDEX override: pseudo-scrip codes above 999900 are indices.
        const qint64 scripCode = token.toLongLong();
        const QString eUp = rawExch.trimmed().toUpper();
        const QString tUp = rawExchType.trimmed().toUpper();
        if (eUp == "N" && tUp == "C" && scripCode > 999900)
            canonExch = "NSE_INDEX";
        else if (eUp == "B" && tUp == "C" && scripCode > 999900)
            canonExch = "BSE_INDEX";

        const bool is_index = (canonExch == "NSE_INDEX" || canonExch == "BSE_INDEX");
        const bool is_deriv =
            (canonExch == "NFO" || canonExch == "BFO" || canonExch == "CDS" || canonExch == "BCD" ||
             canonExch == "MCX");

        InstrumentType t;
        if (is_index)
            t = InstrumentType::INDEX;
        else if (is_deriv) {
            if (brsym.endsWith("CE"))
                t = InstrumentType::CE;
            else if (brsym.endsWith("PE"))
                t = InstrumentType::PE;
            else
                t = InstrumentType::FUT;
        } else
            t = InstrumentType::EQ;

        Instrument inst;
        inst.broker_id = "fivepaisa";
        inst.instrument_token = token.toLongLong();
        inst.exchange_token = inst.instrument_token;
        inst.brsymbol = brsym;
        inst.name = underlyingName.toUpper();
        inst.brexchange = canonExch;
        inst.instrument_type = t;
        inst.exchange = norm::normalise_exchange(canonExch, t);
        inst.strike = csv::field(row, idx, "StrikeRate").toDouble();
        inst.lot_size = qMax(1, csv::field(row, idx, "LotSize").toInt());
        const double tick = csv::field(row, idx, "TickSize").toDouble();
        inst.tick_size = tick > 0.0 ? tick : 0.05;

        const QString exp_nd = norm::expiry_to_nodash(csv::field(row, idx, "Expiry"));
        inst.expiry = norm::nodash_to_display(exp_nd);

        const QString fallback = (t == InstrumentType::EQ) ? brsym : underlyingName;
        inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, inst.strike, fallback);
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    LOG_INFO("FivePaisaSource", QString("Parsed %1 FivePaisa instruments").arg(out.size()));
    return out;
}

} // namespace

void register_fivepaisa_source() {
    SymbolResolver::instance().register_source({QStringLiteral("fivepaisa"),
                                                [](const BrokerCredentials& c) { return download_fivepaisa(c); },
                                                [](const QByteArray& p) { return parse_fivepaisa(p); }});
}

} // namespace fincept::trading
