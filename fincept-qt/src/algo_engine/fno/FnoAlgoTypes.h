#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::algo::fno {

// One leg of an F&O algo strategy, authored as a RULE (not a fixed contract).
// Concrete strikes/expiries are resolved at entry time by FnoLegResolver.
struct AlgoFnoLeg {
    QString kind;          // "CE" | "PE" | "FUT"
    QString side = "BUY";  // "BUY" | "SELL"
    int     lots = 1;

    // Strike rule. strike_value meaning depends on strike_mode:
    //   ATM         → ignored
    //   ATM_OFFSET  → number of strike steps from ATM (+ = higher strike)
    //   DELTA       → target absolute delta (0..1)
    //   ABSOLUTE    → exact strike price (pinned)
    QString strike_mode = "ATM";  // "ATM"|"ATM_OFFSET"|"DELTA"|"ABSOLUTE"
    double  strike_value = 0;

    // Expiry rule. expiry_value meaning depends on expiry_mode:
    //   WEEKLY/MONTHLY → ignored
    //   NEAREST_DTE    → minimum days-to-expiry (string integer)
    //   ABSOLUTE       → "DD-MMM-YY"
    QString expiry_mode = "WEEKLY";  // "WEEKLY"|"MONTHLY"|"NEAREST_DTE"|"ABSOLUTE"
    QString expiry_value;
};

// JSON (de)serialization — mirrors how AlgoStrategy stores entry/exit conditions
// as a QJsonArray TEXT column. Pure, no I/O.
QJsonArray fno_legs_to_json(const QVector<AlgoFnoLeg>& legs);
QVector<AlgoFnoLeg> fno_legs_from_json(const QJsonArray& arr);

} // namespace fincept::algo::fno
