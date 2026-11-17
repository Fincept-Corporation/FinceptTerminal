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

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::akshare {

/// Raising it (any thread) stops the script: a queued run never starts, a running one is killed;
/// its callback then reports cancelled = true.
using CancelToken = std::shared_ptr<std::atomic<bool>>;

struct EndpointsResult {
    bool success = false;
    bool cancelled = false;
    QJsonObject data; // raw script JSON
    QString error;
};

struct QueryResult {
    bool success = false;
    bool cancelled = false;
    QJsonArray rows;
    QStringList columns; // ordered column names from the source DataFrame (empty if the script didn't supply them)
    QStringList ignored_params; // screen boxes (SYMBOL / START / END / PERIOD) the endpoint has no parameter for
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

/// The script's JSON is decoded and shaped on the service's own decode thread (AkShareDecode.h);
/// callbacks run on the GUI thread. A token raised before the callback runs turns the answer into
/// cancelled = true / "Cancelled" (the decoded data is discarded).
class AkShareService : public QObject {
    Q_OBJECT
  public:
    static AkShareService& instance();

    /// Runs `<script> get_all_endpoints`.
    void fetch_endpoints(const QString& script, EndpointsCallback cb, CancelToken cancel = {});

    /// Runs `<script> <endpoint> [extra_args...]`.
    void query(const QString& script, const QString& endpoint, const QStringList& extra_args, QueryCallback cb,
               CancelToken cancel = {});

  private:
    AkShareService() = default;
};

} // namespace fincept::services::akshare
