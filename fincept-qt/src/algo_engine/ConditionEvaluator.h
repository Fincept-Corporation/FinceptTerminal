// src/algo_engine/ConditionEvaluator.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/IndicatorEngine.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QJsonArray>

namespace fincept::algo {

class ConditionEvaluator {
public:
    static ConditionResult evaluate_single(
        const fincept::services::algo::ConditionDef& condition,
        const QVector<OhlcvCandle>& candles);

    static GroupEvalResult evaluate_group(
        const QJsonArray& conditions_json,
        const QString& logic,
        const QVector<OhlcvCandle>& candles);

private:
    static fincept::services::algo::ConditionDef parse_condition(const QJsonObject& obj);
    static bool apply_comparison(double lhs, const QString& op, double rhs);
    static bool apply_crossing(double curr, double prev,
                               double target_curr, double target_prev,
                               const QString& op);
    static bool apply_trend(const QVector<double>& values, int count, const QString& op);
};

} // namespace fincept::algo
