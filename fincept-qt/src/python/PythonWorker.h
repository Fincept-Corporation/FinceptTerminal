#pragma once

#include <QDeadlineTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVector>

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
///
/// Liveness: the daemon loop is strictly serial (read → dispatch → write), so
/// one wedged Yahoo request stalls every request behind it. Each request
/// therefore carries a deadline; a periodic sweep fails whatever has expired
/// and kills the daemon so the restart path can recover it. Every request gets
/// exactly one callback — see `sweep_deadlines()` for the invariant.
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
    /// Fail every request whose deadline has passed, then kill a daemon that
    /// still owes us an expired in-flight response.
    void sweep_deadlines();
    /// Run the sweep timer only while something is actually outstanding.
    void update_sweep_timer();

    QProcess* proc_ = nullptr;
    bool ready_ = false;
    bool shutting_down_ = false;
    int restart_count_ = 0;
    static constexpr int kMaxRestarts = 5;
    static constexpr int kReadyTimeoutMs = 15'000; // import yfinance+pandas
    /// How often we check deadlines — not the budget itself.
    static constexpr int kSweepIntervalMs = 5'000;
    /// Wall-clock budget per request, measured from submit() (so it covers
    /// queue wait + daemon-start time, which is what the caller experiences).
    /// Sized above the worst legitimate action: batch_all fans out two
    /// yf.download rounds whose per-HTTP budget is _NET_TIMEOUT (15s) in
    /// scripts/yfinance_data.py, plus yfinance's internal retries.
    static constexpr int kRequestTimeoutMs = 90'000;

    // Pending request tracking. Keyed by request id for response correlation.
    struct Pending {
        QString action;
        QJsonObject payload;
        Callback cb;
        /// Set once at submit(); never extended. A request lives in exactly
        /// one of queue_/in_flight_, so whichever container holds it owns the
        /// right to run its callback.
        QDeadlineTimer deadline;
    };
    QHash<int, Pending> in_flight_;
    // Requests submitted before the daemon was ready, or queued during restart.
    QVector<QPair<int, Pending>> queue_;
    // Monotonic and never reset, including across daemon restarts. The
    // exactly-once guarantee depends on this: a response frame from a killed
    // daemon must never collide with an id we handed out afterwards.
    int next_id_ = 1;

    // Read buffer — frames may span multiple readyRead() signals.
    QByteArray read_buf_;

    // Ready-wait watchdog — if the daemon hasn't handshaked within the
    // timeout, kill it and restart. Guards the handshake only.
    QTimer ready_watchdog_;

    // Per-request watchdog — repeating; started on the first outstanding
    // request and stopped when nothing is outstanding.
    QTimer sweep_timer_;
};

} // namespace fincept::python
