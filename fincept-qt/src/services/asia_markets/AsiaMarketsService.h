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

#include <functional>

namespace fincept::services::asia_markets {

/// Result of an endpoint-list query.
struct EndpointsResult {
    bool success = false;
    QJsonObject data;  // raw script JSON (expected to contain "categories" or "available_endpoints")
    QString error;
};

/// Result of a market-data query (data[] rows from the connector script).
struct QueryResult {
    bool success = false;
    QJsonArray rows;   // normalized data array
    QString error;
};

using EndpointsCallback = std::function<void(const EndpointsResult&)>;
using QueryCallback = std::function<void(const QueryResult&)>;

/// Wraps Asia market connector scripts. All calls are async; callback fires
/// on the main thread. Callers should capture a QPointer to themselves (P8).
class AsiaMarketsService : public QObject {
    Q_OBJECT
  public:
    static AsiaMarketsService& instance();

    /// Lists available endpoints for a script (e.g. "china_market_data_service").
    /// Runs `<script> get_all_endpoints`.
    void fetch_endpoints(const QString& script, EndpointsCallback cb);

    /// Runs `<script> <endpoint> [extra_args...]` and normalizes the result to
    /// a QJsonArray of rows.
    void query(const QString& script, const QString& endpoint,
               const QStringList& extra_args, QueryCallback cb);

  private:
    AsiaMarketsService() = default;
};

} // namespace fincept::services::asia_markets
