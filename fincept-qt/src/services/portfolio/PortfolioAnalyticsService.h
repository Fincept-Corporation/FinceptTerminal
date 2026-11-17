#pragma once
// PortfolioAnalyticsService — thin wrapper around the portfolio Python analytics
// so the views do not spawn Python directly (D1 / P6).
//
// These are one-shot actions (a RUN button or a view's default run on first
// show), not streaming data — therefore this is a non-Producer service with no
// hub topic. Callers invoke the async methods with a QPointer-guarded callback;
// the service forwards to PythonRunner and returns the parsed JSON result (or an
// error).
//
// Every call returns an AnalyticsRequest: its request id (echoed in
// AnalyticsResult::request_id, so a view drops a late reply that belongs to a
// run it no longer waits for) and a cancel token. Raising the token (cancel())
// never spawns a run that is still queued and kills a running script within
// ~250 ms; the callback then receives `cancelled = true` (no error to show).
// ui::AutoLoadPane hosts call cancel() from their CancelFn when the user leaves
// the tab.
//
// `context`: optional keys merged into every payload so the scripts measure in
// the portfolio's base currency on the benchmark's calendar — build it with
// PortfolioService::analytics_context(summary):
//   {"base_currency", "currencies": {SYM: quoted ccy}, "benchmark", "periods_per_year",
//    "period"?, "seed"?, "horizon_days"?, "method"?, ...}
// Keys set explicitly by the call (symbols, weights, risk_free_rate, ...) win.

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services {

/// Raised (store(true)) to cancel a request; shared with PythonRunner::RunOptions::cancel_token.
using AnalyticsCancelToken = std::shared_ptr<std::atomic<bool>>;

struct AnalyticsResult {
    bool success = false;
    QJsonObject data; // valid when success==true
    QString error;    // set when success==false (the script's own plain message when it gave one)
    quint64 request_id = 0; ///< id of the AnalyticsRequest that produced this result (0 = untracked)
    bool cancelled = false; ///< the request was cancelled by its caller — nothing to show or log
};

using AnalyticsCallback = std::function<void(const AnalyticsResult&)>;

/// Handle of one analytics request: compare `id` with AnalyticsResult::request_id
/// to ignore a late reply; cancel() stops the script (queued: never spawned;
/// running: killed) and the callback then reports `cancelled`.
struct AnalyticsRequest {
    quint64 id = 0;
    AnalyticsCancelToken token;

    bool valid() const { return id != 0; }
    void cancel() const {
        if (token)
            token->store(true);
    }
};

/// Wraps the quantstats / optimizer / ffn Python scripts invoked by the
/// portfolio views. All calls are async; callback fires on the main thread.
/// Callers should capture a QPointer to themselves per P8.
class PortfolioAnalyticsService : public QObject {
    Q_OBJECT
  public:
    static PortfolioAnalyticsService& instance();

    /// A new request handle (process-wide unique id, fresh token). Also used by
    /// PortfolioService for its portfolio_metrics.py commands.
    static AnalyticsRequest make_request();

    /// Runs `quantstats_analysis` with `{symbols, weights, risk_free_rate, benchmark, period}` + context.
    /// `period`: 6mo|1y|2y|3y|5y|10y|ytd|max (empty = 1y). A NaN risk_free_rate is sent as
    /// absent (the script then uses 0 and reports meta.risk_free_known = false).
    AnalyticsRequest run_quantstats(const QStringList& symbols, const QList<double>& weights, double risk_free_rate,
                                    const QString& benchmark, AnalyticsCallback cb, const QString& period = {},
                                    const QJsonObject& context = {});

    /// Runs `quantstats_monte_carlo` with `{symbols, weights, num_simulations}` + context
    /// (horizon_days, seed — absent = fresh randomness, method "bootstrap"|"gbm", period).
    AnalyticsRequest run_monte_carlo(const QStringList& symbols, const QList<double>& weights, int num_simulations,
                                     AnalyticsCallback cb, const QJsonObject& context = {});

    /// Runs `optimize_portfolio_weights` with an opaque pre-built args JSON
    /// (the view already constructs a method-specific payload); `context` keys are
    /// added where the args do not set them (period, max_weight, sector_caps, ...).
    AnalyticsRequest optimize_weights(const QString& args_json, AnalyticsCallback cb, const QJsonObject& context = {});

    /// Runs `ffn_analysis` with `{symbols, weights, risk_free_rate}` + context — weights is a symbol→frac map.
    AnalyticsRequest run_ffn(const QStringList& symbols, const QJsonObject& weights_by_symbol, double risk_free_rate,
                             AnalyticsCallback cb, const QJsonObject& context = {});

  private:
    PortfolioAnalyticsService() = default;

    /// Core dispatch: invoke `script` with `args_json` and decode result.
    /// `via_stdin` feeds the JSON on the child's stdin (and closes it) instead of
    /// argv - required for the scripts that only read stdin.
    AnalyticsRequest run_script(const QString& script, const QString& args_json, AnalyticsCallback cb,
                                bool via_stdin = false);
};

} // namespace fincept::services
