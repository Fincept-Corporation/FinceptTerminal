// src/algo_engine/ConditionEvaluator.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/IndicatorEngine.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QJsonArray>
#include <QJsonObject>

namespace fincept::algo {

/// Evaluates a strategy's entry/exit logic against a window of candles whose
/// LAST element is the current bar.
///
/// The condition format is a backward-compatible superset of the original flat
/// schema: `evaluate_group` receives an array of nodes joined by `logic`
/// (AND/OR). Each node is either
///   • a comparison leaf  — `{indicator, params, field, offset, operator,
///                            compare_mode, value | compare_indicator …}`, or
///   • a nested group     — `{"children": [...], "logic": "AND"|"OR",
///                            "negate": bool}`
/// so `(A AND B) OR C` is expressible. Legacy strategies (all-leaf arrays) keep
/// evaluating unchanged — no migration required.
///
/// Every operand carries an integer `offset` (bars ago, 0 = current); the value
/// N bars back is obtained by recomputing the indicator on the window truncated
/// by N bars. `crosses_*` / `rising` / `falling` read the operand one extra bar
/// back.
class ConditionEvaluator {
public:
    static ConditionResult evaluate_single(
        const fincept::services::algo::ConditionDef& condition,
        const QVector<OhlcvCandle>& candles);

    static GroupEvalResult evaluate_group(
        const QJsonArray& children,
        const QString& logic,
        const QVector<OhlcvCandle>& candles);

private:
    static fincept::services::algo::ConditionDef parse_condition(const QJsonObject& obj);
    static bool is_group_node(const QJsonObject& node);
    static bool apply_comparison(double lhs, const QString& op, double rhs);
    static bool apply_crossing(double curr, double prev,
                               double target_curr, double target_prev,
                               const QString& op);
    /// Resolves an indicator operand `offset` bars back. Returns NaN and sets
    /// `*error` (when non-null) on failure / insufficient data.
    static double operand_value(const QString& indicator, const QJsonObject& params,
                                const QString& field, int offset,
                                const QVector<OhlcvCandle>& candles, QString* error);
};

} // namespace fincept::algo
