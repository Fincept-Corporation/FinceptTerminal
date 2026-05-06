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

/// Persistent option-Greeks worker — long-lived Python process running
/// `option_greeks_daemon.py --daemon`. Sibling to `PythonWorker` (which is
/// scoped to yfinance_data.py only); same 4-byte big-endian length-prefix
/// JSON framing, same singleton + lazy-launch + auto-restart shape.
///
/// Scope: option_greeks_daemon.py ONLY. Backed by py_vollib living in the
/// venv-numpy2 venv. Sole supported action: `option_greeks_batch`.
///
/// Requests carry a monotonically increasing `id`. Callbacks are keyed by
/// id and invoked on the Qt event loop when the matching response arrives.
class OptionGreeksWorker : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(bool ok, QJsonObject result, QString error)>;

    static OptionGreeksWorker& instance();

    bool is_ready() const { return ready_; }

    /// Submit one request. Safe to call before the daemon is ready — the
    /// request queues and dispatches once the handshake completes.
    void submit(const QString& action, const QJsonObject& payload, Callback cb);

    /// Graceful shutdown — sent automatically on app exit via destructor.
    void stop();

  private:
    OptionGreeksWorker();
    ~OptionGreeksWorker() override;

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
    static constexpr int kReadyTimeoutMs = 20'000;  // py_vollib + scipy import

    struct Pending {
        QString action;
        QJsonObject payload;
        Callback cb;
    };
    QHash<int, Pending> in_flight_;
    QVector<QPair<int, Pending>> queue_;
    int next_id_ = 1;

    QByteArray read_buf_;

    QTimer ready_watchdog_;
};

} // namespace fincept::python
