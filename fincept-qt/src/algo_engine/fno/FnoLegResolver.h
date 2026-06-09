#pragma once

#include "algo_engine/fno/FnoAlgoTypes.h"
#include "services/options/OptionChainTypes.h"

#include <QString>
#include <QVector>

namespace fincept::algo::fno {

// A leg rule resolved to a concrete contract against an OptionChain snapshot.
struct ResolvedLeg {
    QString kind;        // "CE" | "PE"
    QString side;        // "BUY" | "SELL"
    int     lots = 1;
    double  strike = 0;
    qint64  token = 0;
    QString symbol;      // broker-native symbol taken from the chain row
    bool    valid = false;
    QString error;       // reason resolution failed (when !valid)
};

// Pure resolver: leg rules + an OptionChain snapshot -> concrete contracts.
// No I/O, no globals. FUT legs are out of scope here (they resolve from the
// futures instrument, not the option chain) and return valid=false.
class FnoLegResolver {
  public:
    static ResolvedLeg resolve_one(const AlgoFnoLeg& leg,
                                   const fincept::services::options::OptionChain& chain);
    static QVector<ResolvedLeg> resolve(const QVector<AlgoFnoLeg>& legs,
                                        const fincept::services::options::OptionChain& chain);
};

} // namespace fincept::algo::fno
