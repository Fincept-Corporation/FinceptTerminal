// src/algo_engine/ConditionEvaluator.cpp
#include "algo_engine/ConditionEvaluator.h"

#include <QJsonObject>

namespace fincept::algo {

using services::algo::ConditionDef;

ConditionDef ConditionEvaluator::parse_condition(const QJsonObject& obj) {
    ConditionDef c;
    c.indicator = obj.value("indicator").toString();
    c.params = obj.value("params").toObject();
    c.field = obj.value("field").toString("value");
    c.op = obj.value("operator").toString(">");
    c.value = obj.value("value").toDouble(0);
    c.compare_mode = obj.value("compare_mode").toString("value");
    c.compare_indicator = obj.value("compare_indicator").toString();
    c.compare_params = obj.value("compare_params").toObject();
    c.compare_field = obj.value("compare_field").toString("value");
    return c;
}

bool ConditionEvaluator::apply_comparison(double lhs, const QString& op, double rhs) {
    if (op == ">")  return lhs > rhs;
    if (op == "<")  return lhs < rhs;
    if (op == ">=") return lhs >= rhs;
    if (op == "<=") return lhs <= rhs;
    if (op == "==") return std::abs(lhs - rhs) < 1e-8;
    return false;
}

bool ConditionEvaluator::apply_crossing(double curr, double prev,
                                         double target_curr, double target_prev,
                                         const QString& op) {
    if (op == "crosses_above")
        return prev <= target_prev && curr > target_curr;
    if (op == "crosses_below")
        return prev >= target_prev && curr < target_curr;
    return false;
}

bool ConditionEvaluator::apply_trend(const QVector<double>& /*values*/,
                                      int /*count*/, const QString& /*op*/) {
    // Simplified: compare last two values
    // A more sophisticated version would check N consecutive bars
    return false;
}

ConditionResult ConditionEvaluator::evaluate_single(
    const ConditionDef& condition,
    const QVector<OhlcvCandle>& candles) {

    ConditionResult result;
    result.indicator = condition.indicator;
    result.field = condition.field;
    result.op = condition.op;

    auto ind_result = IndicatorEngine::compute(
        condition.indicator, candles, condition.params, condition.field);

    if (!ind_result.valid) {
        result.error = ind_result.error;
        return result;
    }

    double curr_val = ind_result.current.value(condition.field, 0);
    double prev_val = ind_result.previous.value(condition.field, curr_val);
    result.computed_value = curr_val;

    if (condition.compare_mode == "indicator") {
        auto cmp_result = IndicatorEngine::compute(
            condition.compare_indicator, candles,
            condition.compare_params, condition.compare_field);

        if (!cmp_result.valid) {
            result.error = cmp_result.error;
            return result;
        }

        double target_curr = cmp_result.current.value(condition.compare_field, 0);
        double target_prev = cmp_result.previous.value(condition.compare_field, target_curr);
        result.target_value = target_curr;

        if (condition.op == "crosses_above" || condition.op == "crosses_below") {
            result.met = apply_crossing(curr_val, prev_val, target_curr, target_prev, condition.op);
        } else if (condition.op == "rising" || condition.op == "falling") {
            result.met = (condition.op == "rising") ? (curr_val > prev_val) : (curr_val < prev_val);
        } else {
            result.met = apply_comparison(curr_val, condition.op, target_curr);
        }
    } else {
        result.target_value = condition.value;

        if (condition.op == "crosses_above" || condition.op == "crosses_below") {
            result.met = apply_crossing(curr_val, prev_val,
                                        condition.value, condition.value, condition.op);
        } else if (condition.op == "rising" || condition.op == "falling") {
            result.met = (condition.op == "rising") ? (curr_val > prev_val) : (curr_val < prev_val);
        } else {
            result.met = apply_comparison(curr_val, condition.op, condition.value);
        }
    }

    return result;
}

GroupEvalResult ConditionEvaluator::evaluate_group(
    const QJsonArray& conditions_json,
    const QString& logic,
    const QVector<OhlcvCandle>& candles) {

    GroupEvalResult group;
    group.logic = logic;

    if (conditions_json.isEmpty()) {
        group.triggered = false;
        return group;
    }

    bool is_and = (logic.toUpper() == "AND");
    bool overall = is_and;

    for (const auto& val : conditions_json) {
        auto cond = parse_condition(val.toObject());
        auto result = evaluate_single(cond, candles);
        group.details.append(result);

        if (is_and) {
            overall = overall && result.met;
        } else {
            overall = overall || result.met;
        }
    }

    // Reset to false for OR if first iteration set it wrong
    if (!is_and && !conditions_json.isEmpty()) {
        overall = false;
        for (const auto& d : group.details) {
            if (d.met) { overall = true; break; }
        }
    }

    group.triggered = overall;
    return group;
}

} // namespace fincept::algo
