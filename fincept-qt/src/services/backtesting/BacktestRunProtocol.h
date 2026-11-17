// src/services/backtesting/BacktestRunProtocol.h
//
// The host side of the protocol between the terminal and a backtesting provider
// process (scripts/Analytics/backtesting, contract v2 "PROGRESS LINES" and rule R2):
//
//   * STDERR may carry progress lines, at most 4 per second per phase:
//       @@BT_PROGRESS {"phase":"optimize","done":240,"total":500,"message":"…",
//                      "best":1.21,"etaSec":14,"workers":4}
//     Unknown keys are ignored; an unparseable line is not progress.
//   * STDOUT carries exactly one JSON envelope:
//       {"success": true, "data": {...}}
//       {"success": false, "error": "<sentence>", "errorCode": "DATA|INPUT|ENGINE|MISSING_PACKAGE|
//        CANCELLED|TIMEOUT|INTERNAL", "details": {"traceback", "logs", "symbol"}}
//     Exit code 0 for both; 3 for a cooperative cancel.
//
// classify_run_output() turns what a finished process left behind into one of
// Ok / Failed / Cancelled. A process that was killed, crashed, or printed no
// envelope is NEVER an (empty) success.
//
// Pure QtCore: unit-tested by tests/tst_backtest_protocol.cpp.
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

#include <limits>
#include <optional>

namespace fincept::services::backtest {

/// One parsed progress line.
struct BtProgress {
    QString phase;     ///< data | run | optimize | walk_forward | monte_carlo | sweep | metrics (others kept as sent)
    qint64 done = -1;  ///< units finished in this phase; -1 = not given
    qint64 total = -1; ///< -1 = indeterminate
    QString message;   ///< short plain text from the engine
    double best = std::numeric_limits<double>::quiet_NaN();    ///< best objective so far (optimise / walk-forward)
    double eta_sec = std::numeric_limits<double>::quiet_NaN(); ///< seconds left; NaN = unknown
    int workers = -1;  ///< worker processes in use; -1 = not given

    /// done / total are known (a determinate bar can be drawn).
    bool determinate() const { return total > 0 && done >= 0; }
    /// done / total clamped to [0, 1]; NaN when indeterminate.
    double fraction() const;
};

/// The prefix a progress line starts with (after optional leading blanks).
QString progress_prefix();

/// A progress line -> BtProgress; nullopt for any other line (log text, a
/// malformed object, a line without the prefix).
std::optional<BtProgress> parse_progress_line(const QString& line);

/// Plain label of a phase id ("optimize" -> "Optimising"); the id itself when unknown.
QString progress_phase_label(const QString& phase);

/// One-line text for a status bar / skeleton caption:
///   "Optimising 240 / 500 · best 1.21 · ETA 14 s · 4 workers"
QString progress_text(const BtProgress& p);

/// "14 s", "3 min 20 s", "1 h 05 min"; empty for NaN / negative.
QString format_eta(double seconds);

/// `stderr_text` without its progress lines (what is left is the diagnostic tail).
QString strip_progress_lines(const QString& stderr_text);

/// What a finished provider process amounts to.
struct BtOutcome {
    enum class Kind { Ok, Failed, Cancelled };
    Kind kind = Kind::Failed;
    /// Contract error class (DATA, INPUT, ENGINE, MISSING_PACKAGE, CANCELLED, TIMEOUT, INTERNAL) or one of the
    /// host's own: NO_ENVELOPE (valid JSON without "success"), BAD_OUTPUT (no / unparseable JSON), PROCESS
    /// (the process failed and printed nothing usable). Empty for Ok.
    QString error_code;
    /// One sentence a user can act on (empty for Ok).
    QString message;
    /// Traceback / provider logs / stderr tail for a DETAILS view (never shown as the message).
    QString technical;
    /// The symbol the failure is about, when the provider named one.
    QString symbol;
    /// The parsed stdout envelope (may be empty).
    QJsonObject envelope;
    /// Ok: the result payload (`data`; the root itself for a legacy reply without one).
    QJsonObject data;

    bool ok() const { return kind == Kind::Ok; }
    bool cancelled() const { return kind == Kind::Cancelled; }
};

/// Classify a finished run from PythonRunner's result:
///   runner_success  PythonResult::success
///   exit_code       PythonResult::exit_code (-1 when the process never ran / was killed)
///   runner_error    PythonResult::error (stderr, an envelope's message, "Cancelled", a timeout text)
///   stdout_json     PythonResult::output (the extracted JSON, may be empty)
/// Safe on any thread (parses the JSON).
BtOutcome classify_run_output(bool runner_success, int exit_code, const QString& runner_error,
                              const QByteArray& stdout_json);

/// Exit code of a cooperative cancel.
inline constexpr int kCancelExitCode = 3;

} // namespace fincept::services::backtest

Q_DECLARE_METATYPE(fincept::services::backtest::BtProgress)
Q_DECLARE_METATYPE(fincept::services::backtest::BtOutcome)
