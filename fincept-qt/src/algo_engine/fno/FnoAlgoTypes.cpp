#include "algo_engine/fno/FnoAlgoTypes.h"

namespace fincept::algo::fno {

QJsonArray fno_legs_to_json(const QVector<AlgoFnoLeg>& legs) {
    QJsonArray arr;
    for (const auto& l : legs) {
        QJsonObject o;
        o["kind"]         = l.kind;
        o["side"]         = l.side;
        o["lots"]         = l.lots;
        o["strike_mode"]  = l.strike_mode;
        o["strike_value"] = l.strike_value;
        o["expiry_mode"]  = l.expiry_mode;
        o["expiry_value"] = l.expiry_value;
        arr.append(o);
    }
    return arr;
}

QVector<AlgoFnoLeg> fno_legs_from_json(const QJsonArray& arr) {
    QVector<AlgoFnoLeg> legs;
    for (const auto& v : arr) {
        if (!v.isObject())
            continue; // skip malformed elements rather than injecting an all-default leg
        const QJsonObject o = v.toObject();
        AlgoFnoLeg l;
        l.kind         = o.value("kind").toString();
        l.side         = o.value("side").toString(QStringLiteral("BUY"));
        l.lots         = o.value("lots").toInt(1);
        l.strike_mode  = o.value("strike_mode").toString(QStringLiteral("ATM"));
        l.strike_value = o.value("strike_value").toDouble(0);
        l.expiry_mode  = o.value("expiry_mode").toString(QStringLiteral("WEEKLY"));
        l.expiry_value = o.value("expiry_value").toString();
        legs.append(l);
    }
    return legs;
}

} // namespace fincept::algo::fno
