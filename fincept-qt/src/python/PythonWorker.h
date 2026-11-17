#pragma once

#include <QByteArray>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <functional>
#include <memory>

class QThread;

namespace fincept::python {

class PythonWorkerDecoder;

/// Persistent yfinance worker — long-lived Python process running
/// `yfinance_data.py --daemon`. Avoids the 2–3 second yfinance/pandas import
/// cost that every `PythonRunner::run()` spawn pays.
///
/// Scope: yfinance_data.py ONLY. Do NOT route agent/framework/numpy1 scripts
/// through here — those need PythonRunner's per-script venv routing (numpy1
/// vs numpy2 selection in PythonRunner.cpp::select_venv_for_script).
///
/// Interpreter: ONLY the app-managed venv-numpy2 that PythonSetupManager
/// installs yfinance into, and only once PythonSetupManager reports the
/// essentials installed (is_core_ready()). There is deliberately no system
/// `python` / `python3` fallback: an interpreter the app does not manage has no
/// yfinance, and every start would die with ModuleNotFoundError.
///
/// Framing: 4-byte big-endian length prefix, UTF-8 JSON body. Matches
/// `run_daemon()` in scripts/yfinance_data.py.
///
/// Requests carry a monotonically increasing `id`. Callbacks are keyed by id
/// and invoked on the Qt event loop when the matching response arrives.
///
/// Replies never get parsed on the owner (GUI) thread: the owner only copies
/// the bytes QProcess has buffered and hands them to a decode thread, which
/// splits frames, parses the JSON (a symbol load can be megabytes) and, for
/// submit_prepared(), runs the caller's off-thread step. Decoded frames come
/// back through the owner's event queue in arrival order, so per-request
/// ordering, exactly-once and the deadline sweep work as before. A daemon's
/// death is handled only after every frame it wrote has been delivered.
///
/// State machine (owner thread only):
///
///   Idle ──launch──▶ Starting ──{"ready":true}──▶ Ready
///    │                  │ exit / handshake timeout      │ exit / killed as wedged
///    │                  ▼                               ▼
///    │              BackingOff ◀─────────────────────────┘
///    │                  │ restart_timer_ → launch again (Starting), or Unavailable
///    │                  │ budget spent (failures while setup is idle) → Capped
///    ▼                  ▼
///   Unavailable       Capped ──reset (setup_complete ok / python_ready / retry())──▶ Idle
///
///   Unavailable: script missing, managed venv absent, essentials not installed
///   (setup running / skipped / failed). Re-checked on every submit and on the
///   setup signals; requests fail on the next event-loop turn with
///   PythonRunner::python_unavailable_message() (or "Script not found: …").
///   Capped: requests fail fast with the last daemon error; one probe start is
///   allowed after kCappedProbeIntervalMs so a transient cause can heal.
///   Stopped: terminal, entered by stop().
///
/// Where a request waits: Starting queues; Ready writes straight through;
/// BackingOff queues after a crash of a daemon that had been serving (it is
/// probably transient) but fails fast after a failed START (the next start is
/// likely to fail the same way, so the caller gets the reason now).
///
/// Liveness: the daemon loop is strictly serial (read → dispatch → write), so
/// one wedged Yahoo request stalls every request behind it. Each request
/// therefore carries a deadline; a periodic sweep fails whatever has expired
/// and kills the daemon so the restart path can recover it.
///
/// Exactly-once: every request lives in exactly one of queue_ / in_flight_ /
/// failed_ until its callback runs, and is moved out of that container before
/// the callback is invoked.
class PythonWorker : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(bool ok, QJsonObject result, QString error)>;

    static PythonWorker& instance();

    /// True once the daemon has sent its `{"ready": true}` handshake and
    /// is accepting requests. Owner thread only.
    bool is_ready() const { return state_ == State::Ready; }

    /// Submit one request. `action` maps to the daemon's dispatch table
    /// (batch_all, batch_quotes, batch_sparklines, historical_period, ...).
    /// `payload` is action-specific. The callback is invoked exactly once, on
    /// the worker's thread. Safe to call from any thread and before the daemon
    /// is ready — the request queues while the daemon is starting. When the
    /// daemon cannot run (see the state machine above) the callback fires on the
    /// next event-loop turn with ok=false and a user-facing reason; it is never
    /// invoked synchronously from inside submit().
    void submit(const QString& action, const QJsonObject& payload, Callback cb);

    /// Off-thread reply step for submit_prepared(). Runs on the decode thread
    /// right after the reply is parsed and returns the continuation the owner
    /// thread then runs (exactly once, in reply order, same rules as submit()'s
    /// callback). It must be thread-agnostic — data in, data out, no QObject or
    /// GUI access. A request that ends without a reply (timeout, daemon death,
    /// unavailable, shutdown) runs it on the owner thread with ok=false and an
    /// empty result. The continuation is always destroyed on the owner thread,
    /// so it may own the caller's callback.
    using Prepare = std::function<std::function<void()>(bool ok, const QJsonObject& result, const QString& error)>;

    /// submit() for callers that would otherwise do heavy work with the reply
    /// on the owner thread (serialising it, building a model from it).
    void submit_prepared(const QString& action, const QJsonObject& payload, Prepare prepare);

    /// Explicit user retry: clears the restart budget and any backoff, re-reads
    /// the Python setup status once, and starts the daemon if it can run.
    /// No-op after stop(). Safe to call from any thread.
    void retry();

    /// Graceful shutdown — idempotent. Called from aboutToQuit (and again by
    /// the destructor). Fails every outstanding request synchronously with
    /// "worker shutting down"; nothing is restarted afterwards.
    void stop();

  private:
    friend class PythonWorkerDecoder;

    enum class State { Idle, Unavailable, Starting, Ready, BackingOff, Capped, Stopped };
    /// Why the daemon cannot be launched right now (Unavailable).
    enum class Blocker { None, ScriptMissing, SetupRunning, NotSetUp };

    struct Pending {
        int id = 0;
        QString action;
        QJsonObject payload;
        /// Exactly one of the two is set.
        Callback cb;
        Prepare prepare;
        /// Set once at submit(); never extended.
        QDeadlineTimer deadline;
        /// Started when the request is written to the daemon. A request whose
        /// deadline expired mostly while it waited for the handshake does not
        /// prove the daemon is wedged — see sweep_deadlines().
        QElapsedTimer in_flight_since;
    };
    struct Failed {
        Pending p;
        QString reason;
    };

    /// One daemon frame as the decode thread hands it over.
    struct DecodedFrame {
        /// Length prefix past the cap: the stream cannot be re-synchronised.
        bool corrupt = false;
        quint32 corrupt_size = 0;
        bool ready = false; // {"ready": true} handshake
        int pid = 0;
        int id = 0;
        bool ok = false;
        QString error;
        /// Non-object results are wrapped under "_value". Empty when `prepared`
        /// is set (the Prepare step consumed it on the decode thread).
        QJsonObject result;
        /// Continuation returned by the request's Prepare step, if it has one.
        std::function<void()> prepared;
    };

    /// Prepare steps of outstanding submit_prepared() requests, by id. Written
    /// on the owner thread (added at submit, removed at completion), read on
    /// the decode thread. The entry exists before the request can be written
    /// to the daemon, so the decoder always sees it for a live reply.
    struct PrepareRegistry {
        QMutex mutex;
        QHash<int, Prepare> by_id;
    };

    PythonWorker();
    ~PythonWorker() override;

    void enqueue(Pending p);
    /// Run the request's callback (or Prepare step + continuation) and drop
    /// its registry entry. Owner thread.
    void complete(Pending& p, bool ok, const QJsonObject& result, const QString& error);
    void forget_prepare(const Pending& p);

    static const char* state_name(State s);

    /// Idle / Unavailable → try_launch(); every other state is left alone.
    void ensure_started();
    /// Clear the restart budget and any backoff/cap, then ensure_started().
    void reset_restart_budget(const QString& why);
    /// Empty `reason` and Blocker::None when the daemon may start; then
    /// `python_exe` / `script_path` are filled in.
    Blocker launch_blocker(QString* python_exe, QString* script_path, QString* reason);
    /// Gate + launch. Returns false (and enters Unavailable) when blocked.
    bool try_launch();
    void launch_process(const QString& python_exe, const QString& script_path);
    void enter_unavailable(Blocker blocker, const QString& reason);
    void enter_capped(const QString& cause);
    /// Disconnect and deleteLater the current QProcess (no signal from it can
    /// reach this object afterwards).
    void retire_process();

    void on_process_finished(int exit_code, QProcess::ExitStatus status);
    /// Second half of on_process_finished(), run once the decode thread has
    /// delivered every frame of stream `gen`.
    void finish_process_death(quint64 gen, int exit_code, QProcess::ExitStatus status);
    void on_process_error(QProcess::ProcessError err);
    /// The daemon is gone (exit, crash, kill, or failed to start).
    void handle_daemon_death(const QString& cause);
    void on_ready_read();
    void dispatch_queued();

    // ── reply decoding (decode thread) ───────────────────────────────────────
    void ensure_decoder();
    /// Stop and join the decode thread. Idempotent; aboutToQuit / destructor
    /// only (the wait is bounded by one frame's parse).
    void shutdown_decoder();
    /// Hand bytes of the current stdout stream to the decode thread.
    void feed_decoder(QByteArray chunk);
    /// The current stream is finished with: nothing more of it is delivered.
    void drop_stream();
    /// Decoded frames of stream `gen`, in arrival order. Owner thread.
    void on_frames_decoded(quint64 gen, const QVector<DecodedFrame>& frames);

    // ── stderr: buffered by line, logged as one entry per burst ──────────────
    void on_stderr_ready();
    /// Split complete lines out of stderr_partial_; `final` also takes the
    /// unterminated remainder (process gone).
    void split_stderr_lines(bool final);
    void add_stderr_line(QByteArray line);
    void flush_stderr_log();
    /// Concise cause for a dying daemon from its recent stderr ("" if none).
    QString stderr_cause(bool allow_last_line) const;

    // ── completion ───────────────────────────────────────────────────────────
    /// Park a request for failure on the next event-loop turn.
    void fail_later(Pending p, const QString& reason);
    void fail_queue_later(const QString& reason);
    void fail_in_flight_later(const QString& reason);
    /// Run every parked failure callback now.
    void flush_failures();
    /// Fail every request whose deadline has passed, then kill a daemon that
    /// still owes us an expired in-flight response.
    void sweep_deadlines();
    /// Run the sweep timer only while something is actually outstanding.
    void update_sweep_timer();

    QProcess* proc_ = nullptr;
    State state_ = State::Idle;

    // Restart budget. Counts consecutive daemon failures that happened while
    // Python setup was idle; a clean handshake (or a reset) zeroes it.
    int restart_count_ = 0;
    // Consecutive failures of any kind — drives the backoff delay only.
    int backoff_step_ = 0;
    static constexpr int kMaxRestarts = 5;
    static constexpr int kBackoffBaseMs = 1'000; // 1, 2, 4, 8, 16, 30, 30 … s
    static constexpr int kBackoffMaxMs = 30'000;
    /// Capped: allow one probe start (on submit) after this long.
    static constexpr int kCappedProbeIntervalMs = 5 * 60 * 1'000;
    /// Handshake budget. A cold import of pandas+yfinance with no .pyc on a
    /// slow disk / under antivirus — exactly what the first launch after setup
    /// looks like while compileall still runs — takes well over 15 s.
    static constexpr int kReadyTimeoutMs = 60'000;
    /// How often we check deadlines — not the budget itself.
    static constexpr int kSweepIntervalMs = 5'000;
    /// Wall-clock budget per request, measured from submit() (so it covers
    /// queue wait + daemon-start time, which is what the caller experiences).
    /// Sized above the worst legitimate action: batch_all fans out two
    /// yf.download rounds whose per-HTTP budget is _NET_TIMEOUT (15s) in
    /// scripts/yfinance_data.py, plus yfinance's internal retries.
    static constexpr int kRequestTimeoutMs = 90'000;
    /// An expired request only proves the daemon wedged once the daemon itself
    /// has held it this long.
    static constexpr int kWedgeMinInFlightMs = kRequestTimeoutMs / 2;

    // Pending request tracking. Keyed by request id for response correlation.
    QHash<int, Pending> in_flight_;
    // Requests submitted while the daemon is starting, or held across a restart.
    QVector<QPair<int, Pending>> queue_;
    // Requests already decided to fail; completed by flush_failures().
    QVector<Failed> failed_;
    // Monotonic and never reset, including across daemon restarts. The
    // exactly-once guarantee depends on this: a response frame from a killed
    // daemon must never collide with an id we handed out afterwards.
    int next_id_ = 1;

    // Decode thread and the decoder living on it. The decoder owns the frame
    // buffer (frames may span multiple readyRead() signals).
    QThread* decode_thread_ = nullptr;
    PythonWorkerDecoder* decoder_ = nullptr;
    // Identifies the current daemon's stdout stream. Bumped at every launch and
    // whenever a stream is dropped; decoded frames of any other stream are
    // ignored, so nothing from a retired daemon or a stopped worker is delivered.
    quint64 stream_gen_ = 0;
    std::shared_ptr<PrepareRegistry> prepares_ = std::make_shared<PrepareRegistry>();

    // stderr of the current daemon: unterminated tail, last lines (for the
    // failure cause), and the block awaiting one multi-line log entry.
    QByteArray stderr_partial_;
    QStringList stderr_tail_;
    QStringList stderr_log_block_;

    // Why we killed the daemon ourselves (handshake timeout, wedge, bad frame);
    // takes precedence over stderr when describing its death.
    QString kill_reason_;
    // BackingOff: was the last failure a failed START (fail fast) or a crash
    // of a daemon that had been serving (queue for the restart)?
    bool last_start_failed_ = false;
    QString last_failure_;
    // Unavailable / Capped bookkeeping.
    Blocker blocker_ = Blocker::None;
    QString unavailable_reason_;
    QString capped_reason_;
    QElapsedTimer capped_since_;
    // Started at each launch — handshake / lifetime durations for the log.
    QElapsedTimer launch_clock_;
    // is_core_ready() is a cache that check_status() fills; re-read it once
    // (and again after retry()) instead of trusting a never-filled cache.
    bool status_probed_ = false;

    // Handshake watchdog — kills a daemon that has not said ready in time.
    QTimer ready_watchdog_;
    // Per-request deadline sweep — repeating; runs only while work is outstanding.
    QTimer sweep_timer_;
    // BackingOff → next launch attempt.
    QTimer restart_timer_;
    // Zero-delay single shot that completes failed_ on the next event-loop turn.
    QTimer fail_timer_;
    // Debounce that turns a burst of stderr (a traceback) into one log entry.
    QTimer stderr_log_timer_;
};

} // namespace fincept::python
