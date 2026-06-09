#include "trading/instruments/parsers/KotakInstrumentParser.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/parsers/CsvUtil.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QStringView>

namespace fincept::trading {
namespace {

const QString kMarker = QStringLiteral("#SEG:");

// Map a master CSV filename to its segment key.
QString segment_for_file(const QString& url) {
    const QString f = url.toLower();
    if (f.contains("nse_cm"))
        return "NSE_CM";
    if (f.contains("bse_cm"))
        return "BSE_CM";
    if (f.contains("nse_fo"))
        return "NSE_FO";
    if (f.contains("bse_fo"))
        return "BSE_FO";
    if (f.contains("cde_fo"))
        return "CDE_FO";
    if (f.contains("mcx_fo"))
        return "MCX_FO";
    return {};
}

QByteArray download_kotak(const BrokerCredentials& creds) {
    // access_token packed: trading_token:::trading_sid:::base_url:::portal_token:::server_id
    const QStringList parts = creds.access_token.split(":::");
    if (parts.size() < 4) {
        LOG_WARN("KotakSource", "Kotak access_token not in packed form — cannot fetch master");
        return {};
    }
    const QString base_url = parts[2];
    const QString token = parts[3];
    if (base_url.isEmpty() || token.isEmpty())
        return {};

    const QByteArray paths_resp =
        http_get_blocking(base_url + "/script-details/1.0/masterscrip/file-paths", {{"Authorization", token}}, 30000);
    if (paths_resp.isEmpty())
        return {};
    const QJsonArray files = QJsonDocument::fromJson(paths_resp)
                                 .object()
                                 .value("data")
                                 .toObject()
                                 .value("filesPaths")
                                 .toArray();

    QByteArray combined;
    for (const QJsonValue& fv : files) {
        const QString url = fv.toString();
        const QString seg = segment_for_file(url);
        if (seg.isEmpty())
            continue; // skip nse_com etc.
        const QByteArray csv = http_get_blocking(url, {}, 30000);
        if (csv.isEmpty())
            continue;
        combined += (kMarker + seg + "\n").toUtf8();
        combined += csv;
        if (!csv.endsWith('\n'))
            combined += '\n';
    }
    return combined;
}

QVector<Instrument> parse_kotak(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');

    QString seg;
    QHash<QString, int> idx;
    bool want_header = false;
    for (const QStringView lv : lines) {
        const QString line = lv.trimmed().toString();
        if (line.isEmpty())
            continue;
        if (line.startsWith(kMarker)) {
            seg = line.mid(kMarker.size());
            want_header = true;
            continue;
        }
        if (want_header) {
            // Kotak headers carry stray ';' and spaces (e.g. "dStrikePrice;", "dTickSize ").
            QString clean = line;
            clean.remove(';');
            idx = csv::header_index(clean);
            want_header = false;
            continue;
        }
        const QStringList row = csv::split_csv_line(line);
        const bool is_fo = (seg == "NSE_FO" || seg == "BSE_FO" || seg == "CDE_FO" || seg == "MCX_FO");

        const QString token = csv::field(row, idx, "pSymbol");
        if (token.isEmpty() || token.toLongLong() == 0)
            continue;

        Instrument inst;
        inst.broker_id = "kotak";
        inst.instrument_token = token.toLongLong();
        inst.exchange_token = inst.instrument_token;
        inst.brsymbol = csv::field(row, idx, "pTrdSymbol");
        inst.lot_size = qMax(1, csv::field(row, idx, "lLotSize").toInt());
        const double tick = csv::field(row, idx, "dTickSize").toDouble();
        inst.tick_size = tick > 0.0 ? tick / 100.0 : 0.05;

        if (!is_fo) {
            // Equity / index: NSE_CM, BSE_CM
            const bool is_nse = (seg == "NSE_CM");
            const QString isin = csv::field(row, idx, "pISIN");
            const QString name = csv::field(row, idx, "pSymbolName");
            inst.name = name.toUpper();
            if (isin.trimmed().isEmpty()) { // no ISIN → index
                inst.instrument_type = InstrumentType::INDEX;
                inst.brexchange = is_nse ? "NSE" : "BSE";
                inst.exchange = is_nse ? "NSE_INDEX" : "BSE_INDEX";
                inst.symbol = norm::normalise_index_symbol(name);
            } else {
                inst.instrument_type = InstrumentType::EQ;
                inst.brexchange = is_nse ? "NSE" : "BSE";
                inst.exchange = inst.brexchange;
                inst.symbol = norm::strip_eq_suffix(name);
            }
            if (inst.symbol.isEmpty())
                continue;
            out.append(inst);
            continue;
        }

        // F&O segments.
        const QString opt = csv::field(row, idx, "pOptionType").toUpper();
        InstrumentType t = (opt == "CE") ? InstrumentType::CE
                           : (opt == "PE") ? InstrumentType::PE
                                           : InstrumentType::FUT; // XX
        inst.instrument_type = t;
        inst.name = csv::field(row, idx, "pSymbolName").toUpper();
        inst.strike = csv::field(row, idx, "dStrikePrice").toDouble() / 100.0;
        inst.brexchange = csv::field(row, idx, "pExchSeg");

        QString canon_exch = (seg == "NSE_FO") ? "NFO" : (seg == "BSE_FO") ? "BFO"
                                                     : (seg == "CDE_FO")   ? "CDS"
                                                                           : "MCX";
        inst.exchange = norm::normalise_exchange(canon_exch, t);

        // Expiry: unix seconds; NSE_FO & CDE_FO carry a +315513000 epoch offset.
        qint64 esec = csv::field(row, idx, "lExpiryDate").toLongLong();
        QString exp_nd;
        if (esec > 0) {
            if (seg == "NSE_FO" || seg == "CDE_FO")
                esec += 315513000;
            const QDate d = QDateTime::fromSecsSinceEpoch(esec, QTimeZone::UTC).date();
            exp_nd = norm::expiry_to_nodash(d.toString("yyyy-MM-dd"));
            inst.expiry = norm::nodash_to_display(exp_nd);
        }
        inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, inst.strike, inst.name);
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    LOG_INFO("KotakSource", QString("Parsed %1 Kotak instruments").arg(out.size()));
    return out;
}

} // namespace

void register_kotak_source() {
    SymbolResolver::instance().register_source({QStringLiteral("kotak"),
                                                [](const BrokerCredentials& c) { return download_kotak(c); },
                                                [](const QByteArray& p) { return parse_kotak(p); }});
}

} // namespace fincept::trading
