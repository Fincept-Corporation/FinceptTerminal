#pragma once
// AkShareService — thin wrapper around user-triggered AkShare connector
// scripts so the screen does not spawn Python directly (D1 / P6).
//
// AkShare is a browse-and-query tool (pick a source, pick an endpoint, see
// data). No streaming cadence, no cross-screen sharing — this is a one-shot
// user action, so a non-Producer service wrapper is correct.

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::services::akshare {

struct EndpointsResult {
    bool success = false;
    QJsonObject data;  // raw script JSON
    QString error;
};

struct QueryResult {
    bool success = false;
    QJsonArray rows;
    QString error;
};

using EndpointsCallback = std::function<void(const EndpointsResult&)>;
using QueryCallback = std::function<void(const QueryResult&)>;

class AkShareService : public QObject {
    Q_OBJECT
  public:
    static AkShareService& instance();

    /// Runs `<script> get_all_endpoints`.
    void fetch_endpoints(const QString& script, EndpointsCallback cb);

    /// Runs `<script> <endpoint> [extra_args...]`.
    void query(const QString& script, const QString& endpoint,
               const QStringList& extra_args, QueryCallback cb);

  private:
    AkShareService() = default;
};

} // namespace fincept::services::akshare
