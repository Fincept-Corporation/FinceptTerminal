#pragma once
// ArenaPrompts — deterministic system/user prompt builders (spec §3.5).
#include "services/alpha_arena/ArenaTypes.h"
#include "services/alpha_arena/PaperExchange.h"
namespace fincept::arena {
/// JSON output contract appended to EVERY system prompt (also the override).
QString decision_contract();
QString system_prompt(const CompetitionRow& comp);
QString user_prompt(const MarketSnapshot& market, const AccountView& acct,
                    const QVector<DecisionRow>& recent, int round_seq, qint64 started_at);
} // namespace fincept::arena
