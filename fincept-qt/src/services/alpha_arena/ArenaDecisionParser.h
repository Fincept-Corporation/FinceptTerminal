#pragma once
// Tolerant parser for the arena decision contract (spec §5). Pure.
#include "services/alpha_arena/ArenaTypes.h"
namespace fincept::arena {
struct ParsedDecision {
    QVector<AgentAction> actions;
    QString error;                       // empty = ok
    bool ok() const { return error.isEmpty(); }
    QString actions_json() const;        // canonical re-serialization for storage
};
/// instruments + max_leverage gate validation; never throws.
ParsedDecision parse_decision(const QString& raw, const QStringList& instruments, double max_leverage);
} // namespace fincept::arena
