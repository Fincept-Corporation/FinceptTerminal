// src/algo_engine/AlgoScanner.cpp
#include "algo_engine/AlgoScanner.h"
#include "algo_engine/ConditionEvaluator.h"
#include "core/logging/Logger.h"

#include <QJsonDocument>

namespace fincept::algo {

AlgoScanner& AlgoScanner::instance() {
    static AlgoScanner s;
    return s;
}

void AlgoScanner::scan(const QJsonArray& conditions, const QStringList& symbols,
                       const QString& timeframe, int lookback_days, const QString& logic,
                       DataSource source, const QString& broker_id, const QString& account_id) {
    if (conditions.isEmpty()) {
        emit scan_error(QStringLiteral("No conditions provided"));
        return;
    }
    if (symbols.isEmpty()) {
        emit scan_error(QStringLiteral("No symbols provided"));
        return;
    }

    LOG_INFO("AlgoScanner", QString("Scanning %1 symbols via %2")
             .arg(symbols.size()).arg(data_source_to_string(source)));

    CandleDataFetcher::instance().fetch_multi(
        symbols, timeframe, lookback_days, source, broker_id, account_id,
        [this, conditions, logic](const QHash<QString, QVector<OhlcvCandle>>& data,
                                  const QStringList& fetch_errors) {
            QVector<ScanMatch> matches;
            QStringList all_errors = fetch_errors;
            int scanned = 0;

            for (auto it = data.begin(); it != data.end(); ++it) {
                scanned++;
                const auto& candles = it.value();
                if (candles.size() < 20) {
                    all_errors.append(QString("%1: insufficient data (%2 candles)")
                                          .arg(it.key()).arg(candles.size()));
                    continue;
                }

                auto eval = ConditionEvaluator::evaluate_group(conditions, logic, candles);
                if (eval.triggered) {
                    ScanMatch m;
                    m.symbol = it.key();
                    m.price = candles.last().close;
                    m.details = eval.details;
                    matches.append(m);
                }
            }

            QJsonObject out;
            out["success"] = true;
            out["total_scanned"] = scanned;
            out["condition_count"] = conditions.size();

            QJsonArray matches_arr;
            for (const auto& m : matches) {
                QJsonObject mo;
                mo["symbol"] = m.symbol;
                mo["price"] = m.price;
                QJsonArray conds;
                for (const auto& d : m.details) {
                    QJsonObject co;
                    co["indicator"] = d.indicator;
                    co["field"] = d.field;
                    co["operator"] = d.op;
                    co["value"] = d.target_value;
                    conds.append(co);
                }
                mo["conditions"] = conds;
                matches_arr.append(mo);
            }
            out["matches"] = matches_arr;

            QJsonArray err_arr;
            for (const auto& e : all_errors) {
                QJsonObject eo;
                eo["error"] = e;
                err_arr.append(eo);
            }
            out["errors"] = err_arr;

            emit scan_complete(out);
            LOG_INFO("AlgoScanner", QString("Scan done: %1 matches / %2 scanned")
                     .arg(matches.size()).arg(scanned));
        });
}

} // namespace fincept::algo
