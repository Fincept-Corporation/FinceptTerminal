#include "trading/instruments/parsers/TradejiniInstrumentParser.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/parsers/CsvUtil.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QStringView>

namespace fincept::trading {
namespace {

const QString kBase = QStringLiteral("https://api.tradejini.com/v2/api/mkt-data/scrips/symbol-store");
const QString kMarker = QStringLiteral("#GRP:");

// Stable fallback group list (used if the store index fetch fails). "Spot" is
// intentionally excluded.
const QStringList kFallbackGroups = {"Securities",     "FutureContracts", "NSEOptions",       "BSEOptions",
                                     "CurrencyFuture", "CurrencyOptions", "CommodityFuture",  "CommodityOptions",
                                     "Index"};

QByteArray download_tradejini(const BrokerCredentials&) {
    // Step 1: fetch the group list (public; no auth needed for the scrip store).
    QStringList groups;
    const QByteArray index = http_get_blocking(kBase + "?version=0");
    if (!index.isEmpty()) {
        const QJsonObject root = QJsonDocument::fromJson(index).object();
        const QJsonArray store = root.value("d").toObject().value("symbolStore").toArray();
        for (const QJsonValue& g : store) {
            const QString name = g.toObject().value("name").toString();
            if (!name.isEmpty() && name.compare("Spot", Qt::CaseInsensitive) != 0)
                groups << name;
        }
    }
    if (groups.isEmpty())
        groups = kFallbackGroups;

    // Step 2: fetch each group's CSV, combined with markers.
    QByteArray combined;
    for (const QString& g : groups) {
        const QByteArray csv = http_get_blocking(QString("%1/%2?version=0").arg(kBase, g));
        if (csv.isEmpty())
            continue;
        combined += (kMarker + g + "\n").toUtf8();
        combined += csv;
        if (!csv.endsWith('\n'))
            combined += '\n';
    }
    return combined;
}

QVector<Instrument> parse_tradejini(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');

    QString group;
    QHash<QString, int> idx;
    bool want_header = false;
    for (const QStringView lv : lines) {
        const QString line = lv.trimmed().toString();
        if (line.isEmpty())
            continue;
        if (line.startsWith(kMarker)) {
            group = line.mid(kMarker.size());
            want_header = true;
            continue;
        }
        if (want_header) {
            idx = csv::header_index(line);
            want_header = false;
            continue;
        }
        const QStringList row = csv::split_csv_line(line);
        const QString id = csv::field(row, idx, "id");
        if (id.isEmpty() || id.contains("spot", Qt::CaseInsensitive))
            continue;
        const QStringList p = id.split('_');

        // Classify by group.
        InstrumentType t;
        if (group == "Securities")
            t = InstrumentType::EQ;
        else if (group == "Index")
            t = InstrumentType::INDEX;
        else if (group.endsWith("Options"))
            t = InstrumentType::CE; // refined below from id optType
        else if (group.endsWith("Future") || group == "FutureContracts")
            t = InstrumentType::FUT;
        else
            continue;

        QString raw_exch;
        QString base;
        QString expiry_raw;
        double strike = 0.0;
        if (t == InstrumentType::EQ || t == InstrumentType::INDEX) {
            raw_exch = p.isEmpty() ? QString() : p.last(); // …_<EXCH>
            base = csv::field(row, idx, "dispName");
        } else if (t == InstrumentType::FUT) {
            if (p.size() < 4)
                continue;
            base = p[1];
            raw_exch = p[2];
            expiry_raw = p[3];
        } else { // option
            if (p.size() < 6)
                continue;
            base = p[1];
            raw_exch = p[2];
            expiry_raw = p[3];
            strike = p[4].toDouble();
            t = (p[5].toUpper() == "PE") ? InstrumentType::PE : InstrumentType::CE;
        }
        if (raw_exch.isEmpty())
            continue;

        const QString exch_up = raw_exch.toUpper();
        Instrument inst;
        inst.broker_id = "tradejini";
        inst.brsymbol = id; // the order symId
        inst.instrument_token = csv::field(row, idx, "excToken").toLongLong();
        if (inst.instrument_token == 0)
            inst.instrument_token = norm::stable_token(id);
        inst.exchange_token = inst.instrument_token;
        inst.lot_size = qMax(1, csv::field(row, idx, "lot").toInt());
        const double tick = csv::field(row, idx, "tick").toDouble();
        inst.tick_size = tick > 0.0 ? tick : 0.05;
        inst.strike = strike;

        const QString disp = csv::field(row, idx, "dispName");
        const QString desc = csv::field(row, idx, "desc");
        inst.instrument_type = t;

        const QString exp_nd = norm::expiry_to_nodash(expiry_raw);
        inst.expiry = norm::nodash_to_display(exp_nd);

        if (t == InstrumentType::EQ) {
            inst.name = disp.toUpper();
            inst.brexchange = exch_up;
            inst.exchange = norm::normalise_exchange(exch_up, t);
            inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, 0.0, disp);
        } else if (t == InstrumentType::INDEX) {
            inst.name = norm::normalise_index_symbol(disp.isEmpty() ? csv::field(row, idx, "symbol") : disp);
            inst.symbol = inst.name;
            inst.brexchange = exch_up;
            inst.exchange = norm::normalise_exchange(exch_up, t);
        } else {
            inst.name = base.toUpper();
            inst.brexchange = exch_up;
            inst.exchange = norm::normalise_exchange(exch_up, t);
            inst.symbol = norm::synthesize_symbol(inst.name, t, exp_nd, strike, base);
        }
        if (inst.symbol.isEmpty())
            continue;
        out.append(inst);
    }

    LOG_INFO("TradejiniSource", QString("Parsed %1 Tradejini instruments").arg(out.size()));
    return out;
}

} // namespace

void register_tradejini_source() {
    SymbolResolver::instance().register_source({QStringLiteral("tradejini"),
                                                [](const BrokerCredentials& c) { return download_tradejini(c); },
                                                [](const QByteArray& p) { return parse_tradejini(p); }});
}

} // namespace fincept::trading
