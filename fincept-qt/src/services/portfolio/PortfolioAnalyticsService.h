#pragma once
// PortfolioAnalyticsService — thin wrapper around user-triggered portfolio
// Python analytics so the views do not spawn Python directly (D1 / P6).
//
// These are one-shot "Run" button actions, not streaming data — therefore
// this is a non-Producer service with no hub topic. Callers invoke the
// async methods with a QPointer-guarded callback; the service forwards to
// PythonRunner and returns the parsed JSON result (or an error).

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

namespace fincept::services {

struct AnalyticsResult {
    bool success = false;
    QJsonObject data;  // valid when success==true
    QString error;     // set when success==false
};

using AnalyticsCallback = std::function<void(const AnalyticsResult&)>;

/// Wraps the quantstats / optimizer / ffn Python scripts invoked by the
/// portfolio views. All calls are async; callback fires on the main thread.
/// Callers should capture a QPointer to themselves per P8.
class PortfolioAnalyticsService : public QObject {
    Q_OBJECT
  public:
    static PortfolioAnalyticsService& instance();

    /// Runs `quantstats_analysis` with `{symbols, weights}` args.
    void run_quantstats(const QStringList& symbols, const QList<double>& weights,
                        AnalyticsCallback cb);

    /// Runs `quantstats_monte_carlo` with `{symbols, weights, num_simulations}`.
    void run_monte_carlo(const QStringList& symbols, const QList<double>& weights,
                         int num_simulations, AnalyticsCallback cb);

    /// Runs `optimize_portfolio_weights` with an opaque pre-built args JSON
    /// (the view already constructs a method-specific payload).
    void optimize_weights(const QString& args_json, AnalyticsCallback cb);

    /// Runs `ffn_analysis` with `{symbols, weights}` — weights is a symbol→frac map.
    void run_ffn(const QStringList& symbols, const QJsonObject& weights_by_symbol,
                 AnalyticsCallback cb);

  private:
    PortfolioAnalyticsService() = default;

    /// Core dispatch: invoke `script` with `args_json` and decode result.
    void run_script(const QString& script, const QString& args_json, AnalyticsCallback cb);
};

} // namespace fincept::services
