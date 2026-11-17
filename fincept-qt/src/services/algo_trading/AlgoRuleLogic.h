// src/services/algo_trading/AlgoRuleLogic.h
#pragma once
// AlgoRuleLogic — pure rules about the condition JSON the Algo Trading tab edits and the
// native engine evaluates (ConditionEvaluator): plain-language text, inline validation,
// the "blank draft" placeholder, and the indicator warm-up a backtest needs.
//
// Condition JSON (leaf): {indicator, params, field, offset, operator, compare_mode:"value"|
// "indicator", value, value2 (between), compare_indicator, compare_params, compare_field,
// compare_offset}. Group: {children:[...], logic:"AND"|"OR", negate?}.
//
// Qt Core only (+ the indicator catalogue in AlgoTradingTypes.h); unit-tested in
// tests/tst_algo_rule_logic.cpp. Text is translated through QCoreApplication::translate with
// the context "fincept::services::algo::rules".

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::services::algo::rules {

/// Which block a sentence describes (its first word).
enum class Side { Entry, Exit, Scan, Alert };

/// "SMA(50)", "MACD(12, 26, 9) signal line", "Close", "RSI(14) 1 bar ago".
QString operand_text(const QString& indicator, const QJsonObject& params, const QString& field, int offset);
/// One leaf: "SMA(50) crosses above SMA(200)", "RSI(14) < 30", "Close is between 10 and 20".
QString condition_text(const QJsonObject& condition);
/// The whole block: "Enter when SMA(50) crosses above SMA(200)", "Exit when RSI(14) > 60 or
/// (Close < SMA(20) and Volume > 1000000)"; an empty block says so ("No entry rule yet").
QString rules_sentence(const QJsonArray& conditions, const QString& logic, Side side);

/// A problem with one row of a block. `index` is the top-level row, `child` the row inside a
/// group (-1 = the row / group itself). Blocking problems stop RUN / SAVE / DEPLOY.
struct RuleIssue {
    int index = -1;
    int child = -1;
    QString reason;
    bool blocking = true;
};
/// Every problem of a block, in row order (empty = fine).
QVector<RuleIssue> check_conditions(const QJsonArray& conditions);
/// The first blocking problem of a strategy for a message ("ENTRY rule 2: …"), empty when it
/// can run. An exit is needed unless a stop-loss, take-profit or trailing stop is set.
QString validate_strategy(const QJsonArray& entry, const QJsonArray& exit, double stop_loss_pct,
                          double take_profit_pct, double trailing_stop_pct);

/// The one row a blank Builder form used to start with: Close > 0 (value mode, current bar).
bool is_placeholder_condition(const QJsonObject& condition);
/// True when both blocks are empty or hold only that placeholder row — a draft nobody wrote
/// (the default run opens a template instead of backtesting it).
bool is_blank_rules(const QJsonArray& entry, const QJsonArray& exit);

/// Bars of history the slowest indicator of these rules needs before its first valid value
/// (periods, EMA convergence, MACD slow + signal, offsets, the previous bar of a cross),
/// at least `minimum`, at most `maximum`.
int warmup_bars(const QJsonArray& entry, const QJsonArray& exit, int minimum = 50, int maximum = 499);

} // namespace fincept::services::algo::rules
