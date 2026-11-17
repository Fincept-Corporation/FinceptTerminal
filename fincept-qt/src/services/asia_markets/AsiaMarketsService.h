#pragma once
// AsiaMarketsService — thin wrapper around user-triggered Asia markets
// Python connector scripts so the screen does not spawn Python directly (D1 / P6).
//
// Asia Markets is a browse-and-query tool (click a category, click an endpoint,
// see data). There is no streaming cadence, no cross-screen sharing — this is
// a one-shot user action, so a non-Producer service wrapper is correct.
//
// Scripts live under scripts/china_market_scripts/, scripts/japan_market_scripts/,
// etc. Each script takes one argument: an endpoint name (or the literal
// "get_all_endpoints" to list available endpoints).

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::asia_markets {

/// Raising it (any thread) stops the script: a queued run never starts, a running one is killed;
/// its callback then reports cancelled = true.
using CancelToken = std::shared_ptr<std::atomic<bool>>;

/// Result of an endpoint-list query.
struct EndpointsResult {
    bool success = false;
    bool cancelled = false;
    QJsonObject data; // raw script JSON (expected to contain "categories" or "available_endpoints")
    QString error;
};

/// Result of a market-data query (data[] rows from the connector script).
struct QueryResult {
    bool success = false;
    bool cancelled = false;
    QJsonArray rows;     // normalized data array
    QStringList columns; // ordered column names from the source DataFrame (empty if the script didn't supply them)
    QStringList ignored_params; // screen boxes (e.g. SYMBOL) the endpoint has no parameter for
    QString error;
    /// The script's own classification of a failure (akshare_screen_args.py): "network" (the site is
    /// unreachable, refusing or rate-limiting), "unavailable" (needs a login / paid key), "usage",
    /// "missing_param"; empty when the script did not say.
    QString error_type;
    /// The endpoint's parameters that were not given ("symbol", "date", …) — the structured
    /// "needs an input" answer of akshare_screen_args.py (error_type "missing_param"); empty otherwise.
    QStringList missing;
    /// Estimated in-memory size of `rows` (ui::autoload::approx_json_bytes, computed on the decode
    /// thread), so the screen's result pane never walks a large result on the GUI thread to size it.
    qint64 approx_bytes = 0;
};

using EndpointsCallback = std::function<void(const EndpointsResult&)>;
using QueryCallback = std::function<void(const QueryResult&)>;

/// Wraps Asia market connector scripts. All calls are async; callback fires
/// on the main thread. Callers should capture a QPointer to themselves (P8).
/// The script's JSON is decoded and shaped on the service's own decode thread
/// (AsiaMarketsDecode.h). A token raised before the callback runs turns the
/// answer into cancelled = true / "Cancelled" (the decoded data is discarded).
class AsiaMarketsService : public QObject {
    Q_OBJECT
  public:
    static AsiaMarketsService& instance();

    /// Lists available endpoints for a script (e.g. "china_market_data_service").
    /// Runs `<script> get_all_endpoints`.
    void fetch_endpoints(const QString& script, EndpointsCallback cb, CancelToken cancel = {});

    /// Runs `<script> <endpoint> [extra_args...]` and normalizes the result to
    /// a QJsonArray of rows.
    void query(const QString& script, const QString& endpoint, const QStringList& extra_args, QueryCallback cb,
               CancelToken cancel = {});

  private:
    AsiaMarketsService() = default;
};

} // namespace fincept::services::asia_markets
