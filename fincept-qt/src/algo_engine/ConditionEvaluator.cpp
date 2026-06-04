// src/algo_engine/ConditionEvaluator.cpp
#include "algo_engine/ConditionEvaluator.h"

#include <QJsonObject>

#include <cmath>
#include <limits>

namespace fincept::algo {

using services::algo::ConditionDef;

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

bool op_needs_prev(const QString& op) {
    return op == "crosses_above" || op == "crosses_below" ||
           op == "rising" || op == "falling" ||
           // "==" is a level *touch*: it needs the previous sample to detect the
           // price passing through the target between two (polled) ticks.
           op == "==";
}
} // namespace

ConditionDef ConditionEvaluator::parse_condition(const QJsonObject& obj) {
    ConditionDef c;
    c.indicator = obj.value("indicator").toString();
    c.params = obj.value("params").toObject();
    c.field = obj.value("field").toString("value");
    c.op = obj.value("operator").toString(">");
    c.value = obj.value("value").toDouble(0);
    c.value2 = obj.value("value2").toDouble(0);
    c.compare_mode = obj.value("compare_mode").toString("value");
    c.compare_indicator = obj.value("compare_indicator").toString();
    c.compare_params = obj.value("compare_params").toObject();
    c.compare_field = obj.value("compare_field").toString("value");
    c.offset = obj.value("offset").toInt(0);
    c.compare_offset = obj.value("compare_offset").toInt(0);
    return c;
}

bool ConditionEvaluator::is_group_node(const QJsonObject& node) {
    return node.contains("children") || node.value("type").toString() == "group";
}

bool ConditionEvaluator::apply_comparison(double lhs, const QString& op, double rhs) {
    if (std::isnan(lhs) || std::isnan(rhs)) return false;
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
    if (std::isnan(curr) || std::isnan(prev) ||
        std::isnan(target_curr) || std::isnan(target_prev))
        return false;
    if (op == "crosses_above")
        return prev <= target_prev && curr > target_curr;
    if (op == "crosses_below")
        return prev >= target_prev && curr < target_curr;
    return false;
}

double ConditionEvaluator::operand_value(const QString& indicator, const QJsonObject& params,
                                         const QString& field, int offset,
                                         const QVector<OhlcvCandle>& candles, QString* error) {
    if (offset < 0) offset = 0;
    QVector<OhlcvCandle> window = candles;
    if (offset > 0) {
        if (offset >= candles.size()) {
            if (error) *error = QStringLiteral("insufficient data for offset %1").arg(offset);
            return kNaN;
        }
        window = candles.mid(0, candles.size() - offset);
    }
    auto r = IndicatorEngine::compute(indicator, window, params, field);
    if (!r.valid) {
        if (error) *error = r.error;
        return kNaN;
    }
    return r.current.value(field, kNaN);
}

ConditionResult ConditionEvaluator::evaluate_single(
    const ConditionDef& condition,
    const QVector<OhlcvCandle>& candles) {

    ConditionResult result;
    result.indicator = condition.indicator;
    result.field = condition.field;
    result.op = condition.op;

    const bool needs_prev = op_needs_prev(condition.op);

    QString err;
    double lhs_curr = operand_value(condition.indicator, condition.params, condition.field,
                                    condition.offset, candles, &err);
    if (!err.isEmpty()) {
        result.error = err;
        return result;
    }
    result.computed_value = lhs_curr;

    double lhs_prev = needs_prev
        ? operand_value(condition.indicator, condition.params, condition.field,
                        condition.offset + 1, candles, nullptr)
        : kNaN;

    double rhs_curr, rhs_prev;
    if (condition.compare_mode == "indicator") {
        QString cerr;
        rhs_curr = operand_value(condition.compare_indicator, condition.compare_params,
                                 condition.compare_field, condition.compare_offset, candles, &cerr);
        if (!cerr.isEmpty()) {
            result.error = cerr;
            return result;
        }
        rhs_prev = needs_prev
            ? operand_value(condition.compare_indicator, condition.compare_params,
                            condition.compare_field, condition.compare_offset + 1, candles, nullptr)
            : kNaN;
    } else {
        rhs_curr = condition.value;
        rhs_prev = condition.value;
    }
    result.target_value = rhs_curr;

    if (condition.op == "crosses_above" || condition.op == "crosses_below") {
        result.met = apply_crossing(lhs_curr, lhs_prev, rhs_curr, rhs_prev, condition.op);
    } else if (condition.op == "rising") {
        result.met = !std::isnan(lhs_curr) && !std::isnan(lhs_prev) && lhs_curr > lhs_prev;
    } else if (condition.op == "falling") {
        result.met = !std::isnan(lhs_curr) && !std::isnan(lhs_prev) && lhs_curr < lhs_prev;
    } else if (condition.op == "between") {
        result.met = !std::isnan(lhs_curr) && lhs_curr >= condition.value && lhs_curr <= condition.value2;
        result.target_value = condition.value2;
    } else if (condition.op == "==") {
        // Level touch / equality cross. A live price polled every few seconds skips
        // the exact value (280.35 → 280.45 never equals 280.40), so plain float ==
        // never fires. Instead, trigger when the operands meet OR cross between the
        // previous and current sample — i.e. lhs−rhs is ~0 now, or changed sign
        // since the last sample (passed through equality, from either direction).
        double tol = std::abs(rhs_curr) * 1e-7;
        if (tol < 1e-9) tol = 1e-9;
        const double curr_diff = lhs_curr - rhs_curr;
        const double prev_diff = lhs_prev - rhs_prev; // NaN on the first sample
        if (std::isnan(lhs_curr) || std::isnan(rhs_curr))
            result.met = false;
        else if (std::abs(curr_diff) <= tol)
            result.met = true; // sitting on the level (within float tolerance)
        else if (!std::isnan(prev_diff) && ((prev_diff < 0) != (curr_diff < 0)))
            result.met = true; // crossed through the level since the last sample
        else
            result.met = false;
    } else {
        result.met = apply_comparison(lhs_curr, condition.op, rhs_curr);
    }

    return result;
}

GroupEvalResult ConditionEvaluator::evaluate_group(
    const QJsonArray& children,
    const QString& logic,
    const QVector<OhlcvCandle>& candles) {

    GroupEvalResult group;
    group.logic = logic;

    if (children.isEmpty()) {
        group.triggered = false;
        return group;
    }

    const bool is_and = (logic.toUpper() != "OR"); // default AND
    bool overall = is_and;

    for (const auto& val : children) {
        const QJsonObject node = val.toObject();

        bool met;
        if (is_group_node(node)) {
            auto sub = evaluate_group(node.value("children").toArray(),
                                      node.value("logic").toString(
                                          node.value("op").toString("AND")),
                                      candles);
            met = node.value("negate").toBool(false) ? !sub.triggered : sub.triggered;
            group.details.append(sub.details); // flatten nested detail for reporting
        } else {
            auto cond = parse_condition(node);
            auto r = evaluate_single(cond, candles);
            group.details.append(r);
            met = r.met;
        }

        if (is_and) {
            overall = overall && met;
            if (!overall) break; // short-circuit
        } else {
            overall = overall || met;
            if (overall) break; // short-circuit
        }
    }

    group.triggered = overall;
    return group;
}

} // namespace fincept::algo
