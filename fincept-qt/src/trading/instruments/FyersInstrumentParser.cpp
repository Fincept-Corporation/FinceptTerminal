#include "trading/instruments/FyersInstrumentParser.h"

#include "trading/instruments/InstrumentNormalize.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::trading {

static InstrumentType inst_type_from_fyers(int exInstType, const QString& optType) {
    if (optType == "CE") return InstrumentType::CE;
    if (optType == "PE") return InstrumentType::PE;
    if (exInstType == 11) return InstrumentType::FUT;
    if (exInstType == 0)  return InstrumentType::EQ;
    return InstrumentType::UNKNOWN;
}

static QString normalise_exchange(int segment, const QString& exchangeName) {
    // segment: 10=CM, 11=FO, 12=CD, 20=BSE_CM, 21=BSE_FO, 30=MCX_COM
    if (segment == 11) return QStringLiteral("NFO");
    if (segment == 12) return QStringLiteral("CDS");
    if (segment == 21) return QStringLiteral("BFO");
    if (segment == 30) return QStringLiteral("MCX");
    if (exchangeName == "BSE") return QStringLiteral("BSE");
    return QStringLiteral("NSE");
}

static QString format_expiry(qint64 unix_ts) {
    if (unix_ts <= 0) return {};
    // IST = UTC+5:30. Offset directly instead of QTimeZone("Asia/Kolkata")
    // which can silently fail on Windows when the IANA tz database is missing.
    constexpr int kIstOffsetSecs = 5 * 3600 + 30 * 60;
    QDateTime dt = QDateTime::fromSecsSinceEpoch(unix_ts + kIstOffsetSecs, QTimeZone::UTC);
    return dt.toString("dd-MMM-yy").toUpper(); // "26-MAY-26"
}

QVector<Instrument> FyersInstrumentParser::parse(const QByteArray& json_data) {
    QVector<Instrument> result;

    auto doc = QJsonDocument::fromJson(json_data);
    if (!doc.isObject()) {
        LOG_ERROR("FyersParser", "Invalid JSON — expected object at top level");
        return result;
    }

    QJsonObject root = doc.object();
    result.reserve(root.size());

    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.value().isObject()) continue;
        QJsonObject entry = it.value().toObject();

        Instrument inst;
        inst.broker_id = QStringLiteral("fyers");

        QString fyToken = entry.value("fyToken").toString();
        inst.instrument_token = fyToken.toLongLong();
        inst.exchange_token = entry.value("exToken").toVariant().toLongLong();

        QString symTicker = entry.value("symTicker").toString(); // "NSE:BANKNIFTY26MAYFUT"
        inst.brexchange = entry.value("exchangeName").toString();

        // brsymbol must NOT include exchange prefix — OptionChainService prepends
        // brexchange + ":" when constructing quote symbols. Strip "NSE:" etc.
        int colon = symTicker.indexOf(':');
        inst.brsymbol = (colon >= 0) ? symTicker.mid(colon + 1) : symTicker;

        int segment = entry.value("segment").toInt();
        int exInstType = entry.value("exInstType").toInt();
        QString optType = entry.value("optType").toString();
        double strike = entry.value("strikePrice").toDouble(-1);
        qint64 expiryTs = entry.value("expiryDate").toVariant().toLongLong();

        inst.exchange = normalise_exchange(segment, inst.brexchange);
        inst.instrument_type = inst_type_from_fyers(exInstType, optType);

        QString underSym = entry.value("underSym").toString();
        QString exSymbol = entry.value("exSymbol").toString();
        inst.name = underSym.isEmpty() ? exSymbol : underSym;

        inst.lot_size = entry.value("minLotSize").toInt(1);
        inst.tick_size = entry.value("tickSize").toDouble(0.05);

        if (expiryTs > 0)
            inst.expiry = format_expiry(expiryTs);

        if (strike > 0 && (inst.instrument_type == InstrumentType::CE ||
                           inst.instrument_type == InstrumentType::PE))
            inst.strike = strike;

        // Build normalised symbol
        if (inst.instrument_type == InstrumentType::EQ ||
            inst.instrument_type == InstrumentType::UNKNOWN) {
            inst.symbol = exSymbol;
        } else if (inst.instrument_type == InstrumentType::FUT) {
            // NIFTY + 26MAY26 + FUT → NIFTY26MAY26FUT
            QString exp_compact = inst.expiry;
            exp_compact.remove('-');
            inst.symbol = inst.name + exp_compact + "FUT";
        } else {
            // CE/PE: NIFTY + 26MAY26 + 24000 + CE
            QString exp_compact = inst.expiry;
            exp_compact.remove('-');
            inst.symbol = inst.name + exp_compact + norm::format_strike(inst.strike) +
                          instrument_type_str(inst.instrument_type);
        }

        result.append(std::move(inst));
    }

    int nfo_count = 0, nfo_with_expiry = 0;
    for (const auto& i : result) {
        if (i.exchange == "NFO") {
            ++nfo_count;
            if (!i.expiry.isEmpty()) ++nfo_with_expiry;
        }
    }
    LOG_INFO("FyersParser", QString("Parsed %1 instruments (NFO=%2, with_expiry=%3)")
                                .arg(result.size()).arg(nfo_count).arg(nfo_with_expiry));
    return result;
}

} // namespace fincept::trading
