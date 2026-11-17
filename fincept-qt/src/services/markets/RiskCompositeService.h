#pragma once
// RiskCompositeService — one documented risk-on / risk-off score for the whole
// app (dashboard tile, Market Pulse), replacing the ad-hoc "sentiment" scores.
//
// Five cross-asset signals, each from the shared daily histories
// (MarketDataService::performance_history_topic, 1y of daily closes):
//
//   equity_trend   S&P 500 (^GSPC) close vs its 50-day average
//   volatility     VIX level, inverted (low VIX = risk-on)
//   credit         high yield vs Treasuries: 20-day return of HYG / IEF
//   risk_fx        AUD/JPY 20-day return (carry currency vs funding haven)
//   copper_gold    20-day return of the copper / gold ratio (HG=F / GC=F)
//
// Each signal is standardised against its own last year (z = (x - mean) / sd
// over up to 252 daily observations, at least 60), clipped to ±2. The score is
//   50 + 25 × mean(z)            (0 = deep risk-off, 100 = strong risk-on)
// and is computed ONLY when all five inputs are available — a partial basket
// never produces a number; `missing` says what is absent.
//
// The signals are global (US-listed and global futures / FX): risk appetite is
// set in global markets whatever the user's region. Consumers attach() while
// visible; the service holds its hub subscriptions only while attached.

#include <QDate>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::services {

struct RiskCompositeInput {
    QString id;          ///< "equity_trend", "volatility", "credit", "risk_fx", "copper_gold"
    QString name;        ///< translated short name
    QString description; ///< translated: what it measures
    QStringList symbols; ///< Yahoo inputs
    double raw = std::numeric_limits<double>::quiet_NaN(); ///< the measured value (unit in raw_text)
    double z = std::numeric_limits<double>::quiet_NaN();   ///< standardised, clipped to ±2 (+ = risk-on)
    QString raw_text;    ///< formatted raw value ("+1.8 % vs 50d avg", "VIX 14.2")
    QString error;       ///< why it is unavailable (empty when available)
    bool available() const { return z == z; }
};

struct RiskComposite {
    QVector<RiskCompositeInput> inputs;
    bool complete = false;
    double score = std::numeric_limits<double>::quiet_NaN(); ///< 0..100, NaN unless complete
    QString regime;      ///< translated label ("Risk-on", ...), empty unless complete
    QStringList missing; ///< names of unavailable signals
    QDate as_of;         ///< latest daily bar used
};

class RiskCompositeService : public QObject {
    Q_OBJECT
  public:
    static RiskCompositeService& instance();

    /// Start (first consumer) / stop (last consumer) the hub subscriptions.
    /// Consumers are dropped automatically when destroyed.
    void attach(QObject* consumer);
    void detach(QObject* consumer);

    const RiskComposite& current() const { return current_; }
    /// Force-refresh every input (user refresh).
    void refresh();

    /// Translated, multi-line explanation of the method (for tooltips / info).
    static QString methodology();
    /// Every Yahoo symbol the score reads.
    static QStringList input_symbols();

  signals:
    void updated();

  private:
    RiskCompositeService() = default;
    void subscribe_inputs();
    void recompute();

    QSet<QObject*> consumers_;
    bool subscribed_ = false;
    QHash<QString, QVector<QPair<QDate, double>>> closes_; ///< symbol -> ascending daily closes
    QHash<QString, QString> errors_;                        ///< symbol -> last hub error
    RiskComposite current_;
};

} // namespace fincept::services
