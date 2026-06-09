#include "trading/instruments/parsers/UpstoxInstrumentParser.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDecompress.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::trading {
namespace {

QByteArray download_upstox(const BrokerCredentials&) {
    const QByteArray gz =
        http_get_blocking("https://assets.upstox.com/market-quote/instruments/exchange/complete.json.gz");
    if (gz.isEmpty())
        return {};
    return decompress::gunzip(gz);
}

// Upstox segment → canonical exchange. Empty result = drop the row (e.g. NSE_COM).
QString seg_to_exchange(const QString& seg) {
    static const QHash<QString, QString> m = {
        {"NSE_EQ", "NSE"},   {"NSE_FO", "NFO"},        {"NCD_FO", "CDS"},
        {"NSE_INDEX", "NSE_INDEX"}, {"BSE_INDEX", "BSE_INDEX"}, {"BSE_EQ", "BSE"},
        {"BSE_FO", "BFO"},   {"BCD_FO", "BCD"},        {"MCX_FO", "MCX"},
    };
    return m.value(seg.trimmed().toUpper(), QString());
}

QVector<Instrument> parse_upstox(const QByteArray& json) {
    QVector<Instrument> out;
    if (json.isEmpty())
        return out;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_ERROR("UpstoxSource", "Upstox master JSON parse failed: " + err.errorString());
        return out;
    }

    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();

        const QString seg = o.value("segment").toString();
        const QString exch = seg_to_exchange(seg);
        if (exch.isEmpty())
            continue; // unsupported segment (commodities spot etc.)

        const QString key = o.value("instrument_key").toString();
        if (key.isEmpty())
            continue;

        const QString it_raw = o.value("instrument_type").toString().toUpper();
        InstrumentType t;
        if (seg.endsWith("_INDEX", Qt::CaseInsensitive) || it_raw == "INDEX")
            t = InstrumentType::INDEX;
        else if (it_raw == "FUT")
            t = InstrumentType::FUT;
        else if (it_raw == "CE")
            t = InstrumentType::CE;
        else if (it_raw == "PE")
            t = InstrumentType::PE;
        else
            t = InstrumentType::EQ;

        Instrument inst;
        inst.broker_id = "upstox";
        inst.broker_token = key;
        inst.instrument_token = norm::stable_token(key);
        inst.exchange_token = static_cast<qint64>(o.value("exchange_token").toString().toLongLong());
        inst.brsymbol = o.value("trading_symbol").toString().trimmed();
        inst.name = o.value("name").toString().trimmed().toUpper();
        inst.brexchange = seg.trimmed().toUpper();
        inst.instrument_type = t;
        inst.exchange = norm::normalise_exchange(exch, t);
        inst.strike = o.value("strike_price").toDouble();
        inst.lot_size = qMax(1, o.value("lot_size").toInt());
        const double tick = o.value("tick_size").toDouble();
        inst.tick_size = tick > 0.0 ? tick : 0.05;

        QString exp_nd;
        const qint64 exp_ms = static_cast<qint64>(o.value("expiry").toDouble());
        if (exp_ms > 0) {
            const QDate d = QDateTime::fromMSecsSinceEpoch(exp_ms, QTimeZone::UTC).date();
            exp_nd = norm::expiry_to_nodash(d.toString("yyyy-MM-dd"));
            inst.expiry = norm::nodash_to_display(exp_nd);
        }

        const QString fallback = (t == InstrumentType::EQ) ? inst.brsymbol : inst.name;
        inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, inst.strike, fallback);
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    LOG_INFO("UpstoxSource", QString("Parsed %1 Upstox instruments").arg(out.size()));
    return out;
}

} // namespace

void register_upstox_source() {
    SymbolResolver::instance().register_source({QStringLiteral("upstox"),
                                                [](const BrokerCredentials& c) { return download_upstox(c); },
                                                [](const QByteArray& p) { return parse_upstox(p); }});
}

} // namespace fincept::trading
