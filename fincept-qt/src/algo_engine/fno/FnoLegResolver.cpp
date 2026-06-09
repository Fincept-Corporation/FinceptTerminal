#include "algo_engine/fno/FnoLegResolver.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::algo::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;

static int FnoLegResolver_atm_index(const OptionChain& chain) {
    int idx = -1;
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < chain.rows.size(); ++i) {
        const double d = std::abs(chain.rows[i].strike - chain.atm_strike);
        if (d < best) {
            best = d;
            idx = i;
        }
    }
    return idx;
}

static int FnoLegResolver_nearest_strike_index(const OptionChain& chain, double strike) {
    int idx = -1;
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < chain.rows.size(); ++i) {
        const double d = std::abs(chain.rows[i].strike - strike);
        if (d < best) {
            best = d;
            idx = i;
        }
    }
    return idx;
}

ResolvedLeg FnoLegResolver::resolve_one(const AlgoFnoLeg& leg, const OptionChain& chain) {
    ResolvedLeg out;
    out.kind = leg.kind;
    out.side = leg.side;
    out.lots = leg.lots;

    if (chain.rows.isEmpty()) {
        out.error = QStringLiteral("empty chain");
        return out;
    }
    if (leg.kind != QLatin1String("CE") && leg.kind != QLatin1String("PE")) {
        out.error = QStringLiteral("non-option leg not resolvable from chain");
        return out;
    }
    const bool is_call = (leg.kind == QLatin1String("CE"));

    int idx = -1;
    if (leg.strike_mode == QLatin1String("ABSOLUTE")) {
        idx = FnoLegResolver_nearest_strike_index(chain, leg.strike_value);
    } else if (leg.strike_mode == QLatin1String("ATM")) {
        idx = FnoLegResolver_atm_index(chain);
    } else if (leg.strike_mode == QLatin1String("ATM_OFFSET")) {
        const int a = FnoLegResolver_atm_index(chain);
        if (a >= 0)
            idx = std::clamp(a + static_cast<int>(std::lround(leg.strike_value)), 0,
                             static_cast<int>(chain.rows.size()) - 1);
    } else if (leg.strike_mode == QLatin1String("DELTA")) {
        double best = std::numeric_limits<double>::max();
        for (int i = 0; i < chain.rows.size(); ++i) {
            const auto& g = is_call ? chain.rows[i].ce_greeks : chain.rows[i].pe_greeks;
            if (!g.valid)
                continue;
            const double d = std::abs(std::abs(g.delta) - std::abs(leg.strike_value));
            if (d < best) {
                best = d;
                idx = i;
            }
        }
        if (idx < 0) {
            out.error = QStringLiteral("no valid greeks for DELTA resolution");
            return out;
        }
    } else {
        out.error = QStringLiteral("unknown strike_mode: ") + leg.strike_mode;
        return out;
    }

    if (idx < 0 || idx >= chain.rows.size()) {
        out.error = QStringLiteral("strike not found");
        return out;
    }
    const OptionChainRow& r = chain.rows[idx];
    out.strike = r.strike;
    out.token = is_call ? r.ce_token : r.pe_token;
    out.symbol = is_call ? r.ce_symbol : r.pe_symbol;
    out.valid = (out.token != 0 && !out.symbol.isEmpty());
    if (!out.valid)
        out.error = QStringLiteral("no contract for strike/side in chain");
    return out;
}

QVector<ResolvedLeg> FnoLegResolver::resolve(const QVector<AlgoFnoLeg>& legs, const OptionChain& chain) {
    QVector<ResolvedLeg> out;
    out.reserve(legs.size());
    for (const auto& l : legs)
        out.append(resolve_one(l, chain));
    return out;
}

} // namespace fincept::algo::fno
