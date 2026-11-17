#pragma once
// PythonCliService — generic one-shot wrapper for user-triggered Python CLI
// scripts that return a JSON document on stdout. Exists so screens can
// satisfy D1 (no direct PythonRunner calls) without inventing a domain
// service for each one-off analytical feature.
//
// Use this for **ONE-SHOT** user actions only (a button click that runs
// `<script> <args...>` and displays the result). Anything streaming, cached
// across screens, or refresh-on-cadence belongs on DataHub via a Producer.
// See §D of CLAUDE.md for the exact rule.

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::python_cli {

struct CliResult {
    bool success = false;
    QJsonObject data; // parsed top-level JSON object
    QString error;
    /// The run was stopped through CliRunOptions::cancel_token (error is "Cancelled").
    bool cancelled = false;
};

using CliCallback = std::function<void(const CliResult&)>;

/// Per-call policy for run(script, args, options, cb). Defaults = run(script, args, cb).
struct CliRunOptions {
    /// Watchdog in ms; the process is killed on expiry and the call fails with
    /// "Script timed out after N s and was killed". 0 = the runner's default for the script.
    int timeout_ms = 0;
    /// Raise it (from any thread) to stop the run: a queued run never starts, a running one is
    /// killed within ~250 ms, and the callback gets success=false, cancelled=true.
    std::shared_ptr<std::atomic<bool>> cancel_token;
    /// Written to the script's stdin (then closed). Use it instead of argv for payloads that can
    /// grow (argv is capped at ~32 KB on Windows and visible to other processes).
    QByteArray stdin_data;
    /// Stop a run whose output grows past this many bytes. 0 = no cap.
    qint64 max_output_bytes = 0;
};

/// Drops replies that are no longer wanted: next() when a request starts (it supersedes every
/// earlier one), invalidate() when the context it was made for goes away (the user picked another
/// form, closed the view), is_current(id) in the callback. GUI-thread only.
class RequestGuard {
  public:
    quint64 next() { return ++latest_; }
    void invalidate() { ++latest_; }
    bool is_current(quint64 id) const { return id == latest_; }

  private:
    quint64 latest_ = 0;
};

/// Thin async wrapper around PythonRunner for screens that just need to
/// "run a script and parse its JSON reply."
class PythonCliService : public QObject {
    Q_OBJECT
  public:
    static PythonCliService& instance();

    /// Runs `<script> <args...>`. If stdout contains a JSON object it is
    /// delivered in `CliResult::data`. `success = false` on process failure,
    /// empty output, or a top-level `{"success": false, "error": ...}`
    /// payload. Callback fires on the main thread.
    void run(const QString& script, const QStringList& args, CliCallback cb);
    /// Same, with a timeout, a cancel token and / or stdin input.
    void run(const QString& script, const QStringList& args, const CliRunOptions& options, CliCallback cb);

  private:
    PythonCliService() = default;
};

} // namespace fincept::services::python_cli
