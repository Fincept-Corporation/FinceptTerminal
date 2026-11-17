#pragma once
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQueue>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace fincept::python {

struct PythonResult {
    bool success = false;
    QString output;
    QString error;
    int exit_code = -1;
};

/// Runs Python scripts as subprocesses and returns JSON output.
/// Locates python from venv or system PATH.
/// Limits concurrent processes to avoid overwhelming the system.
class PythonRunner : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(PythonResult)>;
    /// Per-line streaming callback. Invoked on the thread that owns PythonRunner
    /// (main thread in practice) as each complete `\n`-terminated line arrives.
    /// `is_stderr` is true for stderr lines, false for stdout.
    using StreamCallback = std::function<void(QString line, bool is_stderr)>;

    /// Per-request policy. Every field defaults to the behaviour every existing
    /// caller of run() already gets, so opting in is always explicit.
    struct RunOptions {
        /// False when the script's stdout is deliberately NOT JSON (plain text,
        /// CSV, markdown, a bare token). With `expect_json = true` (the default)
        /// PythonResult::success additionally requires that the extracted
        /// payload actually PARSES as JSON and does not carry a script-level
        /// `{"error": ...}` envelope — without that check a script that prints
        /// `{"error": "rate limited"}` and exits 0 is laundered into an empty
        /// success and the UI renders blank instead of falling back to cache.
        ///
        /// Set this per request. Do NOT re-introduce a script-name list here:
        /// whether the output is JSON is a property of the *call*, not the file
        /// (several scripts have both JSON and text-emitting subcommands).
        bool expect_json = true;

        /// Watchdog budget in milliseconds. On expiry the process is killed,
        /// which drives the normal finished/errorOccurred path and frees the
        /// concurrency slot. `0` (or negative) disables the watchdog entirely —
        /// use only for something that genuinely has no upper bound, and
        /// remember that three un-timed hangs wedge every Python-backed feature
        /// until restart (max_concurrent_ is 3).
        ///
        /// Negative sentinel `kTimeoutFromScript` means "let PythonRunner pick
        /// from the script path" (see default_timeout_for_script()).
        int timeout_ms = kTimeoutFromScript;

        /// Written to the child's stdin after spawn; the write channel is then
        /// closed so the script sees EOF. See the note on run() below.
        QByteArray stdin_data{};

        /// Optional cancellation token. Raising it (from any thread) fails a
        /// request that is still queued without spawning it, and kills a running
        /// process within ~250 ms; the callback then receives
        /// `success=false, error="Cancelled"`. Null = not cancellable (default).
        std::shared_ptr<std::atomic<bool>> cancel_token{};

        /// When set, ONLY these stored credentials are injected into the child's
        /// environment (the env vars the script itself reads); every other key
        /// in SecureStorage stays out. Unset = every managed credential, as the
        /// terminal's own screens expect. Agent-started runs (the MCP `python`
        /// tool) always set it, so a script an agent runs cannot read keys it has
        /// no business with (exchange secrets in a data connector's environment).
        std::optional<QStringList> credential_allowlist{};

        /// Stop a run whose stdout grows past this many bytes: the process is
        /// killed and the result fails with "output too large". 0 = no cap.
        qint64 max_output_bytes = 0;

        /// Which lines reach the stream callback (`on_line`). True (default) =
        /// stdout and stderr lines, as before. False = stderr lines only: stdout
        /// is still buffered in full for the result (PythonResult::output, JSON
        /// extraction, max_output_bytes — all unchanged) but is never scanned
        /// for line ends or decoded to QString for streaming on the GUI thread.
        /// Use it when the script reports progress on stderr and prints one
        /// large result on stdout. No effect without a stream callback.
        bool stream_stdout = true;
    };

    /// Sentinel for RunOptions::timeout_ms — resolve the budget from the script
    /// path instead of the caller.
    static constexpr int kTimeoutFromScript = -1;

    static PythonRunner& instance();

    /// Run a script asynchronously. Callback invoked on Qt event loop.
    /// Requests are queued if max concurrency is reached.
    /// Optional `on_line` delivers each complete stdout/stderr line as it arrives
    /// (stderr only with RunOptions::stream_stdout = false, via run_with_options).
    ///
    /// `stdin_data` (optional) is written to the child's stdin immediately after
    /// spawn, and the write channel is then closed so the script sees EOF. Use
    /// this — never argv — for anything sensitive (TOTP seeds, API keys, session
    /// tokens) or large: argv is world-readable to same-user processes
    /// (Win32_Process.CommandLine / /proc/<pid>/cmdline) and is capped at ~32 KB
    /// on Windows. When `stdin_data` is empty the write channel is left exactly
    /// as it is today (untouched), so existing callers are unaffected.
    void run(const QString& script, const QStringList& args, Callback cb, StreamCallback on_line = {},
             const QByteArray& stdin_data = {});

    /// Same as run(), with explicit per-request policy. This is the entry point
    /// for callers that need a non-default timeout or whose script does not emit
    /// JSON — e.g.
    ///   run_with_options(script, args, {.expect_json = false}, cb);
    ///   run_with_options(script, args, {.timeout_ms = 30 * 60 * 1000}, cb);
    void run_with_options(const QString& script, const QStringList& args, const RunOptions& opts, Callback cb,
                          StreamCallback on_line = {});

    /// Run arbitrary Python code (for notebook/colab cells).
    /// Creates a temp file, executes it, returns stdout/stderr.
    void run_code(const QString& code, Callback cb);

    /// Resolve path to python executable
    QString python_path() const;

    /// Resolve path to scripts directory
    QString scripts_dir() const;

    /// Build the standard Python process environment used by all finagent/PyFincept
    /// subprocess spawns. Sets PYTHONIOENCODING, PYTHONDONTWRITEBYTECODE,
    /// PYTHONUNBUFFERED, FINCEPT_DATA_DIR, FINAGENT_DATA_DIR,
    /// FINAGENT_RUNTIME_CACHE_SIZE, and PYTHONPATH (derived from scripts_dir()).
    /// Callers that run a script inside a sub-package may still need to prepend
    /// a "parent-of-pkg" directory to PYTHONPATH themselves — this helper only
    /// sets the shared base env.
    QProcessEnvironment build_python_env() const;

    /// Check if python is available
    bool is_available() const;

    /// Application quit (aboutToQuit): kill every running script together with
    /// everything it started (its process tree), drop the queue — no callback
    /// runs any more — and forget the cached credentials. Idempotent; a no-op
    /// when the runner was never created. GUI thread.
    static void shutdown();

    /// Drop the decrypted credentials cached for build_python_env() (terminal
    /// locked, quit). The next spawn reads them from SecureStorage again.
    /// Thread-safe.
    static void forget_credentials();

    /// User-facing reason a Python-backed feature cannot run yet: "Analytics
    /// libraries are still installing (N%)…" while PythonSetupManager is running,
    /// "Python is not set up yet — …" otherwise. The exact text run()/run_code()
    /// return when no interpreter is resolved; PythonWorker reuses it so the
    /// daemon path and the subprocess path say the same thing. Thread-safe.
    static QString python_unavailable_message();

    /// Set max concurrent Python processes (default: 3)
    void set_max_concurrent(int n) { max_concurrent_ = n; }

    /// Running + queued script requests. Owner thread (the GUI thread) only —
    /// used by the UI automation layer to tell "still loading" from "idle".
    int pending_count() const { return active_count_ + static_cast<int>(queue_.size()); }

  signals:
    void python_ready();

  private:
    PythonRunner();
    QString find_python_sync() const;
    void find_python_async();
    /// Re-resolve the interpreter once PythonSetupManager reports the managed
    /// venvs usable (core_ready / setup_complete) — a runner constructed before
    /// setup finished otherwise keeps an empty or system path until restart.
    void refresh_interpreter();
    QString find_scripts_dir() const;
    void start_next();
    void shutdown_now();

    QString python_path_;
    QString scripts_dir_;
    bool python_init_done_ = false;
    bool shutting_down_ = false; // shutdown() ran: nothing is spawned any more

    // Concurrency limiter
    static constexpr int DEFAULT_MAX_CONCURRENT = 3;
    int max_concurrent_ = DEFAULT_MAX_CONCURRENT;
    int active_count_ = 0;

    struct QueuedRequest {
        QString script;
        QStringList args;
        Callback cb;
        StreamCallback on_line;
        /// Written to the child's stdin after spawn; the write channel is closed
        /// afterwards. Empty = leave stdin untouched (legacy behaviour).
        QByteArray stdin_data{};
        /// True only for scripts that implement the `@<path>` argument-spill
        /// convention (see script_supports_arg_spill() in PythonRunner.cpp).
        /// Spilling a >8 KB argument for a script that does NOT implement it
        /// hands the script the literal string "@C:\...\fincept_arg_*.json",
        /// which it then fails to JSON-parse.
        bool supports_arg_spill = false;
        /// See RunOptions::expect_json.
        bool expect_json = true;
        /// Resolved watchdog budget in ms; 0 = no watchdog.
        int timeout_ms = 0;
        /// See RunOptions::cancel_token. Null = not cancellable.
        std::shared_ptr<std::atomic<bool>> cancel_token{};
        /// See RunOptions::credential_allowlist.
        std::optional<QStringList> credential_allowlist{};
        /// See RunOptions::max_output_bytes.
        qint64 max_output_bytes = 0;
        /// See RunOptions::stream_stdout.
        bool stream_stdout = true;
    };
    QQueue<QueuedRequest> queue_;

    // Incremental output buffering per process
    struct ProcessBuffers {
        QByteArray stdout_buf;
        QByteArray stderr_buf;
        int stdout_streamed = 0; // offset into stdout_buf already streamed
        int stderr_streamed = 0;
        StreamCallback on_line;
        qint64 start_ms = 0;       // spawn timestamp for duration diagnostics
        bool stream_stdout = true; // RunOptions::stream_stdout
    };
    QHash<QProcess*, ProcessBuffers> proc_buffers_;
};

/// Extract last JSON line/block from Python output
QString extract_json(const QString& output);

/// Pure pieces of the per-line stream (PythonRunner's stream callback), QtCore
/// only and tested by tst_python_stream_lines. The runner calls them once per
/// line, looking its buffers up again between lines (the callback may re-enter
/// run()).
namespace stream_detail {

/// Whether lines of one channel go to the stream callback: never without a
/// callback; stderr always; stdout only while `stream_stdout` (RunOptions).
/// When false the channel's bytes are not even scanned for line ends.
inline bool streams_channel(bool has_callback, bool stream_stdout, bool is_stderr) {
    return has_callback && (is_stderr || stream_stdout);
}

/// The next complete line of `buf` from `*offset`: true with `*line` = the bytes
/// up to (not including) the '\n', one trailing '\r' removed (CRLF), and
/// `*offset` moved past the '\n'. False when no '\n' follows `*offset` — a
/// partial tail stays in the buffer for the next read (and is not streamed when
/// the process ends: the full buffer is the result / error text).
inline bool next_line(const QByteArray& buf, int* offset, QByteArray* line) {
    const qsizetype nl = buf.indexOf('\n', *offset);
    if (nl < 0)
        return false;
    *line = buf.mid(*offset, nl - *offset);
    if (line->endsWith('\r'))
        line->chop(1);
    *offset = static_cast<int>(nl + 1);
    return true;
}

} // namespace stream_detail

} // namespace fincept::python
