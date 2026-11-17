// src/algo_engine/AlgoScanner.cpp
#include "algo_engine/AlgoScanner.h"

#include "algo_engine/AlgoMarket.h"
#include "algo_engine/ConditionEvaluator.h"
#include "algo_engine/ScanSignal.h"
#include "core/logging/Logger.h"

#include <QJsonDocument>

#include <algorithm>
#include <cmath>

namespace fincept::algo {

namespace {

// Leaf rules of a (possibly nested) condition tree, depth first — the order
// ConditionEvaluator::evaluate_group appends their details in.
void scanner_flatten_leaves(const QJsonArray& nodes, QVector<QJsonObject>& out) {
    for (const auto& v : nodes) {
        const QJsonObject o = v.toObject();
        if (o.contains(QStringLiteral("children")) || o.value(QStringLiteral("type")).toString() == QLatin1String("group"))
            scanner_flatten_leaves(o.value(QStringLiteral("children")).toArray(), out);
        else
            out.append(o);
    }
}

QString scanner_bias_key(scan_signal::Bias b) {
    switch (b) {
    case scan_signal::Bias::Bullish:
        return QStringLiteral("bullish");
    case scan_signal::Bias::Bearish:
        return QStringLiteral("bearish");
    case scan_signal::Bias::Neutral:
        break;
    }
    return QStringLiteral("neutral");
}

} // namespace

AlgoScanner& AlgoScanner::instance() {
    static AlgoScanner s;
    return s;
}

void AlgoScanner::scan(const QJsonArray& conditions, const QStringList& symbols, const QString& timeframe,
                       int lookback_days, const QString& logic, DataSource source, const QString& broker_id,
                       const QString& account_id, const QString& exchange_hint) {
    if (conditions.isEmpty()) {
        emit scan_error(QStringLiteral("No conditions provided"));
        return;
    }
    if (symbols.isEmpty()) {
        emit scan_error(QStringLiteral("No symbols provided"));
        return;
    }

    LOG_DEBUG("AlgoScanner",
              QString("Scanning %1 symbols via %2").arg(symbols.size()).arg(data_source_to_string(source)));

    QVector<QJsonObject> leaves;
    scanner_flatten_leaves(conditions, leaves);

    // The rules are evaluated on the latest bar, but every indicator needs history
    // behind it: the panel's default range "Today" fetched one day — no daily bar
    // before the open, never the 20 bars required below — so every symbol came back
    // "No candles" / "insufficient data". Always fetch ~100 bars of warm-up.
    {
        const int bar_s = bar_seconds_for(timeframe);
        int warmup_days = 2;
        if (bar_s >= 86400)
            warmup_days = static_cast<int>(std::ceil(150.0 * bar_s / 86400.0)); // ~100 sessions
        else if (bar_s > 0)
            warmup_days = std::max(2, static_cast<int>(std::ceil(100.0 * bar_s / (6.5 * 3600.0) * 1.5)));
        if (lookback_days < warmup_days) {
            LOG_INFO("AlgoScanner", QString("Range %1 d too short for indicator warm-up — fetching %2 d")
                                        .arg(lookback_days)
                                        .arg(warmup_days));
            lookback_days = warmup_days;
        }
    }

    CandleDataFetcher::instance().fetch_multi(
        symbols, timeframe, lookback_days, source, broker_id, account_id,
        [this, conditions, logic, leaves](const QHash<QString, QVector<OhlcvCandle>>& data,
                                          const QStringList& fetch_errors) {
            QJsonArray matches_arr;
            QStringList all_errors = fetch_errors;
            int scanned = 0;

            for (auto it = data.begin(); it != data.end(); ++it) {
                scanned++;
                const auto& candles = it.value();
                if (candles.size() < 20) {
                    all_errors.append(QString("%1: insufficient data (%2 candles)").arg(it.key()).arg(candles.size()));
                    continue;
                }

                // full_details: every rule with its met flag (OR logic short-circuited
                // after the first hit, AND exported only up to evaluation order).
                const auto eval = ConditionEvaluator::evaluate_group(conditions, logic, candles, /*full_details*/ true);
                if (!eval.triggered)
                    continue;

                QJsonObject mo;
                mo["symbol"] = it.key();
                const OhlcvCandle& last = candles.last();
                mo["price"] = last.close;
                mo["bar_time"] = static_cast<double>(last.open_time);
                // Change vs the previous SESSION's last close (exchange trading date),
                // whatever the bar size.
                double prev_close = std::nan("");
                for (int i = candles.size() - 2; i >= 0; --i) {
                    if (candles[i].session_date != last.session_date || last.session_date == 0) {
                        prev_close = candles[i].close;
                        break;
                    }
                }
                if (std::isfinite(prev_close) && prev_close > 0) {
                    mo["prev_close"] = prev_close;
                    mo["change_pct"] = (last.close / prev_close - 1.0) * 100.0;
                }
                QJsonArray spark;
                for (int i = std::max(0, static_cast<int>(candles.size()) - 60); i < candles.size(); ++i)
                    spark.append(candles[i].close);
                mo["spark"] = spark;

                // Rule details with their original definitions (for the direction).
                const bool aligned = eval.details.size() == leaves.size();
                QJsonArray conds, matched_defs;
                int matched = 0;
                for (int i = 0; i < eval.details.size(); ++i) {
                    const auto& d = eval.details[i];
                    QJsonObject def = aligned ? leaves[i] : QJsonObject{};
                    if (def.isEmpty()) {
                        def["indicator"] = d.indicator;
                        def["field"] = d.field;
                        def["operator"] = d.op;
                        def["value"] = d.target_value;
                    }
                    const auto rb = scan_signal::classify(def);
                    QJsonObject co;
                    co["indicator"] = d.indicator;
                    co["field"] = d.field;
                    co["operator"] = d.op;
                    co["value"] = d.target_value;
                    co["computed"] = std::isfinite(d.computed_value) ? QJsonValue(d.computed_value)
                                                                     : QJsonValue(QJsonValue::Null);
                    co["met"] = d.met; // did THIS rule match (OR groups match on one)
                    co["bias"] = scanner_bias_key(rb.bias);
                    co["bias_reason"] = rb.reason;
                    if (!d.error.isEmpty())
                        co["error"] = d.error;
                    conds.append(co);
                    if (d.met) {
                        ++matched;
                        matched_defs.append(def);
                    }
                }
                mo["conditions"] = conds;
                mo["matched"] = matched;
                mo["bias"] = scanner_bias_key(scan_signal::combine(matched_defs));
                matches_arr.append(mo);
            }

            QJsonObject out;
            out["success"] = true;
            out["total_scanned"] = scanned;
            out["requested"] = static_cast<int>(data.size() + fetch_errors.size());
            // Leaf rules, not top-level nodes: a nested group counted as ONE condition,
            // so "2/3 matched" could not be compared with the per-rule columns.
            out["condition_count"] = static_cast<int>(leaves.size());
            out["matches"] = matches_arr;

            QJsonArray err_arr;
            for (const auto& e : all_errors) {
                QJsonObject eo;
                eo["error"] = e;
                err_arr.append(eo);
            }
            out["errors"] = err_arr;

            emit scan_complete(out);
            LOG_DEBUG("AlgoScanner",
                      QString("Scan done: %1 matches / %2 scanned").arg(matches_arr.size()).arg(scanned));
        },
        exchange_hint);
}

} // namespace fincept::algo
