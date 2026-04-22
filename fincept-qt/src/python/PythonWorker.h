#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>

#include <functional>

namespace fincept::python {

/// Persistent yfinance worker — long-lived Python process running
/// `yfinance_data.py --daemon`. Avoids the 2–3 second yfinance/pandas import
/// cost that every `PythonRunner::run()` spawn pays.
///
/// Scope: yfinance_data.py ONLY. Do NOT route agent/framework/numpy1 scripts
/// through here — those need PythonRunner's per-script venv routing (numpy1
/// vs numpy2 selection in PythonRunner.cpp::select_venv_for_script).
///
/// Framing: 4-byte big-endian length prefix, UTF-8 JSON body. Matches
/// `run_daemon()` in scripts/yfinance_data.py.
///
/// Requests carry a monotonically increasing `id`. Callbacks are keyed by id
/// and invoked on the Qt event loop when the matching response arrives.
///
/// Lifecycle: `start()` is lazy — call from the first producer. Restart on
/// unexpected exit is automatic (up to a retry cap). `stop()` closes stdin
/// and waits briefly for a clean exit.
class PythonWorker : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(bool ok, QJsonObject result, QString error)>;

    static PythonWorker& instance();

    /// True once the daemon has sent its `{"ready": true}` handshake and
    /// is accepting requests. Requests submitted before this queue up and
    /// dispatch once ready.
    bool is_ready() const { return ready_; }

    /// Submit one request. `action` maps to the daemon's dispatch table
    /// (batch_all, batch_quotes, batch_sparklines, historical_period, ...).
    /// `payload` is action-specific. Callback is invoked on the hub thread.
    /// Safe to call before the daemon is ready — the request queues.
    void submit(const QString& action, const QJsonObject& payload, Callback cb);

    /// Graceful shutdown — sent automatically on app exit via destructor.
    void stop();

  private:
    PythonWorker();
    ~PythonWorker() override;

    void ensure_started();
    void launch_process();
    void on_process_finished(int exit_code, QProcess::ExitStatus status);
    void on_process_error(QProcess::ProcessError err);
    void on_ready_read();
    void try_drain_frames();
    void dispatch_queued();
    void fail_all_pending(const QString& reason);

    QProcess* proc_ = nullptr;
    bool ready_ = false;
    bool shutting_down_ = false;
    int restart_count_ = 0;
    static constexpr int kMaxRestarts = 5;
    static constexpr int kReadyTimeoutMs = 15'000;  // import yfinance+pandas

    // Pending request tracking. Keyed by request id for response correlation.
    struct Pending {
        QString action;
        QJsonObject payload;
        Callback cb;
    };
    QHash<int, Pending> in_flight_;
    // Requests submitted before the daemon was ready, or queued during restart.
    QVector<QPair<int, Pending>> queue_;
    int next_id_ = 1;

    // Read buffer — frames may span multiple readyRead() signals.
    QByteArray read_buf_;

    // Ready-wait watchdog — if the daemon hasn't handshaked within the
    // timeout, kill it and restart.
    QTimer ready_watchdog_;
};

} // namespace fincept::python
