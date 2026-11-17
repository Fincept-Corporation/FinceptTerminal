#pragma once
// InstrumentSync — keeps the local instrument index in step with the backend symbol
// master (symbol system WP3 / plan E7):
//
//   InstrumentIndex::read_sync_cursor -> GET /symbols/changes?since=<cursor>&limit=N
//     -> InstrumentIndex::apply_master_delta -> (has_more ? next page : wait 24 h)
//
// The endpoint is not on the live API yet. The first 404 switches the sync off for the
// session, silently (one INFO log line, endpoint_unavailable() once, no user-visible
// error); when the backend ships the route nothing here changes — the next start-up
// syncs. 401 / 403 also stop it for the session (a signed-out user). 410 (cursor too
// old) re-fetches from the beginning once. Network / server errors back off (5 min,
// 30 min, 2 h, then the normal interval) and are logged, never shown.
//
// start() also starts the InstrumentIndex (seed + broker-master import), so the
// deferred start-up chain in main.cpp needs one call. The production instance (HTTP
// through HttpClient, the index through InstrumentIndex) is defined in
// InstrumentResolverSources.cpp; tests construct their own with fake Ops.
// GUI thread only. Nothing blocks: HTTP and the index are asynchronous.

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class QTimer;

namespace fincept::services {

class InstrumentSync : public QObject {
    Q_OBJECT
  public:
    struct HttpReply {
        int status = 0;     ///< HTTP status; 0 = no response (network / timeout / parse failure)
        QJsonDocument body; ///< parsed body on success
        QString error;      ///< raw error text for the log
    };
    using HttpDone = std::function<void(const HttpReply&)>;
    using CursorDone = std::function<void(const QString& cursor)>;
    using DeltaDone = std::function<void(bool ok, const QString& cursor_or_error, bool has_more)>;

    struct Ops {
        /// GET a Fincept API path ("/symbols/changes?..."), reply on `context`'s thread.
        std::function<void(const QString& path, QObject* context, HttpDone)> get;
        std::function<void(QObject* context, CursorDone)> read_cursor;
        std::function<void(const QJsonObject& delta, QObject* context, DeltaDone)> apply_delta;
        std::function<void()> start_index; ///< optional: InstrumentIndex::start()
        int first_delay_ms = 90 * 1000;              ///< after start(): leave start-up alone
        int interval_ms = 24 * 60 * 60 * 1000;       ///< between successful runs
        int page_limit = 1000;                       ///< rows per page requested
        int max_pages_per_run = 200;                 ///< safety stop for one run
    };

    /// The application's sync (production Ops; InstrumentResolverSources.cpp).
    static InstrumentSync& instance();

    explicit InstrumentSync(Ops ops, QObject* parent = nullptr);
    ~InstrumentSync() override;

    /// Idempotent: starts the index and schedules the first run after Ops::first_delay_ms.
    void start();
    /// Run one sync now (no-op while running or after the endpoint was found missing).
    void sync_now();

    bool is_enabled() const { return !disabled_; }
    bool is_running() const { return running_; }
    /// Path of the changes endpoint.
    static QString changes_path() { return QStringLiteral("/symbols/changes"); }

  signals:
    /// One run ended: pages applied and the stored cursor ("" when nothing was applied).
    void sync_finished(int pages, const QString& cursor);
    /// The backend does not serve the endpoint (404) or refuses it (401 / 403); emitted once.
    void endpoint_unavailable();

  private:
    void run_cycle();
    void fetch(const QString& cursor);
    void on_reply(const HttpReply& reply);
    void finish_run(bool ok);
    void schedule(int ms);
    void disable(const QString& why);

    Ops ops_;
    QTimer* timer_ = nullptr;
    bool started_ = false;
    bool running_ = false;
    bool disabled_ = false;
    bool full_refetch_done_ = false;
    int failures_ = 0;
    int pages_ = 0;
    QString cursor_;
};

} // namespace fincept::services
