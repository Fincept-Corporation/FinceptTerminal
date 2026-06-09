#pragma once

#include "algo_engine/fno/FnoAlgoTypes.h"
#include "services/options/OptionChainTypes.h"

#include <QVector>

namespace fincept::algo::fno {

// Resolve rule-based legs against a live chain and convert them to a concrete
// options::Strategy whose StrategyLegs (strike/lots/entry_price/iv/lot_size) feed
// the F&O analytics/payoff components for a live PREVIEW. Pure: no I/O, no globals.
// Unresolved legs (e.g. FUT, or a strike with no contract) are skipped.
fincept::services::options::Strategy
build_preview_strategy(const QVector<AlgoFnoLeg>& legs,
                       const fincept::services::options::OptionChain& chain);

} // namespace fincept::algo::fno
