#include "trading/instruments/parsers/XtsInstrumentParser.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QStringView>

namespace fincept::trading {
namespace {

const QString kMd = QStringLiteral("https://ttblaze.iifl.com/apimarketdata");
const QString kSegMarker = QStringLiteral("#SEG:");
const QString kIdxMarker = QStringLiteral("#IDX:");

const QStringList kSegments = {"NSECM", "NSECD", "NSEFO", "BSECM", "BSEFO", "MCXFO"};

QByteArray download_iifl(const BrokerCredentials& creds) {
    // access_token packed: trade_token:::feed_token
    const QStringList parts = creds.access_token.split(":::");
    const QString feed = parts.size() > 1 ? parts[1] : QString();
    if (feed.isEmpty()) {
        LOG_WARN("XtsSource", "IIFL feed token unavailable — cannot fetch master");
        return {};
    }
    const QMap<QString, QString> headers = {{"authorization", feed}, {"Accept", "application/json"}};

    QByteArray combined;
    for (const QString& seg : kSegments) {
        QJsonObject body;
        body["exchangeSegmentList"] = QJsonArray{seg};
        const QByteArray resp = http_post_blocking(kMd + "/instruments/master",
                                                   QJsonDocument(body).toJson(QJsonDocument::Compact),
                                                   "application/json", headers, 60000);
        if (resp.isEmpty())
            continue;
        const QJsonObject o = QJsonDocument::fromJson(resp).object();
        if (o.value("type").toString() != "success")
            continue;
        const QString result = o.value("result").toString();
        if (result.isEmpty())
            continue;
        combined += (kSegMarker + seg + "\n").toUtf8();
        combined += result.toUtf8();
        combined += '\n';
    }

    // Index lists (NSE=1, BSE=11).
    for (const QString& xseg : {QStringLiteral("1"), QStringLiteral("11")}) {
        const QByteArray resp =
            http_get_blocking(kMd + "/instruments/indexlist?exchangeSegment=" + xseg, headers, 30000);
        if (resp.isEmpty())
            continue;
        const QJsonArray list =
            QJsonDocument::fromJson(resp).object().value("result").toObject().value("indexList").toArray();
        if (list.isEmpty())
            continue;
        combined += (kIdxMarker + xseg + "\n").toUtf8();
        for (const QJsonValue& v : list) {
            combined += v.toString().toUtf8();
            combined += '\n';
        }
    }
    return combined;
}

QString seg_exchange(const QString& seg) {
    if (seg == "NSECM")
        return "NSE";
    if (seg == "BSECM")
        return "BSE";
    if (seg == "NSEFO")
        return "NFO";
    if (seg == "NSECD")
        return "CDS";
    if (seg == "BSEFO")
        return "BFO";
    if (seg == "MCXFO")
        return "MCX";
    return {};
}

QVector<Instrument> parse_iifl(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');

    QString seg;       // current market-data segment
    QString idx_seg;   // current index-list segment ("1"/"11")
    for (const QStringView lv : lines) {
        const QString line = lv.trimmed().toString();
        if (line.isEmpty())
            continue;
        if (line.startsWith(kSegMarker)) {
            seg = line.mid(kSegMarker.size());
            idx_seg.clear();
            continue;
        }
        if (line.startsWith(kIdxMarker)) {
            idx_seg = line.mid(kIdxMarker.size());
            seg.clear();
            continue;
        }

        if (!idx_seg.isEmpty()) {
            // Index list line: "NIFTY 50_26000" → name + token (split on last '_').
            const int us = line.lastIndexOf('_');
            if (us <= 0)
                continue;
            const QString name = line.left(us);
            const QString token = line.mid(us + 1);
            Instrument inst;
            inst.broker_id = "iifl";
            inst.instrument_token = token.toLongLong();
            if (inst.instrument_token == 0)
                inst.instrument_token = norm::stable_token(name);
            inst.exchange_token = inst.instrument_token;
            inst.instrument_type = InstrumentType::INDEX;
            inst.name = norm::normalise_index_symbol(name);
            inst.symbol = inst.name;
            inst.brsymbol = name;
            inst.brexchange = (idx_seg == "11") ? "BSE" : "NSE";
            inst.exchange = (idx_seg == "11") ? "BSE_INDEX" : "NSE_INDEX";
            inst.lot_size = 1;
            inst.tick_size = 0.05;
            out.append(inst);
            continue;
        }

        if (seg.isEmpty())
            continue;
        const QStringList c = line.split('|');
        if (c.size() < 13)
            continue;

        const QString exch = seg_exchange(seg);
        const bool is_fo = (seg == "NSEFO" || seg == "NSECD" || seg == "BSEFO" || seg == "MCXFO");

        Instrument inst;
        inst.broker_id = "iifl";
        inst.instrument_token = c[1].toLongLong();
        inst.exchange_token = inst.instrument_token;
        inst.name = c[3].trimmed().toUpper();
        inst.lot_size = qMax(1, c[12].toInt());
        const double tick = c[11].toDouble();
        inst.tick_size = tick > 0.0 ? tick : 0.05;
        inst.brexchange = seg;

        if (!is_fo) {
            const QString series = c.value(5).trimmed().toUpper();
            if (series == "EQ" || series == "BE") {
                inst.instrument_type = InstrumentType::EQ;
                inst.brsymbol = c.value(14, c[3]);
                inst.exchange = exch;
                inst.symbol = norm::strip_eq_suffix(inst.name);
            } else if (series == "SPOT") {
                inst.instrument_type = InstrumentType::INDEX;
                inst.brsymbol = c.value(14, c[3]);
                inst.exchange = norm::normalise_exchange(exch, InstrumentType::INDEX);
                inst.symbol = norm::normalise_index_symbol(inst.name);
            } else {
                continue;
            }
            if (inst.symbol.isEmpty())
                continue;
            out.append(inst);
            continue;
        }

        // F&O: need the extended columns.
        if (c.size() < 20)
            continue;
        const QString expiry_raw = c[16];
        if (expiry_raw == "1") // MCXFO junk/spot rows
            continue;
        const double strike = c[17].toDouble();
        const QString opt = c[18].trimmed();
        InstrumentType t;
        if (opt == "3")
            t = InstrumentType::CE;
        else if (opt == "4")
            t = InstrumentType::PE;
        else if (opt == "1")
            t = InstrumentType::FUT;
        else { // CDS inconsistency → infer from description suffix
            const QString d = c[4].trimmed().toUpper();
            t = d.endsWith("CE") ? InstrumentType::CE : d.endsWith("PE") ? InstrumentType::PE : InstrumentType::FUT;
        }

        inst.instrument_type = t;
        inst.strike = (t == InstrumentType::FUT) ? 0.0 : strike;
        inst.brsymbol = c.value(19, c[4]); // DisplayName, fallback Description
        const QString exp_nd = norm::expiry_to_nodash(expiry_raw);
        inst.expiry = norm::nodash_to_display(exp_nd);
        inst.exchange = norm::normalise_exchange(exch, t);
        inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, inst.strike, inst.name);
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    LOG_INFO("XtsSource", QString("Parsed %1 IIFL instruments").arg(out.size()));
    return out;
}

} // namespace

void register_iifl_source() {
    SymbolResolver::instance().register_source({QStringLiteral("iifl"),
                                                [](const BrokerCredentials& c) { return download_iifl(c); },
                                                [](const QByteArray& p) { return parse_iifl(p); }});
}

} // namespace fincept::trading
