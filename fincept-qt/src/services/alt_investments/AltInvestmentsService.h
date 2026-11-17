#pragma once
// AltInvestmentsService — the Alternative Investments tab's one door to its Python scripts, so the
// screen and its panels never hold script paths, run processes or cache replies themselves.
//
//   using namespace fincept::services::alt;
//   AltCall c;
//   c.script = kPrivateMarketsScript;
//   c.command = QStringLiteral("metrics");
//   c.input = QJsonObject{{"flows", flows}, {"commitment", 1e7}};   // sent on stdin (never argv)
//   c.timeout_ms = 60 * 1000;
//   const quint64 id = AltInvestmentsService::instance().call(c, this, [this](const AltReply& r) {
//       if (r.cancelled) return;
//       r.ok ? render(r.data) : show_error(r.error);                // r.error is plain language
//   });
//   ...
//   AltInvestmentsService::instance().cancel(id);                   // stop it (callback never runs)
//
// Script convention for the scripts this service runs (private_markets.py, fund_analytics.py,
// alt_market_snapshot.py): `<script> <command> --stdin`, the input object as JSON on stdin, one JSON
// object on stdout ({"success": true, "data": {...}} or {"success": false, "error": "..."}). cli.py
// keeps its own form (`<analyzer> --data <json> --method <m>`): set `args` and leave `command` empty.
//
// Every call is asynchronous (PythonCliService -> PythonRunner: a QProcess, output parsed off the GUI
// thread), cancellable, bounded by a timeout, and optionally cached (CacheManager, keyed on script +
// arguments + input) — `cached()` reads a cached reply without running anything, for sources that
// must not run by themselves (rate-limited free data: show the last result with its "as of").

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::alt {

// Script paths, relative to the scripts directory.
inline const QString kCliScript = QStringLiteral("Analytics/alternateInvestment/cli.py");
inline const QString kPrivateMarketsScript = QStringLiteral("Analytics/alternateInvestment/private_markets.py");
inline const QString kFundAnalyticsScript = QStringLiteral("Analytics/alternateInvestment/fund_analytics.py");
inline const QString kMarketSnapshotScript = QStringLiteral("Analytics/alternateInvestment/alt_market_snapshot.py");
inline const QString kRealEstateScript = QStringLiteral("Analytics/alternateInvestment/real_estate_proforma.py");

struct AltCall {
    QString script;
    /// First argument; the input then goes on stdin with "--stdin". Empty = use `args` as given.
    QString command;
    /// Extra / raw arguments (cli.py form when `command` is empty).
    QStringList args;
    /// Sent as compact JSON on stdin (only when `command` is set).
    QJsonObject input;
    int timeout_ms = 90 * 1000;
    /// > 0: a successful reply is cached this long and an identical call is answered from it.
    int cache_ttl_sec = 0;
    /// Data-source label kept with a cached reply ("Federal Reserve H.15").
    QString source;
};

struct AltReply {
    bool ok = false;
    QJsonObject data;   ///< the reply's "data" object (or the whole object when it has none)
    QJsonObject raw;    ///< the whole reply
    QString error;      ///< plain-language reason when !ok
    bool cancelled = false;
    bool from_cache = false;
    QDateTime as_of;    ///< when the reply was computed (a cached reply keeps its original time)
    QString source;
};

class AltInvestmentsService : public QObject {
    Q_OBJECT
  public:
    static AltInvestmentsService& instance();

    using Callback = std::function<void(const AltReply&)>;

    /// Start `call`. `cb` runs once on the GUI thread (a cache hit: before call() returns) unless
    /// `ctx` is destroyed first or the request is cancelled. Returns the request id (never 0).
    quint64 call(const AltCall& call, QObject* ctx, Callback cb);
    /// Stop request `id`: a queued run never starts, a running one is killed; its callback never runs.
    void cancel(quint64 id);
    /// The cached reply of an identical call, without running anything. False when none / expired.
    bool cached(const AltCall& call, AltReply* out) const;
    /// Forget the cached reply of `call` (a user REFRESH).
    void drop_cached(const AltCall& call);

    /// Plain-language text for a runner / script failure ("Script timed out after 60 s" -> ...).
    static QString plain_error(const QString& raw);

  private:
    explicit AltInvestmentsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(AltInvestmentsService)

    static QString cache_key(const AltCall& call);

    struct Pending {
        std::shared_ptr<std::atomic<bool>> cancel;
    };
    QHash<quint64, Pending> pending_;
    quint64 next_id_ = 0;
};

} // namespace fincept::services::alt
