// src/algo_engine/AlgoScanner.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/CandleDataFetcher.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace fincept::algo {

struct ScanMatch {
    QString symbol;
    double price = 0;
    QVector<ConditionResult> details;
};

class AlgoScanner : public QObject {
    Q_OBJECT
  public:
    static AlgoScanner& instance();

    /// Result (scan_complete): {success, total_scanned, condition_count (LEAF rules),
    /// matches: [{symbol, price, prev_close, change_pct, bar_time, spark: [closes],
    /// bias: bullish|bearish|neutral, matched, conditions: [{indicator, field, operator,
    /// value, computed, met, bias, bias_reason, label}]}], errors: [{error}]}.
    /// `exchange_hint` = listing venue of bare symbols (see CandleDataFetcher::fetch).
    void scan(const QJsonArray& conditions, const QStringList& symbols, const QString& timeframe, int lookback_days,
              const QString& logic, DataSource source = DataSource::Auto, const QString& broker_id = {},
              const QString& account_id = {}, const QString& exchange_hint = {});

  signals:
    void scan_complete(const QJsonObject& result);
    void scan_error(const QString& error);

  private:
    AlgoScanner() = default;
    Q_DISABLE_COPY(AlgoScanner)
};

} // namespace fincept::algo
