#include "algo_engine/fno/FnoStrategyPreview.h"

#include "algo_engine/fno/FnoLegResolver.h"

namespace fincept::algo::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using fincept::services::options::Strategy;
using fincept::services::options::StrategyLeg;
using fincept::trading::InstrumentType;

Strategy build_preview_strategy(const QVector<AlgoFnoLeg>& legs, const OptionChain& chain) {
    Strategy s;
    s.name = chain.underlying.isEmpty() ? QStringLiteral("Preview") : chain.underlying;
    s.underlying = chain.underlying;
    s.expiry = chain.expiry;

    const QVector<ResolvedLeg> resolved = FnoLegResolver::resolve(legs, chain);
    for (const auto& rl : resolved) {
        if (!rl.valid)
            continue; // skip FUT/unresolved legs in the preview
        const bool is_call = (rl.kind == QLatin1String("CE"));

        const OptionChainRow* row = nullptr;
        for (const auto& r : chain.rows) {
            if ((is_call ? r.ce_token : r.pe_token) == rl.token) {
                row = &r;
                break;
            }
        }
        if (!row)
            continue;

        StrategyLeg sl;
        sl.instrument_token = rl.token;
        sl.symbol = rl.symbol;
        sl.type = is_call ? InstrumentType::CE : InstrumentType::PE;
        sl.strike = rl.strike;
        sl.expiry = chain.expiry;
        sl.lot_size = row->lot_size;
        sl.lots = (rl.side == QLatin1String("SELL")) ? -rl.lots : rl.lots;
        sl.entry_price = is_call ? row->ce_quote.ltp : row->pe_quote.ltp;
        sl.iv_at_entry = is_call ? row->ce_iv : row->pe_iv;
        sl.is_active = true;
        s.legs.append(sl);
    }
    return s;
}

} // namespace fincept::algo::fno
